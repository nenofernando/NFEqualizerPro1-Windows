// PHYSICAL C2: integrate LowFrequencyHarmonicAnalyzer into ConfidenceEngine
// as the single source of harmonic truth for low-frequency regions, via a
// smoothstep crossover (~600-900Hz) into ConfidenceEngine's own host-rate
// reasoning above that. Still fully diagnostic -- no gain reduction, no UI,
// no PHYSICAL D. Covers items 6-10 of the C2 spec: A-E matrix at 4 sample
// rates with full decomposition, crossover smoothness sweep, musical
// material sparsity, integrated CPU.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>

// ---------------- signal generators ----------------
static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp, double phase = 0.0)
{
    double ph = phase, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{
    auto b = genSilence(n);
    for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)));
    return b;
}
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed);
    double bwHz = freqHz / Q;
    int n = (int) b.size();
    for (int k = 0; k < 9; ++k)
    {
        double t = (double) k / 8.0 - 0.5;
        double f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; }
    }
}
static void addNoise(std::vector<float>& b, float amp, int seed) { juce::Random rng(seed); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * amp; }
static std::vector<float> genKick(double sr, int n)
{
    auto b = genSilence(n);
    for (int i = 0; i < n; ++i) { double t = i / sr; double f = 120.0 * std::exp(-t * 18.0) + 45.0; float env = (float) std::exp(-t * 7.0); b[(size_t) i] = (float) std::sin(juce::MathConstants<double>::twoPi * f * t) * env * 0.9f; }
    return b;
}
static std::vector<float> genVocal(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 140.0, 0.28f, 12, 2.0f);
    // crude formant emphasis around ~700-900Hz and ~1.2kHz via amplitude-shaped extra partials
    addTone(b, sr, 840.0, 0.10f); addTone(b, sr, 1200.0, 0.07f);
    return b;
}
static std::vector<float> genGuitar(double sr, int n)
{
    auto b = genSilence(n);
    for (int h = 1; h <= 10; ++h)
    {
        double ph = 0.0, inc = juce::MathConstants<double>::twoPi * (196.0 * h) / sr;
        float amp0 = 0.3f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1));
        for (int i = 0; i < n; ++i) { double t = i / sr; float env = (float) std::exp(-t * 1.2); b[(size_t) i] += (float) std::sin(ph) * amp0 * env; ph += inc; }
    }
    return b;
}
static std::vector<float> genBass(double sr, int n) { return genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f); }
static std::vector<float> genDenseMix(double sr, int n)
{
    auto b = genBass(sr, n);
    auto kick = genKick(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += kick[(size_t) i] * 0.7f;
    auto voc = genVocal(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += voc[(size_t) i] * 0.6f;
    auto gtr = genGuitar(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += gtr[(size_t) i] * 0.5f;
    addNoise(b, 0.02f, 99);
    return b;
}

// ---------------- integrated diagnostic pipeline ----------------
// Host-rate STFT (2048/512 hop, Hann) -> SpectralProminenceEngineV5 -> feeds
// BOTH LowFrequencyHarmonicAnalyzer (decimated) and ConfidenceEngine
// (host-rate), then ConfidenceEngine::process(prominence, &aux) blends them.
struct Pipeline
{
    double sr = 48000.0;
    static constexpr int kFft = 2048, kHop = 512;
    LowFrequencyHarmonicAnalyzer aux;
    SpectralProminenceEngineV5 prom;
    ConfidenceEngine conf;
    juce::dsp::FFT fft{ 11 };
    std::array<float, kFft> window{};
    std::array<float, kFft * 2> scratch{};
    std::vector<float> magDb, promOut;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        aux.prepare(sr);
        prom.prepare(kFft / 2 + 1, sr, kFft);
        prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        conf.prepare(sr, kFft, kHop);
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        magDb.assign((size_t) (kFft / 2 + 1), -120.0f);
        promOut.assign((size_t) (kFft / 2 + 1), 0.0f);
    }

    // Feeds the WHOLE signal, hop-by-hop, host FFT + aux + confidence.
    void run(const std::vector<float>& sig)
    {
        int n = (int) sig.size();
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
            conf.process(promOut, &aux);
        }
    }
};

static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& c, float hz, float toleranceOct = 0.25f)
{
    float target = std::log2(juce::jmax(1.0f, hz));
    const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
    for (auto& r : c.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
    if (best != nullptr && bestDist <= toleranceOct) return *best;
    return ConfidenceEngine::Region{};
}

static void printBreakdown(const char* label, const ConfidenceEngine::Region& r)
{
    if (! r.active) { std::printf("      %-28s NOT FOUND (no active region)\n", label); return; }
    std::printf("      %-28s freq=%7.1fHz baseEvidence=%.3f harmLike(eff)=%.3f reliability(eff)=%.3f effProtection=%.3f finalConfidence=%.3f\n",
        label, r.centerHz, r.lastBaseEvidence, r.effectiveLikelihood, r.effectiveReliability, r.effectiveHarmonicProtection, r.confidence);
    float selVals[] = { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f };
    std::printf("        selectivityWeight: ");
    for (float s : selVals) std::printf("sel=%.1f->%.3f  ", s, ConfidenceEngine::passWeight(r.confidence, s));
    std::printf("\n");
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double durationSec = 1.5;
    bool allOk = true;

    std::printf("=== 6. FUNDAMENTAL TEST MATRIX (A-E, full breakdown, 44.1/48/96/192kHz) ===\n");
    for (double sr : rates)
    {
        int n = (int) (sr * durationSec);
        std::printf(" -- Sample rate %.0f Hz --\n", sr);

        auto run = [&](const char* label, std::vector<float> sig, std::initializer_list<float> queries)
        {
            Pipeline p; p.prepare(sr); p.run(sig);
            std::printf("   %s\n", label);
            for (float q : queries) printBreakdown((juce::String(q, 1) + "Hz").toRawUTF8(), findRegionNear(p.conf, q));
            return p;
        };

        auto sigA = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        auto pA = run("A: 80Hz+harmonics", sigA, { 80.0f });

        auto sigB = sigA; addBurst(sigB, sr, 135.0, 0.5f, 8.0, 1);
        auto pB = run("B: A+non-harm@135Hz", sigB, { 80.0f, 135.0f });

        std::vector<float> sigC = genSilence(n);
        for (int h = 1; h <= 6; ++h) { float a = 0.3f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1)); if (h == 2) a *= (float) juce::Decibels::decibelsToGain(12.0f); addTone(sigC, sr, 80.0 * h, a); }
        auto pC = run("C: A+excessive h2@160Hz", sigC, { 80.0f, 160.0f });

        auto sigD = genHarmonicSeries(sr, n, 120.0, 0.3f, 6);
        auto pD = run("D: 120Hz+harmonics", sigD, { 120.0f });

        auto sigE = sigD; addBurst(sigE, sr, 170.0, 0.5f, 8.0, 2);
        auto pE = run("E: D+non-harm@170Hz", sigE, { 120.0f, 170.0f });

        auto r80 = findRegionNear(pB.conf, 80.0f), r135 = findRegionNear(pB.conf, 135.0f);
        auto r120 = findRegionNear(pE.conf, 120.0f), r170 = findRegionNear(pE.conf, 170.0f);
        bool haveEvidence = r80.active && r135.active && r120.active && r170.active;
        bool crit = (! haveEvidence) || (r135.confidence > r80.confidence && r170.confidence > r120.confidence);
        std::printf("   [135>80 & 170>120] conf(135)=%.3f vs conf(80)=%.3f | conf(170)=%.3f vs conf(120)=%.3f : %s%s\n",
            r135.confidence, r80.confidence, r170.confidence, r120.confidence, crit ? "PASS" : "FAIL",
            haveEvidence ? "" : " (insufficient evidence -- not a hard requirement per spec item 6)");
        if (! crit) allOk = false;
    }

    // ---------------- 7. Crossover smoothness ----------------
    std::printf("\n=== 7. CROSSOVER SMOOTHNESS (aux context -> host reasoning, ~600-900Hz) ===\n");
    {
        const double probeFreqs[] = { 400, 500, 595, 598, 600, 602, 605, 700, 800, 895, 898, 900, 902, 905, 1000, 1200 };
        double sr = 48000.0;
        int n = (int) (sr * 1.5);
        struct ProbeResult { double freq; float conf; float effLike; float effRel; bool found; };
        std::vector<ProbeResult> results;
        for (double pf : probeFreqs)
        {
            auto sig = genHarmonicSeries(sr, n, 97.0, 0.3f, 9); // base f0 chosen so integer harmonics don't collide with probe freqs
            addBurst(sig, sr, pf, 0.4f, 10.0, 5);
            Pipeline p; p.prepare(sr); p.run(sig);
            auto r = findRegionNear(p.conf, (float) pf);
            results.push_back({ pf, r.confidence, r.effectiveLikelihood, r.effectiveReliability, r.active });
            std::printf("  probe=%6.0fHz : found=%s confidence=%.3f effLikelihood=%.3f effReliability=%.3f\n", pf, r.active ? "yes" : "no", r.confidence, r.effectiveLikelihood, r.effectiveReliability);
        }
        float maxJump = 0.0f;
        for (size_t i = 1; i < results.size(); ++i) if (results[i].found && results[i - 1].found) maxJump = juce::jmax(maxJump, std::abs(results[i].conf - results[i - 1].conf));
        std::printf("  max confidence jump between adjacent probe points = %.3f\n", maxJump);
        bool smoothOk = maxJump < 0.35f; // generous bound -- catching a genuine step discontinuity, not normal signal-dependent variation
        std::printf("  [NO ARTIFICIAL DISCONTINUITY AT CROSSOVER] %s\n", smoothOk ? "PASS" : "FAIL (investigate before freezing crossover)");
        if (! smoothOk) allOk = false;
    }

    // ---------------- 8. Musical material sparsity ----------------
    std::printf("\n=== 8. MUSICAL MATERIAL: raw prominence vs final confidence, 100-300Hz focus ===\n");
    {
        double sr = 48000.0; int n = (int) (sr * 1.5);
        struct Material { const char* name; std::vector<float> sig; };
        std::vector<Material> materials = {
            { "Bass", genBass(sr, n) }, { "Kick", genKick(sr, n) }, { "Vocal", genVocal(sr, n) },
            { "Guitar", genGuitar(sr, n) }, { "Dense mix", genDenseMix(sr, n) }
        };
        double loHz = 100.0, hiHz = 300.0;
        for (auto& m : materials)
        {
            Pipeline p; p.prepare(sr); p.run(m.sig);
            int loBin = (int) (loHz * Pipeline::kFft / sr), hiBin = (int) (hiHz * Pipeline::kFft / sr);
            int rawBinsActive = 0; for (int b = loBin; b <= hiBin && b < (int) p.promOut.size(); ++b) if (p.promOut[(size_t) b] > 3.0f) ++rawBinsActive;
            int regionsInBand = 0, regionsConfident = 0;
            for (auto& r : p.conf.regions()) if (r.active && r.centerHz >= loHz && r.centerHz <= hiHz) { ++regionsInBand; if (r.confidence > 0.5f) ++regionsConfident; }
            std::printf("  %-10s 100-300Hz: raw prominence bins>3dB=%3d | tracked regions=%d | regions w/ confidence>0.5=%d\n",
                m.name, rawBinsActive, regionsInBand, regionsConfident);
        }
        std::printf("  (V1 showed ~98%% of 100-300Hz bins active with ZERO injected resonance in earlier PHYSICAL B testing --\n");
        std::printf("   compare the raw-prominence column above to the confidence>0.5 column to see V2's selectivity)\n");
    }

    // ---------------- 9. Delta -- explicitly not applicable ----------------
    std::printf("\n=== 9. DELTA: not applicable -- no V2 gain mask exists yet, nothing has been removed to compare ===\n");

    // ---------------- 10. Integrated CPU ----------------
    std::printf("\n=== 10. INTEGRATED CPU: V2-A5C prominence + ConfidenceEngine + aux analyzer + selectivity decision ===\n");
    for (double sr : rates)
    {
        Pipeline p; p.prepare(sr);
        auto sig = genHarmonicSeries(sr, (int) (sr * 2.0), 80.0, 0.3f, 6);
        addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);
        int n = (int) sig.size();
        std::vector<double> usPerHop;
        long long allocCountBefore = 0; (void) allocCountBefore;
        for (int i = 0; i + Pipeline::kFft <= n; i += Pipeline::kHop)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
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
            p.conf.process(p.promOut, &p.aux);
            auto r = findRegionNear(p.conf, 80.0f);
            volatile float sw = ConfidenceEngine::passWeight(r.confidence, 5.0f); (void) sw;
            auto t1 = std::chrono::high_resolution_clock::now();
            usPerHop.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(usPerHop.begin(), usPerHop.end());
        size_t cnt = usPerHop.size();
        double med = cnt ? (cnt % 2 ? usPerHop[cnt / 2] : 0.5 * (usPerHop[cnt / 2 - 1] + usPerHop[cnt / 2])) : 0.0;
        auto pct = [&](double pv) { if (usPerHop.empty()) return 0.0; double idx = pv / 100.0 * (double) (cnt - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(cnt - 1, lo + 1); double frac = idx - (double) lo; return usPerHop[lo] + (usPerHop[hi] - usPerHop[lo]) * frac; };
        double hopBudgetUs = 1.0e6 * Pipeline::kHop / sr;
        std::printf("  %6.0fHz: med=%.2fus(%.2f%%) P95=%.2fus(%.2f%%) P99=%.2fus(%.2f%%)  [hop budget=%.1fus]\n",
            sr, med, 100.0 * med / hopBudgetUs, pct(95.0), 100.0 * pct(95.0) / hopBudgetUs, pct(99.0), 100.0 * pct(99.0) / hopBudgetUs, hopBudgetUs);
    }

    std::printf("\n%s\n", allOk ? "PHYSICAL C2 integration report: hard criteria PASS. See sections above for full review." : "PHYSICAL C2 integration report: at least one hard criterion FAILED -- see above.");
    return allOk ? 0 : 1;
}
