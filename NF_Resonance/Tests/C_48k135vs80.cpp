// Final-gate follow-up: 48kHz / 80Hz(fundamental) vs 135Hz(non-harmonic)
// final gate. Full per-region decomposition + temporal (not just last-frame)
// behavior, same method as C2.3f (96kHz/120vs170). Ground truth (which is
// fundamental, which is resonance) is used ONLY in this harness's own
// reporting -- never fed into the DSP.

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
    double sr = 48000.0; const int kFft = 2048, kHop = 512;
    int n = (int) (sr * 2.0); // longer duration for real temporal stats
    auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
    addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);

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

    std::vector<float> c80hist, c135hist; int gt135beats80 = 0, bothFoundFrames = 0, totalFrames = 0;
    const int warmupFrames = 15;
    ConfidenceEngine::Region last80, last135;
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
        auto r80 = findRegionNear(80.0f), r135 = findRegionNear(135.0f);
        if ((int) (i / kHop) >= warmupFrames)
        {
            if (r80.active) c80hist.push_back(r80.confidence);
            if (r135.active) c135hist.push_back(r135.confidence);
            if (r80.active && r135.active) { ++bothFoundFrames; if (r135.confidence > r80.confidence) ++gt135beats80; }
            int fr = (int) (i / kHop);
            if (fr >= warmupFrames && fr < warmupFrames + 40)
                std::printf("    frame %3d: 135Hz %s candEv=%.3f prom=%.2f | 80Hz %s candEv=%.3f\n",
                    fr, r135.active ? "ACTIVE" : "absent", r135.candidateEvidence, r135.peakProminenceDb,
                    r80.active ? "ACTIVE" : "absent", r80.candidateEvidence);
        }
        last80 = r80; last135 = r135;
    }
    std::printf("  135Hz final framesPresent=%d framesAbsent=%d | 80Hz final framesPresent=%d framesAbsent=%d\n",
        last135.framesPresent, last135.framesAbsent, last80.framesPresent, last80.framesAbsent);

    auto pct = [](std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; };

    std::printf("=== 48kHz, 80Hz(fundamental) vs 135Hz(non-harmonic) ===\n");
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
    printFull("80Hz (fundamental)", last80);
    printFull("135Hz (non-harmonic)", last135);

    std::printf("\n-- item 2: temporal behavior (post warm-up, %d frames total, %d with both found) --\n", totalFrames - warmupFrames, bothFoundFrames);
    std::printf("  80Hz confidence: P50=%.3f P90=%.3f max=%.3f (n=%d)\n", pct(c80hist, 50), pct(c80hist, 90), c80hist.empty() ? 0.0f : *std::max_element(c80hist.begin(), c80hist.end()), (int) c80hist.size());
    std::printf("  135Hz confidence: P50=%.3f P90=%.3f max=%.3f (n=%d)\n", pct(c135hist, 50), pct(c135hist, 90), c135hist.empty() ? 0.0f : *std::max_element(c135hist.begin(), c135hist.end()), (int) c135hist.size());
    std::printf("  %% of frames (both found) where 135>80: %.1f%% (%d/%d)\n", bothFoundFrames ? 100.0 * gt135beats80 / bothFoundFrames : 0.0, gt135beats80, bothFoundFrames);

    std::printf("\n-- item 3: classification --\n");
    bool highConfWrong = last80.effectiveReliability > 0.6f && last80.persistence > 0.7f && last80.confidence > 0.5f
                       && ConfidenceEngine::passWeight(last80.confidence, 5.0f) > 0.5f;
    std::printf("  80Hz effReliability=%.3f persistence=%.3f confidence=%.3f selWeight@5=%.3f\n",
        last80.effectiveReliability, last80.persistence, last80.confidence, ConfidenceEngine::passWeight(last80.confidence, 5.0f));
    std::printf("  CLASSIFICATION: %s\n", highConfWrong ? "CASE A -- high-confidence wrong decision, needs fix" : "CASE B -- ambiguous/insufficient evidence, documentable corner case");

    return 0;
}
