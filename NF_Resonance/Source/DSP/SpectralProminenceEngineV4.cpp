#include "SpectralProminenceEngineV4.h"
#include <algorithm>
#include <chrono>

void SpectralProminenceEngineV4::recomputeGeometry()
{
    narrowCoreR.assign((size_t) bins, 0); narrowCtxR.assign((size_t) bins, 0);
    narrowCoreLimited.assign((size_t) bins, 0); narrowCtxLimited.assign((size_t) bins, 0);
    for (int i = 0; i < bins; ++i)
    {
        double fCenter = juce::jmax(1.0, (double) i * sampleRate / fftSize);
        auto radiusFor = [&](double octaves) {
            double fLow = fCenter * std::pow(2.0, -octaves / 2.0);
            double fHigh = fCenter * std::pow(2.0, octaves / 2.0);
            int binLow = (int) std::round(fLow * fftSize / sampleRate);
            int binHigh = (int) std::round(fHigh * fftSize / sampleRate);
            return juce::jmax(i - binLow, binHigh - i);
        };
        int desiredCore = radiusFor(coreOct);
        int core = juce::jmax(1, desiredCore);
        int desiredCtx = radiusFor(ctxOct);
        int ctx = juce::jmax(core + minContextBins, desiredCtx);
        narrowCoreR[(size_t) i] = core; narrowCtxR[(size_t) i] = ctx;
        narrowCoreLimited[(size_t) i] = desiredCore < 1 ? 1 : 0;
        narrowCtxLimited[(size_t) i] = (desiredCtx < core + minContextBins) ? 1 : 0;
    }
    buildGrid(broadPPO, broadGridBin, binBroadGridLo, binBroadGridHi, binBroadGridFrac, broadGridValue);
    buildGrid(mediumPPO, mediumGridBin, binMediumGridLo, binMediumGridHi, binMediumGridFrac, mediumGridValue);
}

void SpectralProminenceEngineV4::buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch)
{
    const double fLow = 20.0, fHigh = juce::jmax(fLow * 1.01, sampleRate * 0.5);
    int numPoints = juce::jmax(2, (int) std::round(ppo * std::log2(fHigh / fLow)) + 1);
    gridBinOut.assign((size_t) numPoints, 0);
    for (int g = 0; g < numPoints; ++g)
    {
        double freq = fLow * std::pow(2.0, (double) g / ppo);
        gridBinOut[(size_t) g] = juce::jlimit(0, bins - 1, (int) std::round(freq * fftSize / sampleRate));
    }
    gridValueScratch.assign((size_t) numPoints, 0.0f);
    binLoOut.assign((size_t) bins, 0); binHiOut.assign((size_t) bins, 0); binFracOut.assign((size_t) bins, 0.0f);
    for (int i = 0; i < bins; ++i)
    {
        double f = juce::jmax(1.0, (double) i * sampleRate / fftSize);
        double g = ppo * std::log2(juce::jmax(f, fLow) / fLow);
        int gLo = juce::jlimit(0, numPoints - 1, (int) std::floor(g));
        int gHi = juce::jlimit(0, numPoints - 1, gLo + 1);
        binLoOut[(size_t) i] = gLo; binHiOut[(size_t) i] = gHi;
        binFracOut[(size_t) i] = (gHi > gLo) ? juce::jlimit(0.0f, 1.0f, (float) (g - gLo)) : 0.0f;
    }
}

void SpectralProminenceEngineV4::setNarrowScaleParams(double c, double x) { coreOct = c; ctxOct = x; if (bins > 0) recomputeGeometry(); }
void SpectralProminenceEngineV4::setMinContextBins(int n) { minContextBins = juce::jmax(1, n); if (bins > 0) recomputeGeometry(); }
void SpectralProminenceEngineV4::setTemporalCache(bool enabled, int maxAge, double freqTolOct, double scoreTolDb)
{
    cacheEnabled = enabled; maxCacheAge = juce::jmax(1, maxAge); cacheFreqTolOct = freqTolOct; cacheScoreTolDb = scoreTolDb;
}

