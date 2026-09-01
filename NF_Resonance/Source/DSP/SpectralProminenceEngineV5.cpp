#include "SpectralProminenceEngineV5.h"
#include <algorithm>
#include <chrono>

void SpectralProminenceEngineV5::recomputeGeometry()
{
    narrowCoreR.assign((size_t) bins, 0); narrowCtxR.assign((size_t) bins, 0);
    narrowCoreLimited.assign((size_t) bins, 0); narrowCtxLimited.assign((size_t) bins, 0);
    for (int i = 0; i < bins; ++i)
    {
        double fCenter = juce::jmax(1.0, (double) i * sampleRate / fftSize);
        auto radiusFor = [&](double octaves) {
            double fLow = fCenter * std::pow(2.0, -octaves / 2.0), fHigh = fCenter * std::pow(2.0, octaves / 2.0);
            int binLow = (int) std::round(fLow * fftSize / sampleRate), binHigh = (int) std::round(fHigh * fftSize / sampleRate);
            return juce::jmax(i - binLow, binHigh - i);
        };
        int desiredCore = radiusFor(narrowCoreOct);
        int core = juce::jmax(1, desiredCore);
        int desiredCtx = radiusFor(narrowCtxOct);
        int ctx = juce::jmax(core + minContextBins, desiredCtx);
        narrowCoreR[(size_t) i] = core; narrowCtxR[(size_t) i] = ctx;
        narrowCoreLimited[(size_t) i] = desiredCore < 1 ? 1 : 0;
        narrowCtxLimited[(size_t) i] = (desiredCtx < core + minContextBins) ? 1 : 0;
    }
    buildGrid(broadPPO, broadGridBin, binBroadGridLo, binBroadGridHi, binBroadGridFrac, broadGridValue);
    buildGrid(mediumPPO, mediumGridBin, binMediumGridLo, binMediumGridHi, binMediumGridFrac, mediumGridValue);

    // Precompute BROAD/MEDIUM core/context radii per grid point HERE (prepare-time),
    // not in computeProminence() -- previously radiusFor() (with std::pow) was called
    // twice per grid point EVERY FRAME (~2000+ pow() calls/frame across both grids).
    auto radiusForAtBin = [&](int b, double octaves) {
        double fCenter = juce::jmax(1.0, (double) b * sampleRate / fftSize);
        double fLow = fCenter * std::pow(2.0, -octaves / 2.0), fHigh = fCenter * std::pow(2.0, octaves / 2.0);
        int binLow = (int) std::round(fLow * fftSize / sampleRate), binHigh = (int) std::round(fHigh * fftSize / sampleRate);
        return juce::jmax(b - binLow, binHigh - b);
    };
    broadGridCoreR.assign(broadGridBin.size(), 0); broadGridCtxR.assign(broadGridBin.size(), 0);
    for (size_t g = 0; g < broadGridBin.size(); ++g)
    {
        int b = broadGridBin[g];
        int cr = juce::jmax(1, radiusForAtBin(b, 0.35));
        int xr = juce::jmax(cr + minContextBins, radiusForAtBin(b, 3.0));
        broadGridCoreR[g] = cr; broadGridCtxR[g] = xr;
    }
    mediumGridCoreR.assign(mediumGridBin.size(), 0); mediumGridCtxR.assign(mediumGridBin.size(), 0);
    for (size_t g = 0; g < mediumGridBin.size(); ++g)
    {
        int b = mediumGridBin[g];
        int cr = juce::jmax(1, radiusForAtBin(b, 0.15));
        int xr = juce::jmax(cr + minContextBins, radiusForAtBin(b, 1.2));
        mediumGridCoreR[g] = cr; mediumGridCtxR[g] = xr;
    }

    // V2-A5B: precompute the 3 fixed blocks per side (for O1_BlockTrimmedMean6
    // and O1_RobustSideSlope) and the tilt-aware lerp weight (for
    // O1_LeftRightInterp and O1_RobustSideSlope), once, here -- never in
    // compute(). A block's [lo,hi] uses hi<lo to mark "empty" (matches
    // sidebandMean()'s existing empty-range contract, returns 0.0f).
    for (int k = 0; k < 3; ++k)
    {
        narrowLeftBlockLo[(size_t) k].assign((size_t) bins, 0); narrowLeftBlockHi[(size_t) k].assign((size_t) bins, -1);
        narrowRightBlockLo[(size_t) k].assign((size_t) bins, 0); narrowRightBlockHi[(size_t) k].assign((size_t) bins, -1);
    }
    narrowInterpWeight.assign((size_t) bins, 0.5f);
    auto splitIntoBlocks = [&](int a, int b, int lo[3], int hi[3])
    {
        a = juce::jmax(0, a); b = juce::jmin(bins - 1, b);
        if (b < a) { for (int k = 0; k < 3; ++k) { lo[k] = 0; hi[k] = -1; } return; }
        int len = b - a + 1, base = len / 3, rem = len % 3, pos = a;
        for (int k = 0; k < 3; ++k) { int sz = base + (k < rem ? 1 : 0); if (sz <= 0) { lo[k] = pos; hi[k] = pos - 1; } else { lo[k] = pos; hi[k] = pos + sz - 1; pos += sz; } }
    };
    auto logHzAtBin = [&](int b) { double hz = juce::jmax(1.0, (double) b * sampleRate / fftSize); return std::log2(hz); };
    for (int i = 0; i < bins; ++i)
    {
        int core = narrowCoreR[(size_t) i], ctx = narrowCtxR[(size_t) i];
        int leftA = i - ctx, leftB = i - core - 1, rightA = i + core + 1, rightB = i + ctx;
        int lLo[3], lHi[3], rLo[3], rHi[3];
        splitIntoBlocks(leftA, leftB, lLo, lHi);
        splitIntoBlocks(rightA, rightB, rLo, rHi);
        for (int k = 0; k < 3; ++k) { narrowLeftBlockLo[(size_t) k][(size_t) i] = lLo[k]; narrowLeftBlockHi[(size_t) k][(size_t) i] = lHi[k]; narrowRightBlockLo[(size_t) k][(size_t) i] = rLo[k]; narrowRightBlockHi[(size_t) k][(size_t) i] = rHi[k]; }

        bool hasLeft = leftB >= juce::jmax(0, leftA), hasRight = rightB >= rightA;
        if (hasLeft && hasRight)
        {
            double leftCenterLog = logHzAtBin((juce::jmax(0, leftA) + leftB) / 2);
            double rightCenterLog = logHzAtBin((rightA + juce::jmin(bins - 1, rightB)) / 2);
            double binLog = logHzAtBin(i);
            double span = rightCenterLog - leftCenterLog;
            double w = span > 1e-9 ? (binLog - leftCenterLog) / span : 0.5;
            narrowInterpWeight[(size_t) i] = (float) juce::jlimit(0.0, 1.0, w);
        }
        else narrowInterpWeight[(size_t) i] = hasRight ? 1.0f : 0.0f;
    }
}

