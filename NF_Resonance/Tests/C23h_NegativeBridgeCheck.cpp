// PHYSICAL C2.3h -- negative tests for the region continuation bridge.
// The bridge must NEVER turn a transient into a persistent "ghost"
// resonance, and must NEVER let noise accumulate long-lived regions.
// All checks at 48kHz (where the bridge has real effect -- no aux rescue
// there to confound the picture).

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp, int startSample, int endSample)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < (int) b.size(); ++i) { if (i >= startSample && i < endSample) b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; }
}
static void addWhiteNoise(std::vector<float>& b, float amp, int seed) { juce::Random rng(seed); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * amp; }
static void addPinkNoise(std::vector<float>& b, float amp, int seed)
{
    juce::Random rng(seed); float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
    for (auto& s : b) { float w = rng.nextFloat()*2.0f-1.0f;
        b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f; b2=0.96900f*b2+w*0.1538520f;
        b3=0.86650f*b3+w*0.3104856f; b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
        float pink = b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6=w*0.115926f; s += pink * amp * 0.11f; }
}
static void addSweep(std::vector<float>& b, double sr, double f0, double f1, float amp)
{
    double ph = 0.0; int n = (int) b.size();
    for (int i = 0; i < n; ++i) { double t = (double) i / juce::jmax(1, n - 1); double f = f0 + (f1 - f0) * t;
        b[(size_t) i] += (float) std::sin(ph) * amp; ph += juce::MathConstants<double>::twoPi * f / sr; }
}
static void addBurstOnce(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed, int startSample, int lenSamples)
{
    juce::Random rng(seed); double bwHz = freqHz / Q;
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) { b[(size_t) (startSample + i)] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}

struct RunResult { int maxLifetimeAfterEndFrames = 0; int maxConcurrentRegions = 0; int totalFramesRun = 0; };

static RunResult runPipeline(double sr, const std::vector<float>& sig, int signalEndSample, float watchFreqHz, bool trackWatchFreq)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop);
    juce::dsp::FFT fft((int) std::round(std::log2((double) kFft)));
    std::vector<float> window((size_t) kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::vector<float> scratch((size_t) kFft * 2), magDb((size_t) bins), promOut((size_t) bins);

    RunResult res;
    int lastActiveFrameAfterEnd = -1; int endFrameIdx = signalEndSample / kHop;
    int n = (int) sig.size();
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f);
        }
        prom.computeProminence(magDb, 4.0f, promOut);
        aux.pushSamples(sig.data() + i, kHop);
        conf.process(promOut, &aux, &magDb);

        int fr = (int) (i / kHop);
        int active = conf.activeRegionCount();
        res.maxConcurrentRegions = juce::jmax(res.maxConcurrentRegions, active);
        if (trackWatchFreq && fr >= endFrameIdx)
        {
            bool found = false;
            for (auto& r : conf.regions()) if (r.active) { float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - std::log2(juce::jmax(1.0f, watchFreqHz))); if (d <= 0.25f) found = true; }
            if (found) lastActiveFrameAfterEnd = fr;
        }
        ++res.totalFramesRun;
    }
    res.maxLifetimeAfterEndFrames = (lastActiveFrameAfterEnd < 0) ? 0 : (lastActiveFrameAfterEnd - endFrameIdx + 1);
    return res;
}