void SpectralProminenceEngineV4::setTieredScheduling(bool enabled, int t1, int t2, int t3, double newnessTol, double changeThr)
{
    tieredEnabled = enabled; tier1Slots = juce::jmax(0, t1); tier2Slots = juce::jmax(0, t2); tier3Slots = juce::jmax(0, t3);
    newnessFreqTol = newnessTol; changeThresholdDb = changeThr;
}

void SpectralProminenceEngineV4::prepare(int numBins, double sr, int fft, int bPPO, int mPPO)
{
    sampleRate = sr; fftSize = fft; bins = numBins; broadPPO = juce::jmax(4, bPPO); mediumPPO = juce::jmax(4, mPPO);
    prefixSum.assign((size_t) bins + 1, 0.0f);
    narrowCheap.assign((size_t) bins, 0.0f);
    narrowFinal.assign((size_t) bins, 0.0f);
    regions.reserve((size_t) bins);
    regionPriorityOrder.reserve((size_t) bins);
    localMaxima.reserve((size_t) bins);
    splitPoints.reserve((size_t) bins);
    cache.assign(256, CachedRegion{});
    waitingTable.assign(512, WaitingEntry{});
    lastResults.reserve((size_t) bins);
    priorityScratch.assign((size_t) bins, 0.0f);
    waitFramesScratch.assign((size_t) bins, 0);
    tierScratch.assign((size_t) bins, 0); isNewScratch.assign((size_t) bins, 0); takenScratch.assign((size_t) bins, 0);
    t1candScratch.reserve((size_t) bins); t2candScratch.reserve((size_t) bins); t3candScratch.reserve((size_t) bins);
    recomputeGeometry();
    int maxRadius = 0;
    for (int i = 0; i < bins; ++i) maxRadius = juce::jmax(maxRadius, narrowCtxR[(size_t) i]);
    robustScratch.assign((size_t) (maxRadius * 2 + 4), 0.0f);
}

void SpectralProminenceEngineV4::blendWeights(float sharpness, float& wN, float& wM, float& wB)
{
    float s = juce::jlimit(0.0f, 10.0f, sharpness) / 10.0f;
    wN = s * s; wB = (1.0f - s) * (1.0f - s); wM = juce::jmax(0.0f, 1.0f - wN - wB);
}

float SpectralProminenceEngineV4::cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    float sumCtx = prefixSum[(size_t) (ctxB + 1)] - prefixSum[(size_t) ctxA];
    float sumCore = prefixSum[(size_t) (coreB + 1)] - prefixSum[(size_t) coreA];
    int count = (ctxB - ctxA + 1) - (coreB - coreA + 1);
    if (count <= 0) return prefixSum[(size_t) (bin + 1)] - prefixSum[(size_t) bin];
    return (sumCtx - sumCore) / (float) count;
}

float SpectralProminenceEngineV4::robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    scratch.clear();
    for (int i = ctxA; i <= ctxB; ++i) if (i < coreA || i > coreB) scratch.push_back(magDb[(size_t) i]);
    if (scratch.empty()) return magDb[(size_t) bin];
    int count = (int) scratch.size();
    int idx = juce::jlimit(0, count - 1, (int) (count * (float) percentile));
    std::nth_element(scratch.begin(), scratch.begin() + idx, scratch.begin() + count);
    return scratch[(size_t) idx];
}

