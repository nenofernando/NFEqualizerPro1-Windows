// PHYSICAL C2.3a/b: refine the prominence-assistance blend with
// mainLowBinReliability (evidence-based, not hardcoded bin thresholds),
// separate auxProminenceReliability/auxFrequencyReliability, a needForAux
// factor, and a frequency-location blend kept entirely separate from the
// prominence-value blend (magnitude-based, log-Hz space). Also
// instruments the Bass Clean @192kHz 3->6 region question with real
// per-frame data instead of guessing. Diagnostic only.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static std::vector<float> genWhiteNoise(int n, float amp) { std::vector<float> b((size_t) n); juce::Random rng(31); for (auto& s : b) s = (rng.nextFloat() * 2.0f - 1.0f) * amp; return b; }
static std::vector<float> genVocalMale(double sr, int n) { return genHarmonicSeries(sr, n, 100.0, 0.3f, 10, 2.5f); }

static float crossoverWeight(float hz, float lowHz, float highHz) { if (hz <= lowHz) return 1.0f; if (hz >= highHz) return 0.0f; float t = (hz - lowHz) / (highHz - lowHz); return 1.0f - (t * t * (3.0f - 2.0f * t)); }
static float resolutionAdvantageWeight(double mainBinHz, double auxBinHz) { if (mainBinHz <= 0.0) return 0.0f; double ratio = auxBinHz / mainBinHz; return (float) juce::jlimit(0.0, 1.0, 1.0 - ratio); }
static float parabolicDelta(float l, float c, float r) { float denom = l - 2.0f * c + r; if (std::abs(denom) < 1.0e-6f) return 0.0f; return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom); }

// C2.3a item 1: evidence-based main low-bin reliability. No hardcoded
// "bin<=3=bad" -- derived from (a) magnitude-vs-prominence local-ranking
// disagreement between this bin's two neighbors, (b) resolution coarseness
// (binHz relative to the bin's own frequency -- naturally worse near DC,
// without a hardcoded index), (c) how much prominence's own asymmetry
// diverges from magnitude's own asymmetry at this bin.
static float mainLowBinReliability(int bin, const std::vector<float>& magDb, const std::vector<float>& promOut, double binHz)
{
    int n = (int) magDb.size();
    if (bin < 1 || bin >= n - 1) return 0.5f;
    float magL = magDb[(size_t) (bin - 1)], magC = magDb[(size_t) bin], magR = magDb[(size_t) (bin + 1)];
    float promL = promOut[(size_t) (bin - 1)], promC = promOut[(size_t) bin], promR = promOut[(size_t) (bin + 1)];

    // (a) local-max agreement: does magnitude's own best-of-3 match prominence's?
    auto bestOf3 = [](float l, float c, float r) { if (c >= l && c >= r) return 0; return (l > r) ? -1 : 1; };
    int magBest = bestOf3(magL, magC, magR), promBest = bestOf3(promL, promC, promR);
    float orderAgreement = (magBest == promBest) ? 1.0f : 0.0f;

    // (c) asymmetry divergence: normalize each domain's own L/R imbalance, compare.
    float magAsym = (magR - magL) / juce::jmax(1.0f, std::abs(magR) + std::abs(magL));
    float promAsym = (promR - promL) / juce::jmax(1.0f, std::abs(promR) + std::abs(promL));
    float asymDivergence = juce::jlimit(0.0f, 1.0f, std::abs(magAsym - promAsym));

    // (b) resolution coarseness relative to this bin's own frequency -- an
    // evidence-derived stand-in for "how close to DC", not a fixed index.
    float freqHz = (float) (bin * binHz);
    float relRes = juce::jlimit(0.0f, 1.0f, 1.0f - (float) (binHz / juce::jmax(1.0, (double) freqHz)));

    float reliability = juce::jlimit(0.0f, 1.0f, relRes * orderAgreement * (1.0f - asymDivergence));
    return reliability;
}

