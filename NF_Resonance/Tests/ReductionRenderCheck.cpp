// REDUCTION visualization audit (Sonic Alpha V2). Verifies that
// SpectrumComponent::resampleReductionForDisplay (the min-preserving
// bin->render-point downsample) does not lose narrow real notches that a
// naive single-point linear-interpolation resample would silently skip
// between two sample points. No DSP file is touched or modified -- this
// drives GainMaskEngine for real appliedReductionSnapshot-equivalent data,
// then exercises ONLY the rendering-side static function.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/ResonanceDetector.h"
#include "UI/SpectrumComponent.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < (int) b.size(); ++i) { b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; }
}

// Ten closely-spaced resonances between 1kHz and 5kHz, each riding a low
// harmonic bed so PHYSICAL C's prominence machinery has real structure to
// flag (a bare isolated sine has nothing to stand prominent against).
static std::vector<float> genCloseResonances1to5k(double sr, int n)
{
    auto b = genSilence(n);
    addTone(b, sr, 220.0, 0.10f); addTone(b, sr, 440.0, 0.07f);
    const double freqs[] = { 1200, 1600, 2000, 2400, 2800, 3200, 3600, 4000, 4400, 4800 };
    for (double f : freqs) addTone(b, sr, f, 0.17f);
    return b;
}

// OLD-STYLE reference: single-point linear interpolation per render point
// (the behaviour this session's audit replaced) -- kept here ONLY as a
// comparison baseline for this test, not used anywhere in the real plugin.
static void oldStyleResample(const std::vector<float>& binReductionDb, double sampleRate, int fftSize, int numPts, std::vector<float>& out)
{
    out.assign((size_t) numPts, 0.0f);
    const int bins = (int) binReductionDb.size();
    auto binPosForHz = [&](float hz) { return juce::jlimit(0.0f, (float) (bins - 1), hz * (float) fftSize / (float) sampleRate); };
    const double lo20 = std::log10(20.0), range = std::log10(20000.0) - lo20;
    for (int k = 0; k < numPts; ++k)
    {
        float t = (float) k / (float) (numPts - 1);
        float hz = (float) std::pow(10.0, lo20 + (double) t * range);
        float binPos = binPosForHz(hz);
        size_t lo = (size_t) binPos, hi = juce::jmin((size_t) (bins - 1), lo + 1);
        float frac = binPos - (float) lo;
        out[(size_t) k] = binReductionDb[lo] + (binReductionDb[hi] - binReductionDb[lo]) * frac;
    }
}

