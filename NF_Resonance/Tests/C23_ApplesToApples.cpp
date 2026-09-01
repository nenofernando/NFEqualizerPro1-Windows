// PHYSICAL C2.3 item 7/8/9/10: apples-to-apples comparison using the REAL,
// unmodified ConfidenceEngine (same interpolation, tracker, matching, soft
// admission, top-K) for BOTH paths -- the only difference is whether the
// prominence array fed in is raw main-only or blended with
// resolution-aware aux assistance (computed here, not yet wired into
// production ConfidenceEngine.cpp).

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
static std::vector<float> genWhiteNoise(int n, float amp) { std::vector<float> b((size_t) n); juce::Random rng(31); for (auto& s : b) s = (rng.nextFloat() * 2.0f - 1.0f) * amp; return b; }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }

static float crossoverWeight(float hz, float lowHz, float highHz) { if (hz <= lowHz) return 1.0f; if (hz >= highHz) return 0.0f; float t = (hz - lowHz) / (highHz - lowHz); return 1.0f - (t * t * (3.0f - 2.0f * t)); }
static float resolutionAdvantageWeight(double mainBinHz, double auxBinHz) { if (mainBinHz <= 0.0) return 0.0f; double ratio = auxBinHz / mainBinHz; return (float) juce::jlimit(0.0, 1.0, 1.0 - ratio); }

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
        aux.prepare(sr);
        conf.prepare(sr, kFft, kHop);
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        magDb.assign((size_t) (kFft / 2 + 1), -120.0f); promOut.assign((size_t) (kFft / 2 + 1), 0.0f); promOutBlended.assign((size_t) (kFft / 2 + 1), 0.0f);
    }

    void run(const std::vector<float>& sig)
    {
        int n = (int) sig.size();
        double hostBinHz = sr / kFft;
        int blendBinLimit = juce::jmin((int) magDb.size() - 2, (int) std::ceil(crossHigh / hostBinHz) + 2); // only bother blending bins that could ever get nonzero weight
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
                    float queryHz = (float) (b * hostBinHz);
                    float auxEst = 0, auxRel = 0;
                    float auxDb = aux.auxProminenceFor(queryHz, &auxEst, &auxRel);
                    float freqWeight = crossoverWeight(queryHz, crossLow, crossHigh);
                    float resAdv = resolutionAdvantageWeight(hostBinHz, aux.analysisBinHz());
                    float auxWeight = freqWeight * resAdv * auxRel;
                    promOutBlended[(size_t) b] = auxWeight * auxDb + (1.0f - auxWeight) * promOut[(size_t) b];
                }
                feedProm = &promOutBlended;
            }
            conf.process(*feedProm, nullptr); // aux is NOT passed here -- this test isolates prominence assistance from harmonic-context blending (Policy A), per item 11 keeping them separate
        }
    }
};

