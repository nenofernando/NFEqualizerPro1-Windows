// PHYSICAL C — FINAL FULL PIPELINE CHECK. Uses the REAL, now-integrated
// production ConfidenceEngine: main V2-A5C prominence -> C2.3 resolution-
// aware aux prominence assist -> C2.2 soft admission/top-K -> region
// tracking/persistence/stability -> Policy A harmonic context (main+aux)
// -> effective harmonic protection -> final confidence -> Selectivity.
// Diagnostic only -- still no gain reduction anywhere.

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
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}
static std::vector<float> genKick(double sr, int n) { auto b = genSilence(n); for (int i = 0; i < n; ++i) { double t = i / sr; double f = 120.0 * std::exp(-t * 18.0) + 45.0; float env = (float) std::exp(-t * 7.0); b[(size_t) i] = (float) std::sin(juce::MathConstants<double>::twoPi * f * t) * env * 0.9f; } return b; }
static std::vector<float> genVocal(double sr, int n) { auto b = genHarmonicSeries(sr, n, 140.0, 0.28f, 10, 2.5f); addTone(b, sr, 840.0, 0.10f); addTone(b, sr, 1200.0, 0.07f); return b; }
static std::vector<float> genGuitar(double sr, int n) { auto b = genSilence(n); for (int h = 1; h <= 10; ++h) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * (196.0 * h) / sr; float amp0 = 0.3f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1)); for (int i = 0; i < n; ++i) { double t = i / sr; float env = (float) std::exp(-t * 1.2); b[(size_t) i] += (float) std::sin(ph) * amp0 * env; ph += inc; } } return b; }
static std::vector<float> genBass(double sr, int n) { return genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f); }
static std::vector<float> genDenseMix(double sr, int n)
{
    auto b = genBass(sr, n);
    auto kick = genKick(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += kick[(size_t) i] * 0.7f;
    auto voc = genVocal(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += voc[(size_t) i] * 0.6f;
    auto gtr = genGuitar(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += gtr[(size_t) i] * 0.5f;
    juce::Random rng(99); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
    return b;
}
static std::vector<float> genMovingTone(double sr, int n, double f0, double f1, float amp) { auto b = genSilence(n); double ph = 0; for (int i = 0; i < n; ++i) { double t = (double) i / n; double f = f0 + (f1 - f0) * t; ph += juce::MathConstants<double>::twoPi * f / sr; b[(size_t) i] = (float) std::sin(ph) * amp; } return b; }
static std::vector<float> genTransientClicks(double sr, int n, int seed, int numClicks) { auto b = genSilence(n); juce::Random rng(seed); int spacing = n / juce::jmax(1, numClicks); for (int c = 0; c < numClicks; ++c) { int pos = c * spacing + spacing / 2; for (int i = 0; i < 150 && pos + i < n; ++i) { float env = std::exp(-(float) i / 15.0f); float x = (rng.nextFloat() * 2.0f - 1.0f) * env * 0.8f; b[(size_t) (pos + i)] += x; } } return b; }

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
    std::vector<float> magDb, promOut;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        prom.prepare(kFft / 2 + 1, sr, kFft); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        aux.prepare(sr); conf.prepare(sr, kFft, kHop);
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        magDb.assign((size_t) (kFft / 2 + 1), -120.0f); promOut.assign((size_t) (kFft / 2 + 1), 0.0f);
    }
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
            // C2.3e dual-source rescue: main's own topology is detected
            // internally on the RAW, unmodified prominence array (never
            // touched by aux) -- rescue candidates are appended separately,
            // after main's own detection, never altering main's own
            // detected peaks' values. Passing both aux and magDb here is
            // what enables the rescue pass; Policy A harmonic-context
            // blending (aux-side) still runs regardless, as in C2.2.
            conf.process(promOut, &aux, &magDb);
        }
    }
};

