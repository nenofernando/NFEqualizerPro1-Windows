#include "SpectralProminenceEngineV3.h"
#include <algorithm>

void SpectralProminenceEngineV3::computeCoreContextFor(double coreOct, double contextOct,
    std::vector<int>& coreOut, std::vector<int>& ctxOut, std::vector<uint8_t>& coreLimOut, std::vector<uint8_t>& ctxLimOut)
{
    coreOut.assign((size_t) bins, 0); ctxOut.assign((size_t) bins, 0);
    coreLimOut.assign((size_t) bins, 0); ctxLimOut.assign((size_t) bins, 0);
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
        int desiredCtx = radiusFor(contextOct);
        int ctx = juce::jmax(core + minContextBins, desiredCtx);
        coreOut[(size_t) i] = core; ctxOut[(size_t) i] = ctx;
        coreLimOut[(size_t) i] = desiredCore < 1 ? 1 : 0;
        ctxLimOut[(size_t) i] = (desiredCtx < core + minContextBins) ? 1 : 0;
    }
}

void SpectralProminenceEngineV3::buildGrid(int ppo, std::vector<int>& gridBinOut, std::vector<int>& binLoOut, std::vector<int>& binHiOut, std::vector<float>& binFracOut, std::vector<float>& gridValueScratch)
{
    const double fLow = 20.0, fHigh = juce::jmax(fLow * 1.01, sampleRate * 0.5);
    int numPoints = juce::jmax(2, (int) std::round(ppo * std::log2(fHigh / fLow)) + 1);
    gridBinOut.assign((size_t) numPoints, 0);
    for (int g = 0; g < numPoints; ++g)
    {
        double freq = fLow * std::pow(2.0, (double) g / ppo);
        int b = (int) std::round(freq * fftSize / sampleRate);
        gridBinOut[(size_t) g] = juce::jlimit(0, bins - 1, b);
    }
    gridValueScratch.assign((size_t) numPoints, 0.0f);
    binLoOut.assign((size_t) bins, 0); binHiOut.assign((size_t) bins, 0); binFracOut.assign((size_t) bins, 0.0f);
    for (int i = 0; i < bins; ++i)
    {
        double f = juce::jmax(1.0, (double) i * sampleRate / fftSize);
        double g = ppo * std::log2(juce::jmax(f, fLow) / fLow);
        int gLo = juce::jlimit(0, numPoints - 1, (int) std::floor(g));
        int gHi = juce::jlimit(0, numPoints - 1, gLo + 1);
        float frac = (gHi > gLo) ? juce::jlimit(0.0f, 1.0f, (float) (g - gLo)) : 0.0f;
        binLoOut[(size_t) i] = gLo; binHiOut[(size_t) i] = gHi; binFracOut[(size_t) i] = frac;
    }
}

void SpectralProminenceEngineV3::recomputeGeometry()
{
    computeCoreContextFor(narrowP.coreOctaves, narrowP.contextOctaves, coreR[Narrow], ctxR[Narrow], coreLimited[Narrow], ctxLimited[Narrow]);
    computeCoreContextFor(mediumP.coreOctaves, mediumP.contextOctaves, coreR[Medium], ctxR[Medium], coreLimited[Medium], ctxLimited[Medium]);
    computeCoreContextFor(broadP.coreOctaves,  broadP.contextOctaves,  coreR[Broad],  ctxR[Broad],  coreLimited[Broad],  ctxLimited[Broad]);
    buildGrid(broadPPO, broadGridBin, binBroadGridLo, binBroadGridHi, binBroadGridFrac, broadGridValue);
    buildGrid(mediumPPO, mediumGridBin, binMediumGridLo, binMediumGridHi, binMediumGridFrac, mediumGridValue);
}

void SpectralProminenceEngineV3::setScaleParams(ScaleParams narrow, ScaleParams medium, ScaleParams broad)
{
    narrowP = narrow; mediumP = medium; broadP = broad;
    if (bins > 0) recomputeGeometry();
}
void SpectralProminenceEngineV3::setMinContextBins(int n) { minContextBins = juce::jmax(1, n); if (bins > 0) recomputeGeometry(); }

void SpectralProminenceEngineV3::prepare(int numBins, double sr, int fft, int bPPO, int mPPO)
{
    sampleRate = sr; fftSize = fft; bins = numBins; broadPPO = juce::jmax(4, bPPO); mediumPPO = juce::jmax(4, mPPO);
    prefixSum.assign((size_t) bins + 1, 0.0f);
    prefixSumSq.assign((size_t) bins + 1, 0.0f);
    narrowCheap.assign((size_t) bins, 0.0f);
    narrowFinal.assign((size_t) bins, 0.0f);
    narrowCandidateIdx.reserve((size_t) bins);
    recomputeGeometry();
    int maxRadius = 0;
    for (int i = 0; i < bins; ++i) maxRadius = juce::jmax(maxRadius, ctxR[Narrow][(size_t) i]);
    robustScratch.assign((size_t) (maxRadius * 2 + 4), 0.0f);
}