static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& c, float hz, float tol = 0.25f)
{
    float target = std::log2(juce::jmax(1.0f, hz));
    const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
    for (auto& r : c.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
    if (best && bestDist <= tol) return *best;
    return ConfidenceEngine::Region{};
}

int main()
{
    const double freqs[] = { 80, 100, 120, 170, 250, 500, 1000, 4000 };
    const float amps[] = { 3.0f, 4.0f, 6.0f, 9.0f };
    const double qs[] = { 10.0, 6.0, 3.0 }; const char* qNames[] = { "narrow", "medium", "broad" };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    std::printf("=== 7. APPLES-TO-APPLES: main-only vs main+aux-prominence-blend (same ConfidenceEngine) ===\n");
    int total = 0, mainMissed = 0, blendMissed = 0;
    int total4448 = 0, mainMissed4448 = 0, blendMissed4448 = 0;
    for (double sr : rates)
        for (double f : freqs)
            for (float amp : amps)
                for (int qi = 0; qi < 3; ++qi)
                {
                    int n = (int) (sr * 0.5);
                    auto sig = genSilence(n);
                    addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, qs[(size_t) qi], 5);

                    Pipeline pMain; pMain.prepare(sr, false); pMain.run(sig);
                    Pipeline pBlend; pBlend.prepare(sr, true); pBlend.run(sig);
                    bool mainFound = findRegionNear(pMain.conf, (float) f).active;
                    bool blendFound = findRegionNear(pBlend.conf, (float) f).active;

                    ++total; if (! mainFound) ++mainMissed; if (! blendFound) ++blendMissed;
                    bool is4448 = (sr == 44100.0 || sr == 48000.0);
                    if (is4448) { ++total4448; if (! mainFound) ++mainMissed4448; if (! blendFound) ++blendMissed4448; }
                }
    std::printf("  ALL: total=%d main-missed=%d (%.1f%%) blend-missed=%d (%.1f%%)\n", total, mainMissed, 100.0 * mainMissed / total, blendMissed, 100.0 * blendMissed / total);
    std::printf("  44.1/48kHz ONLY: total=%d main-missed=%d (%.1f%%) blend-missed=%d (%.1f%%)\n", total4448, mainMissed4448, 100.0 * mainMissed4448 / total4448, blendMissed4448, 100.0 * blendMissed4448 / total4448);

    // breakdown by frequency
    std::printf("  -- breakdown by frequency (missed count / total per freq, all SR/amp/width) --\n");
    for (double f : freqs)
    {
        int tot = 0, mm = 0, bm = 0;
        for (double sr : rates) for (float amp : amps) for (int qi = 0; qi < 3; ++qi)
        {
            int n = (int) (sr * 0.5); auto sig = genSilence(n); addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, qs[(size_t) qi], 5);
            Pipeline pMain; pMain.prepare(sr, false); pMain.run(sig);
            Pipeline pBlend; pBlend.prepare(sr, true); pBlend.run(sig);
            ++tot; if (! findRegionNear(pMain.conf, (float) f).active) ++mm; if (! findRegionNear(pBlend.conf, (float) f).active) ++bm;
        }
        std::printf("    f=%6.0fHz: main-missed=%3d/%3d (%.0f%%) blend-missed=%3d/%3d (%.0f%%)\n", f, mm, tot, 100.0*mm/tot, bm, tot, 100.0*bm/tot);
    }

    // pool occupancy / candidate counts on no-resonance controls
    std::printf("\n=== 8/9. POOL OCCUPANCY, CANDIDATE COUNTS -- controls (must not regress) ===\n");
    for (double sr : rates)
    {
        int n = (int) (sr * 1.0);
        struct Ctl { const char* name; std::vector<float> sig; };
        std::vector<Ctl> controls = { { "White noise", genWhiteNoise(n, 0.15f) }, { "Bass clean", genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f) } };
        for (auto& c : controls)
        {
            Pipeline pMain; pMain.prepare(sr, false); Pipeline pBlend; pBlend.prepare(sr, true);
            pMain.run(c.sig); pBlend.run(c.sig);
            std::printf("  sr=%6.0fHz %-11s main: activeRegions=%2d | blend: activeRegions=%2d\n", sr, c.name, pMain.conf.activeRegionCount(), pBlend.conf.activeRegionCount());
        }
    }

    // CPU: aux harmonic-analyzer-only vs aux harmonic+prominence-assistance
    std::printf("\n=== 10. CPU: aux (harmonic only) vs aux (harmonic + prominence assistance), Release, profiling OFF ===\n");
    for (double sr : rates)
    {
        int n = (int) (sr * 1.0);
        auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);

        LowFrequencyHarmonicAnalyzer auxOnly; auxOnly.prepare(sr);
        std::vector<double> harmOnlyUs;
        for (int i = 0; i + 512 <= n; i += 512)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            auxOnly.pushSamples(sig.data() + i, 512);
            auto t1 = std::chrono::high_resolution_clock::now();
            harmOnlyUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }

        LowFrequencyHarmonicAnalyzer auxProm; auxProm.prepare(sr);
        double hostBinHz = sr / 2048.0;
        int blendBinLimit = juce::jmin(2048 / 2 - 2, (int) std::ceil(800.0 / hostBinHz) + 2);
        std::vector<double> harmPlusPromUs;
        for (int i = 0; i + 512 <= n; i += 512)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            auxProm.pushSamples(sig.data() + i, 512);
            for (int b = 1; b <= blendBinLimit; ++b) { float est, rel; volatile float p = auxProm.auxProminenceFor((float) (b * hostBinHz), &est, &rel); (void) p; }
            auto t1 = std::chrono::high_resolution_clock::now();
            harmPlusPromUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); size_t n2 = v.size(); return n2 ? (n2 % 2 ? v[n2/2] : 0.5*(v[n2/2-1]+v[n2/2])) : 0.0; };
        double hopBudgetUs = 1.0e6 * 512.0 / sr;
        std::printf("  sr=%6.0fHz: harmonic-only med=%.2fus(%.2f%%) | harmonic+prominence med=%.2fus(%.2f%%) [blendBins=%d, budget=%.1fus]\n",
            sr, median(harmOnlyUs), 100.0*median(harmOnlyUs)/hopBudgetUs, median(harmPlusPromUs), 100.0*median(harmPlusPromUs)/hopBudgetUs, blendBinLimit, hopBudgetUs);
    }

    std::printf("\nC2.3 apples-to-apples report complete.\n");
    return 0;
}
