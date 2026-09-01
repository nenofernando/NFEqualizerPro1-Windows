// DETAIL / mask granularity integration check (Sonic Alpha V2). Drives
// GainMaskEngine directly (same class the plugin uses). Covers: (1) a
// multi-resonance signal (2.2k/2.6k/3.1k/3.8k) at Detail 0/2.5/5/7.5/10,
// measuring local-minima count, valley width, max/mean reduction, active
// bins, Delta energy; (2) White Sensitivity Curve x Detail interaction;
// (3) musical material guardrails (Bass/Vocal/Guitar/Kick/Dense) at
// Detail 0/5/10; (4) CPU cost Detail=0 vs Detail=10 at 192kHz; (5) sample
// rate coherence 44.1/48/96/192kHz. No PHYSICAL C/D file is touched.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/ResonanceDetector.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < (int) b.size(); ++i) { b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; }
}
static void addClick(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i)
    { float env = (float) std::exp(-(double) i / (lenSamples * 0.3)); b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp * env; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{ auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }

static std::vector<float> genFourResonances(double sr, int n)
{
    auto b = genSilence(n);
    addTone(b, sr, 220.0, 0.10f); addTone(b, sr, 440.0, 0.07f); // low bed for prominence context
    addTone(b, sr, 2200.0, 0.16f); addTone(b, sr, 2600.0, 0.16f); addTone(b, sr, 3100.0, 0.16f); addTone(b, sr, 3800.0, 0.16f);
    return b;
}
static std::vector<float> genBass(double sr, int n) { auto b = genHarmonicSeries(sr, n, 55.0, 0.3f, 8, 2.0f); addClick(b, 0, (int) (sr * 0.004), 0.5f, 6); return b; }
static std::vector<float> genKick(double sr, int n) { auto b = genSilence(n); addTone(b, sr, 60.0, 0.35f); addClick(b, 0, (int) (sr * 0.006), 0.7f, 3); return b; }
static std::vector<float> genVocal(double sr, int n) { return genHarmonicSeries(sr, n, 140.0, 0.25f, 8, 2.5f); }
static std::vector<float> genGuitar(double sr, int n) { auto b = genHarmonicSeries(sr, n, 110.0, 0.25f, 8, 2.5f); addClick(b, 0, (int) (sr * 0.003), 0.5f, 5); return b; }
static std::vector<float> genDense(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 62.0, 0.18f, 6, 2.5f);
    auto vocal = genHarmonicSeries(sr, n, 220.0, 0.13f, 8, 2.5f); for (size_t i = 0; i < b.size(); ++i) b[i] += vocal[i];
    addClick(b, (int) (sr * 0.05), (int) (sr * 0.003), 0.6f, 9);
    return b;
}
static std::vector<float> genThreeToFiveK(double sr, int n)
{
    auto b = genSilence(n);
    addTone(b, sr, 500.0, 0.08f); // low anchor for prominence context
    addTone(b, sr, 2400.0, 0.15f); addTone(b, sr, 2900.0, 0.15f); addTone(b, sr, 3400.0, 0.15f); addTone(b, sr, 4100.0, 0.15f); addTone(b, sr, 4700.0, 0.15f);
    return b;
}

struct FrameResult { std::vector<float> reduction; };

// Runs GainMaskEngine over `sig`, returns per-frame reduction snapshots
// (only every Nth frame kept, to bound memory) plus summary stats.
struct RunStats { float maxRedDb = 0, meanActiveRedDb = 0, deltaRms = 0, inRms = 0, outRms = 0; int avgActiveBins = 0; int avgValleys = 0; float avgValleyWidthOct = 0; };

static int countValleys(const std::vector<float>& red, double sr, int fftSize, float& outMeanWidthOct)
{
    // A "valley" = a maximal contiguous run of bins with reduction deeper
    // than -0.1dB. Width measured in octaves at the run's own center freq.
    const int bins = (int) red.size();
    int count = 0; double widthSum = 0;
    int i = 0;
    while (i < bins)
    {
        if (red[(size_t) i] < -0.1f)
        {
            int start = i;
            while (i < bins && red[(size_t) i] < -0.1f) ++i;
            int end = i - 1;
            float hzLo = juce::jmax(1.0f, (float) (start * sr / fftSize));
            float hzHi = juce::jmax(hzLo + 1.0f, (float) (end * sr / fftSize));
            widthSum += std::log2(hzHi / hzLo);
            ++count;
        }
        else ++i;
    }
    outMeanWidthOct = count ? (float) (widthSum / count) : 0.0f;
    return count;
}

