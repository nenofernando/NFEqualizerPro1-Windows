// MAX REDUCTION validation (Sonic Alpha V2). Drives GainMaskEngine/
// SpectralEngine directly. No PHYSICAL C/D/ConfidenceEngine/
// TransientProtectionEngine/gamma/Detail/Sensitivity-Curve/detector file
// touched by this feature -- Max Reduction is a pure post-Detail,
// pre-temporal-smoothing (plus a defensive post-smoothing) clamp.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/SpectralEngine.h"
#include "DSP/ResonanceDetector.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{ double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (int i = 0; i < (int) b.size(); ++i) { b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{ auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
// Strong, dense multi-resonance material -- deliberately asks for MORE
// reduction than any tested cap, at Depth=9 (near max), so the cap is
// actually the binding constraint, not an incidental non-event.
static std::vector<float> genStrongCluster(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 220.0, 0.16f, 6, 2.5f);
    const double partials[] = { 900, 1300, 1700, 2100, 2500, 3300, 3900, 4600 };
    for (double f : partials) addTone(b, sr, f, 0.20f);
    return b;
}

struct MaskResult { std::vector<float> snapshot; };
static MaskResult runGainMask(const std::vector<float>& sig, double sr, float depth, float detail, bool maxRedOn, float maxRedDb,
                               bool curveActive = false, float curveFreq = 0, float curveSensDb = 0)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(depth, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f);
    mask.setDetail(detail);
    mask.setMaxReduction(maxRedOn, maxRedDb);
    float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
    int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
    if (curveActive) { bf[0] = curveFreq; bs[0] = curveSensDb; bw[0] = 1.2f; bsh[0] = 0; bfoc[0] = 0.5f; ba[0] = true; }
    mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);

    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    MaskResult r;
    for (int i = 0; i + kFft <= (int) sig.size(); i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
          magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
        mask.process(magDb, sig.data() + i, kHop, reductionOut);
        r.snapshot = reductionOut;
    }
    return r;
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  %s: %s\n", what, cond ? "PASS" : "FAIL"); if (!cond) allPass = false; };
    const float tol = 0.05f; // floating-point tolerance only, per spec

    std::printf("=== MAX REDUCTION validation ===\n\n");

    // ---- 7. OFF / 1 / 2 / 3 / 6 dB, Depth=9 (asks for more than any cap) ----
    std::printf("-- Depth=9 (strong demand), Max Reduction OFF / 1 / 2 / 3 / 6 dB --\n");
    auto sig = genStrongCluster(sr, n);
    auto rOff = runGainMask(sig, sr, 9.0f, 5.0f, false, 3.0f);
    float minOff = *std::min_element(rOff.snapshot.begin(), rOff.snapshot.end());
    std::printf("  OFF   : min=%.3fdB (unclamped demand -- confirms Depth=9 genuinely asks for more than the caps below)\n", minOff);
    check("OFF genuinely exceeds 3dB (test signal is a real stress case)", minOff < -3.0f - 0.5f);
    for (float cap : { 1.0f, 2.0f, 3.0f, 6.0f })
    {
        auto r = runGainMask(sig, sr, 9.0f, 5.0f, true, cap);
        float minv = *std::min_element(r.snapshot.begin(), r.snapshot.end());
        std::printf("  cap=%.1f: min=%.3fdB\n", cap, minv);
        check((juce::String("no bin exceeds -") + juce::String(cap,1) + "dB (within float tolerance)").toRawUTF8(), minv >= -cap - tol);
    }

    // ---- 8. Detail 0/5/10 with Max Reduction=3dB ----
    std::printf("\n-- Detail 0/5/10, Max Reduction=3dB (cap must hold, Detail still changes granularity) --\n");
    std::vector<float> detailMins;
    for (float d : { 0.0f, 5.0f, 10.0f })
    {
        auto r = runGainMask(sig, sr, 9.0f, d, true, 3.0f);
        float minv = *std::min_element(r.snapshot.begin(), r.snapshot.end());
        int activeBins = 0; for (float v : r.snapshot) if (v < -0.05f) ++activeBins;
        std::printf("  Detail=%-4.0f min=%.3fdB activeBins=%d\n", d, minv, activeBins);
        check((juce::String("Detail=") + juce::String((int) d) + " respects the 3dB cap").toRawUTF8(), minv >= -3.0f - tol);
        detailMins.push_back((float) activeBins);
    }
    check("Detail still changes granularity under the cap (active-bin counts differ)", detailMins[0] != detailMins[2]);

    // ---- 9. White Sensitivity Curve raised, Max Reduction=3dB ----
    std::printf("\n-- White Sensitivity Curve +8dB@2.5kHz, Max Reduction=3dB --\n");
    {
        auto rFlat = runGainMask(sig, sr, 9.0f, 5.0f, true, 3.0f);
        auto rCurve = runGainMask(sig, sr, 9.0f, 5.0f, true, 3.0f, true, 2500.0f, 8.0f);
        float minFlat = *std::min_element(rFlat.snapshot.begin(), rFlat.snapshot.end());
        float minCurve = *std::min_element(rCurve.snapshot.begin(), rCurve.snapshot.end());
        std::printf("  flat curve : min=%.3fdB\n  +8dB@2.5k  : min=%.3fdB\n", minFlat, minCurve);
        check("curve can push activity closer to the cap", minCurve <= minFlat + 1.0e-4f);
        check("curve never pushes past the 3dB cap", minCurve >= -3.0f - tol);
    }

    // ---- 10. Sidechain (Internal vs Sidechain), Max Reduction=3dB, via SpectralEngine ----
    std::printf("\n-- Detector Source Internal vs Sidechain, Max Reduction=3dB (via SpectralEngine, real MAIN output path) --\n");
    {
        const int kFft = 2048, kHop = 512, ch = 2;
        auto runSE = [&](int detectorSource, const juce::AudioBuffer<float>* sc) -> float
        {
            juce::AudioBuffer<float> main(ch, n); main.clear();
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i)
            {
                float x = 0.0f;
                for (double f : { 220.0, 900.0, 1300.0, 1700.0, 2100.0, 2500.0, 3300.0, 3900.0, 4600.0 }) x += 0.15f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * f * i / sr);
                main.setSample(c, i, x);
            }
            SpectralEngine eng; eng.prepare(sr, ch);
            SpectralEngine::Params p; p.depth = 9.0f; p.selectivity = 3.5f; p.attackMs = 10.0f; p.releaseMs = 80.0f;
            p.detectorSource = detectorSource; p.maxReductionEnabled = true; p.maxReductionDb = 3.0f;
            eng.setParams(p); eng.process(main, sc);
            auto red = eng.getLastReduction();
            return red.empty() ? 0.0f : *std::min_element(red.begin(), red.end());
        };
        juce::AudioBuffer<float> scBuf(ch, n); scBuf.clear();
        for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) scBuf.setSample(c, i, 0.22f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * 2500.0 * i / sr));
        float minInternal = runSE(0, nullptr);
        float minSidechain = runSE(1, &scBuf);
        std::printf("  Internal : min=%.3fdB\n  Sidechain: min=%.3fdB\n", minInternal, minSidechain);
        check("Internal respects the 3dB cap on MAIN output", minInternal >= -3.0f - tol);
        check("Sidechain respects the SAME 3dB cap on MAIN output", minSidechain >= -3.0f - tol);
    }

    // ---- Cross-SR: cap holds identically at 44.1/48/96/192kHz ----
    std::printf("\n-- Cross-SR: Max Reduction=3dB holds at 44.1/48/96/192kHz --\n");
    for (double srTest : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        int nTest = (int) (srTest * 1.0);
        auto sigSr = genStrongCluster(srTest, nTest);
        auto r = runGainMask(sigSr, srTest, 9.0f, 5.0f, true, 3.0f);
        float minv = r.snapshot.empty() ? 0.0f : *std::min_element(r.snapshot.begin(), r.snapshot.end());
        bool finite = true; for (float v : r.snapshot) if (! std::isfinite(v)) finite = false;
        std::printf("  sr=%-7.0f min=%.3fdB finite=%s\n", srTest, minv, finite ? "yes" : "NO(!!)");
        check("no NaN/Inf", finite);
        check("cap holds", minv >= -3.0f - tol);
    }

    std::printf("\n%s\n", allPass ? "=== ALL MAX REDUCTION CHECKS PASS ===" : "=== MAX REDUCTION CHECKS FAILED ===");
    return allPass ? 0 : 1;
}
