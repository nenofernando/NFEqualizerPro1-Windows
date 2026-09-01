// SONIC ALPHA CALIBRATION 1 -- audit of the REAL actionWeight/action/
// requestedPeakDb distribution across musical material, at Selectivity=3.5,
// Depth=3/5, gamma=1.0 (current linear mapping) vs candidate gammas.
// Drives GainMaskEngine directly (same class the plugin uses) -- no
// PHYSICAL C/D file is touched or modified by this file.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp, int startSample = 0, int endSample = -1)
{
    if (endSample < 0) endSample = (int) b.size();
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < (int) b.size(); ++i) { if (i >= startSample && i < endSample) b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static void addDecayingResonance(std::vector<float>& b, double sr, double freqHz, float amp, double decaySeconds, int startSample)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freqHz / sr; int n = (int) b.size();
    for (int i = startSample; i < n; ++i) { double t = (double) (i - startSample) / sr; float env = (float) std::exp(-t / decaySeconds); b[(size_t) i] += (float) std::sin(ph) * amp * env; ph += inc; }
}
static void addClick(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i)
    { float env = (float) std::exp(-(double) i / (lenSamples * 0.3)); b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp * env; }
}
static void addNoiseBurst(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp;
}

struct Sample { float actionWeight, action, requestedPeakDb; };

static float pct(std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; }

static std::vector<Sample> runAudit(const std::vector<float>& sig, double sr, float depth, float selectivity, float gamma, float* outMaxAppliedDb = nullptr, float* outMeanAppliedDb = nullptr, int* outActiveBins = nullptr)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(depth, selectivity, 10.0f, 80.0f, 20.0f, 20000.0f);
    mask.setActionShapeGamma(gamma);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));

    std::vector<Sample> samples;
    int n = (int) sig.size();
    double sumAppliedMin = 0.0; float worstApplied = 0.0f; int frames = 0; long activeSum = 0;
    std::vector<float> history((size_t) kFft, 0.0f); int histPos = 0;
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
          magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
        mask.process(magDb, sig.data() + i, kHop, reductionOut);

        for (auto& d : mask.lastRegionActionDebug())
            if (d.active && d.action > 1.0e-4f) samples.push_back({ d.actionWeight, d.action, d.requestedPeakDb });

        float mn = 0; int active = 0;
        for (float v : reductionOut) { mn = juce::jmin(mn, v); if (v < -0.05f) ++active; }
        sumAppliedMin += mn; worstApplied = juce::jmin(worstApplied, mn); activeSum += active; ++frames;
    }
    if (outMaxAppliedDb) *outMaxAppliedDb = worstApplied;
    if (outMeanAppliedDb) *outMeanAppliedDb = frames ? (float) (sumAppliedMin / frames) : 0.0f;
    if (outActiveBins) *outActiveBins = frames ? (int) (activeSum / frames) : 0;
    return samples;
}

