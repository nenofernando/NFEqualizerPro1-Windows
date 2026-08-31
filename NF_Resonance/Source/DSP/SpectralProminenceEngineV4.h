#pragma once
#include <JuceHeader.h>

// V2-A4: region-based NARROW refinement. BROAD (UnweightedMean, grid) and
// MEDIUM (CheapMean, grid) are frozen from V2-A3 -- this class only changes
// how NARROW's expensive stage works.
//
// Instead of running P25 independently at every candidate bin (which wastes
// work on spatially redundant bins belonging to the same spectral salience),
// candidate bins are grouped into CandidateRegions via a valley-depth-aware
// scan (adjacent candidate bins merge UNLESS a sufficiently deep local
// minimum separates them, which keeps two close but genuinely distinct
// resonances as two regions). Each region is refined with either 1 point
// (its peak) or 3 points (peak + both shoulders) depending on width, and the
// resulting robust correction is interpolated across the region's bins --
// never falling back to zero for un-refined bins, which always keep the
// cheap full-bin NARROW estimate from V2-A3.
class SpectralProminenceEngineV4
{
public:
    struct CandidateRegion { int startBin = 0, endBin = 0, peakBin = 0; float peakPreScore = 0; };

    void prepare(int numBins, double sampleRate, int fftSize, int broadPPO = 36, int mediumPPO = 72);
    void setNarrowScaleParams(double coreOctaves, double contextOctaves);
    void setMinContextBins(int n);
    void setPercentile(double p) { percentile = juce::jlimit(0.0, 1.0, p); }
    void setNarrowCandidateThresholdDb(double thresholdDb, double marginDb) { narrowThresholdDb = thresholdDb; narrowMarginDb = juce::jmax(0.1, marginDb); }
    void setValleySplitThresholdDb(double db) { valleySplitDb = db; }
    void setSmallRegionWidthBins(int w) { smallRegionWidth = juce::jmax(1, w); } // <=this width -> 1-point refinement
    void setRegionBudget(int maxRegionsPerFrame) { regionBudget = maxRegionsPerFrame; } // <0 = unlimited

    // Temporal cache: a region whose peak frequency/width/prescore stay close
    // to a previously-refined region can skip full P25 for up to maxCacheAge
    // frames, reusing the cached correction (still validated each frame via
    // the cheap estimate, so it can't go stale silently).
    void setTemporalCache(bool enabled, int maxCacheAgeFrames = 4, double freqToleranceOct = 0.02, double scoreToleranceDb = 1.5);

    // Anti-starvation: a region that keeps losing the priority competition
    // (present as a candidate but never selected for refinement) accumulates
    // an aging bonus added to its own preliminary score each frame it waits.
    // This guarantees a bounded worst-case wait for ANY persistent candidate,
    // not just the highest-scoring ones -- a region can't be starved forever
    // just because higher-scoring regions exist every frame.
    void setFairnessAging(bool enabled, double bonusPerFrameDb = 0.15) { fairnessEnabled = enabled; agingBonusPerFrame = bonusPerFrameDb; }

    // Tiered scheduler with a PROVABLE bounded-deadline guarantee (not just a
    // priority bonus, which offers no formal bound): Tier 1 (urgent -- brand
    // new regions or large score jumps) and Tier 2 (normal -- highest current
    // score) get their own reserved slots; Tier 3's slots are reserved
    // EXCLUSIVELY for the oldest-waiting remaining candidates, strictly by
    // age, regardless of score. Because Tier 3 always processes at least
    // tier3Slots of the oldest candidates every frame independent of Tier 1/2
    // activity, any candidate that keeps existing is guaranteed serviced
    // within ceil(N/tier3Slots) frames, where N is the candidate count --
    // this bound holds by construction, not by tuning a bonus constant.
    void setTieredScheduling(bool enabled, int tier1Slots, int tier2Slots, int tier3Slots, double newnessFreqTolOct = 0.03, double changeThresholdDb = 3.0);
    int lastMaxAgeAtSelection() const { return lastMaxAgeSeen; }

