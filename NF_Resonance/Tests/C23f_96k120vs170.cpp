// PHYSICAL C2.3f: 96kHz / 120Hz(fundamental) vs 170Hz(non-harmonic) final
// gate. Full per-region decomposition + temporal (not just last-frame)
// behavior. Ground truth (which is fundamental, which is resonance) is
// used ONLY in this harness's own reporting -- never fed into the DSP.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
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

static const char* srcName(ConfidenceEngine::CandidateSource s) { return s == ConfidenceEngine::CandidateSource::AuxRescue ? "AUX_RESCUE" : "MAIN"; }

int main()
{
    double sr = 96000.0; const int kFft = 2048, kHop = 512;
    int n = (int) (sr * 2.0); // longer duration for real temporal stats
    auto sig = genHarmonicSeries(sr, n, 120.0, 0.3f, 6);
    addBurst(sig, sr, 170.0, 0.5f, 8.0, 2);

    SpectralProminenceEngineV5 prom; prom.prepare(kFft / 2 + 1, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop);
    juce::dsp::FFT fft(11);
    std::array<float, kFft> window{};
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::array<float, kFft * 2> scratch{};
    std::vector<float> magDb((size_t) (kFft / 2 + 1)), promOut((size_t) (kFft / 2 + 1));

    auto findRegionNear = [&](float hz, float tol = 0.25f) -> ConfidenceEngine::Region {
        float target = std::log2(juce::jmax(1.0f, hz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
        if (best && bestDist <= tol) return *best; return ConfidenceEngine::Region{};
    };

    std::vector<float> c120hist, c170hist; int gt170beats120 = 0, bothFoundFrames = 0, totalFrames = 0;
    const int warmupFrames = 15;
    ConfidenceEngine::Region last120, last170;
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
        conf.process(promOut, &aux, &magDb);

        ++totalFrames;
        auto r120 = findRegionNear(120.0f), r170 = findRegionNear(170.0f);
        if ((int) (i / kHop) >= warmupFrames)
        {
            if (r120.active) c120hist.push_back(r120.confidence);
            if (r170.active) c170hist.push_back(r170.confidence);
            if (r120.active && r170.active) { ++bothFoundFrames; if (r170.confidence > r120.confidence) ++gt170beats120; }
        }
        last120 = r120; last170 = r170;
    }

    auto pct = [](std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; };

    std::printf("=== C2.3f: 96kHz, 120Hz(fundamental) vs 170Hz(non-harmonic) ===\n");
    std::printf("\n-- item 1: full decomposition, last frame --\n");
    auto printFull = [](const char* label, const ConfidenceEngine::Region& r) {
        if (! r.active) { std::printf("  %s: NOT FOUND\n", label); return; }
        std::printf("  %s: source=%s freq=%.1f mainPeakProm=%.2f candEv=%.3f auxRescueAuthority=%.3f\n", label, srcName(r.lastCandidateSource), r.centerHz, r.peakProminenceDb, r.candidateEvidence, r.lastAuxRescueAuthority);
        std::printf("    baseEvidence(prominence=%.3f persistence=%.3f stability=%.3f width=%.3f) = %.3f\n", r.lastProminenceEvidence, r.lastPersistenceEvidence, r.lastStabilityEvidence, r.lastWidthEvidence, r.lastBaseEvidence);
        std::printf("    mainHarmLike=%.3f auxHarmLike=%.3f auxReliability=%.3f effLikelihood=%.3f effReliability=%.3f excessFactor=%.3f effProtection=%.3f\n", r.harmonicLikelihood, r.auxHarmonicLikelihood, r.auxReliability, r.effectiveLikelihood, r.effectiveReliability, r.excessFactor, r.effectiveHarmonicProtection);
        std::printf("    lastHarmonicPenalty=%.3f finalConfidence=%.3f\n", r.lastHarmonicPenalty, r.confidence);
        std::printf("    selectivityWeight: ");
        for (float sel : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f }) std::printf("sel=%.1f->%.3f  ", sel, ConfidenceEngine::passWeight(r.confidence, sel));
        std::printf("\n");
    };
    printFull("120Hz (fundamental)", last120);
    printFull("170Hz (non-harmonic)", last170);

    std::printf("\n-- item 2: temporal behavior (post warm-up, %d frames total, %d with both found) --\n", totalFrames - warmupFrames, bothFoundFrames);
    std::printf("  120Hz confidence: P50=%.3f P90=%.3f max=%.3f (n=%d)\n", pct(c120hist, 50), pct(c120hist, 90), c120hist.empty() ? 0.0f : *std::max_element(c120hist.begin(), c120hist.end()), (int) c120hist.size());
    std::printf("  170Hz confidence: P50=%.3f P90=%.3f max=%.3f (n=%d)\n", pct(c170hist, 50), pct(c170hist, 90), c170hist.empty() ? 0.0f : *std::max_element(c170hist.begin(), c170hist.end()), (int) c170hist.size());
    std::printf("  %% of frames (both found) where 170>120: %.1f%% (%d/%d)\n", bothFoundFrames ? 100.0 * gt170beats120 / bothFoundFrames : 0.0, gt170beats120, bothFoundFrames);

    std::printf("\n-- item 3: classification --\n");
    bool highConfWrong = last120.effectiveReliability > 0.6f && last120.persistence > 0.7f && last120.confidence > 0.5f
                       && ConfidenceEngine::passWeight(last120.confidence, 5.0f) > 0.5f;
    std::printf("  120Hz effReliability=%.3f persistence=%.3f confidence=%.3f selWeight@5=%.3f\n",
        last120.effectiveReliability, last120.persistence, last120.confidence, ConfidenceEngine::passWeight(last120.confidence, 5.0f));
    std::printf("  CLASSIFICATION: %s\n", highConfWrong ? "CASE A -- high-confidence wrong decision, needs fix" : "CASE B -- ambiguous/insufficient evidence, documentable corner case");

    return 0;
}
