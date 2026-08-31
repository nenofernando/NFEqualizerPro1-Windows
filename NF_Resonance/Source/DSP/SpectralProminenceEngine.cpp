#include "SpectralProminenceEngine.h"
#include <algorithm>

void SpectralProminenceEngine::recomputeRadii()
{
    auto computeFor = [&](double octaves, std::vector<int>& radiusOut, std::vector<uint8_t>& limitedOut)
    {
        for (int i = 0; i < bins; ++i)
        {
            // desiredLowHz/desiredHighHz -> binLow/binHigh -> minimum-bin floor,
            // never mixing octave units with raw bin counts directly.
            double fCenter = juce::jmax(1.0, (double) i * sampleRate / fftSize);
            double fLow  = fCenter * std::pow(2.0, -octaves / 2.0);
            double fHigh = fCenter * std::pow(2.0,  octaves / 2.0);
            int binLow  = (int) std::round(fLow  * fftSize / sampleRate);
            int binHigh = (int) std::round(fHigh * fftSize / sampleRate);
            int desiredRadius = juce::jmax(i - binLow, binHigh - i);
            int radius = juce::jmax(minRadiusBins, desiredRadius);
            radiusOut[(size_t) i] = radius;
            limitedOut[(size_t) i] = desiredRadius < minRadiusBins ? 1 : 0;
        }
    };
    computeFor(narrowOctaves, radiusNarrow, narrowLimited);
    computeFor(mediumOctaves, radiusMedium, mediumLimited);
    computeFor(broadOctaves,  radiusBroad,  broadLimited);
}

void SpectralProminenceEngine::prepare(int numBins, double sr, int fft)
{
    sampleRate = sr; fftSize = fft; bins = numBins;
    radiusNarrow.assign((size_t) bins, 0); radiusMedium.assign((size_t) bins, 0); radiusBroad.assign((size_t) bins, 0);
    narrowLimited.assign((size_t) bins, 0); mediumLimited.assign((size_t) bins, 0); broadLimited.assign((size_t) bins, 0);
    recomputeRadii();
    int maxRadius = 0;
    for (int i = 0; i < bins; ++i)
        maxRadius = juce::jmax(maxRadius, juce::jmax(radiusNarrow[(size_t)i], juce::jmax(radiusMedium[(size_t)i], radiusBroad[(size_t)i])));
    window.assign((size_t) (maxRadius * 2 + 4), 0.0f);
    promNarrow.assign((size_t) bins, 0.0f); promMedium.assign((size_t) bins, 0.0f); promBroad.assign((size_t) bins, 0.0f);
}

void SpectralProminenceEngine::setScaleWidthsOctaves(double narrowOct, double mediumOct, double broadOct)
{
    narrowOctaves = narrowOct; mediumOctaves = mediumOct; broadOctaves = broadOct;
    if (bins > 0) recomputeRadii();
}

SpectralProminenceEngine::ScaleInfo SpectralProminenceEngine::scaleInfoAt(int bin) const
{
    ScaleInfo s;
    if (bin < 0 || bin >= bins) return s;
    s.radiusNarrow = radiusNarrow[(size_t) bin]; s.radiusMedium = radiusMedium[(size_t) bin]; s.radiusBroad = radiusBroad[(size_t) bin];
    s.narrowResolutionLimited = narrowLimited[(size_t) bin] != 0;
    s.mediumResolutionLimited = mediumLimited[(size_t) bin] != 0;
    s.broadResolutionLimited  = broadLimited[(size_t) bin] != 0;
    return s;
}