// C2.3c: does THIS source's own prominence-peak bin agree with THIS
// source's own raw-magnitude-peak bin (searched in the same small window)?
// This is the single most diagnostic signal for the known artifact (main
// prominence landing on an entirely DIFFERENT bin than main magnitude at
// 192kHz near DC) vs ordinary sub-bin interpolation noise (main prominence
// and magnitude AGREE on the bin, just differ by a few cents in the
// parabolic refinement -- seen at 96kHz/100Hz, where main was actually
// fine and the C2.3a formula wrongly discounted it).
static int localPeakBin(const std::vector<float>& arr, int approxBin, int radius)
{
    int n = (int) arr.size();
    int best = juce::jlimit(1, n - 2, approxBin); float bestVal = arr[(size_t) best];
    for (int b = juce::jmax(1, approxBin - radius); b <= juce::jmin(n - 2, approxBin + radius); ++b)
        if (arr[(size_t) b] > bestVal) { bestVal = arr[(size_t) b]; best = b; }
    return best;
}
static float physicalConsistencyFromBins(int promBin, int magBin) { return (promBin == magBin) ? 1.0f : juce::jmax(0.0f, 1.0f - 0.5f * std::abs((float) (promBin - magBin))); }

struct BlendedResult { float prominenceDb; float estimatedHz; float auxWeight; float mainAuthority; float auxAuthority; };

static BlendedResult effectiveProminenceAndFrequency(
    float mainProm, float mainMagHz, // main's own raw-magnitude sub-bin estimate at this bin
    float queryHz, const LowFrequencyHarmonicAnalyzer& aux, double mainBinHz,
    int mainBin, const std::vector<float>& magDb, const std::vector<float>& promOut,
    float crossLow, float crossHigh)
{
    float auxEst = 0, auxValRel = 0, auxFreqRel = 0;
    float auxDb = aux.auxProminenceFor(queryHz, &auxEst, &auxValRel, &auxFreqRel);
    float freqWeight = crossoverWeight(queryHz, crossLow, crossHigh);
    float resAdv = resolutionAdvantageWeight(mainBinHz, aux.analysisBinHz());
    float mainReliab = mainLowBinReliability(mainBin, magDb, promOut, mainBinHz);

    // C2.3c: physical consistency PER SOURCE -- does that source's own
    // prominence peak bin agree with that source's own magnitude peak bin?
    int mainMagBin = localPeakBin(magDb, mainBin, 2);
    float mainPhysCons = physicalConsistencyFromBins(mainBin, mainMagBin);
    double auxBinHz = aux.analysisBinHz();
    int auxApproxBin = juce::jlimit(1, (int) aux.debugMagDb().size() - 2, (int) std::round((double) queryHz / auxBinHz));
    int auxPromBin = localPeakBin(std::vector<float>(aux.debugProminence().begin(), aux.debugProminence().end()), auxApproxBin, 2);
    int auxMagBin = localPeakBin(std::vector<float>(aux.debugMagDb().begin(), aux.debugMagDb().end()), auxApproxBin, 2);
    float auxPhysCons = physicalConsistencyFromBins(auxPromBin, auxMagBin);

    // C2.3c authority: physicalConsistency is the DOMINANT, near-gating
    // term for each source (a source whose own prominence and magnitude
    // disagree on which BIN is the peak is structurally broken and gets
    // near-zero authority regardless of anything else); its own
    // reliability/valueReliability only provides a MILD secondary
    // adjustment on top once consistency is confirmed -- this is what
    // stops ordinary sub-bin interpolation noise (96kHz/100Hz: main
    // consistent, just a few cents off) from being treated the same as a
    // genuine structural failure (192kHz/100Hz: main inconsistent by a
    // whole bin).
    float mainAuthority = mainPhysCons * juce::jlimit(0.3f, 1.0f, 0.5f + 0.5f * mainReliab);
    float auxAuthority = freqWeight * resAdv * auxPhysCons * juce::jlimit(0.3f, 1.0f, 0.5f + 0.5f * auxValRel);

    float valueAuxWeight = (mainAuthority + auxAuthority > 1.0e-6f) ? juce::jlimit(0.0f, 1.0f, auxAuthority / (mainAuthority + auxAuthority)) : 0.0f;
    float blendedProm = valueAuxWeight * auxDb + (1.0f - valueAuxWeight) * mainProm;

    // Frequency location: SEPARATE blend, magnitude-based on BOTH sides, in
    // log-Hz space -- never derived from blendedProm, and NOT tied 1:1 to
    // the value-authority ratio (tried that, it broke 120Hz@192kHz: aux's
    // own physical consistency was only 0.217 there -- genuinely a bit
    // ambiguous -- but aux's raw-magnitude LOCATION was still far closer to
    // the truth than main's completely-broken one, so location trust needs
    // its own weighting, using auxFreqRel directly plus mainPhysCons as a
    // floor-lift for main's own location claim (a main that's internally
    // inconsistent shouldn't get to argue for its own location either).
    float mainLocationTrust = mainPhysCons; // 0 when main's own prom/mag bins disagree -- exactly the artifact signature
    float auxLocationTrust = resAdv * auxFreqRel;
    float freqAuxWeight = (mainLocationTrust + auxLocationTrust > 1.0e-6f) ? juce::jlimit(0.0f, 1.0f, auxLocationTrust / (mainLocationTrust + auxLocationTrust)) : 0.5f;
    float logMain = std::log2(juce::jmax(1.0f, mainMagHz));
    float logAux = std::log2(juce::jmax(1.0f, auxEst));
    float logBlend = freqAuxWeight * logAux + (1.0f - freqAuxWeight) * logMain;
    float effHz = std::pow(2.0f, logBlend);

    return { blendedProm, effHz, valueAuxWeight, mainAuthority, auxAuthority };
}

