#include "SpectralProminenceEngineV2.h"
#include <algorithm>

void SpectralProminenceEngineV2::setScaleParams(ScaleParams narrow, ScaleParams medium, ScaleParams broad)
{
    narrowP = narrow; mediumP = medium; broadP = broad;
    if (bins > 0) recomputeGeometry();
}

void SpectralProminenceEngineV2::setMinContextBins(int n)
{
    minContextBins = juce::jmax(1, n);
    if (bins > 0) recomputeGeometry();
}

void SpectralProminenceEngineV2::computeCoreContextFor(double coreOct, double contextOct,
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
        coreOut[(size_t) i] = core;
        ctxOut[(size_t) i] = ctx;
        coreLimOut[(size_t) i] = desiredCore < 1 ? 1 : 0;
        // context is "resolution limited" if we had to force it wider than the
        // desired octave width just to guarantee minContextBins of real context.
        ctxLimOut[(size_t) i] = (desiredCtx < core + minContextBins) ? 1 : 0;
    }
}

void SpectralProminenceEngineV2::recomputeGeometry()
{
    computeCoreContextFor(narrowP.coreOctaves, narrowP.contextOctaves, coreR[Narrow], ctxR[Narrow], coreLimited[Narrow], ctxLimited[Narrow]);
    computeCoreContextFor(mediumP.coreOctaves, mediumP.contextOctaves, coreR[Medium], ctxR[Medium], coreLimited[Medium], ctxLimited[Medium]);
    computeCoreContextFor(broadP.coreOctaves,  broadP.contextOctaves,  coreR[Broad],  ctxR[Broad],  coreLimited[Broad],  ctxLimited[Broad]);

    // Auxiliary log-frequency grid.
    const double fLow = 20.0, fHigh = juce::jmax(fLow * 1.01, sampleRate * 0.5);
    int numPoints = juce::jmax(2, (int) std::round(pointsPerOctave * std::log2(fHigh / fLow)) + 1);
    gridBin.assign((size_t) numPoints, 0);
    for (int g = 0; g < numPoints; ++g)
    {
        double freq = fLow * std::pow(2.0, (double) g / pointsPerOctave);
        int b = (int) std::round(freq * fftSize / sampleRate);
        gridBin[(size_t) g] = juce::jlimit(0, bins - 1, b);
    }
    for (int s = 0; s < 3; ++s) gridCheapProm[(size_t) s].assign((size_t) numPoints, 0.0f);

    binGridLo.assign((size_t) bins, 0); binGridHi.assign((size_t) bins, 0); binGridFrac.assign((size_t) bins, 0.0f);
    for (int i = 0; i < bins; ++i)
    {
        double f = juce::jmax(1.0, (double) i * sampleRate / fftSize);
        double g = pointsPerOctave * std::log2(juce::jmax(f, fLow) / fLow);
        int gLo = juce::jlimit(0, numPoints - 1, (int) std::floor(g));
        int gHi = juce::jlimit(0, numPoints - 1, gLo + 1);
        float frac = (gHi > gLo) ? juce::jlimit(0.0f, 1.0f, (float) (g - gLo)) : 0.0f;
        binGridLo[(size_t) i] = gLo; binGridHi[(size_t) i] = gHi; binGridFrac[(size_t) i] = frac;
    }
}

void SpectralProminenceEngineV2::prepare(int numBins, double sr, int fft, int ppo)
{
    sampleRate = sr; fftSize = fft; bins = numBins; pointsPerOctave = juce::jmax(4, ppo);
    prefixSum.assign((size_t) bins + 1, 0.0f);
    binCandidateWeight.assign((size_t) bins, 0.0f);
    for (int s = 0; s < 3; ++s) binCheapProm[(size_t) s].assign((size_t) bins, 0.0f);
    recomputeGeometry();
    int maxRadius = 0;
    for (int s = 0; s < 3; ++s) for (int i = 0; i < bins; ++i) maxRadius = juce::jmax(maxRadius, ctxR[(size_t) s][(size_t) i]);
    robustScratch.assign((size_t) (maxRadius * 2 + 4), 0.0f);
}