void SpectralProminenceEngineV5::buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch)
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

void SpectralProminenceEngineV5::setNarrowScaleParams(double c, double x) { narrowCoreOct = c; narrowCtxOct = x; if (bins > 0) recomputeGeometry(); }
void SpectralProminenceEngineV5::setMinContextBins(int n) { minContextBins = juce::jmax(1, n); if (bins > 0) recomputeGeometry(); }

void SpectralProminenceEngineV5::prepare(int numBins, double sr, int fft, int bPPO, int mPPO)
{
    sampleRate = sr; fftSize = fft; bins = numBins; broadPPO = juce::jmax(4, bPPO); mediumPPO = juce::jmax(4, mPPO);
    prefixSum.assign((size_t) bins + 1, 0.0f);
    prefixSumSq.assign((size_t) bins + 1, 0.0f);
    narrowProm.assign((size_t) bins, 0.0f);
    recomputeGeometry();
}

SpectralProminenceEngineV5::ScaleGeometryInfo SpectralProminenceEngineV5::scaleGeometryAt(int bin, Scale s) const
{
    ScaleGeometryInfo g;
    if (bin < 0 || bin >= bins || s != Narrow) return g;
    g.coreRadius = narrowCoreR[(size_t) bin]; g.contextRadius = narrowCtxR[(size_t) bin];
    g.coreResolutionLimited = narrowCoreLimited[(size_t) bin] != 0;
    g.contextResolutionLimited = narrowCtxLimited[(size_t) bin] != 0;
    return g;
}

void SpectralProminenceEngineV5::blendWeights(float sharpness, float& wN, float& wM, float& wB)
{
    float s = juce::jlimit(0.0f, 10.0f, sharpness) / 10.0f;
    wN = s * s; wB = (1.0f - s) * (1.0f - s); wM = juce::jmax(0.0f, 1.0f - wN - wB);
}

float SpectralProminenceEngineV5::sidebandMean(int a, int b) const
{
    a = juce::jmax(0, a); b = juce::jmin(bins - 1, b);
    if (b < a) return 0.0f;
    return (prefixSum[(size_t) (b + 1)] - prefixSum[(size_t) a]) / (float) (b - a + 1);
}