void SpectralProminenceEngineV4::groupCandidatesIntoRegions()
{
    regions.clear();
    lastCandidateBins = 0;
    float lo = (float) (narrowThresholdDb - narrowMarginDb * 0.5);
    int n = bins, i = 0;
    while (i < n)
    {
        if (narrowCheap[(size_t) i] <= lo) { ++i; continue; }
        int segStart = i;
        while (i < n && narrowCheap[(size_t) i] > lo) { ++lastCandidateBins; ++i; }
        int segEnd = i - 1;

        // local maxima within [segStart, segEnd]
        localMaxima.clear();
        for (int b = segStart; b <= segEnd; ++b)
        {
            float v = narrowCheap[(size_t) b];
            float left = (b > segStart) ? narrowCheap[(size_t) (b - 1)] : v - 1.0f;
            float right = (b < segEnd) ? narrowCheap[(size_t) (b + 1)] : v - 1.0f;
            if (v >= left && v >= right) localMaxima.push_back(b);
        }
        if (localMaxima.empty()) localMaxima.push_back(segStart);

        // accepted split points between consecutive maxima with a deep-enough valley
        splitPoints.clear();
        for (size_t k = 0; k + 1 < localMaxima.size(); ++k)
        {
            int m1 = localMaxima[k], m2 = localMaxima[k + 1];
            int valley = m1; float valleyVal = narrowCheap[(size_t) m1];
            for (int b = m1; b <= m2; ++b) if (narrowCheap[(size_t) b] < valleyVal) { valleyVal = narrowCheap[(size_t) b]; valley = b; }
            float depth = juce::jmin(narrowCheap[(size_t) m1], narrowCheap[(size_t) m2]) - valleyVal;
            if (depth >= (float) valleySplitDb) splitPoints.push_back(valley);
        }

        // partition [segStart, segEnd] at split points into regions
        int subStart = segStart;
        for (int sp : splitPoints)
        {
            int subEnd = sp;
            int peak = subStart; float peakVal = narrowCheap[(size_t) subStart];
            for (int b = subStart; b <= subEnd; ++b) if (narrowCheap[(size_t) b] > peakVal) { peakVal = narrowCheap[(size_t) b]; peak = b; }
            regions.push_back({ subStart, subEnd, peak, peakVal });
            subStart = subEnd + 1;
        }
        {
            int peak = subStart; float peakVal = narrowCheap[(size_t) subStart];
            for (int b = subStart; b <= segEnd; ++b) if (narrowCheap[(size_t) b] > peakVal) { peakVal = narrowCheap[(size_t) b]; peak = b; }
            regions.push_back({ subStart, segEnd, peak, peakVal });
        }
    }
    lastRegions = (int) regions.size();
}

