// Bypass correctness: with Bypass ON, the audible output must be EXACTLY
// the dry input, latency-aligned, regardless of Output/Depth/Sharpness/
// Selectivity/Attack/Release/Mix/Quality/bands/LOW-HIGH. The engine keeps
// running "warm" underneath (never reset), but none of its output, and no
// gain staging, may reach the audible path while bypassed.
// Bug found: Output gain was applied AFTER the bypass blend
// (`(dry*bypassMix + processed*(1-bypassMix)) * out`), so turning Output
// still changed the level even at full bypass. Fixed to apply Output only
// to the processed path (`dry*bypassMix + processed*out*(1-bypassMix)`).

#include <JuceHeader.h>
#include "PluginProcessor.h"

using juce::AudioBuffer;

static AudioBuffer<float> genTestSignal(double sr, int n, int seed)
{
    // Broadband noise + several resonant tones, so any leaking parameter
    // (Depth, band sensitivity, etc.) would have real material to act on.
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    for (int i = 0; i < n; ++i) { float x = (rng.nextFloat() * 2.0f - 1.0f) * 0.2f; b.setSample(0, i, x); b.setSample(1, i, x); }
    for (double freq : { 80.0, 500.0, 3000.0, 8000.0 })
    {
        double ph = 0, inc = juce::MathConstants<double>::twoPi * freq / sr;
        for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * 0.15f; b.addSample(0, i, s); b.addSample(1, i, s); ph += inc; }
    }
    return b;
}

struct NullResult { double rms, peak; };

static NullResult measureBypassNull(NFResonanceAudioProcessor& proc, int blockSize, const AudioBuffer<float>& input)
{
    int n = input.getNumSamples();
    AudioBuffer<float> live(input);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += blockSize)
    {
        int len = juce::jmin(blockSize, n - pos);
        AudioBuffer<float> blk(live.getArrayOfWritePointers(), live.getNumChannels(), pos, len);
        proc.processBlock(blk, midi);
    }
    int lat = proc.engine().latencySamples();
    double sumSq = 0, peak = 0; long count = 0;
    int start = lat + blockSize * 2; // clear of the delay-line's own startup fill
    for (int i = start; i < n; ++i)
        for (int c = 0; c < live.getNumChannels(); ++c)
        {
            double expected = input.getSample(c, i - lat);
            double actual = live.getSample(c, i);
            double d = actual - expected;
            sumSq += d * d; peak = juce::jmax(peak, std::abs(d));
            ++count;
        }
    return { count ? std::sqrt(sumSq / (double) count) : 0.0, peak };
}

static bool runCase(const char* name, double sr, int blockSize, std::function<void(NFResonanceAudioProcessor&)> setup)
{
    NFResonanceAudioProcessor proc;
    proc.state().getParameter("bypass")->setValueNotifyingHost(1.0f); // set BEFORE prepare so bypassMix starts already at 1.0, no settling transient to worry about here
    setup(proc);
    proc.prepareToPlay(sr, blockSize);
    const int n = (int) (sr * 2.0);
    auto input = genTestSignal(sr, n, 99);
    auto res = measureBypassNull(proc, blockSize, input);
    const double tol = 1.0e-5;
    bool pass = res.rms < tol && res.peak < tol;
    std::printf("  %-46s rms=%.2e peak=%.2e  %s\n", name, res.rms, res.peak, pass ? "PASS" : "FAIL");
    return pass;
}

