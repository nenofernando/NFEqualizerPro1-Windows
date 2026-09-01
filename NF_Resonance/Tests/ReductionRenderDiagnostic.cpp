// DIAGNOSTIC ONLY (Sonic Alpha V2) -- compares GainMaskEngine's real
// appliedReductionSnapshot() against the curve SpectrumComponent::paint()
// actually draws, stage by stage, to find out whether the renderer is
// hiding real detail the mask already produced, or whether the mask
// itself (Depth/Detail/gamma/ConfidenceEngine) is the limiting factor.
// Does NOT modify GainMaskEngine or SpectrumComponent -- this file only
// reads their public APIs and, where SpectrumComponent's own pipeline
// stages are not separately exposed as testable functions (the box-
// average / gating / valley-splitting stages after resampleReductionFor-
// Display), duplicates that exact logic read directly from
// Source/UI/SpectrumComponent.cpp's paint() for this diagnostic's own
// use, so the numbers reported here are what the UI would actually show.

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
static void addNoiseBurst(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp;
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{ auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }

// A denser, more "musical" multi-resonance signal than a bare tone cluster
// -- several inharmonic problem partials layered on a harmonic bed plus a
// touch of broadband noise, closer to what a real LUNA session would
// present to the detector than an idealized clean tone stack.
static std::vector<float> genDenseCluster(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 220.0, 0.14f, 6, 2.5f);
    const double partials[] = { 1150, 1480, 1820, 2260, 2650, 3080, 3540, 4020, 4550, 5100 };
    for (double f : partials) addTone(b, sr, f, 0.15f);
    addNoiseBurst(b, (int) (sr * 0.2), (int) (sr * 0.05), 0.06f, 7);
    return b;
}

// ---- Exact duplicate of SpectrumComponent::paint()'s post-resample
// pipeline (box-average -> gate -> coarse spans -> valley splitting ->
// taper), read directly from Source/UI/SpectrumComponent.cpp. Kept in sync
// by hand for this diagnostic; NOT used by the real plugin. ----
struct RenderedProfile { std::vector<float> envAt; int numPts; };
static RenderedProfile runFullRenderPipeline(const std::vector<float>& snapshot, double sr, int fftSize, int numPts)
{
    std::vector<float> rawRedAt;
    SpectrumComponent::resampleReductionForDisplay(snapshot, sr, fftSize, numPts, rawRedAt);

    const float octavesPerPoint = (std::log10(20000.0f) - std::log10(20.0f)) / std::log10(2.0f) / (float) (numPts - 1);
    auto oct2pts = [&](float oct) { return juce::jmax(1, (int) std::round(oct / octavesPerPoint)); };
    const int smoothRadius = juce::jmax(1, oct2pts(0.01f));
    std::vector<float> smAt((size_t) numPts, 0.0f);
    for (int k = 0; k < numPts; ++k)
    {
        double sum = 0.0; int cnt = 0;
        for (int j = juce::jmax(0, k - smoothRadius); j <= juce::jmin(numPts - 1, k + smoothRadius); ++j) { sum += rawRedAt[(size_t) j]; ++cnt; }
        smAt[(size_t) k] = (float) (sum / cnt);
    }

    const float gateThresholdDb = 0.15f;
    const int maxGapPoints = oct2pts(0.06f);
    const int taperPoints = oct2pts(0.02f);
    const int minPeakSepPoints = oct2pts(0.08f);
    const float splitRecoverFrac = 0.55f;
    std::vector<bool> active((size_t) numPts);
    for (int k = 0; k < numPts; ++k) active[(size_t) k] = std::abs(smAt[(size_t) k]) > gateThresholdDb;

    struct Span { int start, end; };
    std::vector<Span> coarse;
    for (int k = 0; k < numPts; )
    {
        if (! active[(size_t) k]) { ++k; continue; }
        int start = k, end = k;
        while (true)
        {
            int next = end + 1;
            while (next < numPts && ! active[(size_t) next] && (next - end) <= maxGapPoints) ++next;
            if (next < numPts && active[(size_t) next] && (next - end) <= maxGapPoints) end = next;
            else break;
        }
        coarse.push_back({ start, end });
        k = end + 1;
    }

    std::vector<Span> regions;
    for (auto& c : coarse)
    {
        std::vector<int> peaks;
        for (int k = c.start; k <= c.end; ++k)
        {
            float v = std::abs(smAt[(size_t) k]);
            if (v <= gateThresholdDb) continue;
            bool isPeak = (k == c.start || v >= std::abs(smAt[(size_t) (k - 1)])) && (k == c.end || v >= std::abs(smAt[(size_t) (k + 1)]));
            if (isPeak) peaks.push_back(k);
        }
        std::vector<int> kept;
        for (int p : peaks)
        {
            if (kept.empty() || (p - kept.back()) >= minPeakSepPoints) kept.push_back(p);
            else if (std::abs(smAt[(size_t) p]) > std::abs(smAt[(size_t) kept.back()])) kept.back() = p;
        }
        if (kept.size() <= 1) { regions.push_back(c); continue; }
        std::vector<int> splitPoints;
        for (size_t i = 0; i + 1 < kept.size(); ++i)
        {
            int a = kept[i], b = kept[i + 1];
            int valleyIdx = a; float valleyMag = std::abs(smAt[(size_t) a]);
            for (int k = a; k <= b; ++k) { float v = std::abs(smAt[(size_t) k]); if (v < valleyMag) { valleyMag = v; valleyIdx = k; } }
            float smallerPeak = juce::jmin(std::abs(smAt[(size_t) a]), std::abs(smAt[(size_t) b]));
            if (valleyMag <= gateThresholdDb || valleyMag <= splitRecoverFrac * smallerPeak) splitPoints.push_back(valleyIdx);
        }
        int segStart = c.start;
        for (int sp : splitPoints) { regions.push_back({ segStart, sp }); segStart = sp + 1; }
        regions.push_back({ segStart, c.end });
    }

    std::vector<float> envAt((size_t) numPts, 0.0f);
    for (auto& r : regions)
    {
        int len = r.end - r.start;
        int taper = juce::jmin(taperPoints, juce::jmax(1, len / 2));
        for (int k = r.start; k <= r.end; ++k)
        {
            float w = 1.0f;
            int distStart = k - r.start, distEnd = r.end - k;
            if (distStart < taper) { float u = (float) distStart / (float) taper; w = juce::jmin(w, u * u * (3.0f - 2.0f * u)); }
            if (distEnd < taper) { float u = (float) distEnd / (float) taper; w = juce::jmin(w, u * u * (3.0f - 2.0f * u)); }
            envAt[(size_t) k] = smAt[(size_t) k] * w;
        }
    }
    return { envAt, numPts };
}

