#pragma once
#include <JuceHeader.h>

// V2-A2: hybrid candidate-based prominence engine.
//
// The FINAL mask always stays at the full FFT bin resolution (1025 bins for
// a 2048-point FFT) -- this class never interpolates the final answer from a
// coarse grid. Cost reduction instead comes from a three-stage pipeline:
//
//   1. CHEAP PASS: an unweighted windowed mean (via per-frame prefix sums,
//      O(1) per query regardless of radius) evaluated only at a coarse
//      auxiliary log-frequency grid (configurable points/octave).
//   2. INTERPOLATE + SOFT-GATE: that cheap per-grid-point prominence is
//      linearly interpolated (in log-frequency) onto every real FFT bin,
//      giving a smooth "candidate score" per bin -- never a hard per-bin
//      decision. A soft margin (sigmoid) turns that into a candidateWeight
//      in [0,1], generous enough that weak/borderline candidates are still
//      partially refined rather than silently dropped.
//   3. REFINE: only bins with candidateWeight above a small epsilon get the
//      expensive, robust (percentile) computation -- evaluated at THAT BIN'S
//      OWN full resolution (not the coarse grid), using the same core/context
//      geometry as the reference (V2-A) engine. Final value blends cheap and
//      refined by candidateWeight, so there is no discontinuity at the
//      candidate/non-candidate boundary.
//
// CORE vs CONTEXT (replaces the old single "radius" per scale): each scale
// has a small CORE width (the region that might BE the resonance, excluded
// from its own baseline) and an independently-sized CONTEXT width (how far
// out to look for reference spectrum), with an enforced minimum number of
// usable context bins per side -- a narrow scale's CORE can stay narrow while
// its CONTEXT is deliberately much wider, so it always has enough real
// neighborhood to form a reliable baseline.
//
// Everything geometry-related (bin<->Hz, grid<->bin interpolation weights,
// core/context radii, resolution-limited flags) is precomputed once in
// prepare() (called on sample-rate/FFT change). The per-frame path only
// builds one prefix-sum array (O(bins)) and touches pre-sized scratch
// buffers -- zero heap allocation after prepare().
class SpectralProminenceEngineV2
{
public:
    struct ScaleParams { double coreOctaves = 0.05, contextOctaves = 0.5; };

    void prepare(int numBins, double sampleRate, int fftSize, int pointsPerOctave = 48);
    void setScaleParams(ScaleParams narrow, ScaleParams medium, ScaleParams broad);
    void setMinContextBins(int n);
    void setPercentile(double p) { percentile = juce::jlimit(0.0, 1.0, p); }
    void setCandidateThresholdDb(double thresholdDb, double marginDb) { candidateThresholdDb = thresholdDb; candidateMarginDb = juce::jmax(0.1, marginDb); }

    // The cheap grid-point "signal" estimate used to be a single raw bin
    // value, which inherits 100% of normal per-bin spectral noise/variance --
    // random content alone (pink noise, real music) then falsely reads as
    // "candidate" almost everywhere, defeating the CPU savings. Smoothing the
    // SIGNAL side (not the baseline, and never the bins used in stage 3's
    // refinement, which always reads the original unsmoothed magDb) over a
    // small local window averages out that per-bin noise while a genuine
    // resonance -- narrow or not -- still shows elevated energy across its
    // own few neighboring bins, so real candidates keep triggering normally.
    enum class SignalSmoothMethod { None, MovingAvg3, MovingAvg5, Triangular3, Triangular5, Median3 };
    void setSignalSmoothMethod(SignalSmoothMethod m) { signalSmoothMethod = m; }

    // Full hybrid pipeline (stages 1-3 above). No allocation.
    void computeProminenceHybrid(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    // Cheap-only path at FULL bin resolution (no grid, no robust refinement
    // anywhere) -- the standalone "fast approximation" for direct comparison
    // against percentile-based accuracy.
    void computeProminenceFastApprox(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    // Reference (unoptimized) full-bin robust computation, for A/B accuracy
    // comparison against the hybrid pipeline -- same core/context geometry,
    // just evaluated at every bin unconditionally (this is what V2-A's
    // computeProminence() effectively costs).
    void computeProminenceFullBinReference(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    struct ScaleGeometryInfo { int coreRadius = 0, contextRadius = 0; bool coreResolutionLimited = false, contextResolutionLimited = false; };
    enum Scale { Narrow = 0, Medium = 1, Broad = 2 };
    ScaleGeometryInfo scaleGeometryAt(int bin, Scale s) const;

    int numCandidateBinsLastCall() const { return lastCandidateCount; }
    int gridPointCount() const { return (int) gridBin.size(); }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int bins = 1025;
    int pointsPerOctave = 48;
    int minContextBins = 4;
    double percentile = 0.25;
    double candidateThresholdDb = 1.0;
    double candidateMarginDb = 4.0;
    SignalSmoothMethod signalSmoothMethod = SignalSmoothMethod::None;

    ScaleParams narrowP, mediumP{ 0.12, 1.0 }, broadP{ 0.3, 2.5 };

    // Per-bin, per-scale core/context radii (bins), precomputed in prepare().
    std::vector<int> coreR[3], ctxR[3];
    std::vector<uint8_t> coreLimited[3], ctxLimited[3];

    // Auxiliary log-frequency grid: nearest real bin per grid point.
    std::vector<int> gridBin;
    // Per real bin: bracketing grid indices + interpolation fraction.
    std::vector<int> binGridLo, binGridHi;
    std::vector<float> binGridFrac;

    // Per-frame scratch (sized once in prepare()).
    std::vector<float> prefixSum;         // bins+1
    std::vector<float> gridCheapProm[3];  // per-scale cheap prominence at grid points
    std::vector<float> binCheapProm[3];   // interpolated onto real bins
    std::vector<float> binCandidateWeight;
    std::vector<float> robustScratch;     // for nth_element in the refinement stage
    int lastCandidateCount = 0;

    void recomputeGeometry();
    void computeCoreContextFor(double coreOct, double contextOct, std::vector<int>& coreOut, std::vector<int>& ctxOut, std::vector<uint8_t>& coreLimOut, std::vector<uint8_t>& ctxLimOut);
    float cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const; // O(1) via prefixSum
    float smoothedSignalAt(const std::vector<float>& magDb, int bin) const;         // stage-1 only; never touches refinement
    float robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const;
    static void blendWeights(float sharpness, float& wN, float& wM, float& wB);
};