    void computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut);

    int lastCandidateBinCount() const { return lastCandidateBins; }
    int lastRegionCount() const { return lastRegions; }
    int lastRegionsRefinedCount() const { return lastRegionsRefined; }
    int lastRegionsFromCacheCount() const { return lastRegionsFromCache; }
    // Per-region info for the just-completed frame (parallel to region list order below).
    struct RegionResult { int startBin, endBin, peakBin; float peakPreScore; float priority; bool refined; bool fromCache; int rank; int framesWaited; int tier = 0; };
    const std::vector<RegionResult>& lastRegionResults() const { return lastResults; }
    double lastBroadUs() const { return broadUs; }
    double lastMediumUs() const { return mediumUs; }
    double lastGroupingUs() const { return groupingUs; }
    double lastNarrowRefineUs() const { return narrowRefineUs; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int bins = 1025;
    int broadPPO = 36, mediumPPO = 72;
    int minContextBins = 6;
    double percentile = 0.25;
    double narrowThresholdDb = 2.5, narrowMarginDb = 3.0;
    double valleySplitDb = 3.0;
    int smallRegionWidth = 3;
    int regionBudget = -1;
    bool cacheEnabled = false; int maxCacheAge = 4; double cacheFreqTolOct = 0.02; double cacheScoreTolDb = 1.5;

    double coreOct = 0.04, ctxOct = 0.9;
    std::vector<int> narrowCoreR, narrowCtxR;
    std::vector<uint8_t> narrowCoreLimited, narrowCtxLimited;

    std::vector<int> broadGridBin, mediumGridBin;
    std::vector<int> binBroadGridLo, binBroadGridHi; std::vector<float> binBroadGridFrac;
    std::vector<int> binMediumGridLo, binMediumGridHi; std::vector<float> binMediumGridFrac;
    std::vector<float> broadGridValue, mediumGridValue;

    std::vector<float> prefixSum;
    std::vector<float> narrowCheap, narrowFinal;
    std::vector<CandidateRegion> regions;      // preallocated to `bins` capacity
    std::vector<int> regionPriorityOrder;      // preallocated to `bins` capacity
    std::vector<int> localMaxima, splitPoints; // scratch for grouping, preallocated to `bins`
    std::vector<float> robustScratch;

    // Temporal cache state (persists across frames -- no per-call allocation)
    struct CachedRegion { bool active = false; double freqOct = 0; float score = 0; float correctionDb = 0; int age = 0; };
    std::vector<CachedRegion> cache; // preallocated fixed slots

    // Fairness/aging state (persists across frames -- no per-call allocation)
    bool fairnessEnabled = false; double agingBonusPerFrame = 0.15;
    bool tieredEnabled = false; int tier1Slots = 8, tier2Slots = 4, tier3Slots = 4;
    double newnessFreqTol = 0.03, changeThresholdDb = 3.0;
    int lastMaxAgeSeen = 0;
    struct WaitingEntry { bool active = false; double freqOct = 0; int framesWaiting = 0; float lastScore = 0; };
    std::vector<WaitingEntry> waitingTable; // preallocated fixed slots
    std::vector<RegionResult> lastResults;  // preallocated to `bins` capacity
    std::vector<float> priorityScratch;     // preallocated to `bins` capacity
    std::vector<int> waitFramesScratch;     // preallocated to `bins` capacity
    std::vector<uint8_t> tierScratch, isNewScratch, takenScratch; // preallocated to `bins` capacity
    std::vector<int> t1candScratch, t2candScratch, t3candScratch; // preallocated to `bins` capacity

    int lastCandidateBins = 0, lastRegions = 0, lastRegionsRefined = 0, lastRegionsFromCache = 0;
    double broadUs = 0, mediumUs = 0, groupingUs = 0, narrowRefineUs = 0;

    void recomputeGeometry();
    void buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch);
    float cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const;
    float robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const;
    void groupCandidatesIntoRegions();
    static void blendWeights(float sharpness, float& wN, float& wM, float& wB);
};