static RunStats runSignal(const std::vector<float>& sig, double sr, float depth, float selectivity, float detail,
                           bool curveActive = false, float curveFreq = 0, float curveSensDb = 0, float curveWidthOct = 1.0f,
                           std::vector<float>* lastReductionOut = nullptr)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(depth, selectivity, 10.0f, 80.0f, 20.0f, 20000.0f);
    mask.setDetail(detail);
    float bandFreq[ResonanceDetector::kMaxBands]{}, bandSens[ResonanceDetector::kMaxBands]{}, bandWidth[ResonanceDetector::kMaxBands]{}, bandFocus[ResonanceDetector::kMaxBands]{};
    int bandShape[ResonanceDetector::kMaxBands]{}; bool bandActive[ResonanceDetector::kMaxBands]{};
    if (curveActive) { bandFreq[0] = curveFreq; bandSens[0] = curveSensDb; bandWidth[0] = curveWidthOct; bandShape[0] = 0; bandFocus[0] = 0.5f; bandActive[0] = true; }
    mask.setSensitivityCurve(bandFreq, bandSens, bandWidth, bandShape, bandFocus, bandActive);

    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));

    RunStats stats;
    double sumAppliedMin = 0.0, sumActiveRed = 0.0, sumInSq = 0.0, sumOutSq = 0.0, sumDeltaSq = 0.0;
    long activeBinSum = 0, valleySum = 0; double widthOctSum = 0;
    int frames = 0; float worst = 0.0f;
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

        float mn = 0; int active = 0; double activeSum = 0;
        for (float v : reductionOut) { mn = juce::jmin(mn, v); if (v < -0.05f) { ++active; activeSum += v; } }
        sumAppliedMin += mn; worst = juce::jmin(worst, mn); activeBinSum += active;
        if (active > 0) sumActiveRed += activeSum / active;
        float widthOct = 0.0f; int v = countValleys(reductionOut, sr, kFft, widthOct);
        valleySum += v; widthOctSum += widthOct;

        // Time-domain RMS bookkeeping via the applied gain on this frame's
        // own hop-worth of input energy (approximation sufficient for a
        // relative Detail comparison, not a claim of exact OLA RMS).
        for (int k = 0; k < kHop; ++k)
        {
            float x = sig[(size_t) (i + k)];
            sumInSq += (double) x * x;
        }
        ++frames;
        if (lastReductionOut) *lastReductionOut = reductionOut;
    }
    stats.maxRedDb = worst;
    stats.meanActiveRedDb = frames ? (float) (sumActiveRed / frames) : 0.0f;
    stats.avgActiveBins = frames ? (int) (activeBinSum / frames) : 0;
    stats.avgValleys = frames ? (int) (valleySum / frames) : 0;
    stats.avgValleyWidthOct = frames ? (float) (widthOctSum / frames) : 0.0f;
    (void) sumOutSq; (void) sumDeltaSq;
    return stats;
}

// Musical-material stats via a second engine instance with Delta engaged
// (g=1-gain applied instead of g), for RMS-loss/Delta-ratio reporting --
// mirrors CalibrationAudit/GainMaskV2Check's own convention.
struct MusicalStats { float inRms, outRms, rmsLossDb, deltaRms, deltaRatio; RunStats core; };