// Strict local-minima count: index i is a real local minimum if it is more
// negative than BOTH neighbours (or tied with one side on a plateau's
// edge), magnitude above a tiny noise floor -- this is what the user means
// by "minimos locais reais", distinct from the coarser "valley" (gated
// contiguous-run) metric used elsewhere in this session's tests.
static int countStrictLocalMinima(const std::vector<float>& v, float noiseFloorDb)
{
    int n = (int) v.size(), count = 0;
    for (int i = 0; i < n; ++i)
    {
        if (std::abs(v[i]) <= noiseFloorDb) continue;
        bool leOnLeft = (i == 0) || v[i] <= v[i - 1];
        bool leOnRight = (i == n - 1) || v[i] <= v[i + 1];
        bool ltSomewhere = (i > 0 && v[i] < v[i - 1]) || (i < n - 1 && v[i] < v[i + 1]);
        if (leOnLeft && leOnRight && ltSomewhere) ++count;
    }
    return count;
}

struct ValleyStats { int count; float deepestDb; float meanWidthOct; float meanDepthDb; };
static ValleyStats countValleysWidthDepth(const std::vector<float>& v, double sr, int fftSizeOrNumPts, bool isBinArray, float gateDb)
{
    ValleyStats s{ 0, 0.0f, 0.0f, 0.0f };
    int n = (int) v.size(), i = 0;
    double widthSum = 0, depthSum = 0; int cnt = 0;
    while (i < n)
    {
        if (std::abs(v[i]) > gateDb)
        {
            int start = i; float deepest = v[i]; double dsum = 0; int dcount = 0;
            while (i < n && std::abs(v[i]) > gateDb) { deepest = juce::jmin(deepest, v[i]); dsum += v[i]; ++dcount; ++i; }
            int end = i - 1;
            float hzLo, hzHi;
            if (isBinArray) { hzLo = juce::jmax(1.0f, (float) (start * sr / fftSizeOrNumPts)); hzHi = juce::jmax(hzLo + 1.0f, (float) (end * sr / fftSizeOrNumPts)); }
            else { double lo20 = std::log10(20.0), range = std::log10(20000.0) - lo20; double tLo = (double) start / (double) (fftSizeOrNumPts - 1), tHi = (double) end / (double) (fftSizeOrNumPts - 1);
                   hzLo = (float) std::pow(10.0, lo20 + tLo * range); hzHi = (float) std::pow(10.0, lo20 + tHi * range); hzHi = juce::jmax(hzHi, hzLo + 1.0f); }
            widthSum += std::log2(hzHi / hzLo); depthSum += dsum / dcount; ++cnt;
            s.deepestDb = juce::jmin(s.deepestDb, deepest);
        }
        else ++i;
    }
    s.count = cnt;
    s.meanWidthOct = cnt ? (float) (widthSum / cnt) : 0.0f;
    s.meanDepthDb = cnt ? (float) (depthSum / cnt) : 0.0f;
    return s;
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    const int numPts = 165; // matches the analyzer's own realistic plot width (~660px/4)

    std::printf("=== DIAGNOSTIC: appliedReductionSnapshot vs rendered UI curve ===\n");
    std::printf("(read-only -- no DSP or renderer file modified by this pass)\n\n");

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
        auto sig = genDenseCluster(sr, n);
        std::vector<float> snapshot;
        for (int i = 0; i + kFft <= (int) sig.size(); i += kHop)
        {
            for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            for (int b = 0; b < bins; ++b)
            { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
              magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
            mask.process(magDb, sig.data() + i, kHop, reductionOut);
            snapshot = reductionOut; // steady-state snapshot == appliedReductionSnapshot()'s own content at this point
        }

        int strictMinimaSnapshot = countStrictLocalMinima(snapshot, 0.02f);
        auto valleysSnapshot = countValleysWidthDepth(snapshot, sr, kFft, true, 0.15f);

        auto rendered = runFullRenderPipeline(snapshot, sr, kFft, numPts);
        int strictMinimaRendered = countStrictLocalMinima(rendered.envAt, 0.02f);
        auto valleysRendered = countValleysWidthDepth(rendered.envAt, sr, numPts, false, 0.15f);

        // Also isolate JUST the resample step (before box-average/gating/
        // splitting) to see how much detail survives resampling alone vs
        // how much the LATER stages (box-average, gate, valley-splitting)
        // additionally remove.
        std::vector<float> resampledOnly;
        SpectrumComponent::resampleReductionForDisplay(snapshot, sr, kFft, numPts, resampledOnly);
        auto valleysResampledOnly = countValleysWidthDepth(resampledOnly, sr, numPts, false, 0.15f);

        std::printf("########## Detail=%.0f ##########\n", detailVal);
        std::printf("  SNAPSHOT (real, %d bins):\n", (int) snapshot.size());
        std::printf("    strict local minima = %d\n", strictMinimaSnapshot);
        std::printf("    gated valleys = %d | meanWidth=%.3foct | meanDepth=%.2fdB | maxDepth=%.2fdB\n",
            valleysSnapshot.count, valleysSnapshot.meanWidthOct, valleysSnapshot.meanDepthDb, valleysSnapshot.deepestDb);
        std::printf("  AFTER resample only (min-preserving, %d pts):\n", numPts);
        std::printf("    gated valleys = %d | meanWidth=%.3foct | meanDepth=%.2fdB | maxDepth=%.2fdB\n",
            valleysResampledOnly.count, valleysResampledOnly.meanWidthOct, valleysResampledOnly.meanDepthDb, valleysResampledOnly.deepestDb);
        std::printf("  AFTER full render pipeline (box-avg + gate + split + taper):\n");
        std::printf("    strict local minima = %d\n", strictMinimaRendered);
        std::printf("    gated valleys (= what's actually visible) = %d | meanWidth=%.3foct | meanDepth=%.2fdB | maxDepth=%.2fdB\n",
            valleysRendered.count, valleysRendered.meanWidthOct, valleysRendered.meanDepthDb, valleysRendered.deepestDb);
        int lostByResample = valleysSnapshot.count - valleysResampledOnly.count;
        int lostByLaterStages = valleysResampledOnly.count - valleysRendered.count;
        std::printf("  DIAGNOSIS: valleys lost at resample stage = %d | valleys lost at box-avg/gate/split stage = %d\n\n", lostByResample, lostByLaterStages);
    }

    std::printf("No file was modified by this diagnostic. Report the numbers above before deciding whether/where to fix the renderer.\n");
    return 0;
}
