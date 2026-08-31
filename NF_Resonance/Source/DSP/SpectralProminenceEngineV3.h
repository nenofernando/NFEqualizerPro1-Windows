#pragma once
#include <JuceHeader.h>

// V2-A3: scale-specific prominence engine. NARROW/MEDIUM/BROAD are treated
// as different mathematical problems rather than three parameterizations of
// the same P25 computation:
//
//   BROAD  -- represents wide spectral trend/tilt. Computed on a coarse
//             auxiliary log-frequency grid (its job is stability and correct
//             tilt-tracking, not resolving anything narrow) with a cheap,
//             robust-ish estimator, then interpolated onto the 1025 real
//             bins. Cost is small and CONTENT-INDEPENDENT (no candidate
//             gating -- always the same grid size).
//   MEDIUM -- intermediate. Also grid-based (finer than BROAD), cheap by
//             default; only escalates to full-bin P25 if that is shown to
//             matter. Cost is also predictable/content-independent.
//   NARROW -- the only scale allowed to be expensive, and the only one
//             candidate-gated. A cheap full-bin estimate (O(1) per bin via
//             prefix sums) is ALWAYS available everywhere -- being a weak
//             candidate never zeroes a bin's prominence, it only decides
//             whether that bin ALSO gets the expensive P25 refinement. The
//             number of bins refined per frame is capped (a real, bounded
//             realtime budget); if more bins qualify than the budget allows,
//             the highest-priority ones (by cheap prominence) are refined
//             first and the rest keep their cheap estimate.
//
// Core vs context per scale (as in V2-A2): a small CORE (excluded from that
// scale's own baseline) and an independently-sized CONTEXT (with an enforced
// minimum bin count), so NARROW's core can stay small while its context is
// deliberately wide enough to always have real reference spectrum.
class SpectralProminenceEngineV3
{
public:
    enum class BroadMethod { UnweightedMean, WinsorizedMean, ZScoreTrimmedMean, RobustLinearRegression };
    enum class MediumMethod { CheapMean, Percentile };

    struct ScaleParams { double coreOctaves; double contextOctaves; };

    void prepare(int numBins, double sampleRate, int fftSize, int broadPPO = 36, int mediumPPO = 72);
    void setScaleParams(ScaleParams narrow, ScaleParams medium, ScaleParams broad);
    void setMinContextBins(int n);
    void setBroadMethod(BroadMethod m) { broadMethod = m; }
    void setMediumMethod(MediumMethod m) { mediumMethod = m; }
    void setPercentile(double p) { percentile = juce::jlimit(0.0, 1.0, p); }
    void setNarrowCandidateThresholdDb(double thresholdDb, double marginDb) { narrowThresholdDb = thresholdDb; narrowMarginDb = juce::jmax(0.1, marginDb); }
    void setNarrowRefinementBudget(int maxBinsPerFrame) { narrowBudget = juce::jmax(0, maxBinsPerFrame); }

    // Full pipeline: BROAD trend -> MEDIUM trend -> NARROW candidate+bounded
    // refinement -> multi-scale prominence -> Sharpness blend. No allocation.
    void computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    int lastNarrowCandidateCount() const { return lastNarrowCandidates; }
    int lastNarrowRefinedCount() const { return lastNarrowRefined; }

    struct ScaleGeometryInfo { int coreRadius = 0, contextRadius = 0; bool coreResolutionLimited = false, contextResolutionLimited = false; };
    enum Scale { Narrow = 0, Medium = 1, Broad = 2 };
    ScaleGeometryInfo scaleGeometryAt(int bin, Scale s) const;

private:
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int bins = 1025;
    int broadPPO = 36, mediumPPO = 72;
    int minContextBins = 6;
    double percentile = 0.25;
    double narrowThresholdDb = 2.5, narrowMarginDb = 3.0;
    int narrowBudget = 128;
    BroadMethod broadMethod = BroadMethod::RobustLinearRegression;
    MediumMethod mediumMethod = MediumMethod::CheapMean;

    ScaleParams narrowP{ 0.04, 0.9 }, mediumP{ 0.15, 1.2 }, broadP{ 0.35, 3.0 };

    std::vector<int> coreR[3], ctxR[3];
    std::vector<uint8_t> coreLimited[3], ctxLimited[3];

    // BROAD grid
    std::vector<int> broadGridBin;
    std::vector<int> binBroadGridLo, binBroadGridHi; std::vector<float> binBroadGridFrac;
    std::vector<float> broadGridValue;

    // MEDIUM grid (used only if mediumMethod==CheapMean)
    std::vector<int> mediumGridBin;
    std::vector<int> binMediumGridLo, binMediumGridHi; std::vector<float> binMediumGridFrac;
    std::vector<float> mediumGridValue;

    std::vector<float> prefixSum, prefixSumSq;
    std::vector<float> narrowCheap; // full-bin cheap NARROW estimate, always populated
    std::vector<float> narrowFinal; // cheap, upgraded to robust for budgeted top-K
    std::vector<int> narrowCandidateIdx; // scratch for top-K selection, sized to bins in prepare()
    std::vector<float> robustScratch;
    int lastNarrowCandidates = 0, lastNarrowRefined = 0;

    void recomputeGeometry();
    void buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch);
    void computeCoreContextFor(double coreOct, double contextOct, std::vector<int>& coreOut, std::vector<int>& ctxOut, std::vector<uint8_t>& coreLimOut, std::vector<uint8_t>& ctxLimOut);

    float cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const;
    float winsorizedMeanExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius) const;
    float zscoreTrimmedMeanExcludingCore(int bin, int coreRadius, int contextRadius) const;
    float robustLinearRegressionExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius) const;
    float robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const;
    static void blendWeights(float sharpness, float& wN, float& wM, float& wB);
};