float SpectralProminenceEngineV5::narrowEstimateAt(int bin) const
{
    int core = narrowCoreR[(size_t) bin], ctx = narrowCtxR[(size_t) bin];
    int leftA = bin - ctx, leftB = bin - core - 1;
    int rightA = bin + core + 1, rightB = bin + ctx;
    bool hasLeft = leftB >= leftA, hasRight = rightB >= rightA;

    switch (narrowMethod)
    {
        case NarrowMethod::SidebandMean:
        {
            float l = hasLeft ? sidebandMean(leftA, leftB) : 0.0f;
            float r = hasRight ? sidebandMean(rightA, rightB) : 0.0f;
            if (hasLeft && hasRight) return 0.5f * (l + r);
            return hasLeft ? l : (hasRight ? r : sidebandMean(bin, bin));
        }
        case NarrowMethod::SidebandMin:
        {
            float l = hasLeft ? sidebandMean(leftA, leftB) : 1e9f;
            float r = hasRight ? sidebandMean(rightA, rightB) : 1e9f;
            if (hasLeft && hasRight) return juce::jmin(l, r);
            return hasLeft ? l : (hasRight ? r : sidebandMean(bin, bin));
        }
        case NarrowMethod::WinsorizedSideband:
        {
            auto winsorizedSide = [&](int a, int b) -> float
            {
                a = juce::jmax(0, a); b = juce::jmin(bins - 1, b);
                if (b < a) return 0.0f;
                int count = b - a + 1;
                float sum = prefixSum[(size_t)(b + 1)] - prefixSum[(size_t) a];
                float sumSq = prefixSumSq[(size_t)(b + 1)] - prefixSumSq[(size_t) a];
                float mean = sum / count;
                if (count <= 1) return mean;
                float var = juce::jmax(0.0f, sumSq / count - mean * mean);
                float sd = std::sqrt(var);
                float lo = mean - 1.5f * sd, hi = mean + 1.5f * sd;
                double sum2 = 0; int n2 = 0;
                for (int i = a; i <= b; ++i)
                {
                    float v = prefixSum[(size_t)(i + 1)] - prefixSum[(size_t) i];
                    if (v >= lo && v <= hi) { sum2 += v; ++n2; }
                }
                return n2 > 0 ? (float) (sum2 / n2) : mean;
            };
            float l = hasLeft ? winsorizedSide(leftA, leftB) : 0.0f;
            float r = hasRight ? winsorizedSide(rightA, rightB) : 0.0f;
            if (hasLeft && hasRight) return 0.5f * (l + r);
            return hasLeft ? l : (hasRight ? r : sidebandMean(bin, bin));
        }
        case NarrowMethod::O1_LeftRightInterp:
        {
            // A: two O(1) prefix-sum reads (whole side, no blocks needed here)
            // lerp'd by the precomputed tilt-aware log-frequency weight.
            float l = hasLeft ? sidebandMean(leftA, leftB) : 0.0f;
            float r = hasRight ? sidebandMean(rightA, rightB) : 0.0f;
            if (hasLeft && hasRight) { float w = narrowInterpWeight[(size_t) bin]; return l + (r - l) * w; }
            return hasLeft ? l : (hasRight ? r : sidebandMean(bin, bin));
        }
        case NarrowMethod::O1_BlockTrimmedMean6:
        {
            // B: 6 fixed prefix-sum block means (3 left + 3 right), drop the
            // running min/max while summing, average the remaining 4. Fixed
            // 6-element pass -- no sort, no loop over sideband bins.
            float v[6];
            for (int k = 0; k < 3; ++k) v[k]     = sidebandMean(narrowLeftBlockLo[(size_t) k][(size_t) bin],  narrowLeftBlockHi[(size_t) k][(size_t) bin]);
            for (int k = 0; k < 3; ++k) v[3 + k] = sidebandMean(narrowRightBlockLo[(size_t) k][(size_t) bin], narrowRightBlockHi[(size_t) k][(size_t) bin]);
            float mn = v[0], mx = v[0], sum = 0.0f;
            for (float x : v) { sum += x; mn = juce::jmin(mn, x); mx = juce::jmax(mx, x); }
            return (sum - mn - mx) * 0.25f;
        }
        case NarrowMethod::O1_RobustSideSlope:
        {
            // C: fixed 3-element median network per side (3 compares, no
            // sort), then lerp the two side medians by the same tilt-aware
            // log-frequency weight used in A.
            auto median3 = [](float a, float b, float c) -> float
            {
                if (a > b) { float t = a; a = b; b = t; }
                if (b > c) { float t = b; b = c; c = t; }
                if (a > b) { float t = a; a = b; b = t; }
                return b;
            };
            float lv[3], rv[3];
            for (int k = 0; k < 3; ++k) lv[k] = sidebandMean(narrowLeftBlockLo[(size_t) k][(size_t) bin],  narrowLeftBlockHi[(size_t) k][(size_t) bin]);
            for (int k = 0; k < 3; ++k) rv[k] = sidebandMean(narrowRightBlockLo[(size_t) k][(size_t) bin], narrowRightBlockHi[(size_t) k][(size_t) bin]);
            float l = median3(lv[0], lv[1], lv[2]), r = median3(rv[0], rv[1], rv[2]);
            if (hasLeft && hasRight) { float w = narrowInterpWeight[(size_t) bin]; return l + (r - l) * w; }
            return hasLeft ? l : (hasRight ? r : sidebandMean(bin, bin));
        }
        case NarrowMethod::MeanOfGroupMeans:
        default:
        {
            // Split each sideband into up to 3 groups; collect group means (<=6 total); take their median.
            float groupMeans[6]; int nGroups = 0;
            auto addGroups = [&](int a, int b)
            {
                a = juce::jmax(0, a); b = juce::jmin(bins - 1, b);
                if (b < a) return;
                int len = b - a + 1;
                int numG = juce::jmin(3, juce::jmax(1, len / 4));
                int chunk = juce::jmax(1, len / numG);
                int pos = a;
                for (int g = 0; g < numG && pos <= b && nGroups < 6; ++g)
                {
                    int gEnd = (g == numG - 1) ? b : juce::jmin(b, pos + chunk - 1);
                    groupMeans[nGroups++] = sidebandMean(pos, gEnd);
                    pos = gEnd + 1;
                }
            };
            if (hasLeft) addGroups(leftA, leftB);
            if (hasRight) addGroups(rightA, rightB);
            if (nGroups == 0) return sidebandMean(bin, bin);
            std::sort(groupMeans, groupMeans + nGroups);
            return groupMeans[nGroups / 2];
        }
    }
}

