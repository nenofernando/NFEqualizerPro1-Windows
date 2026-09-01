// PHYSICAL C2: characterize the "dual uncertainty" state (auxReliability
// AND mainReliability both low) BEFORE choosing any correction. Compares
// three OFFLINE policies computed from the same diagnostic fields
// ConfidenceEngine already exposes -- none of them touch production code.
//   Policy A -- current production formula (region.confidence as-is).
//   Policy B -- uncertainty damping: baseEvidence itself is mildly pulled
//               toward neutral (0.5) in proportion to (1-effectiveReliability),
//               capped at 40% pull so a real problematic resonance under
//               uncertain context isn't crushed either.
//   Policy C -- conservative hold: while both aux and main reliability stay
//               below 0.25, confidence leans on a slow-moving average of
//               its own recent history instead of recomputing fresh from
//               baseEvidence every frame; recovers immediately once either
//               source becomes reliable again.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
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
static std::vector<float> genKick(double sr, int n)
{
    auto b = genSilence(n);
    for (int i = 0; i < n; ++i) { double t = i / sr; double f = 120.0 * std::exp(-t * 18.0) + 45.0; float env = (float) std::exp(-t * 7.0); b[(size_t) i] = (float) std::sin(juce::MathConstants<double>::twoPi * f * t) * env * 0.9f; }
    return b;
}
static std::vector<float> genVocalMale(double sr, int n) { return genHarmonicSeries(sr, n, 100.0, 0.3f, 10, 2.5f); }

struct FrameSample { float baseEvidence = 0, harmonicPenalty = 1, effReliability = 0, auxReliability = 0, mainReliability = 0, auxLikelihood = 0, mainLikelihood = 0, effLikelihood = 0, confidenceA = 0; bool found = false; };

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

    static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& c, float hz, float toleranceOct = 0.25f)
    {
        float target = std::log2(juce::jmax(1.0f, hz));
        const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
        for (auto& r : c.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
        if (best != nullptr && bestDist <= toleranceOct) return *best;
        return ConfidenceEngine::Region{};
    }

    // Runs the whole signal, recording per-hop FrameSample history for TWO
    // query frequencies (fundamental, resonance).
    void runWithHistory(const std::vector<float>& sig, float fundHz, float resHz, std::vector<FrameSample>& fundHist, std::vector<FrameSample>& resHist)
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

            float auxRel = aux.currentContext().f0Reliability;
            float mainRel = conf.currentMainF0Reliability();
            auto sample = [&](float hz) {
                FrameSample s;
                auto r = findRegionNear(conf, hz);
                s.found = r.active;
                if (r.active)
                {
                    s.baseEvidence = r.lastBaseEvidence; s.harmonicPenalty = r.lastHarmonicPenalty;
                    s.effReliability = r.effectiveReliability; s.effLikelihood = r.effectiveLikelihood;
                    s.confidenceA = r.confidence;
                }
                s.auxReliability = auxRel; s.mainReliability = mainRel;
                s.auxLikelihood = aux.harmonicLikelihoodFor(hz);
                return s;
            };
            fundHist.push_back(sample(fundHz));
            resHist.push_back(sample(resHz));
        }
    }
};

// Offline policy computation from recorded history.
static float policyB(const FrameSample& s)
{
    if (! s.found) return 0.0f;
    float pull = 0.4f * (1.0f - s.effReliability);
    float damped = s.baseEvidence * (1.0f - pull) + 0.5f * pull;
    return juce::jlimit(0.0f, 1.0f, damped * s.harmonicPenalty);
}
static std::vector<float> policyC(const std::vector<FrameSample>& hist)
{
    std::vector<float> out; out.reserve(hist.size());
    float held = 0.0f; bool haveHeld = false;
    for (auto& s : hist)
    {
        if (! s.found) { out.push_back(held); continue; }
        bool bothUncertain = s.auxReliability < 0.25f && s.mainReliability < 0.25f;
        if (! haveHeld) { held = s.confidenceA; haveHeld = true; }
        else if (bothUncertain) held = 0.9f * held + 0.1f * s.confidenceA; // slow-moving average while uncertain
        else held = s.confidenceA; // snap to fresh value once either source is trustworthy again
        out.push_back(held);
    }
    return out;
}
static float variance(const std::vector<float>& v)
{
    if (v.size() < 2) return 0.0f;
    double mean = 0; for (float x : v) mean += x; mean /= (double) v.size();
    double var = 0; for (float x : v) { double d = x - mean; var += d * d; } var /= (double) v.size();
    return (float) var;
}

struct CaseResult
{
    double sr, fundHz, ratio, ampDb, qVal;
    bool dualUncertainAtEnd = false;
    float confA_fund = 0, confA_res = 0, confB_fund = 0, confB_res = 0, confC_fund = 0, confC_res = 0;
    bool invertedA = false, invertedB = false, invertedC = false;
    bool missedA = false, missedB = false, missedC = false; // resonance confidence < 0.5 -> not flagged as a candidate
    float stabilityA = 0, stabilityC = 0; // variance of confidence over the run (lower = more stable)
};

