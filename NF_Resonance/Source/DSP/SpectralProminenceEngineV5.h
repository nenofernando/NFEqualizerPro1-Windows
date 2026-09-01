#pragma once
#include <JuceHeader.h>
#include <array>

// Per-stage profiling (prefix/broad/medium/narrow/blend timing via
// std::chrono inside computeProminence() itself) is OFF by default -- a
// commercial/production build must never pay for internal clock reads on
// the audio thread. Define NF_PROMINENCE_PROFILING=1 (e.g. via a debug/
// diagnostic build config) to compile the timing calls back in; the
// lastXxxUs() accessors return 0 when profiling is compiled out, not stale
// data from a previous build.
#ifndef NF_PROMINENCE_PROFILING
 #define NF_PROMINENCE_PROFILING 0
#endif

// V2-A5: deterministic fast prominence engine. P25 is now an OFFLINE ORACLE
// used only in benchmarks -- the realtime path never runs nth_element, never
// schedules/gates/caches candidates, never has a content-dependent cost.
// Every bin gets a real, continuous prominence estimate every frame -- no
// bin's score is ever forced to zero for lacking a "candidate" designation.
//
//   BROAD  -- frozen from V2-A3/A4: UnweightedMean on a coarse grid,
//             interpolated onto the 1025 real bins.
//   MEDIUM -- frozen from V2-A3/A4: CheapMean on a finer grid, interpolated.
//   NARROW -- NEW: a fast robust approximation computed directly at EVERY
//             real bin (no grid, so a genuinely narrow resonance between any
//             two points can never be missed), using only O(radius) sideband
//             statistics via prefix sums -- no per-bin sort/selection.
//
// Core vs context stays separate per scale, as validated in V2-A3/A4 (NARROW
// keeps a small core but a properly wide context -- never back to radius=1).
class SpectralProminenceEngineV5
{
public:
    // Candidates for NARROW's fast-robust estimator (item 5 in the request):
    //   SidebandMean       -- average of the left/right sideband means (O(1) each via prefix sums).
    //   SidebandMin        -- min of the two sideband means (mild protection: a peak's own skirt biases at most one side).
    //   WinsorizedSideband -- each sideband's mean+std computed in O(1), then a single O(radius) pass excludes
    //                         values beyond +-1.5 sigma (no sort), sideband results then averaged.
    //   MeanOfGroupMeans   -- context split into a few small groups; each group's mean is O(1) via prefix
    //                         sums; the MEDIAN of that small set of group means is the estimate (sorting
    //                         ~6-8 numbers is negligible, never a per-bin full-context sort).
    // V2-A5B: WinsorizedSideband and MeanOfGroupMeans both rescan O(radius)
    // bins per query (a per-bin sort/filter pass over the whole sideband) --
    // diagnosed as the structural cause of NARROW dominating >97% of
    // compute() cost. The three methods below are true O(1)-per-bin: each
    // reads a small FIXED number of prefix-sum block means (no loop whose
    // trip count depends on context radius), combined via a fixed-size
    // compare/sum network (no std::sort, no nth_element).
    //   O1_LeftRightInterp   -- A: leftMean/rightMean (each O(1) via prefix
    //                           sum over the WHOLE side, not blocks), lerp'd
    //                           by each bin's precomputed log-frequency
    //                           position between the two sides' spans (tilt-
    //                           aware, unlike a flat 50/50 average).
    //                           2 prefix-sum reads + 1 lerp -- no loop at all.
    //   O1_BlockTrimmedMean6 -- B: context split into 3 fixed blocks per side
    //                           (L1..L3 | CORE | R1..R3), one prefix-sum mean
    //                           each (6 total), combined by dropping the min
    //                           and max of the 6 and averaging the remaining
    //                           4 (running min/max while summing -- 6 fixed
    //                           compares, no sort).
    //   O1_RobustSideSlope   -- C: each side gets its own robust estimate
    //                           from its 3 blocks via a fixed 3-element
    //                           median network (3 compares, no sort), then
    //                           the two side estimates are lerp'd using the
    //                           same log-frequency weight as A.
    enum class NarrowMethod { SidebandMean, SidebandMin, WinsorizedSideband, MeanOfGroupMeans,
                               O1_LeftRightInterp, O1_BlockTrimmedMean6, O1_RobustSideSlope };