static void reportDistribution(const char* label, const std::vector<Sample>& s)
{
    if (s.empty()) { std::printf("  %s: NO ACTIVE SAMPLES\n", label); return; }
    std::vector<float> aw, ac, rq;
    for (auto& x : s) { aw.push_back(x.actionWeight); ac.push_back(x.action); rq.push_back(x.requestedPeakDb); }
    std::printf("  %s (n=%d region-frame samples):\n", label, (int) s.size());
    std::printf("    actionWeight   P50=%.3f P90=%.3f max=%.3f\n", pct(aw,50), pct(aw,90), *std::max_element(aw.begin(),aw.end()));
    std::printf("    action(final)  P50=%.3f P90=%.3f max=%.3f\n", pct(ac,50), pct(ac,90), *std::max_element(ac.begin(),ac.end()));
    std::printf("    requestedPeak  P50=%.2fdB P90=%.2fdB max=%.2fdB\n", pct(rq,50), pct(rq,90), *std::min_element(rq.begin(),rq.end()));
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 1.5);
    auto genBass = [&](double s, int nn){ auto b = genHarmonicSeries(s, nn, 55.0, 0.3f, 8, 2.0f); addClick(b, 0, (int)(s*0.004), 0.5f, 6); return b; };
    auto genVocal = [&](double s, int nn){ auto b = genHarmonicSeries(s, nn, 140.0, 0.25f, 8, 2.5f); addNoiseBurst(b, (int)(s*0.1), (int)(s*0.05), 0.3f, 4); return b; };
    auto genGuitar = [&](double s, int nn){ auto b = genHarmonicSeries(s, nn, 110.0, 0.25f, 8, 2.5f); addClick(b, 0, (int)(s*0.003), 0.5f, 5); return b; };
    auto genDense = [&](double s, int nn){
        auto b = genHarmonicSeries(s, nn, 62.0, 0.18f, 6, 2.5f);
        auto vocal = genHarmonicSeries(s, nn, 220.0, 0.13f, 8, 2.5f); for (size_t i=0;i<b.size();++i) b[i]+=vocal[i];
        addClick(b, (int)(s*0.05), (int)(s*0.003), 0.6f, 9); addDecayingResonance(b, s, 60.0, 0.5f, 0.2, (int)(s*0.05));
        addNoiseBurst(b, (int)(s*0.4), (int)(s*0.02), 0.35f, 10);
        return b; };

    std::printf("=== SONIC ALPHA CALIBRATION 1: actionWeight audit, Selectivity=3.5 ===\n");
    for (float depth : { 3.0f, 5.0f })
    {
        std::printf("\n########## Depth=%.0f, gamma=1.0 (CURRENT/original mapping) ##########\n", depth);
        float maxDb, meanDb; int activeBins;
        auto sBass = runAudit(genBass(sr, n), sr, depth, 3.5f, 1.0f, &maxDb, &meanDb, &activeBins);
        std::printf("  [Bass]   maxApplied=%.2fdB meanApplied=%.2fdB avgActiveBins=%d\n", maxDb, meanDb, activeBins); reportDistribution("Bass", sBass);
        auto sVocal = runAudit(genVocal(sr, n), sr, depth, 3.5f, 1.0f, &maxDb, &meanDb, &activeBins);
        std::printf("  [Vocal]  maxApplied=%.2fdB meanApplied=%.2fdB avgActiveBins=%d\n", maxDb, meanDb, activeBins); reportDistribution("Vocal", sVocal);
        auto sGuitar = runAudit(genGuitar(sr, n), sr, depth, 3.5f, 1.0f, &maxDb, &meanDb, &activeBins);
        std::printf("  [Guitar] maxApplied=%.2fdB meanApplied=%.2fdB avgActiveBins=%d\n", maxDb, meanDb, activeBins); reportDistribution("Guitar", sGuitar);
        auto sDense = runAudit(genDense(sr, n), sr, depth, 3.5f, 1.0f, &maxDb, &meanDb, &activeBins);
        std::printf("  [Dense]  maxApplied=%.2fdB meanApplied=%.2fdB avgActiveBins=%d\n", maxDb, meanDb, activeBins); reportDistribution("Dense", sDense);
    }

    std::printf("\n=== GAMMA SWEEP (Selectivity=3.5, Depth=3 and 5) ===\n");
    for (float depth : { 3.0f, 5.0f })
    {
        std::printf(" -- Depth=%.0f --\n", depth);
        for (float gamma : { 1.0f, 0.7f, 0.55f, 0.4f })
        {
            float vMax, vMean, dMax, dMean; int activeBins;
            (void) runAudit(genVocal(sr, n), sr, depth, 3.5f, gamma, &vMax, &vMean, &activeBins);
            (void) runAudit(genDense(sr, n), sr, depth, 3.5f, gamma, &dMax, &dMean, &activeBins);
            std::printf("   gamma=%.2f: Vocal maxApplied=%.2fdB meanApplied=%.2fdB | Dense maxApplied=%.2fdB meanApplied=%.2fdB\n",
                gamma, vMax, vMean, dMax, dMean);
        }
    }

    return 0;
}