void SpectralProminenceEngineV5::computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);

#if NF_PROMINENCE_PROFILING
    auto tStart = std::chrono::steady_clock::now();
#endif

    prefixSum[0] = 0.0f; prefixSumSq[0] = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        prefixSum[(size_t) (i + 1)] = prefixSum[(size_t) i] + magDb[(size_t) i];
        prefixSumSq[(size_t) (i + 1)] = prefixSumSq[(size_t) i] + magDb[(size_t) i] * magDb[(size_t) i];
    }
#if NF_PROMINENCE_PROFILING
    auto tP1 = std::chrono::steady_clock::now();
    prefixUs = std::chrono::duration<double, std::micro>(tP1 - tStart).count();
#endif

    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);

    // BROAD/MEDIUM core/context radii are precomputed once in recomputeGeometry()
    // (broadGridCoreR/CtxR, mediumGridCoreR/CtxR) -- no std::pow/log2 per frame here.
    for (size_t g = 0; g < broadGridBin.size(); ++g)
    {
        int b = broadGridBin[g];
        int cr = broadGridCoreR[g], xr = broadGridCtxR[g];
        broadGridValue[g] = magDb[(size_t) b] - sidebandMean(b - xr, b - cr - 1) * 0.5f - sidebandMean(b + cr + 1, b + xr) * 0.5f;
    }
#if NF_PROMINENCE_PROFILING
    auto tB1 = std::chrono::steady_clock::now();
    broadUs = std::chrono::duration<double, std::micro>(tB1 - tP1).count();
#endif

    for (size_t g = 0; g < mediumGridBin.size(); ++g)
    {
        int b = mediumGridBin[g];
        int cr = mediumGridCoreR[g], xr = mediumGridCtxR[g];
        mediumGridValue[g] = magDb[(size_t) b] - sidebandMean(b - xr, b - cr - 1) * 0.5f - sidebandMean(b + cr + 1, b + xr) * 0.5f;
    }
#if NF_PROMINENCE_PROFILING
    auto tM1 = std::chrono::steady_clock::now();
    mediumUs = std::chrono::duration<double, std::micro>(tM1 - tB1).count();
#endif

    for (int i = 0; i < n; ++i)
        narrowProm[(size_t) i] = magDb[(size_t) i] - narrowEstimateAt(i);
#if NF_PROMINENCE_PROFILING
    auto tN1 = std::chrono::steady_clock::now();
    narrowUs = std::chrono::duration<double, std::micro>(tN1 - tM1).count();
#endif

    for (int i = 0; i < n; ++i)
    {
        float pB = juce::jmap(binBroadGridFrac[(size_t) i], broadGridValue[(size_t) binBroadGridLo[(size_t) i]], broadGridValue[(size_t) binBroadGridHi[(size_t) i]]);
        float pM = juce::jmap(binMediumGridFrac[(size_t) i], mediumGridValue[(size_t) binMediumGridLo[(size_t) i]], mediumGridValue[(size_t) binMediumGridHi[(size_t) i]]);
        float pN = narrowProm[(size_t) i];
        prominenceOut[(size_t) i] = wN * pN + wM * pM + wB * pB;
    }
#if NF_PROMINENCE_PROFILING
    auto tEnd = std::chrono::steady_clock::now();
    blendUs = std::chrono::duration<double, std::micro>(tEnd - tN1).count();
    totalUs = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
#endif
}
