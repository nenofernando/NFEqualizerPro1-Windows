// LOW/HIGH ON/OFF semantics: lowEnabled=false must open the detector's
// range at the low extreme WITHOUT touching the saved lowHz value (same for
// highEnabled/highHz). Uses the real NFResonanceAudioProcessor end-to-end.

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
static AudioBuffer<float> genResonant(double sr, int n, double freq, float noiseAmp, float spikeAmp, int seed)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    for (int i = 0; i < n; ++i) { float x = (rng.nextFloat() * 2.0f - 1.0f) * noiseAmp; b.setSample(0, i, x); b.setSample(1, i, x); }
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
    std::printf("LOW/HIGH ON/OFF semantics check\n");
    std::printf("================================================================\n\n");

    NFResonanceAudioProcessor proc;
    proc.prepareToPlay(sr, blockSize);
    proc.state().getParameter("depth")->setValueNotifyingHost(proc.state().getParameter("depth")->convertTo0to1(6.0f));
    proc.state().getParameter("mix")->setValueNotifyingHost(1.0f);
    proc.state().getParameter("output")->setValueNotifyingHost(proc.state().getParameter("output")->convertTo0to1(0.0f));

    auto binForHz = [&](double freq) { return (int) std::round(freq * 2048.0 / sr); };
    auto reductionAt = [&](double sigFreq, double probeFreq) {
        auto sig = genResonant(sr, n, sigFreq, 0.02f, 0.05f, 7);
        runThrough(proc, sig, blockSize);
        auto red = proc.engine().getLastReduction();
        return (double) red[(size_t) juce::jlimit(0, (int) red.size() - 1, binForHz(probeFreq))];
    };
    auto setEnabled = [&](const char* id, bool on) {
        auto* ep = proc.state().getParameter(id);
        ep->beginChangeGesture(); ep->setValueNotifyingHost(on ? 1.0f : 0.0f); ep->endChangeGesture();
    };

    // Confirm factory default: LOW=100Hz/HIGH=16kHz, both ON.
    float lowHz0 = proc.state().getRawParameterValue("lowHz")->load();
    float highHz0 = proc.state().getRawParameterValue("highHz")->load();
    std::printf("LOW=%.1fHz HIGH=%.1fHz lowEnabled=%.0f highEnabled=%.0f\n\n",
        lowHz0, highHz0, proc.state().getRawParameterValue("lowEnabled")->load(), proc.state().getRawParameterValue("highEnabled")->load());

    // LOW ON / HIGH ON: a resonance at 60Hz (below LOW=100) must NOT be reduced.
    double r1 = reductionAt(60.0, 60.0);
    std::printf("Case LOW ON/HIGH ON  -- resonance @60Hz (below LOW): reduction=%.3fdB  %s\n",
        r1, std::abs(r1) < 0.5 ? "PASS (correctly outside range)" : "FAIL");
    if (std::abs(r1) >= 0.5) ok = false;

    // LOW OFF / HIGH ON: same 60Hz resonance must NOW be reduced (range opened).
    setEnabled("lowEnabled", false);
    double r2 = reductionAt(60.0, 60.0);
    std::printf("Case LOW OFF/HIGH ON -- resonance @60Hz: reduction=%.3fdB  %s\n",
        r2, r2 < -1.0 ? "PASS (range opened at the bottom)" : "FAIL");
    if (r2 >= -1.0) ok = false;
    // lowHz value itself must be untouched by disabling.
    float lowHzAfterOff = proc.state().getRawParameterValue("lowHz")->load();
    std::printf("  lowHz after disabling = %.1f (expected unchanged %.1f)  %s\n\n", lowHzAfterOff, lowHz0,
        std::abs(lowHzAfterOff - lowHz0) < 0.01f ? "PASS" : "FAIL");
    if (std::abs(lowHzAfterOff - lowHz0) >= 0.01f) ok = false;

    // Re-enable LOW: value must come back exactly to what it was (100Hz).
    setEnabled("lowEnabled", true);
    double r3 = reductionAt(60.0, 60.0);
    std::printf("Case LOW re-enabled  -- resonance @60Hz: reduction=%.3fdB  %s (lowHz restored to %.1f)\n\n",
        r3, std::abs(r3) < 0.5 ? "PASS (range closed again exactly as before)" : "FAIL", proc.state().getRawParameterValue("lowHz")->load());
    if (std::abs(r3) >= 0.5) ok = false;

    // LOW ON / HIGH OFF: resonance at 18kHz (above HIGH=16k) must NOT be
    // reduced with HIGH on, but MUST be reduced once HIGH is off.
    double r4 = reductionAt(18000.0, 18000.0);
    std::printf("Case LOW ON/HIGH ON  -- resonance @18kHz (above HIGH): reduction=%.3fdB  %s\n",
        r4, std::abs(r4) < 0.5 ? "PASS (correctly outside range)" : "FAIL");
    if (std::abs(r4) >= 0.5) ok = false;

    setEnabled("highEnabled", false);
    double r5 = reductionAt(18000.0, 18000.0);
    float highHzAfterOff = proc.state().getRawParameterValue("highHz")->load();
    std::printf("Case LOW ON/HIGH OFF -- resonance @18kHz: reduction=%.3fdB  %s\n",
        r5, r5 < -1.0 ? "PASS (range opened at the top)" : "FAIL");
    std::printf("  highHz after disabling = %.1f (expected unchanged %.1f)  %s\n\n", highHzAfterOff, highHz0,
        std::abs(highHzAfterOff - highHz0) < 0.01f ? "PASS" : "FAIL");
    if (r5 >= -1.0) ok = false;
    if (std::abs(highHzAfterOff - highHz0) >= 0.01f) ok = false;

    // Both OFF: range must be the FULL spectrum (test both a very low and a
    // very high resonance, both should now be reduced).
    setEnabled("lowEnabled", false);
    double r6lo = reductionAt(40.0, 40.0);
    double r6hi = reductionAt(19500.0, 19500.0);
    std::printf("Case LOW OFF/HIGH OFF -- resonance @40Hz: reduction=%.3fdB  @19.5kHz: reduction=%.3fdB\n",
        r6lo, r6hi);
    bool bothOpen = r6lo < -1.0 && r6hi < -1.0;
    std::printf("  %s\n\n", bothOpen ? "PASS: range is integral/full with both OFF." : "FAIL: range not fully open with both OFF.");
    if (! bothOpen) ok = false;

    // Restore both ON, both at 40/19500Hz from the previous case.
    setEnabled("lowEnabled", true);
    setEnabled("highEnabled", true);

    // Editing lowHz/highHz (knob turn OR host automation -- same parameter
    // write either way) must NEVER touch lowEnabled/highEnabled. Only a
    // double-click on the UI handle may (that's UI-only, untestable here,
    // already covered by manual DAW testing) -- what IS testable at this
    // level is that plain parameter writes leave enabled state alone.
    setEnabled("lowEnabled", false);
    { auto* lp = proc.state().getParameter("lowHz"); lp->beginChangeGesture(); lp->setValueNotifyingHost(lp->convertTo0to1(250.0f)); lp->endChangeGesture(); }
    bool lowStillOff = proc.state().getRawParameterValue("lowEnabled")->load() < 0.5f;
    std::printf("Case LOW OFF -- lowHz automated 100->250Hz: lowHz now=%.1f  lowEnabled=%.0f  %s\n",
        proc.state().getRawParameterValue("lowHz")->load(), proc.state().getRawParameterValue("lowEnabled")->load(),
        lowStillOff ? "PASS: still OFF, frequency edit did not reactivate it." : "FAIL: editing lowHz reactivated LOW.");
    if (! lowStillOff) ok = false;
    setEnabled("lowEnabled", true);

    setEnabled("highEnabled", false);
    { auto* hp = proc.state().getParameter("highHz"); hp->beginChangeGesture(); hp->setValueNotifyingHost(hp->convertTo0to1(12000.0f)); hp->endChangeGesture(); }
    bool highStillOff = proc.state().getRawParameterValue("highEnabled")->load() < 0.5f;
    std::printf("Case HIGH OFF -- highHz automated 16000->12000Hz: highHz now=%.1f  highEnabled=%.0f  %s\n\n",
        proc.state().getRawParameterValue("highHz")->load(), proc.state().getRawParameterValue("highEnabled")->load(),
        highStillOff ? "PASS: still OFF, frequency edit did not reactivate it." : "FAIL: editing highHz reactivated HIGH.");
    if (! highStillOff) ok = false;
    setEnabled("highEnabled", true);

    std::printf("================================================================\n");
    std::printf("%s\n", ok ? "ALL LOW/HIGH ON/OFF CHECKS PASSED" : "SOME LOW/HIGH ON/OFF CHECKS FAILED");
    std::printf("================================================================\n");
    return ok ? 0 : 1;
}