struct MainProm
{
    SpectralProminenceEngineV5 prom;
    juce::dsp::FFT fft{ 11 };
    std::array<float, 2048> window{};
    std::array<float, 4096> scratch{};
    std::vector<float> magDb, promOut;
    void prepare(double sr) { prom.prepare(2048 / 2 + 1, sr, 2048); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f);
        magDb.assign(2048 / 2 + 1, -120.0f); promOut.assign(2048 / 2 + 1, 0.0f); }
    void processFrame(const float* samples)
    {
        for (int k = 0; k < 2048; ++k) scratch[(size_t) k] = samples[k] * window[(size_t) k];
        std::fill(scratch.begin() + 2048, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins = 2048 / 2 + 1;
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / 2048.0f + 1e-12f, -120.0f);
        }
        prom.computeProminence(magDb, 4.0f, promOut);
    }
};

// ---------------- item: targeted recall test (80/100/120/170/250Hz @ 96/192kHz + spot-check 44.1/48) ----------------
struct Pipeline
{
    static constexpr int kFft = 2048, kHop = 512;
    double sr = 48000.0;
    SpectralProminenceEngineV5 prom;
    LowFrequencyHarmonicAnalyzer aux;
    ConfidenceEngine conf;
    juce::dsp::FFT fft{ 11 };
    std::array<float, kFft> window{};
    std::array<float, kFft * 2> scratch{};
    std::vector<float> magDb, promOut, promOutBlended;
    bool useBlend = false;
    float crossLow = 300.0f, crossHigh = 800.0f;