struct ValleyStats { int count; float deepestDb; };
static ValleyStats countActiveValleys(const std::vector<float>& v, float gateDb)
{
    ValleyStats s{ 0, 0.0f };
    int i = 0, n = (int) v.size();
    while (i < n)
    {
        if (std::abs(v[(size_t) i]) > gateDb)
        {
            float deepest = v[(size_t) i];
            while (i < n && std::abs(v[(size_t) i]) > gateDb) { deepest = juce::jmin(deepest, v[(size_t) i]); ++i; }
            ++s.count; s.deepestDb = juce::jmin(s.deepestDb, deepest);
        }
        else ++i;
    }
    return s;
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  %s: %s\n", what, cond ? "PASS" : "FAIL"); if (!cond) allPass = false; };

    std::printf("=== REDUCTION render audit: min-preserving vs old point-sample ===\n\n");

    for (float detailVal : { 0.0f, 5.0f, 10.0f })
    {
        GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
        mask.setParams(5.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f); mask.setDetail(detailVal);
        float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
        int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
        mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);

        juce::dsp::FFT fft(11);
        std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        auto sig = genCloseResonances1to5k(sr, n);
        std::vector<float> lastReduction;
        for (int i = 0; i + kFft <= (int) sig.size(); i += kHop)
        {
            for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            for (int b = 0; b < bins; ++b)
            { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
              magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
            mask.process(magDb, sig.data() + i, kHop, reductionOut);
            lastReduction = reductionOut; // steady-state (last frame) snapshot, same convention as appliedReductionSnapshot()
        }

        // Ground truth: valleys directly in the real per-bin snapshot (what appliedReductionSnapshot() itself holds).
        auto groundTruth = countActiveValleys(lastReduction, 0.05f);

        // Render-point counts at a realistic analyzer width (~660px plot -> numPts=165, matches resized()'s spectrum.setBounds width).
        const int numPts = 165;
        std::vector<float> oldRender, newRender;
        oldStyleResample(lastReduction, sr, kFft, numPts, oldRender);
        SpectrumComponent::resampleReductionForDisplay(lastReduction, sr, kFft, numPts, newRender);
        auto oldStats = countActiveValleys(oldRender, 0.05f);
        auto newStats = countActiveValleys(newRender, 0.05f);

        std::printf("-- Detail=%.0f --\n", detailVal);
        std::printf("  Ground truth (real bins):     valleys=%d deepest=%.2fdB\n", groundTruth.count, groundTruth.deepestDb);
        std::printf("  OLD point-sample render:      valleys=%d deepest=%.2fdB\n", oldStats.count, oldStats.deepestDb);
        std::printf("  NEW min-preserving render:    valleys=%d deepest=%.2fdB\n", newStats.count, newStats.deepestDb);
        check("NEW render preserves at least as many valleys as OLD", newStats.count >= oldStats.count);
        check("NEW render's deepest point is at least as deep as OLD's (never shallower)", newStats.deepestDb <= oldStats.deepestDb + 1.0e-4f);
        check("NEW render never exceeds the real ground-truth deepest value (no invented depth)", newStats.deepestDb >= groundTruth.deepestDb - 1.0e-4f);
    }

    std::printf("\n-- Detail 0 vs 5 vs 10: render should show progressively more independent valleys --\n");
    {
        std::vector<int> valleyCounts;
        for (float detailVal : { 0.0f, 5.0f, 10.0f })
        {
            GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
            mask.setParams(5.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f); mask.setDetail(detailVal);
            float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
            int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
            mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);
            juce::dsp::FFT fft(11);
            std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
            for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
            auto sig = genCloseResonances1to5k(sr, n);
            std::vector<float> lastReduction;
            for (int i = 0; i + kFft <= (int) sig.size(); i += kHop)
            {
                for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
                std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform(scratch.data());
                for (int b = 0; b < bins; ++b)
                { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
                  magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
                mask.process(magDb, sig.data() + i, kHop, reductionOut);
                lastReduction = reductionOut;
            }
            std::vector<float> newRender;
            SpectrumComponent::resampleReductionForDisplay(lastReduction, sr, kFft, 165, newRender);
            auto s = countActiveValleys(newRender, 0.05f);
            std::printf("  Detail=%-4.0f rendered valleys=%d\n", detailVal, s.count);
            valleyCounts.push_back(s.count);
        }
        check("Detail 10 renders >= valleys than Detail 5, which renders >= valleys than Detail 0", valleyCounts[2] >= valleyCounts[1] && valleyCounts[1] >= valleyCounts[0]);
    }

    // Synthetic direct-bin test: isolates the RENDERER from PHYSICAL C's own
    // region grouping (which, on the real detector, merges closely-spaced
    // resonances into few broad regions well before rendering ever sees
    // them -- confirmed above, out of scope here). Builds a per-bin array
    // with several genuinely narrow (a few bins wide) notches, matching
    // what Detail=10's near-raw octave smoothing can leave behind, and
    // checks the renderer alone -- exactly the "audit only the
    // visualization" scope this check is for.
    std::printf("\n-- Synthetic direct-bin test: narrow few-bin-wide notches, renderer only --\n");
    {
        const int synBins = bins; // 1025, matches the real engine's own bin count at this fftSize
        std::vector<float> synth((size_t) synBins, 0.0f);
        // Ten narrow notches (3 bins wide each) between 1kHz and 5kHz, at
        // 48kHz/2048 that's ~23.4Hz/bin -- each notch ~70Hz wide, spaced
        // ~400Hz apart so they are clearly independent in the ground truth.
        auto hzToBin = [&](double hz) { return (int) std::round(hz * kFft / sr); };
        const double freqs[] = { 1200, 1600, 2000, 2400, 2800, 3200, 3600, 4000, 4400, 4800 };
        for (double f : freqs)
        {
            int c = hzToBin(f);
            for (int d = -1; d <= 1; ++d) { int b = c + d; if (b >= 0 && b < synBins) synth[(size_t) b] = -3.0f + (d != 0 ? 1.5f : 0.0f); }
        }
        auto gt = countActiveValleys(synth, 0.05f);
        std::vector<float> oldR, newR;
        oldStyleResample(synth, sr, kFft, 165, oldR);
        SpectrumComponent::resampleReductionForDisplay(synth, sr, kFft, 165, newR);
        auto oldS = countActiveValleys(oldR, 0.05f);
        auto newS = countActiveValleys(newR, 0.05f);
        std::printf("  Ground truth (10 narrow 3-bin notches): valleys=%d deepest=%.2fdB\n", gt.count, gt.deepestDb);
        std::printf("  OLD point-sample render:                valleys=%d deepest=%.2fdB\n", oldS.count, oldS.deepestDb);
        std::printf("  NEW min-preserving render:               valleys=%d deepest=%.2fdB\n", newS.count, newS.deepestDb);
        check("NEW render recovers meaningfully more of the real narrow notches than OLD", newS.count > oldS.count);
        check("NEW render's deepest point matches the real ground truth exactly (no dilution)", std::abs(newS.deepestDb - gt.deepestDb) < 1.0e-4f);
        check("OLD render lost real valleys and/or depth vs ground truth (demonstrates the bug this fixes)", oldS.count < gt.count || oldS.deepestDb > gt.deepestDb + 0.1f);
    }

    std::printf("\n%s\n", allPass ? "=== ALL REDUCTION RENDER CHECKS PASS ===" : "=== REDUCTION RENDER CHECKS FAILED ===");
    return allPass ? 0 : 1;
}