    void prepare(int numBins, double sampleRate, int fftSize, int broadPPO = 36, int mediumPPO = 72);
    void setNarrowMethod(NarrowMethod m) { narrowMethod = m; }
    // The validated A5C combination (this default) is narrowCoreOct=0.18 /
    // narrowCtxOct=1.20 + O1_RobustSideSlope -- see the geometry comment
    // below. Exposed so callers/tests can assert the ACTIVE method rather
    // than trust a silent default never to have drifted.
    NarrowMethod activeNarrowMethod() const { return narrowMethod; }
    void setNarrowScaleParams(double coreOctaves, double contextOctaves);
    void setMinContextBins(int n);

    void computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    struct ScaleGeometryInfo { int coreRadius = 0, contextRadius = 0; bool coreResolutionLimited = false, contextResolutionLimited = false; };
    enum Scale { Narrow = 0, Medium = 1, Broad = 2 };
    ScaleGeometryInfo scaleGeometryAt(int bin, Scale s) const;

    double lastPrefixUs() const { return prefixUs; }
    double lastBroadUs() const { return broadUs; }
    double lastMediumUs() const { return mediumUs; }
    double lastNarrowUs() const { return narrowUs; }
    double lastBlendUs() const { return blendUs; }
    double lastTotalUs() const { return totalUs; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int bins = 1025;
    int broadPPO = 36, mediumPPO = 72;
    int minContextBins = 6;
    // V2-A5C calibration: widened from 0.04/0.90 -- the V2-A5B precision run
    // found Method C (O1_RobustSideSlope) systematically underestimating
    // prominence by ~8-12% at 1kHz+ even on a flat baseline, traced to the
    // injected/real resonance's own energy leaking past too-narrow a core
    // exclusion into the context blocks used for the baseline. A controlled
    // sweep (Tests/V2A5C_GeometrySweep.cpp) over core/context width at
    // Q=3/6/12/24/48, +2..+18dB, 250Hz-16kHz, 48kHz confirmed this widened
    // geometry cuts the Q=12 bias from -0.675dB to -0.146dB (MAE 0.715 ->
    // 0.189dB) with NO regression at any tested Q, and monotonicity stayed
    // 100% (50/50) at every candidate tried, including this one. Still O(1)
    // per bin -- only the two octave-width constants changed, no new loops.
    double narrowCoreOct = 0.18, narrowCtxOct = 1.20;
    // Default changed to the actually-validated A5C combination
    // (O1_RobustSideSlope, confirmed by Tests/V2A5C_GeometrySweep.cpp) --
    // was WinsorizedSideband, which nothing had validated for THIS geometry.
    // Every existing test already sets its method explicitly (including one
    // that deliberately restores WinsorizedSideband for its own comparison),
    // so this change is safe: no test relied on the old implicit default.
    NarrowMethod narrowMethod = NarrowMethod::O1_RobustSideSlope;

    std::vector<int> narrowCoreR, narrowCtxR;
    std::vector<uint8_t> narrowCoreLimited, narrowCtxLimited;
    // BROAD/MEDIUM core/context is derived per grid point directly (fixed octave widths), no per-bin table needed.

    // V2-A5B O(1) NARROW precomputation (per bin, filled once in
    // recomputeGeometry()): 3 fixed blocks per side + the tilt-aware lerp
    // weight. compute() only ever does array lookups + sidebandMean() here,
    // never geometry arithmetic.
    std::array<std::vector<int>, 3> narrowLeftBlockLo, narrowLeftBlockHi, narrowRightBlockLo, narrowRightBlockHi;
    std::vector<float> narrowInterpWeight; // 0 = fully left, 1 = fully right

    std::vector<int> broadGridBin, mediumGridBin;
    std::vector<int> binBroadGridLo, binBroadGridHi; std::vector<float> binBroadGridFrac;
    std::vector<int> binMediumGridLo, binMediumGridHi; std::vector<float> binMediumGridFrac;
    std::vector<float> broadGridValue, mediumGridValue;

    std::vector<float> prefixSum, prefixSumSq;
    std::vector<float> narrowProm; // per-bin, filled every frame

    double prefixUs = 0, broadUs = 0, mediumUs = 0, narrowUs = 0, blendUs = 0, totalUs = 0;

    // Precomputed once in prepare(): per-grid-point core/context radii for
    // BROAD and MEDIUM, so computeProminence() never calls std::pow/log2 in
    // its per-frame path -- only array lookups.
    std::vector<int> broadGridCoreR, broadGridCtxR, mediumGridCoreR, mediumGridCtxR;

    void recomputeGeometry();
    void buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch);
    float sidebandMean(int a, int b) const; // O(1) mean of [a,b] via prefix sum
    float narrowEstimateAt(int bin) const;  // dispatches to the selected NarrowMethod
    static void blendWeights(float sharpness, float& wN, float& wM, float& wB);
};