SpectralProminenceEngineV3::ScaleGeometryInfo SpectralProminenceEngineV3::scaleGeometryAt(int bin, Scale s) const
{
    ScaleGeometryInfo g;
    if (bin < 0 || bin >= bins) return g;
    g.coreRadius = coreR[(size_t) s][(size_t) bin]; g.contextRadius = ctxR[(size_t) s][(size_t) bin];
    g.coreResolutionLimited = coreLimited[(size_t) s][(size_t) bin] != 0;
    g.contextResolutionLimited = ctxLimited[(size_t) s][(size_t) bin] != 0;
    return g;
}

void SpectralProminenceEngineV3::blendWeights(float sharpness, float& wN, float& wM, float& wB)
{
    float s = juce::jlimit(0.0f, 10.0f, sharpness) / 10.0f;
    wN = s * s; wB = (1.0f - s) * (1.0f - s); wM = juce::jmax(0.0f, 1.0f - wN - wB);
}

float SpectralProminenceEngineV3::cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    float sumCtx = prefixSum[(size_t) (ctxB + 1)] - prefixSum[(size_t) ctxA];
    float sumCore = prefixSum[(size_t) (coreB + 1)] - prefixSum[(size_t) coreA];
    int count = (ctxB - ctxA + 1) - (coreB - coreA + 1);
    if (count <= 0) return prefixSum[(size_t) (bin + 1)] - prefixSum[(size_t) bin];
    return (sumCtx - sumCore) / (float) count;
}

float SpectralProminenceEngineV3::zscoreTrimmedMeanExcludingCore(int bin, int coreRadius, int contextRadius) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    float sumCtx = prefixSum[(size_t) (ctxB + 1)] - prefixSum[(size_t) ctxA];
    float sumCore = prefixSum[(size_t) (coreB + 1)] - prefixSum[(size_t) coreA];
    float sqCtx = prefixSumSq[(size_t) (ctxB + 1)] - prefixSumSq[(size_t) ctxA];
    float sqCore = prefixSumSq[(size_t) (coreB + 1)] - prefixSumSq[(size_t) coreA];
    int count = (ctxB - ctxA + 1) - (coreB - coreA + 1);
    if (count <= 1) return cheapMeanExcludingCore(bin, coreRadius, contextRadius);
    float mean = (sumCtx - sumCore) / (float) count;
    float meanSq = (sqCtx - sqCore) / (float) count;
    float var = juce::jmax(0.0f, meanSq - mean * mean);
    float sd = std::sqrt(var);
    float lo = mean - 1.5f * sd, hi = mean + 1.5f * sd;
    // Second pass, no sort: accumulate only in-range values directly from magDb via prefixSum deltas
    // is not possible analytically once we clip, so scan the (small, grid-point-only) window directly.
    double sum2 = 0; int n2 = 0;
    for (int i = ctxA; i <= ctxB; ++i)
    {
        if (i >= coreA && i <= coreB) continue;
        float v = prefixSum[(size_t) (i + 1)] - prefixSum[(size_t) i];
        if (v >= lo && v <= hi) { sum2 += v; ++n2; }
    }
    return n2 > 0 ? (float) (sum2 / n2) : mean;
}

float SpectralProminenceEngineV3::winsorizedMeanExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    thread_local std::vector<float> scratch; scratch.clear();
    for (int i = ctxA; i <= ctxB; ++i) if (i < coreA || i > coreB) scratch.push_back(magDb[(size_t) i]);
    if (scratch.empty()) return magDb[(size_t) bin];
    std::sort(scratch.begin(), scratch.end());
    int n = (int) scratch.size();
    int loIdx = juce::jlimit(0, n - 1, (int) (n * 0.10f));
    int hiIdx = juce::jlimit(0, n - 1, (int) (n * 0.90f));
    float loVal = scratch[(size_t) loIdx], hiVal = scratch[(size_t) hiIdx];
    double sum = 0;
    for (float v : scratch) sum += juce::jlimit(loVal, hiVal, v);
    return (float) (sum / n);
}