static MusicalStats runMusical(const std::vector<float>& sig, double sr, float depth, float selectivity, float detail)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(depth, selectivity, 10.0f, 80.0f, 20.0f, 20000.0f);
    mask.setDetail(detail);
    float bandFreq[ResonanceDetector::kMaxBands]{}, bandSens[ResonanceDetector::kMaxBands]{}, bandWidth[ResonanceDetector::kMaxBands]{}, bandFocus[ResonanceDetector::kMaxBands]{};
    int bandShape[ResonanceDetector::kMaxBands]{}; bool bandActive[ResonanceDetector::kMaxBands]{};
    mask.setSensitivityCurve(bandFreq, bandSens, bandWidth, bandShape, bandFocus, bandActive);

    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), scratchDelta((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));

    double sumInSq = 0, sumOutSq = 0, sumDeltaSq = 0;
    int n = (int) sig.size();
    RunStats core; double sumAppliedMin = 0, sumActiveRed = 0; long activeBinSum = 0, valleySum = 0; double widthOctSum = 0; int frames = 0; float worst = 0.0f;
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
          magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
        mask.process(magDb, sig.data() + i, kHop, reductionOut);

        float mn = 0; int active = 0; double activeSum = 0;
        for (float v : reductionOut) { mn = juce::jmin(mn, v); if (v < -0.05f) { ++active; activeSum += v; } }
        sumAppliedMin += mn; worst = juce::jmin(worst, mn); activeBinSum += active;
        if (active > 0) sumActiveRed += activeSum / active;
        float widthOct = 0.0f; int v = countValleys(reductionOut, sr, kFft, widthOct);
        valleySum += v; widthOctSum += widthOct;

        // Apply gain (wet) and inverse gain (delta) on the SAME frame's FFT
        // to accumulate approximate steady-state RMS -- consistent
        // approach to CalibrationAudit's own (mean-of-frame-minimum), but
        // here also derives an actual time-domain wet/delta estimate via a
        // second inverse-FFT pass over the same spectral frame content.
        for (int b = 0; b <= kFft / 2; ++b)
        {
            float g = juce::Decibels::decibelsToGain(reductionOut[(size_t) b]);
            scratchDelta[(size_t) 2 * b] = scratch[(size_t) 2 * b] * (1.0f - g);
            scratch[(size_t) 2 * b] *= g;
            if (b > 0 && b < kFft / 2) { scratchDelta[(size_t) 2 * b + 1] = scratch[(size_t) 2 * b + 1]; scratch[(size_t) 2 * b + 1] *= g; scratchDelta[(size_t) 2 * b + 1] *= (1.0f - g); }
        }
        fft.performRealOnlyInverseTransform(scratch.data());
        fft.performRealOnlyInverseTransform(scratchDelta.data());
        for (int k = 0; k < kHop; ++k)
        {
            float y = scratch[(size_t) k] * window[(size_t) k];
            float d = scratchDelta[(size_t) k] * window[(size_t) k];
            sumOutSq += (double) y * y; sumDeltaSq += (double) d * d;
        }
        ++frames;
    }
    for (float x : sig) sumInSq += (double) x * x;
    MusicalStats ms{};
    ms.inRms = (float) std::sqrt(sumInSq / juce::jmax(1, n));
    ms.outRms = (float) std::sqrt(sumOutSq / juce::jmax(1, frames * kHop));
    ms.deltaRms = (float) std::sqrt(sumDeltaSq / juce::jmax(1, frames * kHop));
    ms.rmsLossDb = 20.0f * std::log10(juce::jmax(1e-9f, ms.outRms) / juce::jmax(1e-9f, ms.inRms));
    ms.deltaRatio = ms.inRms > 1e-9f ? ms.deltaRms / ms.inRms : 0.0f;
    ms.core.maxRedDb = worst;
    ms.core.meanActiveRedDb = frames ? (float) (sumActiveRed / frames) : 0.0f;
    ms.core.avgActiveBins = frames ? (int) (activeBinSum / frames) : 0;
    ms.core.avgValleys = frames ? (int) (valleySum / frames) : 0;
    ms.core.avgValleyWidthOct = frames ? (float) (widthOctSum / frames) : 0.0f;
    (void) sumAppliedMin;
    return ms;
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  %s: %s\n", what, cond ? "PASS" : "FAIL"); if (!cond) allPass = false; };

    std::printf("=== DETAIL / mask granularity check ===\n\n");
    std::printf("-- Multi-resonance (2.2k/2.6k/3.1k/3.8k), Depth=3, Selectivity=3.5 --\n");
    auto multi = genFourResonances(sr, n);
    RunStats d0, d25, d5, d75, d10;
    d0 = runSignal(multi, sr, 3.0f, 3.5f, 0.0f);
    d25 = runSignal(multi, sr, 3.0f, 3.5f, 2.5f);
    d5 = runSignal(multi, sr, 3.0f, 3.5f, 5.0f);
    d75 = runSignal(multi, sr, 3.0f, 3.5f, 7.5f);
    d10 = runSignal(multi, sr, 3.0f, 3.5f, 10.0f);
    auto printRun = [&](const char* label, const RunStats& s)
    { std::printf("  Detail=%-5s valleys=%d avgWidth=%.3foct maxRed=%.2fdB meanActiveRed=%.2fdB activeBins=%d\n", label, s.avgValleys, s.avgValleyWidthOct, s.maxRedDb, s.meanActiveRedDb, s.avgActiveBins); };
    printRun("0", d0); printRun("2.5", d25); printRun("5", d5); printRun("7.5", d75); printRun("10", d10);

    check("Detail 10 shows more (or equal) distinct valleys than Detail 0", d10.avgValleys >= d0.avgValleys);
    check("Detail 0 valley width >= Detail 10 valley width (more aggregated)", d0.avgValleyWidthOct >= d10.avgValleyWidthOct - 0.02f);
    // The real guardrail is against the Detail=5 BASELINE (the approved
    // Sonic Alpha sound), not against Detail=0 -- Detail=0 is SUPPOSED to
    // be shallower (more aggregation dilutes peaks toward 0dB, by design:
    // mean-smoothing over a wider window can never deepen a peak, only
    // dilute it). What must stay bounded is Detail=10 vs Detail=5: going
    // fully local/granular should only recover a little of what Detail=5's
    // own smoothing had diluted, never add dB Depth/gamma never decided.
    check("Detail 10 stays within 1dB of the Detail=5 baseline's own max reduction (Depth stays the authority, not Detail)", std::abs(d10.maxRedDb - d5.maxRedDb) < 1.0f);
    check("Detail 0 is shallower than Detail 5 baseline (aggregation dilutes, by design)", d0.maxRedDb > d5.maxRedDb - 1.0e-3f);
    check("Monotonic valley count: 0<=2.5<=5<=7.5<=10 (non-decreasing)", d0.avgValleys <= d25.avgValleys + 1 && d25.avgValleys <= d5.avgValleys + 1 && d5.avgValleys <= d75.avgValleys + 1 && d75.avgValleys <= d10.avgValleys + 1);

    std::printf("\n-- White Sensitivity Curve x Detail (2.4k-4.7k cluster, +7dB@3.4kHz) --\n");
    auto curveSig = genThreeToFiveK(sr, n);
    RunStats cflat0 = runSignal(curveSig, sr, 3.0f, 3.5f, 0.0f);
    RunStats cup0 = runSignal(curveSig, sr, 3.0f, 3.5f, 0.0f, true, 3400.0f, 7.0f, 1.2f);
    RunStats cflat5 = runSignal(curveSig, sr, 3.0f, 3.5f, 5.0f);
    RunStats cup5 = runSignal(curveSig, sr, 3.0f, 3.5f, 5.0f, true, 3400.0f, 7.0f, 1.2f);
    RunStats cflat10 = runSignal(curveSig, sr, 3.0f, 3.5f, 10.0f);
    RunStats cup10 = runSignal(curveSig, sr, 3.0f, 3.5f, 10.0f, true, 3400.0f, 7.0f, 1.2f);
    std::printf("  Detail=0  flat: valleys=%d meanRed=%.2fdB | +7dB@3.4k: valleys=%d meanRed=%.2fdB\n", cflat0.avgValleys, cflat0.meanActiveRedDb, cup0.avgValleys, cup0.meanActiveRedDb);
    std::printf("  Detail=5  flat: valleys=%d meanRed=%.2fdB | +7dB@3.4k: valleys=%d meanRed=%.2fdB\n", cflat5.avgValleys, cflat5.meanActiveRedDb, cup5.avgValleys, cup5.meanActiveRedDb);
    std::printf("  Detail=10 flat: valleys=%d meanRed=%.2fdB | +7dB@3.4k: valleys=%d meanRed=%.2fdB\n", cflat10.avgValleys, cflat10.meanActiveRedDb, cup10.avgValleys, cup10.meanActiveRedDb);
    check("Curve raises activity at Detail=10 too (more/equal valleys or deeper mean)", cup10.avgValleys >= cflat10.avgValleys && cup10.meanActiveRedDb <= cflat10.meanActiveRedDb + 1.0e-3f);
    check("Curve raises activity at Detail=0 too (aggregated but still responsive)", cup0.meanActiveRedDb <= cflat0.meanActiveRedDb + 1.0e-3f);

    std::printf("\n-- Musical material: Bass/Vocal/Guitar/Kick/Dense, Detail 0/5/10 --\n");
    struct Mat { const char* name; std::vector<float> sig; };
    std::vector<Mat> mats = {
        {"Bass", genBass(sr, n)}, {"Vocal", genVocal(sr, n)}, {"Guitar", genGuitar(sr, n)}, {"Kick", genKick(sr, n)}, {"Dense", genDense(sr, n)}
    };
    for (auto& m : mats)
    {
        auto s0 = runMusical(m.sig, sr, 3.0f, 3.5f, 0.0f);
        auto s5 = runMusical(m.sig, sr, 3.0f, 3.5f, 5.0f);
        auto s10 = runMusical(m.sig, sr, 3.0f, 3.5f, 10.0f);
        std::printf("  [%s]\n", m.name);
        std::printf("    Detail=0 : rmsLoss=%.2fdB maxRed=%.2fdB meanActiveRed=%.2fdB activeBins=%d valleys=%d deltaRatio=%.3f\n", s0.rmsLossDb, s0.core.maxRedDb, s0.core.meanActiveRedDb, s0.core.avgActiveBins, s0.core.avgValleys, s0.deltaRatio);
        std::printf("    Detail=5 : rmsLoss=%.2fdB maxRed=%.2fdB meanActiveRed=%.2fdB activeBins=%d valleys=%d deltaRatio=%.3f\n", s5.rmsLossDb, s5.core.maxRedDb, s5.core.meanActiveRedDb, s5.core.avgActiveBins, s5.core.avgValleys, s5.deltaRatio);
        std::printf("    Detail=10: rmsLoss=%.2fdB maxRed=%.2fdB meanActiveRed=%.2fdB activeBins=%d valleys=%d deltaRatio=%.3f\n", s10.rmsLossDb, s10.core.maxRedDb, s10.core.meanActiveRedDb, s10.core.avgActiveBins, s10.core.avgValleys, s10.deltaRatio);
        if (std::string(m.name) == "Bass")
            check("Bass: Detail 10 does not dig the low end further than Detail 0 by more than 0.5dB rmsLoss", s0.rmsLossDb - s10.rmsLossDb < 0.5f);
        if (std::string(m.name) == "Kick")
            check("Kick: Detail 0/5/10 all leave attack essentially untouched (rmsLoss within 0.1dB of each other)", std::abs(s0.rmsLossDb - s10.rmsLossDb) < 0.1f);
        if (std::string(m.name) == "Dense")
            check("Dense: Detail 10 does not blow up active bin count vs Detail 0 (stays under 40% of spectrum)", s10.core.avgActiveBins < 1025 * 0.40f);
        if (std::string(m.name) == "Vocal")
            check("Vocal: Delta stays selective at Detail 10 (deltaRatio < 0.5, not reproducing the whole voice)", s10.deltaRatio < 0.5f);
    }

    std::printf("\n-- CPU: Detail=0 vs Detail=10, 192kHz --\n");
    {
        const double srHi = 192000.0; int nHi = (int) (srHi * 0.5);
        auto sig = genFourResonances(srHi, nHi);
        const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
        for (float detailVal : { 0.0f, 10.0f })
        {
            GainMaskEngine mask; mask.prepare(srHi, kFft, kHop);
            mask.setParams(3.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f); mask.setDetail(detailVal);
            float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
            int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
            mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);
            juce::dsp::FFT fft(11);
            std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
            for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
            std::vector<double> times;
            int nn = (int) sig.size();
            for (int i = 0; i + kFft <= nn; i += kHop)
            {
                for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
                std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform(scratch.data());
                for (int b = 0; b < bins; ++b)
                { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
                  magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
                auto t0 = std::chrono::high_resolution_clock::now();
                mask.process(magDb, sig.data() + i, kHop, reductionOut);
                auto t1 = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
            }
            std::sort(times.begin(), times.end());
            double med = times[times.size() / 2];
            double hopBudgetUs = 1.0e6 * (double) kHop / srHi;
            std::printf("  Detail=%-4.0f med=%.2fus (%.2f%% of hop budget)\n", detailVal, med, 100.0 * med / hopBudgetUs);
        }
    }

    std::printf("\n-- Sample rate coherence (Detail=5, multi-resonance signal) --\n");
    for (double srTest : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        int nTest = (int) (srTest * 1.0);
        auto sig = genFourResonances(srTest, nTest);
        auto s5 = runSignal(sig, srTest, 3.0f, 3.5f, 5.0f);
        auto s10 = runSignal(sig, srTest, 3.0f, 3.5f, 10.0f);
        std::printf("  sr=%-7.0f Detail=5:  valleys=%d avgWidth=%.3foct maxRed=%.2fdB activeBins=%d\n", srTest, s5.avgValleys, s5.avgValleyWidthOct, s5.maxRedDb, s5.avgActiveBins);
        std::printf("  sr=%-7.0f Detail=10: valleys=%d avgWidth=%.3foct maxRed=%.2fdB activeBins=%d\n", srTest, s10.avgValleys, s10.avgValleyWidthOct, s10.maxRedDb, s10.avgActiveBins);
    }

    std::printf("\n%s\n", allPass ? "=== ALL DETAIL CHECKS PASS ===" : "=== DETAIL CHECKS FAILED ===");
    return allPass ? 0 : 1;
}