void SpectralProminenceEngineV4::computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);

    prefixSum[0] = 0.0f;
    for (int i = 0; i < n; ++i) prefixSum[(size_t) (i + 1)] = prefixSum[(size_t) i] + magDb[(size_t) i];

    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);

    // ---- BROAD (frozen: UnweightedMean, grid) ----
    auto tB0 = std::chrono::high_resolution_clock::now();
    for (size_t g = 0; g < broadGridBin.size(); ++g)
    {
        int b = broadGridBin[g];
        // BROAD uses a wide, fixed octave window -- reuse narrow's context machinery isn't
        // appropriate here (different geometry), so compute directly with a broad core/context.
        double fCenter = juce::jmax(1.0, (double) b * sampleRate / fftSize);
        auto radiusFor = [&](double oct){ double fLow=fCenter*std::pow(2.0,-oct/2.0), fHigh=fCenter*std::pow(2.0,oct/2.0);
            int lo=(int)std::round(fLow*fftSize/sampleRate), hi=(int)std::round(fHigh*fftSize/sampleRate); return juce::jmax(b-lo,hi-b); };
        int cr = juce::jmax(1, radiusFor(0.35)), xr = juce::jmax(cr + minContextBins, radiusFor(3.0));
        broadGridValue[g] = magDb[(size_t) b] - cheapMeanExcludingCore(b, cr, xr);
    }
    auto tB1 = std::chrono::high_resolution_clock::now();
    broadUs = std::chrono::duration<double, std::micro>(tB1 - tB0).count();

    // ---- MEDIUM (frozen: CheapMean, grid) ----
    for (size_t g = 0; g < mediumGridBin.size(); ++g)
    {
        int b = mediumGridBin[g];
        double fCenter = juce::jmax(1.0, (double) b * sampleRate / fftSize);
        auto radiusFor = [&](double oct){ double fLow=fCenter*std::pow(2.0,-oct/2.0), fHigh=fCenter*std::pow(2.0,oct/2.0);
            int lo=(int)std::round(fLow*fftSize/sampleRate), hi=(int)std::round(fHigh*fftSize/sampleRate); return juce::jmax(b-lo,hi-b); };
        int cr = juce::jmax(1, radiusFor(0.15)), xr = juce::jmax(cr + minContextBins, radiusFor(1.2));
        mediumGridValue[g] = magDb[(size_t) b] - cheapMeanExcludingCore(b, cr, xr);
    }
    auto tM1 = std::chrono::high_resolution_clock::now();
    mediumUs = std::chrono::duration<double, std::micro>(tM1 - tB1).count();

    // ---- NARROW cheap estimate everywhere (always available) ----
    for (int i = 0; i < n; ++i)
        narrowCheap[(size_t) i] = magDb[(size_t) i] - cheapMeanExcludingCore(i, narrowCoreR[(size_t) i], narrowCtxR[(size_t) i]);
    narrowFinal = narrowCheap;

    // ---- Group into regions ----
    groupCandidatesIntoRegions();
    auto tG1 = std::chrono::high_resolution_clock::now();
    groupingUs = std::chrono::duration<double, std::micro>(tG1 - tM1).count();

    // ---- Priority order (peak pre-score + fairness aging bonus) + optional budget ----
    // Match each region to a waiting-table entry by frequency proximity (linear scan --
    // the table is small, typically far fewer active entries than its capacity).
    auto findWaitingSlot = [&](double freqOct) -> int
    {
        int best = -1; double bestDist = 1e9;
        for (int w = 0; w < (int) waitingTable.size(); ++w)
        {
            if (! waitingTable[(size_t) w].active) continue;
            double d = std::abs(waitingTable[(size_t) w].freqOct - freqOct);
            if (d < 0.03 && d < bestDist) { bestDist = d; best = w; }
        }
        return best;
    };

    regionPriorityOrder.clear();
    std::fill(tierScratch.begin(), tierScratch.begin() + (long) regions.size(), 0);
    std::fill(isNewScratch.begin(), isNewScratch.begin() + (long) regions.size(), 0);
    int maxAgeThisFrame = 0;
    for (int r = 0; r < (int) regions.size(); ++r)
    {
        regionPriorityOrder.push_back(r);
        double freqOct = std::log2(juce::jmax(1.0, (double) regions[(size_t) r].peakBin * sampleRate / fftSize));
        int slot = findWaitingSlot(freqOct);
        int waited = (slot >= 0) ? waitingTable[(size_t) slot].framesWaiting : 0;
        waitFramesScratch[(size_t) r] = waited;
        maxAgeThisFrame = juce::jmax(maxAgeThisFrame, waited);
        isNewScratch[(size_t) r] = (slot < 0) ? 1 : 0;
        float change = (slot >= 0) ? (regions[(size_t) r].peakPreScore - waitingTable[(size_t) slot].lastScore) : 0.0f;
        priorityScratch[(size_t) r] = regions[(size_t) r].peakPreScore + (fairnessEnabled && ! tieredEnabled ? (float) (agingBonusPerFrame * waited) : 0.0f);
        tierScratch[(size_t) r] = (isNewScratch[(size_t) r] || change > (float) changeThresholdDb) ? 1 : 2; // provisional; Tier3 assigned during selection
    }
    lastMaxAgeSeen = maxAgeThisFrame;

    lastResults.clear();
    for (int r = 0; r < (int) regions.size(); ++r)
        lastResults.push_back({ regions[(size_t) r].startBin, regions[(size_t) r].endBin, regions[(size_t) r].peakBin,
                                 regions[(size_t) r].peakPreScore, priorityScratch[(size_t) r], false, false, -1, waitFramesScratch[(size_t) r], 0 });

    int budget;
    if (tieredEnabled)
    {
        // Build ordered selection: Tier1 (new/big-change, by score) -> Tier2 (remaining, by score) -> Tier3 (remaining, strictly by age).
        std::vector<int>& order = regionPriorityOrder; // reuse as output buffer
        order.clear();
        std::fill(takenScratch.begin(), takenScratch.begin() + (long) regions.size(), 0);

        t1candScratch.clear(); for (int r=0;r<(int)regions.size();++r) if (tierScratch[(size_t)r]==1) t1candScratch.push_back(r);
        std::sort(t1candScratch.begin(), t1candScratch.end(), [&](int a,int b){ return priorityScratch[(size_t)a] > priorityScratch[(size_t)b]; });
        for (int i=0;i<(int)t1candScratch.size() && (int)order.size()<tier1Slots; ++i) { int r=t1candScratch[(size_t)i]; order.push_back(r); takenScratch[(size_t)r]=1; lastResults[(size_t)r].tier=1; }

        t2candScratch.clear(); for (int r=0;r<(int)regions.size();++r) if (! takenScratch[(size_t)r]) t2candScratch.push_back(r);
        std::sort(t2candScratch.begin(), t2candScratch.end(), [&](int a,int b){ return priorityScratch[(size_t)a] > priorityScratch[(size_t)b]; });
        int t2added=0; for (int i=0;i<(int)t2candScratch.size() && t2added<tier2Slots; ++i) { int r=t2candScratch[(size_t)i]; order.push_back(r); takenScratch[(size_t)r]=1; lastResults[(size_t)r].tier=2; ++t2added; }

        t3candScratch.clear(); for (int r=0;r<(int)regions.size();++r) if (! takenScratch[(size_t)r]) t3candScratch.push_back(r);
        std::sort(t3candScratch.begin(), t3candScratch.end(), [&](int a,int b){ return waitFramesScratch[(size_t)a] > waitFramesScratch[(size_t)b]; }); // oldest first
        int t3added=0; for (int i=0;i<(int)t3candScratch.size() && t3added<tier3Slots; ++i) { int r=t3candScratch[(size_t)i]; order.push_back(r); takenScratch[(size_t)r]=1; lastResults[(size_t)r].tier=3; ++t3added; }

        budget = (int) order.size();
        // rank = selection order within this frame
        for (int idx=0; idx<(int)order.size(); ++idx) lastResults[(size_t)order[(size_t)idx]].rank = idx;
    }
    else
    {
        std::sort(regionPriorityOrder.begin(), regionPriorityOrder.end(),
            [&](int a, int b) { return priorityScratch[(size_t) a] > priorityScratch[(size_t) b]; });
        budget = (regionBudget < 0) ? (int) regions.size() : juce::jmin((int) regions.size(), regionBudget);
        for (int idx = 0; idx < (int) regionPriorityOrder.size(); ++idx)
            lastResults[(size_t) regionPriorityOrder[(size_t) idx]].rank = idx;
    }

    lastRegionsRefined = 0; lastRegionsFromCache = 0;
    for (int idx = 0; idx < budget; ++idx)
    {
        int r = regionPriorityOrder[(size_t) idx];
        auto& reg = regions[(size_t) r];
        double freqOct = std::log2(juce::jmax(1.0, (double) reg.peakBin * sampleRate / fftSize));

        int cacheHitSlot = -1;
        if (cacheEnabled)
        {
            for (int c = 0; c < (int) cache.size(); ++c)
            {
                auto& ce = cache[(size_t) c];
                if (! ce.active) continue;
                if (ce.age >= maxCacheAge) continue;
                if (std::abs(ce.freqOct - freqOct) <= cacheFreqTolOct && std::abs(ce.score - reg.peakPreScore) <= (float) cacheScoreTolDb)
                { cacheHitSlot = c; break; }
            }
        }

        if (cacheHitSlot >= 0)
        {
            float correction = cache[(size_t) cacheHitSlot].correctionDb;
            for (int b = reg.startBin; b <= reg.endBin; ++b) narrowFinal[(size_t) b] = narrowCheap[(size_t) b] + correction;
            cache[(size_t) cacheHitSlot].age++;
            ++lastRegionsFromCache;
            lastResults[(size_t) r].fromCache = true;
            continue;
        }

        int width = reg.endBin - reg.startBin + 1;
        float correctionAtPeak;
        if (width <= smallRegionWidth)
        {
            float robust = magDb[(size_t) reg.peakBin] - robustPercentileExcludingCore(magDb, reg.peakBin, narrowCoreR[(size_t) reg.peakBin], narrowCtxR[(size_t) reg.peakBin], robustScratch);
            correctionAtPeak = robust - narrowCheap[(size_t) reg.peakBin];
            for (int b = reg.startBin; b <= reg.endBin; ++b) narrowFinal[(size_t) b] = narrowCheap[(size_t) b] + correctionAtPeak;
        }
        else
        {
            float robustPeak = magDb[(size_t) reg.peakBin] - robustPercentileExcludingCore(magDb, reg.peakBin, narrowCoreR[(size_t) reg.peakBin], narrowCtxR[(size_t) reg.peakBin], robustScratch);
            float robustLeft = magDb[(size_t) reg.startBin] - robustPercentileExcludingCore(magDb, reg.startBin, narrowCoreR[(size_t) reg.startBin], narrowCtxR[(size_t) reg.startBin], robustScratch);
            float robustRight = magDb[(size_t) reg.endBin] - robustPercentileExcludingCore(magDb, reg.endBin, narrowCoreR[(size_t) reg.endBin], narrowCtxR[(size_t) reg.endBin], robustScratch);
            float corrPeak = robustPeak - narrowCheap[(size_t) reg.peakBin];
            float corrLeft = robustLeft - narrowCheap[(size_t) reg.startBin];
            float corrRight = robustRight - narrowCheap[(size_t) reg.endBin];
            correctionAtPeak = corrPeak;
            for (int b = reg.startBin; b <= reg.endBin; ++b)
            {
                float corr;
                if (b <= reg.peakBin)
                    corr = (reg.peakBin > reg.startBin) ? juce::jmap((float) b, (float) reg.startBin, (float) reg.peakBin, corrLeft, corrPeak) : corrPeak;
                else
                    corr = (reg.endBin > reg.peakBin) ? juce::jmap((float) b, (float) reg.peakBin, (float) reg.endBin, corrPeak, corrRight) : corrPeak;
                narrowFinal[(size_t) b] = narrowCheap[(size_t) b] + corr;
            }
        }
        ++lastRegionsRefined;
        lastResults[(size_t) r].refined = true;

        if (cacheEnabled)
        {
            int freeSlot = -1;
            for (int c = 0; c < (int) cache.size(); ++c) if (! cache[(size_t) c].active) { freeSlot = c; break; }
            if (freeSlot < 0) freeSlot = (int) (reg.peakBin % cache.size()); // simple eviction
            cache[(size_t) freeSlot] = { true, freqOct, reg.peakPreScore, correctionAtPeak, 0 };
        }
    }
    if (cacheEnabled) for (auto& ce : cache) if (ce.active) ce.age = juce::jmin(ce.age, maxCacheAge + 1);

    // ---- Fairness/aging + tiered-scheduler bookkeeping: reset wait for
    // refined/cached regions, increment for excluded ones, and remember each
    // region's score so next frame can detect "new" vs "changed". ----
    if (fairnessEnabled || tieredEnabled)
    {
        for (int r = 0; r < (int) regions.size(); ++r)
        {
            double freqOct = std::log2(juce::jmax(1.0, (double) regions[(size_t) r].peakBin * sampleRate / fftSize));
            bool served = lastResults[(size_t) r].refined || lastResults[(size_t) r].fromCache;
            int slot = findWaitingSlot(freqOct);
            if (slot < 0)
            {
                for (int w = 0; w < (int) waitingTable.size(); ++w) if (! waitingTable[(size_t) w].active) { slot = w; break; }
                if (slot < 0) slot = regions[(size_t) r].peakBin % (int) waitingTable.size(); // simple eviction
                waitingTable[(size_t) slot] = { true, freqOct, 0, 0.0f };
            }
            waitingTable[(size_t) slot].framesWaiting = served ? 0 : (waitingTable[(size_t) slot].framesWaiting + 1);
            waitingTable[(size_t) slot].freqOct = freqOct;
            waitingTable[(size_t) slot].lastScore = regions[(size_t) r].peakPreScore;
        }
    }

    auto tN1 = std::chrono::high_resolution_clock::now();
    narrowRefineUs = std::chrono::duration<double, std::micro>(tN1 - tG1).count();

    // ---- Interpolate BROAD/MEDIUM + blend ----
    for (int i = 0; i < n; ++i)
    {
        float pB = juce::jmap(binBroadGridFrac[(size_t) i], broadGridValue[(size_t) binBroadGridLo[(size_t) i]], broadGridValue[(size_t) binBroadGridHi[(size_t) i]]);
        float pM = juce::jmap(binMediumGridFrac[(size_t) i], mediumGridValue[(size_t) binMediumGridLo[(size_t) i]], mediumGridValue[(size_t) binMediumGridHi[(size_t) i]]);
        float pN = narrowFinal[(size_t) i];
        prominenceOut[(size_t) i] = wN * pN + wM * pM + wB * pB;
    }
}