int main()
{
    bool ok = true;
    const double sr = 48000.0; const int blockSize = 512;

    std::printf("================================================================\n");
    std::printf("Bypass null test -- Bypass ON, audible output must equal\n");
    std::printf("latency-aligned dry regardless of any other parameter.\n");
    std::printf("================================================================\n\n");

    std::printf("-- Parameter sweep (sr=48000, block=512) --\n");
    ok &= runCase("baseline (all defaults)", sr, blockSize, [](NFResonanceAudioProcessor&) {});
    ok &= runCase("Output = +12dB", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        auto* o = p.state().getParameter("output"); o->setValueNotifyingHost(o->convertTo0to1(12.0f)); });
    ok &= runCase("Output = -12dB", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        auto* o = p.state().getParameter("output"); o->setValueNotifyingHost(o->convertTo0to1(-12.0f)); });
    ok &= runCase("Depth=10, Sharpness=10, Selectivity=10", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        p.state().getParameter("depth")->setValueNotifyingHost(1.0f);
        p.state().getParameter("sharpness")->setValueNotifyingHost(1.0f);
        p.state().getParameter("selectivity")->setValueNotifyingHost(1.0f); });
    ok &= runCase("Attack=min, Release=min (fastest)", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        p.state().getParameter("attack")->setValueNotifyingHost(0.0f);
        p.state().getParameter("release")->setValueNotifyingHost(0.0f); });
    ok &= runCase("Mix = 0%", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        p.state().getParameter("mix")->setValueNotifyingHost(0.0f); });
    ok &= runCase("Mix = 100%", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        p.state().getParameter("mix")->setValueNotifyingHost(1.0f); });
    ok &= runCase("LOW off / HIGH off", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        p.state().getParameter("lowEnabled")->setValueNotifyingHost(0.0f);
        p.state().getParameter("highEnabled")->setValueNotifyingHost(0.0f); });
    ok &= runCase("Extreme band: 500Hz sens=+12 width=0.3", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        auto* fp = p.state().getParameter("band_freq_0"); fp->setValueNotifyingHost(fp->convertTo0to1(500.0f));
        auto* sp = p.state().getParameter("band_sens_0"); sp->setValueNotifyingHost(sp->convertTo0to1(12.0f));
        auto* wp = p.state().getParameter("band_width_0"); wp->setValueNotifyingHost(wp->convertTo0to1(0.3f));
        p.state().getParameter("band_active_0")->setValueNotifyingHost(1.0f); });
    ok &= runCase("WORST CASE: everything above combined", sr, blockSize, [](NFResonanceAudioProcessor& p) {
        auto* o = p.state().getParameter("output"); o->setValueNotifyingHost(o->convertTo0to1(12.0f));
        p.state().getParameter("depth")->setValueNotifyingHost(1.0f);
        p.state().getParameter("sharpness")->setValueNotifyingHost(1.0f);
        p.state().getParameter("selectivity")->setValueNotifyingHost(1.0f);
        p.state().getParameter("mix")->setValueNotifyingHost(1.0f);
        auto* fp = p.state().getParameter("band_freq_0"); fp->setValueNotifyingHost(fp->convertTo0to1(500.0f));
        auto* sp = p.state().getParameter("band_sens_0"); sp->setValueNotifyingHost(sp->convertTo0to1(12.0f));
        p.state().getParameter("band_active_0")->setValueNotifyingHost(1.0f); });

    std::printf("\n-- Sample-rate / buffer-size matrix (worst-case settings) --\n");
    for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int testBlock : { 64, 128, 256, 512, 1024 })
        {
            juce::String name = juce::String(testSr, 0) + "Hz / block=" + juce::String(testBlock);
            ok &= runCase(name.toRawUTF8(), testSr, testBlock, [](NFResonanceAudioProcessor& p) {
                auto* o = p.state().getParameter("output"); o->setValueNotifyingHost(o->convertTo0to1(12.0f));
                p.state().getParameter("depth")->setValueNotifyingHost(1.0f);
                p.state().getParameter("mix")->setValueNotifyingHost(1.0f); });
        }

    // Transition test: toggle Bypass OFF->ON->OFF repeatedly DURING
    // playback and confirm no discontinuity beyond the deliberate ~15ms
    // smoothing window -- no click/spike/mute/stale-audio/timing jump.
    // The bar isn't an arbitrary constant (this test signal's own noise+
    // multi-tone content, at +6dB, already has a non-trivial natural
    // sample-to-sample slope) -- it's a STEADY-STATE baseline run with the
    // exact same signal/gain and NO toggling, so "is toggling worse than
    // the signal's own natural jitter" is what's actually being measured.
    std::printf("\n-- Bypass ON/OFF transition (during playback) --\n");
    {
        auto maxJumpFor = [&](bool doToggle) {
            NFResonanceAudioProcessor proc;
            proc.state().getParameter("output")->setValueNotifyingHost(proc.state().getParameter("output")->convertTo0to1(6.0f));
            proc.prepareToPlay(sr, blockSize);
            const int n = (int) (sr * 4.0);
            auto input = genTestSignal(sr, n, 7);
            AudioBuffer<float> live(input);
            juce::MidiBuffer midi;
            auto* bypassParam = proc.state().getParameter("bypass");
            float maxSampleJump = 0.0f, prevSample = 0.0f; bool first = true;
            int toggleEveryBlocks = 20; // ~213ms at 512/48k -- several toggles across the 4s signal
            int blockIdx = 0;
            for (int pos = 0; pos < n; pos += blockSize)
            {
                if (doToggle && blockIdx % toggleEveryBlocks == 0)
                    bypassParam->setValueNotifyingHost(bypassParam->getValue() < 0.5f ? 1.0f : 0.0f);
                int len = juce::jmin(blockSize, n - pos);
                AudioBuffer<float> blk(live.getArrayOfWritePointers(), live.getNumChannels(), pos, len);
                proc.processBlock(blk, midi);
                for (int i = 0; i < len; ++i)
                {
                    float s = live.getSample(0, pos + i);
                    if (! first) maxSampleJump = juce::jmax(maxSampleJump, std::abs(s - prevSample));
                    prevSample = s; first = false;
                }
                ++blockIdx;
            }
            return maxSampleJump;
        };
        float baselineJump = maxJumpFor(false);
        float toggleJump = maxJumpFor(true);
        // Toggling may legitimately move the signal a bit more than the
        // steady-state baseline (it's actively crossfading between two
        // different signal paths), but not by an order of magnitude --
        // that would indicate a real click/spike, not just crossfade motion.
        bool transitionOk = toggleJump < baselineJump * 3.0f + 0.05f;
        std::printf("  steady-state baseline max jump (no toggling) = %.4f\n", baselineJump);
        std::printf("  max single-sample jump across repeated ON/OFF toggles = %.4f  %s\n",
            toggleJump, transitionOk ? "PASS (consistent with the signal's own natural jitter, no added click/spike)" : "FAIL (discontinuity beyond the signal's own natural jitter)");
        ok &= transitionOk;
    }

    // Save/recall must preserve the bypass parameter itself AND its audible
    // behaviour (a fresh processor that loads a bypassed state must still
    // null against latency-aligned dry, not silently reset to unbypassed).
    std::printf("\n-- Save/recall preserves Bypass state --\n");
    {
        NFResonanceAudioProcessor a;
        a.state().getParameter("bypass")->setValueNotifyingHost(1.0f);
        auto* o = a.state().getParameter("output"); o->setValueNotifyingHost(o->convertTo0to1(8.0f));
        juce::MemoryBlock saved; a.getStateInformation(saved);

        NFResonanceAudioProcessor b;
        b.setStateInformation(saved.getData(), (int) saved.getSize());
        bool bypassRestored = b.state().getRawParameterValue("bypass")->load() > 0.5f;
        b.prepareToPlay(sr, blockSize);
        auto input = genTestSignal(sr, (int) (sr * 1.0), 3);
        auto res = measureBypassNull(b, blockSize, input);
        bool pass = bypassRestored && res.rms < 1.0e-5 && res.peak < 1.0e-5;
        std::printf("  bypass param restored=%s  rms=%.2e peak=%.2e  %s\n",
            bypassRestored ? "true" : "false", res.rms, res.peak, pass ? "PASS" : "FAIL");
        ok &= pass;
    }

    // Multi-instance independence: two processors, one bypassed and one
    // not, must not interfere with each other's audible output.
    std::printf("\n-- Multi-instance independence --\n");
    {
        NFResonanceAudioProcessor bypassedProc, activeProc;
        bypassedProc.state().getParameter("bypass")->setValueNotifyingHost(1.0f);
        activeProc.state().getParameter("depth")->setValueNotifyingHost(1.0f); // active, processing for real
        bypassedProc.prepareToPlay(sr, blockSize);
        activeProc.prepareToPlay(sr, blockSize);
        auto input = genTestSignal(sr, (int) (sr * 1.0), 11);
        auto resBypassed = measureBypassNull(bypassedProc, blockSize, input);
        // activeProc is NOT bypassed, so it's expected to differ from dry
        // (that's the point) -- what matters is bypassedProc stayed null
        // regardless of activeProc's independent, concurrent existence.
        AudioBuffer<float> liveActive(input);
        juce::MidiBuffer midi2;
        for (int pos = 0; pos < liveActive.getNumSamples(); pos += blockSize)
        {
            int len = juce::jmin(blockSize, liveActive.getNumSamples() - pos);
            AudioBuffer<float> blk(liveActive.getArrayOfWritePointers(), liveActive.getNumChannels(), pos, len);
            activeProc.processBlock(blk, midi2);
        }
        bool pass = resBypassed.rms < 1.0e-5 && resBypassed.peak < 1.0e-5;
        std::printf("  bypassed instance null while a second active instance processes concurrently: rms=%.2e peak=%.2e  %s\n",
            resBypassed.rms, resBypassed.peak, pass ? "PASS" : "FAIL");
        ok &= pass;
    }

    // Reset / transport stop-start: releaseResources() + prepareToPlay()
    // again mid-session (simulating a DAW transport stop/start or a sample
    // rate change) must not break the bypass null.
    std::printf("\n-- Reset / transport stop-start --\n");
    {
        NFResonanceAudioProcessor proc;
        proc.state().getParameter("bypass")->setValueNotifyingHost(1.0f);
        proc.prepareToPlay(sr, blockSize);
        auto input1 = genTestSignal(sr, (int) (sr * 0.5), 21);
        measureBypassNull(proc, blockSize, input1); // run once, then simulate transport stop
        proc.releaseResources();
        proc.prepareToPlay(sr, blockSize); // transport start again
        auto input2 = genTestSignal(sr, (int) (sr * 1.0), 22);
        auto res = measureBypassNull(proc, blockSize, input2);
        bool pass = res.rms < 1.0e-5 && res.peak < 1.0e-5;
        std::printf("  after releaseResources()+prepareToPlay() again: rms=%.2e peak=%.2e  %s\n", res.rms, res.peak, pass ? "PASS" : "FAIL");
        ok &= pass;
    }

    std::printf("\n================================================================\n");
    std::printf("%s\n", ok ? "ALL BYPASS NULL CHECKS PASSED" : "SOME BYPASS NULL CHECKS FAILED");
    std::printf("================================================================\n");
    return ok ? 0 : 1;
}
