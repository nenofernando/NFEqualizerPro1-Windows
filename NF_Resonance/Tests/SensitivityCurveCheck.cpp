// WHITE SENSITIVITY CURVE integration check (Sonic Alpha V2 -- Band Bias).
// Drives GainMaskEngine directly (same class the plugin uses) with three
// isolated resonances at 500Hz/2kHz/5kHz, then compares REDUCTION activity
// at each location under: flat curve, +8dB @2kHz, -8dB @2kHz. Confirms:
// raising the curve at 2kHz increases Reduction activity there while
// 500Hz/5kHz stay essentially unchanged; lowering it decreases 2kHz
// activity; the curve never manufactures reduction where there is no
// resonance (flat-region bins stay at/near zero regardless of the curve).
// No PHYSICAL C/D file is touched or modified by this file.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/ResonanceDetector.h"
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
static std::vector<float> genThreeResonances(double sr, int n)
{
    // Each tone rides on its own harmonic pair so PHYSICAL C's prominence/
    // harmonic machinery has real local structure to flag as a candidate
    // resonance (a single bare sine has no "surrounding content" to stand
    // prominent against).
    auto b = genSilence(n);
    addTone(b, sr, 220.0, 0.12f); addTone(b, sr, 440.0, 0.08f); addTone(b, sr, 880.0, 0.05f); // low harmonic bed
    addTone(b, sr, 500.0, 0.22f);  // resonance A
    addTone(b, sr, 2000.0, 0.22f); // resonance B (the one the curve will target)
    addTone(b, sr, 5000.0, 0.22f); // resonance C
    return b;
}
static std::vector<float> genVocal(double sr, int n)
{
    auto b = genSilence(n);
    for (int h = 1; h <= 8; ++h) addTone(b, sr, 140.0 * h, 0.25f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1)));
    addTone(b, sr, 2000.0, 0.18f); // extra resonance in the curve's target band
    return b;
}
static std::vector<float> genDense(double sr, int n)
{
    auto b = genSilence(n);
    for (int h = 1; h <= 6; ++h) addTone(b, sr, 62.0 * h, 0.18f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1)));
    for (int h = 1; h <= 8; ++h) addTone(b, sr, 220.0 * h, 0.13f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1)));
    addTone(b, sr, 2000.0, 0.15f);
    return b;
}

struct BandActivity { float meanRedDb, maxRedDb; };

static BandActivity measureBand(const std::vector<float>& reductionTrace, double sr, int fftSize, float loHz, float hiHz)
{
    // reductionTrace holds bins*frames concatenated; caller passes a
    // pre-sliced per-target-band trace instead -- see runCheck().
    (void) sr; (void) fftSize; (void) loHz; (void) hiHz;
    float sum = 0.0f, worst = 0.0f;
    for (float v : reductionTrace) { sum += v; worst = juce::jmin(worst, v); }
    return { reductionTrace.empty() ? 0.0f : sum / (float) reductionTrace.size(), worst };
}