float SpectralProminenceEngine::baselineAt(const std::vector<float>& magDb, int bin, int radius, BaselineMethod m, double gapFraction, double percentile, std::vector<float>& scratch)
{
    const int n = (int) magDb.size();
    const int a = juce::jmax(0, bin - radius), b = juce::jmin(n - 1, bin + radius);
    // Exclusion gap: the innermost `innerRadius` bins on each side of `bin`
    // are skipped, so a real resonance's own energy can't bias its own
    // baseline. If the gap would remove every candidate, fall back to no gap
    // (better a slightly biased estimate than none at all -- happens only at
    // the smallest, resolution-floored radii).
    int innerRadius = (int) std::round(radius * gapFraction);
    bool anyOutsideGap = false;
    for (int i = a; i <= b; ++i) if (std::abs(i - bin) > innerRadius) { anyOutsideGap = true; break; }
    if (! anyOutsideGap) innerRadius = -1; // disables the gap for this call

    auto inGap = [&](int i) { return innerRadius >= 0 && std::abs(i - bin) <= innerRadius; };

    switch (m)
    {
        case BaselineMethod::Median:
        {
            scratch.clear();
            for (int i = a; i <= b; ++i) if (! inGap(i)) scratch.push_back(magDb[(size_t) i]);
            int count = (int) scratch.size();
            if (count == 0) return magDb[(size_t) bin];
            int mid = count / 2;
            std::nth_element(scratch.begin(), scratch.begin() + mid, scratch.begin() + count);
            float med = scratch[(size_t) mid];
            if (count % 2 == 0 && mid > 0)
            {
                std::nth_element(scratch.begin(), scratch.begin() + (mid - 1), scratch.begin() + mid);
                med = 0.5f * (med + scratch[(size_t) (mid - 1)]);
            }
            return med;
        }
        case BaselineMethod::TrimmedMean:
        {
            scratch.clear();
            for (int i = a; i <= b; ++i) if (! inGap(i)) scratch.push_back(magDb[(size_t) i]);
            int count = (int) scratch.size();
            if (count == 0) return magDb[(size_t) bin];
            std::sort(scratch.begin(), scratch.begin() + count);
            int trim = (int) (count * 0.15);
            int lo = trim, hi = count - trim;
            if (hi <= lo) { lo = 0; hi = count; }
            double sum = 0; for (int i = lo; i < hi; ++i) sum += scratch[(size_t) i];
            return (float) (sum / (hi - lo));
        }
        case BaselineMethod::WeightedMean:
        {
            double sumW = 0, sumWV = 0;
            for (int i = a; i <= b; ++i)
            {
                if (inGap(i)) continue;
                float d = (float) std::abs(i - bin) / (float) juce::jmax(1, radius);
                float w = juce::jmax(0.0f, 1.0f - d);
                sumW += w; sumWV += (double) w * magDb[(size_t) i];
            }
            return sumW > 1e-9 ? (float) (sumWV / sumW) : magDb[(size_t) bin];
        }
        case BaselineMethod::Percentile:
        {
            scratch.clear();
            for (int i = a; i <= b; ++i) if (! inGap(i)) scratch.push_back(magDb[(size_t) i]);
            int count = (int) scratch.size();
            if (count == 0) return magDb[(size_t) bin];
            int idx = juce::jlimit(0, count - 1, (int) (count * (float) percentile));
            std::nth_element(scratch.begin(), scratch.begin() + idx, scratch.begin() + count);
            return scratch[(size_t) idx];
        }
        case BaselineMethod::RobustLocalRegression:
        default:
        {
            double sw = 0, swx = 0, swy = 0, swxx = 0, swxy = 0;
            for (int i = a; i <= b; ++i)
            {
                if (inGap(i)) continue;
                double x = i - bin;
                float d = (float) std::abs(i - bin) / (float) juce::jmax(1, radius);
                double w = juce::jmax(0.0f, 1.0f - d);
                double y = magDb[(size_t) i];
                sw += w; swx += w * x; swy += w * y; swxx += w * x * x; swxy += w * x * y;
            }
            double denom = sw * swxx - swx * swx;
            if (std::abs(denom) < 1e-9) return sw > 1e-9 ? (float) (swy / sw) : magDb[(size_t) bin];
            double beta = (sw * swxy - swx * swy) / denom;
            double alpha = (swy - beta * swx) / sw;
            return (float) alpha; // fitted line value at the center bin (x=0)
        }
    }
}

void SpectralProminenceEngine::computeProminence(const std::vector<float>& magDb, float sharpness, std::vector<float>& prominenceOut)
{
    const int n = (int) magDb.size();
    if ((int) prominenceOut.size() != n) prominenceOut.resize((size_t) n);

    const float s = juce::jlimit(0.0f, 10.0f, sharpness) / 10.0f;
    const float wNarrow = s * s;
    const float wBroad  = (1.0f - s) * (1.0f - s);
    const float wMedium = juce::jmax(0.0f, 1.0f - wNarrow - wBroad);

    for (int i = 0; i < n; ++i)
    {
        float bNarrow = baselineAt(magDb, i, radiusNarrow[(size_t) i], method, gapFraction, percentile, window);
        float bMedium = baselineAt(magDb, i, radiusMedium[(size_t) i], method, gapFraction, percentile, window);
        float bBroad  = baselineAt(magDb, i, radiusBroad[(size_t) i],  method, gapFraction, percentile, window);
        float pNarrow = magDb[(size_t) i] - bNarrow;
        float pMedium = magDb[(size_t) i] - bMedium;
        float pBroad  = magDb[(size_t) i] - bBroad;
        prominenceOut[(size_t) i] = wNarrow * pNarrow + wMedium * pMedium + wBroad * pBroad;
    }
}
