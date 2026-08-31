#pragma once
#include <JuceHeader.h>

// V2-A: perceptually-aware spectral prominence estimation.
//
// Operates at the FULL FFT bin resolution (no downsampled log grid for the
// final mask) -- log-frequency awareness comes from expressing each scale's
// neighborhood width in octaves and converting that to a per-bin index range
// once (in prepare()), not from resampling the spectrum itself. Conversion
// pipeline per bin, matching the requested "desiredLowHz/HighHz -> bin
// range -> minimum-bin floor" order (never mixing octave units with raw bin
// counts): centerHz -> {desiredLowHz, desiredHighHz} via the octave half-
// width -> {binLow, binHigh} via round(hz*fftSize/sampleRate) -> radius
// floored to a minimum physically meaningful width. All of this is
// precomputed once per prepare()/sample-rate change; the per-frame path does
// no log/pow calls, only array lookups and window-scan arithmetic.
class SpectralProminenceEngine
{
public:
    enum class BaselineMethod { Median, TrimmedMean, WeightedMean, Percentile, RobustLocalRegression };

    void prepare(int numBins, double sampleRate, int fftSize);
    void setBaselineMethod(BaselineMethod m) { method = m; }
    BaselineMethod baselineMethod() const { return method; }

    // Blends NARROW/MEDIUM/BROAD prominence estimates by `sharpness` (0..10)
    // into `prominenceOut` (dB), one value per bin. No allocation (uses
    // pre-sized scratch buffers from prepare()).
    void computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    struct ScaleInfo { int radiusNarrow=0, radiusMedium=0, radiusBroad=0; bool narrowResolutionLimited=false, mediumResolutionLimited=false, broadResolutionLimited=false; };
    ScaleInfo scaleInfoAt(int bin) const;

    // Octave widths (FULL width, i.e. center*2^(-w/2)..center*2^(+w/2)).
    // Initial research candidates per the approved V2 architecture proposal --
    // not fixed; re-tunable via setScaleWidths() for the V2-A benchmark sweep.
    void setScaleWidthsOctaves(double narrowOct, double mediumOct, double broadOct);

    // Fraction of each scale's radius excluded (centered on the bin) from the
    // baseline computation, so a real resonance's own energy doesn't bias its
    // own reference upward -- the exact contamination the baseline benchmark
    // exposed. 0 = no exclusion (original behavior); e.g. 0.3 excludes the
    // innermost 30% of the radius on each side.
    void setGapFraction(double frac) { gapFraction = juce::jlimit(0.0, 0.9, frac); }

    // Which percentile BaselineMethod::Percentile uses (0..1, e.g. 0.25 = P25).
    void setPercentile(double p) { percentile = juce::jlimit(0.0, 1.0, p); }
    double getPercentile() const { return percentile; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int bins = 1025;
    double narrowOctaves = 1.0/12.0, mediumOctaves = 1.0/3.0, broadOctaves = 1.0;
    static constexpr int minRadiusBins = 1; // smallest physically meaningful neighborhood (i-1..i+1)
    BaselineMethod method = BaselineMethod::Median; // overridden via setBaselineMethod()
    double gapFraction = 0.0;
    double percentile = 0.25;

    std::vector<int> radiusNarrow, radiusMedium, radiusBroad;
    std::vector<uint8_t> narrowLimited, mediumLimited, broadLimited;

    // Scratch buffers, sized once in prepare() -- no per-call allocation.
    std::vector<float> promNarrow, promMedium, promBroad;
    std::vector<float> window; // reused sort/weight scratch, sized to widest possible radius*2+1

    void recomputeRadii();
    static float baselineAt(const std::vector<float>& magDb, int bin, int radius, BaselineMethod m, double gapFraction, double percentile, std::vector<float>& scratch);
};