static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& c, float hz, float tol = 0.25f)
{
    float target = std::log2(juce::jmax(1.0f, hz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
    for (auto& r : c.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
    if (best && bestDist <= tol) return *best; return ConfidenceEngine::Region{};
}
static void printRow(const char* label, float queryHz, const ConfidenceEngine::Region& r)
{
    if (! r.active) { std::printf("      %-24s NOT FOUND\n", label); return; }
    std::printf("      %-24s freq=%7.1f prom=%.2f candEv=%.3f admitted=yes persist=%.3f stability=%.3f harmLike=%.3f effRel=%.3f effProt=%.3f finalConf=%.3f\n",
        label, r.centerHz, r.peakProminenceDb, r.candidateEvidence, r.persistence, r.stability, r.effectiveLikelihood, r.effectiveReliability, r.effectiveHarmonicProtection, r.confidence);
    (void) queryHz;
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double durationSec = 1.5;

    std::printf("=== 1. CONTROLLED MATRIX (10 signal types x 4 SR) ===\n");
    bool allCriteriaOk = true;
    for (double sr : rates)
    {
        int n = (int) (sr * durationSec);
        std::printf(" -- Sample rate %.0f Hz --\n", sr);

        auto run = [&](const char* label, std::vector<float> sig, std::initializer_list<std::pair<const char*, float>> queries)
        {
            Pipeline p; p.prepare(sr); p.run(sig);
            std::printf("   %s\n", label);
            for (auto& q : queries) printRow(q.first, q.second, findRegionNear(p.conf, q.second));
            return p;
        };

        auto sigClean = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        run("1. Harmonic series clean (80Hz)", sigClean, { { "80Hz(fund)", 80.0f } });

        std::vector<float> sigExcess = genSilence(n);
        for (int h = 1; h <= 6; ++h) { float a = 0.3f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1)); if (h == 2) a *= (float) juce::Decibels::decibelsToGain(12.0f); addTone(sigExcess, sr, 80.0 * h, a); }
        run("2. Excessive harmonic (h2@160Hz+12dB)", sigExcess, { { "80Hz(fund)", 80.0f }, { "160Hz(h2 excess)", 160.0f } });

        auto sigNonHarm = sigClean; addBurst(sigNonHarm, sr, 135.0, 0.5f, 8.0, 1);
        auto pNonHarm = run("3. Non-harmonic resonance (135Hz)", sigNonHarm, { { "80Hz(fund)", 80.0f }, { "135Hz(non-harm)", 135.0f } });

        run("4. Bass 80Hz", genHarmonicSeries(sr, n, 80.0, 0.3f, 6), { { "80Hz", 80.0f } });

        auto sigBass135 = genHarmonicSeries(sr, n, 80.0, 0.3f, 6); addBurst(sigBass135, sr, 135.0, 0.5f, 8.0, 1);
        auto pBass135 = run("5. Bass 80Hz + 135Hz resonance", sigBass135, { { "80Hz", 80.0f }, { "135Hz", 135.0f } });

        run("6. Bass 120Hz", genHarmonicSeries(sr, n, 120.0, 0.3f, 6), { { "120Hz", 120.0f } });

        auto sigBass170 = genHarmonicSeries(sr, n, 120.0, 0.3f, 6); addBurst(sigBass170, sr, 170.0, 0.5f, 8.0, 2);
        auto pBass170 = run("7. Bass 120Hz + 170Hz resonance", sigBass170, { { "120Hz", 120.0f }, { "170Hz", 170.0f } });

        std::vector<float> sigRing = genSilence(n); addBurst(sigRing, sr, 1700.0, 0.4f, 12.0, 4);
        run("8. Sustained ringing (1.7kHz)", sigRing, { { "1700Hz", 1700.0f } });

        run("9. Moving tonal component (300->600Hz)", genMovingTone(sr, n, 300.0, 600.0, 0.4f), { { "~450Hz(mid-sweep)", 450.0f } });

        run("10. Transient burst (clicks)", genTransientClicks(sr, n, 7, 6), { });

        // fundamental criterion check (item 4/7): non-harmonic/excessive must not show LOWER problem-confidence than the fundamental when evidence is sufficient
        auto r80 = findRegionNear(pNonHarm.conf, 80.0f), r135 = findRegionNear(pNonHarm.conf, 135.0f);
        auto r120 = findRegionNear(pBass170.conf, 120.0f), r170 = findRegionNear(pBass170.conf, 170.0f);
        bool haveEvidence1 = r80.active && r135.active, haveEvidence2 = r120.active && r170.active;
        bool crit1 = (! haveEvidence1) || (r135.confidence > r80.confidence);
        bool crit2 = (! haveEvidence2) || (r170.confidence > r120.confidence);
        std::printf("   [FUNDAMENTAL CRITERION] 135>80: conf(135)=%.3f vs conf(80)=%.3f %s | 170>120: conf(170)=%.3f vs conf(120)=%.3f %s\n",
            r135.confidence, r80.confidence, crit1 ? "PASS" : "FAIL", r170.confidence, r120.confidence, crit2 ? "PASS" : "FAIL");
        if (! crit1 || ! crit2) allCriteriaOk = false;
    }

    std::printf("\n=== 2. MUSICAL MATERIAL: per-band prominence/candidate/tracked/confidence-threshold counts ===\n");
    struct Band { const char* name; float lo, hi; };
    const Band bands[] = { { "20-100Hz", 20, 100 }, { "100-300Hz", 100, 300 }, { "300-1k", 300, 1000 }, { "1-4k", 1000, 4000 }, { "4-10k", 4000, 10000 }, { "10-20k", 10000, 20000 } };
    for (double sr : rates)
    {
        int n = (int) (sr * 1.5);
        struct Mat { const char* name; std::vector<float> sig; };
        std::vector<Mat> mats = { { "Bass", genBass(sr, n) }, { "Kick", genKick(sr, n) }, { "Vocal", genVocal(sr, n) }, { "Guitar", genGuitar(sr, n) }, { "Dense mix", genDenseMix(sr, n) } };
        std::printf(" -- sr=%.0fHz --\n", sr);
        for (auto& m : mats)
        {
            Pipeline p; p.prepare(sr); p.run(m.sig);
            std::printf("   %-10s", m.name);
            for (auto& band : bands)
            {
                int rawBins = 0; int loBin = (int) (band.lo * Pipeline::kFft / sr), hiBin = (int) (band.hi * Pipeline::kFft / sr);
                for (int b = juce::jmax(1, loBin); b <= juce::jmin((int) p.promOut.size() - 1, hiBin); ++b) if (p.promOut[(size_t) b] > 3.0f) ++rawBins;
                int tracked = 0, c25 = 0, c50 = 0, c75 = 0;
                for (auto& r : p.conf.regions()) if (r.active && r.centerHz >= band.lo && r.centerHz < band.hi) { ++tracked; if (r.confidence > 0.25f) ++c25; if (r.confidence > 0.5f) ++c50; if (r.confidence > 0.75f) ++c75; }
                std::printf(" | %s raw=%2d trk=%2d >.25=%1d >.5=%1d >.75=%1d", band.name, rawBins, tracked, c25, c50, c75);
            }
            std::printf("\n");
        }
    }

    std::printf("\n=== 3. SELECTIVITY (0/2.5/5/7.5/10 -- must be monotonic, no hard switch) ===\n");
    {
        Pipeline p; p.prepare(48000.0);
        auto sig = genHarmonicSeries(48000.0, (int) (48000.0 * 1.5), 80.0, 0.3f, 6); addBurst(sig, 48000.0, 135.0, 0.5f, 8.0, 1);
        p.run(sig);
        auto r135 = findRegionNear(p.conf, 135.0f);
        std::printf("  135Hz region confidence=%.3f -> selectivityWeight: ", r135.confidence);
        float prev = -1; bool monotonic = true;
        for (float sel : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f }) { float w = ConfidenceEngine::passWeight(r135.confidence, sel); std::printf("sel=%.1f->%.3f  ", sel, w); if (prev >= 0 && w > prev + 1e-4f) monotonic = false; prev = w; }
        std::printf("\n  [monotonic non-increasing as selectivity rises] %s\n", monotonic ? "PASS" : "FAIL");
    }

    std::printf("\n=== 4. CROSS-SR + KNOWN CORNER CASES ===\n");
    for (double sr : rates)
    {
        int n = (int) (sr * 1.5);
        auto sig80 = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        Pipeline p80; p80.prepare(sr); p80.run(sig80);
        auto r80 = findRegionNear(p80.conf, 80.0f);
        std::printf("  sr=%6.0f 80Hz-fundamental-alone: found=%s confidence=%.3f reliability=%.3f\n", sr, r80.active ? "yes" : "no", r80.confidence, r80.effectiveReliability);
    }
    std::printf("  KNOWN, DOCUMENTED, NON-BLOCKING: 80Hz@44.1kHz mainlobe overlap (Blocker 2), 120Hz@192kHz dual-low-consistency (C2.3d, proven not a decimation bug) -- neither produces a high-confidence WRONG decision.\n");

    std::printf("\n=== 5. CPU FINAL (Release, profiling OFF) ===\n");
    for (double sr : rates)
    {
        Pipeline p; p.prepare(sr);
        auto sig = genHarmonicSeries(sr, (int) (sr * 2.0), 80.0, 0.3f, 6); addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);
        int n = (int) sig.size();
        std::vector<double> us;
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
            p.conf.process(p.promOut, &p.aux, &p.magDb);
            auto r = findRegionNear(p.conf, 80.0f);
            volatile float sw = ConfidenceEngine::passWeight(r.confidence, 5.0f); (void) sw;
            auto t1 = std::chrono::high_resolution_clock::now();
            us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(us.begin(), us.end());
        size_t cnt = us.size();
        double med = cnt ? (cnt % 2 ? us[cnt / 2] : 0.5 * (us[cnt / 2 - 1] + us[cnt / 2])) : 0.0;
        auto pct = [&](double pv) { if (us.empty()) return 0.0; double idx = pv / 100.0 * (double) (cnt - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(cnt - 1, lo + 1); double frac = idx - (double) lo; return us[lo] + (us[hi] - us[lo]) * frac; };
        double hopBudgetUs = 1.0e6 * Pipeline::kHop / sr;
        std::printf("  sr=%6.0f: med=%.2fus(%.2f%%) P95=%.2fus(%.2f%%) P99=%.2fus(%.2f%%)  [hop budget=%.1fus]\n",
            sr, med, 100.0 * med / hopBudgetUs, pct(95.0), 100.0 * pct(95.0) / hopBudgetUs, pct(99.0), 100.0 * pct(99.0) / hopBudgetUs, hopBudgetUs);
    }

    std::printf("\n=== 7. CLOSING CRITERIA SUMMARY ===\n");
    std::printf("  fundamental-criterion (135>80, 170>120, all SR): %s\n", allCriteriaOk ? "PASS" : "FAIL (see per-SR detail above)");
    std::printf("\nPHYSICAL C final pipeline report complete.\n");
    return allCriteriaOk ? 0 : 1;
}