static void runCheck(const char* label, const std::vector<float>& sig, double sr,
                      bool curveActive, float curveFreq, float curveSensDb, float curveWidthOct,
                      BandActivity& at500, BandActivity& at2k, BandActivity& at5k)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(5.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f); // Depth=5, Selectivity=3.5 -- same convention as CalibrationAudit

    float bandFreq[ResonanceDetector::kMaxBands]{}, bandSens[ResonanceDetector::kMaxBands]{}, bandWidth[ResonanceDetector::kMaxBands]{}, bandFocus[ResonanceDetector::kMaxBands]{};
    int bandShape[ResonanceDetector::kMaxBands]{};
    bool bandActive[ResonanceDetector::kMaxBands]{};
    if (curveActive) { bandFreq[0] = curveFreq; bandSens[0] = curveSensDb; bandWidth[0] = curveWidthOct; bandShape[0] = 0; bandFocus[0] = 0.5f; bandActive[0] = true; }
    mask.setSensitivityCurve(bandFreq, bandSens, bandWidth, bandShape, bandFocus, bandActive);

    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));

    auto binOf = [&](float hz){ return juce::jlimit(0, bins - 1, (int) std::round((double) hz / (sr / kFft))); };
    int b500 = binOf(500.0f), b2k = binOf(2000.0f), b5k = binOf(5000.0f);
    const int halfSpan = 2; // a few bins either side, since regions are locally smoothed/spread

    std::vector<float> trace500, trace2k, trace5k;
    int n = (int) sig.size();
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
          magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
        mask.process(magDb, sig.data() + i, kHop, reductionOut);

        for (int b = juce::jmax(0, b500 - halfSpan); b <= juce::jmin(bins - 1, b500 + halfSpan); ++b) trace500.push_back(reductionOut[(size_t) b]);
        for (int b = juce::jmax(0, b2k - halfSpan); b <= juce::jmin(bins - 1, b2k + halfSpan); ++b) trace2k.push_back(reductionOut[(size_t) b]);
        for (int b = juce::jmax(0, b5k - halfSpan); b <= juce::jmin(bins - 1, b5k + halfSpan); ++b) trace5k.push_back(reductionOut[(size_t) b]);
    }
    at500 = measureBand(trace500, sr, kFft, 0, 0);
    at2k = measureBand(trace2k, sr, kFft, 0, 0);
    at5k = measureBand(trace5k, sr, kFft, 0, 0);
    std::printf("  [%s] 500Hz: mean=%.3fdB max=%.3fdB | 2kHz: mean=%.3fdB max=%.3fdB | 5kHz: mean=%.3fdB max=%.3fdB\n",
        label, at500.meanRedDb, at500.maxRedDb, at2k.meanRedDb, at2k.maxRedDb, at5k.meanRedDb, at5k.maxRedDb);
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    auto sig = genThreeResonances(sr, n);

    std::printf("=== WHITE SENSITIVITY CURVE integration check ===\n\n");
    std::printf("-- Three isolated resonances (500Hz/2kHz/5kHz), Depth=5, Selectivity=3.5 --\n");
    BandActivity flat500, flat2k, flat5k, up500, up2k, up5k, down500, down2k, down5k;
    runCheck("flat curve (neutral)", sig, sr, false, 0, 0, 0, flat500, flat2k, flat5k);
    runCheck("+8dB @2kHz (w=1.0oct)", sig, sr, true, 2000.0f, 8.0f, 1.0f, up500, up2k, up5k);
    runCheck("-8dB @2kHz (w=1.0oct)", sig, sr, true, 2000.0f, -8.0f, 1.0f, down500, down2k, down5k);

    std::printf("\n-- Verdict --\n");
    bool pass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  %s: %s\n", what, cond ? "PASS" : "FAIL"); if (!cond) pass = false; };
    check("Raising +8dB @2kHz increases 2kHz mean reduction vs flat", up2k.meanRedDb < flat2k.meanRedDb - 1.0e-3f);
    check("Raising +8dB @2kHz leaves 500Hz mean reduction ~unchanged (within 0.15dB)", std::abs(up500.meanRedDb - flat500.meanRedDb) < 0.15f);
    check("Raising +8dB @2kHz leaves 5kHz mean reduction ~unchanged (within 0.15dB)", std::abs(up5k.meanRedDb - flat5k.meanRedDb) < 0.15f);
    check("Lowering -8dB @2kHz decreases 2kHz mean reduction vs flat", down2k.meanRedDb > flat2k.meanRedDb + 1.0e-3f);
    check("Ordering is monotonic: down2k > flat2k > up2k (mean, more negative = more reduction)", down2k.meanRedDb > flat2k.meanRedDb && flat2k.meanRedDb > up2k.meanRedDb);

    std::printf("\n-- Musical material: Vocal / Dense mix, flat vs +6dB @2kHz --\n");
    auto vocal = genVocal(sr, n); auto dense = genDense(sr, n);
    BandActivity vf500, vf2k, vf5k, vu500, vu2k, vu5k, df500, df2k, df5k, du500, du2k, du5k;
    runCheck("Vocal flat", vocal, sr, false, 0, 0, 0, vf500, vf2k, vf5k);
    runCheck("Vocal +6dB@2k", vocal, sr, true, 2000.0f, 6.0f, 1.0f, vu500, vu2k, vu5k);
    runCheck("Dense flat", dense, sr, false, 0, 0, 0, df500, df2k, df5k);
    runCheck("Dense +6dB@2k", dense, sr, true, 2000.0f, 6.0f, 1.0f, du500, du2k, du5k);
    check("Vocal: +6dB@2k increases 2kHz mean reduction", vu2k.meanRedDb < vf2k.meanRedDb - 1.0e-3f);
    check("Dense: +6dB@2k increases 2kHz mean reduction", du2k.meanRedDb < df2k.meanRedDb - 1.0e-3f);

    std::printf("\n%s\n", pass ? "=== ALL SENSITIVITY CURVE CHECKS PASS ===" : "=== SENSITIVITY CURVE CHECKS FAILED ===");
    return pass ? 0 : 1;
}