static CaseResult runCase(double sr, double fundHz, double ratio, float ampDb, double Q, bool excessiveHarmonic = false)
{
    int n = (int) (sr * 1.2);
    auto sig = genHarmonicSeries(sr, n, fundHz, 0.3f, 6);
    double resHz = fundHz * ratio;
    if (excessiveHarmonic)
    {
        // replace the closest real harmonic with a boosted version instead of an independent burst
        sig = genSilence(n);
        for (int h = 1; h <= 6; ++h) { float a = 0.3f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1)); if (std::abs(fundHz * h - resHz) < fundHz * 0.5) a *= (float) juce::Decibels::decibelsToGain(ampDb); addTone(sig, sr, fundHz * h, a); }
    }
    else
    {
        addBurst(sig, sr, resHz, (float) juce::Decibels::decibelsToGain(ampDb) * 0.5f, Q, 3);
    }

    Pipeline p; p.prepare(sr);
    std::vector<FrameSample> fundHist, resHist;
    p.runWithHistory(sig, (float) fundHz, (float) resHz, fundHist, resHist);

    CaseResult r; r.sr = sr; r.fundHz = fundHz; r.ratio = ratio; r.ampDb = ampDb; r.qVal = Q;
    if (fundHist.empty() || resHist.empty()) return r;
    auto& fEnd = fundHist.back(); auto& rEnd = resHist.back();
    r.dualUncertainAtEnd = fEnd.auxReliability < 0.25f && fEnd.mainReliability < 0.25f;
    r.confA_fund = fEnd.confidenceA; r.confA_res = rEnd.confidenceA;
    r.confB_fund = policyB(fEnd); r.confB_res = policyB(rEnd);
    auto fC = policyC(fundHist), rC = policyC(resHist);
    r.confC_fund = fC.back(); r.confC_res = rC.back();
    r.invertedA = r.confA_fund > r.confA_res;
    r.invertedB = r.confB_fund > r.confB_res;
    r.invertedC = r.confC_fund > r.confC_res;
    r.missedA = r.confA_res < 0.5f; r.missedB = r.confB_res < 0.5f; r.missedC = r.confC_res < 0.5f;
    std::vector<float> aHist; for (auto& s : fundHist) aHist.push_back(s.confidenceA);
    r.stabilityA = variance(aHist); r.stabilityC = variance(fC);
    return r;
}