float SpectralProminenceEngineV3::robustLinearRegressionExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius) const
{
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(bins - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(bins - 1, bin + coreRadius);
    double sw = 0, swx = 0, swy = 0, swxx = 0, swxy = 0;
    for (int i = ctxA; i <= ctxB; ++i)
    {
        if (i >= coreA && i <= coreB) continue;
        double x = i - bin;
        float d = (float) std::abs(i - bin) / (float) juce::jmax(1, contextRadius);
        double w = juce::jmax(0.0f, 1.0f - d);
        double y = magDb[(size_t) i];
        sw += w; swx += w * x; swy += w * y; swxx += w * x * x; swxy += w * x * y;
    }
    double denom = sw * swxx - swx * swx;
    if (std::abs(denom) < 1e-9) return sw > 1e-9 ? (float) (swy / sw) : magDb[(size_t) bin];
    double beta = (sw * swxy - swx * swy) / denom;
    double alpha = (swy - beta * swx) / sw;
    return (float) alpha;
}

float SpectralProminenceEngineV3::robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const
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

void SpectralProminenceEngineV3::computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);

    prefixSum[0] = 0.0f; prefixSumSq[0] = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        prefixSum[(size_t) (i + 1)] = prefixSum[(size_t) i] + magDb[(size_t) i];
        prefixSumSq[(size_t) (i + 1)] = prefixSumSq[(size_t) i] + magDb[(size_t) i] * magDb[(size_t) i];
    }

    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);

    // ---- BROAD: grid + interpolate. Predictable, content-independent cost. ----
    for (size_t g = 0; g < broadGridBin.size(); ++g)
    {
        int b = broadGridBin[g];
        int cr = coreR[Broad][(size_t) b], xr = ctxR[Broad][(size_t) b];
        float baseline;
        switch (broadMethod)
        {
            case BroadMethod::UnweightedMean:        baseline = cheapMeanExcludingCore(b, cr, xr); break;
            case BroadMethod::WinsorizedMean:         baseline = winsorizedMeanExcludingCore(magDb, b, cr, xr); break;
            case BroadMethod::ZScoreTrimmedMean:      baseline = zscoreTrimmedMeanExcludingCore(b, cr, xr); break;
            case BroadMethod::RobustLinearRegression: default: baseline = robustLinearRegressionExcludingCore(magDb, b, cr, xr); break;
        }
        broadGridValue[g] = magDb[(size_t) b] - baseline;
    }

    // ---- MEDIUM: grid+cheap OR full-bin P25 (predictable either way -- no candidate gating here). ----
    std::vector<float>* mediumPerBin = nullptr; // filled below depending on method
    static thread_local std::vector<float> mediumFullBin;
    if (mediumMethod == MediumMethod::CheapMean)
    {
        for (size_t g = 0; g < mediumGridBin.size(); ++g)
        {
            int b = mediumGridBin[g];
            float baseline = cheapMeanExcludingCore(b, coreR[Medium][(size_t) b], ctxR[Medium][(size_t) b]);
            mediumGridValue[g] = magDb[(size_t) b] - baseline;
        }
    }
    else
    {
        mediumFullBin.resize((size_t) n);
        for (int i = 0; i < n; ++i)
            mediumFullBin[(size_t) i] = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[Medium][(size_t) i], ctxR[Medium][(size_t) i], robustScratch);
        mediumPerBin = &mediumFullBin;
    }

    // ---- NARROW: cheap everywhere, budgeted robust refinement on top-K candidates. ----
    for (int i = 0; i < n; ++i)
        narrowCheap[(size_t) i] = magDb[(size_t) i] - cheapMeanExcludingCore(i, coreR[Narrow][(size_t) i], ctxR[Narrow][(size_t) i]);
    narrowFinal = narrowCheap; // cheap estimate ALWAYS available -- never zeroed for non-candidates

    narrowCandidateIdx.clear();
    float lo = (float) (narrowThresholdDb - narrowMarginDb * 0.5);
    for (int i = 0; i < n; ++i)
        if (narrowCheap[(size_t) i] > lo) narrowCandidateIdx.push_back(i);
    lastNarrowCandidates = (int) narrowCandidateIdx.size();

    int refineCount = juce::jmin((int) narrowCandidateIdx.size(), narrowBudget);
    if (refineCount > 0)
    {
        std::partial_sort(narrowCandidateIdx.begin(), narrowCandidateIdx.begin() + refineCount, narrowCandidateIdx.end(),
            [&](int a, int b) { return narrowCheap[(size_t) a] > narrowCheap[(size_t) b]; });
        for (int k = 0; k < refineCount; ++k)
        {
            int i = narrowCandidateIdx[(size_t) k];
            narrowFinal[(size_t) i] = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[Narrow][(size_t) i], ctxR[Narrow][(size_t) i], robustScratch);
        }
    }
    lastNarrowRefined = refineCount;

    // ---- Interpolate BROAD/MEDIUM grids onto real bins + blend all 3 scales ----
    for (int i = 0; i < n; ++i)
    {
        float pB = juce::jmap(binBroadGridFrac[(size_t) i], broadGridValue[(size_t) binBroadGridLo[(size_t) i]], broadGridValue[(size_t) binBroadGridHi[(size_t) i]]);
        float pM = (mediumPerBin != nullptr) ? (*mediumPerBin)[(size_t) i]
            : juce::jmap(binMediumGridFrac[(size_t) i], mediumGridValue[(size_t) binMediumGridLo[(size_t) i]], mediumGridValue[(size_t) binMediumGridHi[(size_t) i]]);
        float pN = narrowFinal[(size_t) i];
        prominenceOut[(size_t) i] = wN * pN + wM * pM + wB * pB;
    }
}