SpectralProminenceEngineV2::ScaleGeometryInfo SpectralProminenceEngineV2::scaleGeometryAt(int bin, Scale s) const
{
    ScaleGeometryInfo g;
    if (bin < 0 || bin >= bins) return g;
    g.coreRadius = coreR[(size_t) s][(size_t) bin]; g.contextRadius = ctxR[(size_t) s][(size_t) bin];
    g.coreResolutionLimited = coreLimited[(size_t) s][(size_t) bin] != 0;
    g.contextResolutionLimited = ctxLimited[(size_t) s][(size_t) bin] != 0;
    return g;
}

void SpectralProminenceEngineV2::blendWeights(float sharpness, float& wN, float& wM, float& wB)
{
    float s = juce::jlimit(0.0f, 10.0f, sharpness) / 10.0f;
    wN = s * s; wB = (1.0f - s) * (1.0f - s); wM = juce::jmax(0.0f, 1.0f - wN - wB);
}

float SpectralProminenceEngineV2::cheapMeanExcludingCore(int bin, int coreRadius, int contextRadius) const
{
    const int n = bins;
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(n - 1, bin + contextRadius);
    int coreA = juce::jmax(0, bin - coreRadius), coreB = juce::jmin(n - 1, bin + coreRadius);
    float sumCtx = prefixSum[(size_t) (ctxB + 1)] - prefixSum[(size_t) ctxA];
    float sumCore = prefixSum[(size_t) (coreB + 1)] - prefixSum[(size_t) coreA];
    int countCtx = ctxB - ctxA + 1, countCore = coreB - coreA + 1;
    int count = countCtx - countCore;
    if (count <= 0) return prefixSum[(size_t) (bin + 1)] - prefixSum[(size_t) bin]; // degenerate: just this bin's own value
    return (sumCtx - sumCore) / (float) count;
}

float SpectralProminenceEngineV2::smoothedSignalAt(const std::vector<float>& magDb, int bin) const
{
    const int n = (int) magDb.size();
    auto at = [&](int i) { return magDb[(size_t) juce::jlimit(0, n - 1, i)]; };
    switch (signalSmoothMethod)
    {
        case SignalSmoothMethod::None: return magDb[(size_t) bin];
        case SignalSmoothMethod::MovingAvg3: return (at(bin-1) + at(bin) + at(bin+1)) / 3.0f;
        case SignalSmoothMethod::MovingAvg5: return (at(bin-2)+at(bin-1)+at(bin)+at(bin+1)+at(bin+2)) / 5.0f;
        case SignalSmoothMethod::Triangular3: return (at(bin-1) + 2.0f*at(bin) + at(bin+1)) / 4.0f;
        case SignalSmoothMethod::Triangular5: return (at(bin-2) + 2.0f*at(bin-1) + 3.0f*at(bin) + 2.0f*at(bin+1) + at(bin+2)) / 9.0f;
        case SignalSmoothMethod::Median3: default:
        {
            float a = at(bin-1), b = at(bin), c = at(bin+1);
            return juce::jmax(juce::jmin(a,b), juce::jmin(juce::jmax(a,b),c));
        }
    }
}

float SpectralProminenceEngineV2::robustPercentileExcludingCore(const std::vector<float>& magDb, int bin, int coreRadius, int contextRadius, std::vector<float>& scratch) const
{
    const int n = (int) magDb.size();
    int ctxA = juce::jmax(0, bin - contextRadius), ctxB = juce::jmin(n - 1, bin + contextRadius);
    scratch.clear();
    for (int i = ctxA; i <= ctxB; ++i)
        if (std::abs(i - bin) > coreRadius) scratch.push_back(magDb[(size_t) i]);
    if (scratch.empty()) return magDb[(size_t) bin];
    int count = (int) scratch.size();
    int idx = juce::jlimit(0, count - 1, (int) (count * (float) percentile));
    std::nth_element(scratch.begin(), scratch.begin() + idx, scratch.begin() + count);
    return scratch[(size_t) idx];
}