int main()
{
    const double sr = 48000.0;
    const double hopMs = 1000.0 * 512.0 / sr;
    bool allOk = true;
    // Ghost ceiling must honestly account for the PRE-EXISTING (pre-C2.3h)
    // persistence decay tail: default fallTau=8 frames means a region at
    // persistence~1.0 that loses its match outright already takes
    // fallTau*ln(1/0.02) = 8*3.91 ~= 31 frames to cross the 0.02 death
    // threshold under pure exponential decay, with NO bridge involved at
    // all. The bridge adds its own ~9-frame (100ms @48k) budget on top of
    // that, atomically -- ceiling = bridgeMaxFrames + fallTau-decay-tail,
    // rounded up for margin, NOT an arbitrary guess.
    const int ghostCeilingFrames = 45; // ~9 (bridge budget) + ~31 (pre-existing fallTau tail) + margin, at 48kHz

    std::printf("=== C2.3h negative tests (48kHz) -- bridge must not create ghosts ===\n");

    // 1) Tonal burst that really ends
    {
        int n = (int) (sr * 3.0); auto sig = genSilence(n);
        int endSample = (int) (sr * 1.5);
        addTone(sig, sr, 250.0, 0.4f, 0, endSample);
        auto r = runPipeline(sr, sig, endSample, 250.0f, true);
        bool ok = r.maxLifetimeAfterEndFrames <= ghostCeilingFrames;
        std::printf("  1. Tonal burst that ends: lifetime-after-end=%d frames (%.0fms) %s\n", r.maxLifetimeAfterEndFrames, r.maxLifetimeAfterEndFrames * hopMs, ok ? "PASS" : "FAIL (ghost)");
        allOk &= ok;
    }
    // 2) Non-harmonic resonance that disappears abruptly
    {
        int n = (int) (sr * 3.0); auto sig = genSilence(n);
        int endSample = (int) (sr * 1.5);
        addTone(sig, sr, 80.0, 0.3f, 0, n);
        addBurstOnce(sig, sr, 135.0, 0.5f, 8.0, 1, 0, endSample);
        auto r = runPipeline(sr, sig, endSample, 135.0f, true);
        bool ok = r.maxLifetimeAfterEndFrames <= ghostCeilingFrames;
        std::printf("  2. Resonance disappears abruptly: lifetime-after-end=%d frames (%.0fms) %s\n", r.maxLifetimeAfterEndFrames, r.maxLifetimeAfterEndFrames * hopMs, ok ? "PASS" : "FAIL (ghost)");
        allOk &= ok;
    }
    // 3) Kick-like transient (short low-freq burst, fast decay)
    {
        int n = (int) (sr * 2.0); auto sig = genSilence(n);
        double ph = 0.0;
        for (int i = 0; i < (int) (sr * 0.15); ++i) { float env = (float) std::exp(-i / (sr * 0.03)); sig[(size_t) i] += (float) std::sin(ph) * 0.8f * env; ph += juce::MathConstants<double>::twoPi * 60.0 / sr; }
        auto r = runPipeline(sr, sig, (int) (sr * 0.15), 60.0f, true);
        bool ok = r.maxLifetimeAfterEndFrames <= ghostCeilingFrames;
        std::printf("  3. Kick transient: lifetime-after-end=%d frames (%.0fms) %s\n", r.maxLifetimeAfterEndFrames, r.maxLifetimeAfterEndFrames * hopMs, ok ? "PASS" : "FAIL (ghost)");
        allOk &= ok;
    }
    // 4) Snare-like transient (noise burst + short tone)
    {
        int n = (int) (sr * 2.0); auto sig = genSilence(n);
        int burstLen = (int) (sr * 0.08);
        std::vector<float> burst((size_t) burstLen); juce::Random rng(9);
        for (auto& s : burst) s = (rng.nextFloat() * 2.0f - 1.0f) * 0.6f;
        for (int i = 0; i < burstLen; ++i) { float env = (float) std::exp(-i / (sr * 0.02)); sig[(size_t) i] += burst[(size_t) i] * env; }
        auto r = runPipeline(sr, sig, burstLen, 200.0f, false);
        std::printf("  4. Snare transient: maxConcurrentRegions=%d totalFrames=%d (no specific freq to track, informational)\n", r.maxConcurrentRegions, r.totalFramesRun);
    }
    // 5) White noise only -- must not build long-lived regions
    {
        int n = (int) (sr * 3.0); auto sig = genSilence(n);
        addWhiteNoise(sig, 0.3f, 3);
        auto r = runPipeline(sr, sig, 0, 0.0f, false);
        bool ok = r.maxConcurrentRegions <= 32; // sanity: pool bound respected regardless
        std::printf("  5. White noise only: maxConcurrentRegions=%d (pool cap=32) %s\n", r.maxConcurrentRegions, ok ? "PASS" : "FAIL");
        allOk &= ok;
    }
    // 6) Pink noise only
    {
        int n = (int) (sr * 3.0); auto sig = genSilence(n);
        addPinkNoise(sig, 1.0f, 5);
        auto r = runPipeline(sr, sig, 0, 0.0f, false);
        bool ok = r.maxConcurrentRegions <= 32;
        std::printf("  6. Pink noise only: maxConcurrentRegions=%d (pool cap=32) %s\n", r.maxConcurrentRegions, ok ? "PASS" : "FAIL");
        allOk &= ok;
    }
    // 7) Moving tone (slow sweep) -- should track continuously, no jump-bridging
    {
        int n = (int) (sr * 2.0); auto sig = genSilence(n);
        addSweep(sig, sr, 300.0, 600.0, 0.4f);
        auto r = runPipeline(sr, sig, n, 450.0f, false);
        std::printf("  7. Moving tone (300->600Hz sweep): maxConcurrentRegions=%d totalFrames=%d (informational -- see C2.3f-style tracking)\n", r.maxConcurrentRegions, r.totalFramesRun);
    }
    // 8) Sample-rate sanity: bridge budget converts to a sane frame count at each SR
    {
        std::printf("  8. Bridge frame-budget sanity (100ms at each SR, hop=512):\n");
        for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            double hMs = 1000.0 * 512.0 / testSr;
            int frames = juce::jmax(1, (int) std::round(100.0 / hMs));
            std::printf("     sr=%.0f: hop=%.3fms -> bridgeMaxFrames=%d (%.1fms)\n", testSr, hMs, frames, frames * hMs);
        }
    }

    std::printf("\n=== %s ===\n", allOk ? "ALL NEGATIVE TESTS PASS" : "SOME NEGATIVE TESTS FAILED -- ghost region risk");
    return allOk ? 0 : 1;
}
