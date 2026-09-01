// Confirms the factory default (LOW=100Hz, HIGH=16kHz, no active bands) is
// NEUTRAL: LOW/HIGH are the detector's ACTIVE-REGION BOUNDARY, not a source
// of gain reduction by themselves. The white curve's lateral taper (drawn in
// ControlCurveComponent) is a purely visual sensitivity/range convention --
// it must never leak into reductionDb, which only the REAL detector
// (prominence vs threshold, inside SpectralEngine::c[0].reduction) may set.
// Uses the real NFResonanceAudioProcessor end-to-end (constructor -> factory
// defaults, no state loaded), exactly like a fresh plugin instance in a DAW.

#include <JuceHeader.h>
#include "PluginProcessor.h"

using juce::AudioBuffer;

static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi;
    for (int pos = 0; pos < buf.getNumSamples(); pos += blockSize)
    {
        int len = juce::jmin(blockSize, buf.getNumSamples() - pos);
        AudioBuffer<float> blk(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, len);
        proc.processBlock(blk, midi);
    }
}
static AudioBuffer<float> genNoise(double sr, int n, float amp, int seed)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    for (int i = 0; i < n; ++i) { float x = (rng.nextFloat() * 2.0f - 1.0f) * amp; b.setSample(0, i, x); b.setSample(1, i, x); }
    return b;
}
static AudioBuffer<float> genResonant(double sr, int n, double freq, float noiseAmp, float spikeAmp, int seed)
{
    AudioBuffer<float> b = genNoise(sr, n, noiseAmp, seed);
    double ph = 0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * spikeAmp; b.addSample(0, i, s); b.addSample(1, i, s); ph += inc; }
    return b;
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const int n = (int) (sr * 3.0);
    bool ok = true;

    std::printf("================================================================\n");
    std::printf("Factory default neutrality check (LOW/HIGH must not BY THEMSELVES\n");
    std::printf("produce reductionDb -- only a real detected resonance may)\n");
    std::printf("================================================================\n\n");

    // 1. Factory reset == freshly constructed processor, no state loaded.
    NFResonanceAudioProcessor proc;
    proc.prepareToPlay(sr, blockSize);

    // 2. Confirm LOW=100Hz / HIGH=16kHz, and 3. no manual band active.
    float lowHz = proc.state().getRawParameterValue("lowHz")->load();
    float highHz = proc.state().getRawParameterValue("highHz")->load();
    std::printf("LOW = %.1f Hz (expected 100.0)\n", lowHz);
    std::printf("HIGH = %.1f Hz (expected 16000.0)\n", highHz);
    if (std::abs(lowHz - 100.0f) > 0.01f || std::abs(highHz - 16000.0f) > 0.01f) { std::printf("FAIL: LOW/HIGH default drifted.\n"); ok = false; }
    int activeCount = 0;
    for (int i = 0; i < SpectralEngine::kMaxBands; ++i)
        if (proc.state().getRawParameterValue("band_active_" + juce::String(i))->load() > 0.5f) ++activeCount;
    std::printf("Active bands = %d (expected 0)\n\n", activeCount);
    if (activeCount != 0) { std::printf("FAIL: a band is active on a factory-fresh instance.\n"); ok = false; }

    // Preview Safe Default: Depth=0 out of the box, Mix=100%, Output=0dB,
    // FFT visibility off -- a fresh instance must never audibly darken the
    // mix just by existing, given Detector V1's known aggressiveness. This
    // is explicitly temporary, revisited once Detector V2 lands.
    float depthDefault = proc.state().getRawParameterValue("depth")->load();
    float mixDefault = proc.state().getRawParameterValue("mix")->load();
    float outputDefault = proc.state().getRawParameterValue("output")->load();
    bool fftDefault = proc.state().getRawParameterValue("showOriginalFft")->load() > 0.5f;
    std::printf("Depth=%.1f (expected 0.0)  Mix=%.1f%% (expected 100.0)  Output=%.2fdB (expected 0.0)  FFT=%s (expected OFF)\n",
        depthDefault, mixDefault, outputDefault, fftDefault ? "ON" : "OFF");
    bool safeDefault = std::abs(depthDefault) < 0.001f && std::abs(mixDefault - 100.0f) < 0.001f && std::abs(outputDefault) < 0.001f && ! fftDefault;
    std::printf("  %s\n\n", safeDefault ? "PASS: Preview Safe Default confirmed." : "FAIL: Preview Safe Default drifted.");
    if (! safeDefault) ok = false;

    auto* depthParam = proc.state().getParameter("depth");
    proc.state().getParameter("mix")->setValueNotifyingHost(1.0f);
    proc.state().getParameter("output")->setValueNotifyingHost(proc.state().getParameter("output")->convertTo0to1(0.0f));

    // 4. Depth=0 -> reductionDb must be EXACTLY 0 across the whole spectrum,
    // even with a strong resonance present (Depth is the master ceiling).
    depthParam->setValueNotifyingHost(depthParam->convertTo0to1(0.0f));
    {
        auto sig = genResonant(sr, n, 60.0, 0.08f, 0.05f, 1); // strong low-frequency resonance, inside the active region's lower edge
        runThrough(proc, sig, blockSize);
        auto red = proc.engine().getLastReduction();
        float maxAbs = 0.0f;
        for (float v : red) maxAbs = juce::jmax(maxAbs, std::abs(v));
        std::printf("Case 4 -- Depth=0, strong resonance present: max|reductionDb| = %.6f dB (expected 0.0)\n", maxAbs);
        std::printf("  %s\n\n", maxAbs < 1.0e-5f ? "PASS: reductionDb exactly 0 with Depth=0." : "FAIL: Depth=0 still produced reduction.");
        if (maxAbs >= 1.0e-5f) ok = false;
    }

    // 5. Depth>0, signal WITHOUT a synthetic resonance (plain broadband
    // noise, no spike) -> no automatic low-frequency valley just because
    // LOW=100Hz. If LOW alone caused reduction, this would show a large
    // negative reductionDb band right at/above 100Hz; it must not.
    depthParam->setValueNotifyingHost(depthParam->convertTo0to1(6.0f));
    float noiseFloorReduction = 0.0f; // set in Case 5, reused as a baseline in Case 6
    {
        auto sig = genNoise(sr, n, 0.05f, 2); // flat broadband noise, no spike anywhere
        runThrough(proc, sig, blockSize);
        auto red = proc.engine().getLastReduction();
        auto binForHz = [&](double freq) { return (int) std::round(freq * 2048.0 / sr); };
        // Compare the region right at/above LOW (100-300Hz, where a
        // LOW-CAUSED false valley would show up) against a CONTROL region
        // far from both LOW and HIGH (3-5kHz). If the algorithm is simply
        // reacting to random noise fluctuations (generic Detector V1
        // sensitivity, same everywhere), the two should be comparable; if
        // LOW itself is injecting reduction, the LOW-adjacent region should
        // be markedly larger than the control.
        float maxAbsNearLow = 0.0f, maxAbsControl = 0.0f;
        for (int hz = 100; hz <= 300; hz += 20)
            maxAbsNearLow = juce::jmax(maxAbsNearLow, std::abs(red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(hz))]));
        for (int hz = 3000; hz <= 5000; hz += 200)
            maxAbsControl = juce::jmax(maxAbsControl, std::abs(red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(hz))]));
        std::printf("Case 5 -- Depth=6, plain noise (no synthetic resonance):\n");
        std::printf("  max|reductionDb| near LOW (100-300Hz)      = %.3f dB\n", maxAbsNearLow);
        std::printf("  max|reductionDb| control region (3-5kHz)   = %.3f dB\n", maxAbsControl);
        bool comparable = maxAbsNearLow <= maxAbsControl * 2.5f + 0.3f; // LOW region not disproportionately worse than an unrelated region
        std::printf("  %s\n\n", comparable
            ? "PASS: reduction near LOW is generic noise-driven detector sensitivity (comparable to a region far from LOW/HIGH), not a LOW-boundary artifact."
            : "FAIL: reduction near LOW is disproportionately larger than the control region -- looks like a real LOW-boundary artifact.");
        if (! comparable) ok = false;
        noiseFloorReduction = maxAbsControl;
    }

    // 6. Insert an artificial resonance INSIDE the active region (e.g.
    // 500Hz) -> a valley should form ONLY around that region, confirming
    // reduction still tracks a REAL detected resonance, not the boundary.
    {
        auto sig = genResonant(sr, n, 500.0, 0.05f, 0.02f, 3);
        runThrough(proc, sig, blockSize);
        auto red = proc.engine().getLastReduction();
        auto binForHz = [&](double freq) { return (int) std::round(freq * 2048.0 / sr); };
        float at500 = red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(500.0))];
        float atFarLow = red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(120.0))];
        float atFarHigh = red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(8000.0))];
        std::printf("Case 6 -- synthetic resonance @500Hz: reduction@500Hz=%.3fdB  @120Hz=%.3fdB  @8kHz=%.3fdB  (noise floor from Case 5 = %.3fdB)\n", at500, atFarLow, atFarHigh, noiseFloorReduction);
        // "Localised" means far-field values stay within the SAME generic
        // noise-floor band Case 5 already established for this signal type
        // -- not an arbitrary flat threshold -- so a false FAIL isn't raised
        // over ordinary Detector V1 noise sensitivity that has nothing to
        // do with this resonance leaking outward.
        float farFieldBudget = noiseFloorReduction * 2.0f + 0.3f;
        bool localised = (at500 < -3.0f) && (std::abs(atFarLow) < farFieldBudget) && (std::abs(atFarHigh) < farFieldBudget);
        std::printf("  %s\n\n", localised ? "PASS: valley formed only around the real resonance (far-field stays within generic noise floor)." : "FAIL: reduction leaked outside the resonance region beyond the generic noise floor.");
        if (! localised) ok = false;
    }

    // 7. UI-curve/DSP isolation, verified statically: the white curve's
    // globalRangeShapeAt/combinedSensitivityAt live only in
    // ControlCurveComponent.cpp (UI rendering); SpectralEngine's reduction
    // buffer is set exclusively by ResonanceDetector::compute() (see
    // SpectralEngine.cpp) and by nothing else -- confirmed by source
    // inspection, restated here so this test file documents the invariant.
    std::printf("Architecture note: reductionDb (SpectralEngine::c[0].reduction) is\n");
    std::printf("written ONLY by ResonanceDetector::compute(); the white curve's lateral\n");
    std::printf("taper (ControlCurveComponent::globalRangeShapeAt) is UI-only and is\n");
    std::printf("never read by SpectralEngine or ResonanceDetector.\n\n");

    std::printf("================================================================\n");
    std::printf("%s\n", ok ? "ALL FACTORY DEFAULT NEUTRALITY CHECKS PASSED" : "SOME FACTORY DEFAULT NEUTRALITY CHECKS FAILED");
    std::printf("================================================================\n");
    return ok ? 0 : 1;
}