void SpectralProminenceEngineV2::computeProminenceHybrid(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);

    prefixSum[0] = 0.0f;
    for (int i = 0; i < n; ++i) prefixSum[(size_t) (i + 1)] = prefixSum[(size_t) i] + magDb[(size_t) i];

    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);

    // Stage 1: cheap pass at grid points only.
    for (int s = 0; s < 3; ++s)
    {
        auto& gp = gridCheapProm[(size_t) s];
        for (size_t g = 0; g < gridBin.size(); ++g)
        {
            int b = gridBin[g];
            float baseline = cheapMeanExcludingCore(b, coreR[(size_t) s][(size_t) b], ctxR[(size_t) s][(size_t) b]);
            gp[g] = smoothedSignalAt(magDb, b) - baseline; // stage 1 only -- stage 3 refinement reads raw magDb
        }
    }

    // Stage 2: interpolate onto real bins + soft candidate gate.
    lastCandidateCount = 0;
    for (int i = 0; i < n; ++i)
    {
        int gLo = binGridLo[(size_t) i], gHi = binGridHi[(size_t) i]; float frac = binGridFrac[(size_t) i];
        float pN = juce::jmap(frac, gridCheapProm[0][(size_t) gLo], gridCheapProm[0][(size_t) gHi]);
        float pM = juce::jmap(frac, gridCheapProm[1][(size_t) gLo], gridCheapProm[1][(size_t) gHi]);
        float pB = juce::jmap(frac, gridCheapProm[2][(size_t) gLo], gridCheapProm[2][(size_t) gHi]);
        binCheapProm[0][(size_t) i] = pN; binCheapProm[1][(size_t) i] = pM; binCheapProm[2][(size_t) i] = pB;
        float cheapCombined = wN * pN + wM * pM + wB * pB;

        float lo = (float) (candidateThresholdDb - candidateMarginDb * 0.5);
        float t = juce::jlimit(0.0f, 1.0f, (cheapCombined - lo) / (float) candidateMarginDb);
        float weight = t * t * (3.0f - 2.0f * t); // smoothstep
        binCandidateWeight[(size_t) i] = weight;
        if (weight > 0.01f) ++lastCandidateCount;
    }

    // Stage 3: refine candidates at full per-bin resolution; blend by weight.
    for (int i = 0; i < n; ++i)
    {
        float weight = binCandidateWeight[(size_t) i];
        float cheapCombined = wN * binCheapProm[0][(size_t) i] + wM * binCheapProm[1][(size_t) i] + wB * binCheapProm[2][(size_t) i];
        if (weight <= 0.01f) { prominenceOut[(size_t) i] = cheapCombined; continue; }

        float rN = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[0][(size_t) i], ctxR[0][(size_t) i], robustScratch);
        float rM = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[1][(size_t) i], ctxR[1][(size_t) i], robustScratch);
        float rB = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[2][(size_t) i], ctxR[2][(size_t) i], robustScratch);
        float robustCombined = wN * rN + wM * rM + wB * rB;

        prominenceOut[(size_t) i] = cheapCombined * (1.0f - weight) + robustCombined * weight;
    }
}

void SpectralProminenceEngineV2::computeProminenceFastApprox(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);
    prefixSum[0] = 0.0f;
    for (int i = 0; i < n; ++i) prefixSum[(size_t) (i + 1)] = prefixSum[(size_t) i] + magDb[(size_t) i];
    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);
    for (int i = 0; i < n; ++i)
    {
        float pN = magDb[(size_t) i] - cheapMeanExcludingCore(i, coreR[0][(size_t) i], ctxR[0][(size_t) i]);
        float pM = magDb[(size_t) i] - cheapMeanExcludingCore(i, coreR[1][(size_t) i], ctxR[1][(size_t) i]);
        float pB = magDb[(size_t) i] - cheapMeanExcludingCore(i, coreR[2][(size_t) i], ctxR[2][(size_t) i]);
        prominenceOut[(size_t) i] = wN * pN + wM * pM + wB * pB;
    }
}

void SpectralProminenceEngineV2::computeProminenceFullBinReference(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);
    float wN, wM, wB; blendWeights(sharpness, wN, wM, wB);
    for (int i = 0; i < n; ++i)
    {
        float rN = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[0][(size_t) i], ctxR[0][(size_t) i], robustScratch);
        float rM = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[1][(size_t) i], ctxR[1][(size_t) i], robustScratch);
        float rB = magDb[(size_t) i] - robustPercentileExcludingCore(magDb, i, coreR[2][(size_t) i], ctxR[2][(size_t) i], robustScratch);
        prominenceOut[(size_t) i] = wN * rN + wM * rM + wB * rB;
    }
}