    void prepare(double sampleRate, bool blend)
    {
        sr = sampleRate; useBlend = blend;
        prom.prepare(kFft / 2 + 1, sr, kFft); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        aux.prepare(sr); conf.prepare(sr, kFft, kHop);
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        magDb.assign((size_t) (kFft / 2 + 1), -120.0f); promOut.assign((size_t) (kFft / 2 + 1), 0.0f); promOutBlended.assign((size_t) (kFft / 2 + 1), 0.0f);
    }
    void run(const std::vector<float>& sig)
    {
        int n = (int) sig.size();
        double hostBinHz = sr / kFft;
        int blendBinLimit = juce::jmin((int) magDb.size() - 2, (int) std::ceil(crossHigh / hostBinHz) + 2);
        for (int i = 0; i + kFft <= n; i += kHop)
        {
            for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            const int bins = kFft / 2 + 1;
            for (int b = 0; b < bins; ++b)
            {
                float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
                magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f);
            }
            prom.computeProminence(magDb, 4.0f, promOut);
            aux.pushSamples(sig.data() + i, kHop);
            const std::vector<float>* feedProm = &promOut;
            if (useBlend)
            {
                promOutBlended = promOut;
                for (int b = 1; b <= blendBinLimit; ++b)
                {
                    float mainMagHz = (float) ((b + parabolicDelta(magDb[(size_t) (b - 1)], magDb[(size_t) b], magDb[(size_t) (b + 1)])) * hostBinHz);
                    auto r = effectiveProminenceAndFrequency(promOut[(size_t) b], mainMagHz, (float) (b * hostBinHz), aux, hostBinHz, b, magDb, promOut, crossLow, crossHigh);
                    promOutBlended[(size_t) b] = r.prominenceDb;
                }
                feedProm = &promOutBlended;
            }
            conf.process(*feedProm, nullptr);
        }
    }
};
static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& c, float hz, float tol = 0.25f)
{
    float target = std::log2(juce::jmax(1.0f, hz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
    for (auto& r : c.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
    if (best && bestDist <= tol) return *best; return ConfidenceEngine::Region{};
}

int main()
{
    const double targetFreqs[] = { 80, 100, 120, 170, 250 };
    const float amps[] = { 3.0f, 4.0f, 6.0f, 9.0f };
    const double qs[] = { 10.0, 6.0, 3.0 };

    std::printf("=== TARGETED RETEST: 80/100/120/170/250Hz @ 96/192kHz (main vs refined blend) ===\n");
    for (double sr : { 96000.0, 192000.0 })
    {
        std::printf(" -- sr=%.0f --\n", sr);
        for (double f : targetFreqs)
        {
            int mainMissed = 0, blendMissed = 0, total = 0;
            for (float amp : amps) for (double q : qs)
            {
                int n = (int) (sr * 0.5);
                auto sig = genSilence(n);
                addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, q, 5);
                Pipeline pMain; pMain.prepare(sr, false); pMain.run(sig);
                Pipeline pBlend; pBlend.prepare(sr, true); pBlend.run(sig);
                ++total; if (! findRegionNear(pMain.conf, (float) f).active) ++mainMissed;
                if (! findRegionNear(pBlend.conf, (float) f).active) ++blendMissed;
            }
            std::printf("    f=%6.1fHz: main-missed=%d/%d (%.0f%%)  refined-blend-missed=%d/%d (%.0f%%)\n",
                f, mainMissed, total, 100.0*mainMissed/total, blendMissed, total, 100.0*blendMissed/total);
        }
    }

    std::printf("\n=== SPOT-CHECK 44.1/48kHz (must remain == main) ===\n");
    for (double sr : { 44100.0, 48000.0 })
    {
        int mainMissed = 0, blendMissed = 0, total = 0;
        for (double f : targetFreqs) for (float amp : amps) for (double q : qs)
        {
            int n = (int) (sr * 0.5);
            auto sig = genSilence(n);
            addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, q, 5);
            Pipeline pMain; pMain.prepare(sr, false); pMain.run(sig);
            Pipeline pBlend; pBlend.prepare(sr, true); pBlend.run(sig);
            ++total; if (! findRegionNear(pMain.conf, (float) f).active) ++mainMissed;
            if (! findRegionNear(pBlend.conf, (float) f).active) ++blendMissed;
        }
        std::printf("  sr=%.0f: main-missed=%d/%d (%.1f%%)  blend-missed=%d/%d (%.1f%%)  %s\n",
            sr, mainMissed, total, 100.0*mainMissed/total, blendMissed, total, 100.0*blendMissed/total, mainMissed == blendMissed ? "IDENTICAL" : "DIFFERS");
    }

    // ---------------- Bass Clean @192kHz: prove or disprove fragmentation ----------------
    std::printf("\n=== BASS CLEAN @192kHz: per-frame region instrumentation (prove, don't guess) ===\n");
    {
        double sr = 192000.0; int n = (int) (sr * 1.0);
        auto sig = genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f);
        Pipeline p; p.prepare(sr, true);
        int frame = 0;
        std::array<float, ConfidenceEngine::kMaxRegions> prevCenterHz{}; prevCenterHz.fill(0.0f);
        std::array<bool, ConfidenceEngine::kMaxRegions> prevActive{}; prevActive.fill(false);
        int totalNewCreations = 0;
        double hostBinHz = sr / Pipeline::kFft;
        int blendBinLimit = juce::jmin((int) p.magDb.size() - 2, (int) std::ceil(p.crossHigh / hostBinHz) + 2);
        for (int i = 0; i + Pipeline::kFft <= n; i += Pipeline::kHop)
        {
            for (int k = 0; k < Pipeline::kFft; ++k) p.scratch[(size_t) k] = sig[(size_t) (i + k)] * p.window[(size_t) k];
            std::fill(p.scratch.begin() + Pipeline::kFft, p.scratch.end(), 0.0f);
            p.fft.performRealOnlyForwardTransform(p.scratch.data());
            const int bins = Pipeline::kFft / 2 + 1;
            for (int b = 0; b < bins; ++b)
            {
                float re = p.scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : p.scratch[(size_t) (2 * b + 1)];
                p.magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) Pipeline::kFft + 1e-12f, -120.0f);
            }
            p.prom.computeProminence(p.magDb, 4.0f, p.promOut);
            p.aux.pushSamples(sig.data() + i, Pipeline::kHop);
            p.promOutBlended = p.promOut;
            for (int b = 1; b <= blendBinLimit; ++b)
            {
                float mainMagHz = (float) ((b + parabolicDelta(p.magDb[(size_t) (b - 1)], p.magDb[(size_t) b], p.magDb[(size_t) (b + 1)])) * hostBinHz);
                auto r = effectiveProminenceAndFrequency(p.promOut[(size_t) b], mainMagHz, (float) (b * hostBinHz), p.aux, hostBinHz, b, p.magDb, p.promOut, p.crossLow, p.crossHigh);
                p.promOutBlended[(size_t) b] = r.prominenceDb;
            }
            p.conf.process(p.promOutBlended, nullptr);

            // detect new region creations by comparing active-flag transitions per slot index
            for (int rIdx = 0; rIdx < ConfidenceEngine::kMaxRegions; ++rIdx)
            {
                auto& reg = p.conf.regions()[(size_t) rIdx];
                if (reg.active && ! prevActive[(size_t) rIdx])
                {
                    ++totalNewCreations;
                    if (frame < 60 || frame % 20 == 0) // print early frames + periodic samples, not all 375
                        std::printf("  frame=%3d NEW region slot=%2d freq=%7.1fHz baseEvidence=%.3f (nearest prev-active center before this: ", frame, rIdx, reg.centerHz, reg.lastBaseEvidence);
                    if (frame < 60 || frame % 20 == 0)
                    {
                        float bestDist = 1.0e9f, bestHz = 0;
                        for (int p2 = 0; p2 < ConfidenceEngine::kMaxRegions; ++p2) if (prevActive[(size_t) p2]) { float d = std::abs(std::log2(juce::jmax(1.0f, prevCenterHz[(size_t) p2])) - std::log2(juce::jmax(1.0f, reg.centerHz))); if (d < bestDist) { bestDist = d; bestHz = prevCenterHz[(size_t) p2]; } }
                        std::printf("%.1fHz, dist=%.3f oct = %.1f cents)\n", bestHz, bestDist, bestDist * 1200.0f);
                    }
                }
                prevActive[(size_t) rIdx] = reg.active; prevCenterHz[(size_t) rIdx] = reg.centerHz;
            }
            ++frame;
        }
        std::printf("  total region CREATIONS over %d frames = %d | final active region count = %d\n", frame, totalNewCreations, p.conf.activeRegionCount());
        std::printf("  final regions: ");
        for (auto& r : p.conf.regions()) if (r.active) std::printf("%.1fHz(persist=%.2f) ", r.centerHz, r.persistence);
        std::printf("\n");
    }

    // ---------------- other controls: Bass+resonance, Vocal grave ----------------
    std::printf("\n=== OTHER CONTROLS: Bass+resonance, Vocal grave (region counts, main vs refined blend) ===\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        int n = (int) (sr * 1.0);
        auto bassRes = genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f); addBurst(bassRes, sr, 62.0 * 1.63, 0.5f, 8.0, 3);
        auto vocal = genVocalMale(sr, n);
        Pipeline pBassMain; pBassMain.prepare(sr, false); pBassMain.run(bassRes);
        Pipeline pBassBlend; pBassBlend.prepare(sr, true); pBassBlend.run(bassRes);
        Pipeline pVocMain; pVocMain.prepare(sr, false); pVocMain.run(vocal);
        Pipeline pVocBlend; pVocBlend.prepare(sr, true); pVocBlend.run(vocal);
        std::printf("  sr=%6.0f  Bass+res main=%2d blend=%2d | Vocal main=%2d blend=%2d\n",
            sr, pBassMain.conf.activeRegionCount(), pBassBlend.conf.activeRegionCount(), pVocMain.conf.activeRegionCount(), pVocBlend.conf.activeRegionCount());
    }

    // ---------------- CPU + zero-alloc quick check ----------------
    std::printf("\n=== CPU (Release, profiling OFF) ===\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Pipeline p; p.prepare(sr, true);
        auto sig = genHarmonicSeries(sr, (int) (sr * 1.0), 80.0, 0.3f, 6);
        std::vector<double> us;
        int n = (int) sig.size();
        double hostBinHz = sr / Pipeline::kFft;
        int blendBinLimit = juce::jmin((int) p.magDb.size() - 2, (int) std::ceil(p.crossHigh / hostBinHz) + 2);
        for (int i = 0; i + Pipeline::kFft <= n; i += Pipeline::kHop)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int k = 0; k < Pipeline::kFft; ++k) p.scratch[(size_t) k] = sig[(size_t) (i + k)] * p.window[(size_t) k];
            std::fill(p.scratch.begin() + Pipeline::kFft, p.scratch.end(), 0.0f);
            p.fft.performRealOnlyForwardTransform(p.scratch.data());
            const int bins = Pipeline::kFft / 2 + 1;
            for (int b = 0; b < bins; ++b) { float re = p.scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : p.scratch[(size_t) (2 * b + 1)]; p.magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) Pipeline::kFft + 1e-12f, -120.0f); }
            p.prom.computeProminence(p.magDb, 4.0f, p.promOut);
            p.aux.pushSamples(sig.data() + i, Pipeline::kHop);
            p.promOutBlended = p.promOut;
            for (int b = 1; b <= blendBinLimit; ++b)
            {
                float mainMagHz = (float) ((b + parabolicDelta(p.magDb[(size_t) (b - 1)], p.magDb[(size_t) b], p.magDb[(size_t) (b + 1)])) * hostBinHz);
                auto r = effectiveProminenceAndFrequency(p.promOut[(size_t) b], mainMagHz, (float) (b * hostBinHz), p.aux, hostBinHz, b, p.magDb, p.promOut, p.crossLow, p.crossHigh);
                p.promOutBlended[(size_t) b] = r.prominenceDb;
            }
            p.conf.process(p.promOutBlended, nullptr);
            auto t1 = std::chrono::high_resolution_clock::now();
            us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(us.begin(), us.end());
        double med = us[us.size() / 2];
        double budget = 1.0e6 * Pipeline::kHop / sr;
        std::printf("  sr=%6.0f: med=%.2fus (%.2f%% of %.1fus budget)\n", sr, med, 100.0 * med / budget, budget);
    }

    std::printf("\nC2.3a/b report complete.\n");
    return 0;
}