int main()
{
    std::printf("=== 1-3. ADVERSARIAL SWEEP: fundamental x ratio x SR (amp=+6dB, Q=medium=8) ===\n");
    const double fundamentals[] = { 60, 70, 80, 90, 100, 120, 150 };
    const double ratios[] = { 1.35, 1.45, 1.55, 1.63, 1.72, 1.85 };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    std::vector<CaseResult> allCases;
    int dualUncertainCount = 0, dualUncertainInvertedA = 0;
    for (double sr : rates)
        for (double f0 : fundamentals)
            for (double ratio : ratios)
            {
                auto r = runCase(sr, f0, ratio, 6.0f, 8.0);
                allCases.push_back(r);
                if (r.dualUncertainAtEnd) { ++dualUncertainCount; if (r.invertedA) ++dualUncertainInvertedA; }
            }
    std::printf("  total cases=%d  dual-uncertain (auxRel<0.25 & mainRel<0.25)=%d (%.1f%%)  of those, Policy A inverted=%d (%.1f%% of dual-uncertain)\n",
        (int) allCases.size(), dualUncertainCount, 100.0 * dualUncertainCount / juce::jmax(1, (int) allCases.size()),
        dualUncertainInvertedA, 100.0 * dualUncertainInvertedA / juce::jmax(1, dualUncertainCount));

    // breakdown by ratio and by SR to see if it's concentrated
    std::printf("  -- breakdown by ratio (dual-uncertain count / inverted-under-A count) --\n");
    for (double ratio : ratios)
    {
        int du = 0, inv = 0;
        for (auto& r : allCases) if (r.ratio == ratio) { if (r.dualUncertainAtEnd) { ++du; if (r.invertedA) ++inv; } }
        std::printf("    ratio=%.2fx: dual-uncertain=%d inverted=%d\n", ratio, du, inv);
    }
    std::printf("  -- breakdown by sample rate --\n");
    for (double sr : rates)
    {
        int du = 0, inv = 0;
        for (auto& r : allCases) if (r.sr == sr) { if (r.dualUncertainAtEnd) { ++du; if (r.invertedA) ++inv; } }
        std::printf("    sr=%.0fHz: dual-uncertain=%d inverted=%d\n", sr, du, inv);
    }
    std::printf("  -- breakdown by fundamental --\n");
    for (double f0 : fundamentals)
    {
        int du = 0, inv = 0;
        for (auto& r : allCases) if (r.fundHz == f0) { if (r.dualUncertainAtEnd) { ++du; if (r.invertedA) ++inv; } }
        std::printf("    f0=%.0fHz: dual-uncertain=%d inverted=%d\n", f0, du, inv);
    }

    // amplitude/Q sensitivity at a fixed representative point (80Hz, ratio=1.63x, 48kHz)
    std::printf("\n=== amplitude/Q sensitivity (f0=80Hz, ratio=1.63x, sr=48kHz) ===\n");
    const float amps[] = { 3.0f, 6.0f, 9.0f, 12.0f };
    const double qs[] = { 4.0, 8.0, 16.0 }; // narrow/medium/broad
    const char* qNames[] = { "narrow", "medium", "broad" };
    for (float amp : amps)
        for (int qi = 0; qi < 3; ++qi)
        {
            auto r = runCase(48000.0, 80.0, 1.63, amp, qs[(size_t) qi]);
            std::printf("  amp=+%.0fdB Q=%-7s: dualUncertain=%s confA(fund)=%.3f confA(res)=%.3f invertedA=%s\n",
                amp, qNames[qi], r.dualUncertainAtEnd ? "yes" : "no", r.confA_fund, r.confA_res, r.invertedA ? "YES" : "no");
        }

    std::printf("\n=== 4. MUSICAL MATERIAL CONTROLS (occurrence of dual uncertainty, not right/wrong) ===\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // bass clean
        { auto r = runCase(sr, 80.0, 1.63, 6.0f, 8.0); (void) r; }
        int n = (int) (sr * 1.2);
        auto bassClean = genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f);
        Pipeline p1; p1.prepare(sr); std::vector<FrameSample> h1a, h1b; p1.runWithHistory(bassClean, 62.0f, 124.0f, h1a, h1b);
        auto bassNonHarm = bassClean; addBurst(bassNonHarm, sr, 62.0 * 1.63, 0.5f, 8.0, 3);
        Pipeline p2; p2.prepare(sr); std::vector<FrameSample> h2a, h2b; p2.runWithHistory(bassNonHarm, 62.0f, (float) (62.0 * 1.63), h2a, h2b);
        auto kick = genKick(sr, n);
        Pipeline p3; p3.prepare(sr); std::vector<FrameSample> h3a, h3b; p3.runWithHistory(kick, 60.0f, 90.0f, h3a, h3b);
        auto vocal = genVocalMale(sr, n);
        Pipeline p4; p4.prepare(sr); std::vector<FrameSample> h4a, h4b; p4.runWithHistory(vocal, 100.0f, 163.0f, h4a, h4b);
        auto dualUncertain = [](const FrameSample& s) { return s.auxReliability < 0.25f && s.mainReliability < 0.25f; };
        std::printf("  sr=%6.0fHz  bass-clean dualUnc=%s | bass+nonharm dualUnc=%s | kick dualUnc=%s | vocal-male dualUnc=%s\n",
            sr, dualUncertain(h1a.back()) ? "yes" : "no", dualUncertain(h2a.back()) ? "yes" : "no",
            dualUncertain(h3a.back()) ? "yes" : "no", dualUncertain(h4a.back()) ? "yes" : "no");
    }

    std::printf("\n=== 5/7. POLICY A vs B vs C -- aggregate comparison across the main sweep ===\n");
    int invA = 0, invB = 0, invC = 0, missA = 0, missB = 0, missC = 0; float sumStabA = 0, sumStabC = 0;
    for (auto& r : allCases)
    {
        if (r.invertedA) ++invA; if (r.invertedB) ++invB; if (r.invertedC) ++invC;
        if (r.missedA) ++missA; if (r.missedB) ++missB; if (r.missedC) ++missC;
        sumStabA += r.stabilityA; sumStabC += r.stabilityC;
    }
    int total = (int) allCases.size();
    std::printf("  false-fundamental-promotion (conf(fund) > conf(resonance)): A=%d (%.1f%%)  B=%d (%.1f%%)  C=%d (%.1f%%)\n",
        invA, 100.0 * invA / total, invB, 100.0 * invB / total, invC, 100.0 * invC / total);
    std::printf("  missed injected resonance (conf(resonance) < 0.5):          A=%d (%.1f%%)  B=%d (%.1f%%)  C=%d (%.1f%%)\n",
        missA, 100.0 * missA / total, missB, 100.0 * missB / total, missC, 100.0 * missC / total);
    std::printf("  mean temporal variance of fundamental's own confidence:     A=%.4f  C=%.4f  (B not separately tracked -- same shape as A, just damped)\n",
        sumStabA / total, sumStabC / total);
    std::printf("  (Policy C's variance is measured on its OWN held trajectory -- lower means fewer frame-to-frame swings while uncertain)\n");

    std::printf("\n=== RECOMMENDATION ===\n");
    std::printf("  See numbers above. Policy A is production-current; B and C are candidate corrections, NOT applied to production.\n");
    std::printf("  This harness makes no change to ConfidenceEngine.cpp -- awaiting review before any policy is adopted.\n");
    return 0;
}
