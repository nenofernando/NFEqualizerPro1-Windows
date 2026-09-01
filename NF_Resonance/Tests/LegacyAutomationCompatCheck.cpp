// Checkpoint A closure, item 1: proves legacy automation (c500, freq_c500)
// still drives REAL DSP behaviour after the multiband migration -- not just
// a value that keeps showing up in the host, disconnected from processing.
// Uses the real NFResonanceAudioProcessor (APVTS listeners only run on a
// real AudioProcessor instance, not on ResonanceDetector alone).

#include <JuceHeader.h>
#include "PluginProcessor.h"

using juce::AudioBuffer;

static double rms(const AudioBuffer<float>& b, int start, int len)
{
    double s = 0; long n = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i) { double v = b.getSample(c, i); s += v * v; ++n; }
    return n ? std::sqrt(s / (double) n) : 0.0;
}
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
static AudioBuffer<float> genResonant(double sr, int n, double freq, float noiseAmp, float spikeAmp)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(4242);
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

    std::printf("================================================================\n");
    std::printf("Legacy automation compatibility check (c500 / freq_c500 -> real DSP)\n");
    std::printf("================================================================\n\n");

    NFResonanceAudioProcessor proc;
    proc.prepareToPlay(sr, blockSize);
    proc.state().getParameter("depth")->setValueNotifyingHost(proc.state().getParameter("depth")->convertTo0to1(3.0f));
    proc.state().getParameter("mix")->setValueNotifyingHost(1.0f); // 100%
    proc.state().getParameter("output")->setValueNotifyingHost(proc.state().getParameter("output")->convertTo0to1(0.0f));

    // Neutral baseline: silence all 32 bands (band_sens_i = 0) so any effect
    // we see is attributable ONLY to the legacy parameter we automate next.
    for (int i = 0; i < SpectralEngine::kMaxBands; ++i)
    {
        auto* sp = proc.state().getParameter("band_sens_" + juce::String(i));
        sp->setValueNotifyingHost(sp->convertTo0to1(0.0f));
    }

    // Direct per-bin reduction inspection (engine().getLastReduction()) is a
    // far more precise probe than RMS-of-audio here: with a STRONG resonance
    // and default depth, baseline (sens=0) reduction can already saturate at
    // maxRed, making an RMS-based A/B comparison misleadingly flat even when
    // the sensitivity parameter genuinely changed the per-bin dB target.
    // A moderate resonance (prominence sitting near the neutral threshold)
    // avoids that ceiling and shows the sensitivity increase directly.
    auto binForHz = [&](double freq) { return (int) std::round(freq * 2048.0 / sr); };
    auto measureReductionAt = [&](double freq, double sigFreq, float noiseAmp, float spikeAmp) {
        auto sig = genResonant(sr, n, sigFreq, noiseAmp, spikeAmp);
        runThrough(proc, sig, blockSize);
        auto red = proc.engine().getLastReduction();
        int bin = binForHz(freq);
        return (double) red[(size_t) juce::jlimit(0, (int) red.size() - 1, bin)];
    };

    // Calibration sweep: find a spike amplitude whose prominence sits in the
    // LINEAR (non-clamped-by-maxRed) region, so the sens=-10 vs sens=+10
    // comparison isn't misleadingly flat just because both extremes hit the
    // reduction ceiling.
    auto* c500cal = proc.state().getParameter("c500");
    std::printf("-- Calibration sweep (finding a non-saturating spike amplitude) --\n");
    for (float amp : { 0.008f, 0.002f, 0.0006f, 0.00015f, 0.00004f })
    {
        c500cal->setValueNotifyingHost(c500cal->convertTo0to1(-10.0f));
        double lo = measureReductionAt(500.0, 500.0, 0.05f, amp);
        c500cal->setValueNotifyingHost(c500cal->convertTo0to1(10.0f));
        double hi = measureReductionAt(500.0, 500.0, 0.05f, amp);
        std::printf("  spikeAmp=%.5f : sens=-10 -> %.3fdB | sens=+10 -> %.3fdB | delta=%.3fdB\n", amp, lo, hi, lo - hi);
    }
    std::printf("\n");

    // Full-range differential test (-10 vs +10 sensitivity at the SAME
    // frequency/signal) rather than trying to hand-tune a signal that sits
    // exactly at the reduction ceiling -- a strong resonance can legitimately
    // saturate at maxRed for a wide range of sensitivity, which would make a
    // narrower A/B comparison misleadingly flat without being a wiring bug.
    // A full -10->+10 swing must move the result regardless of where the
    // absolute level sits, unless BOTH extremes happen to saturate (checked
    // explicitly below).
    auto* c500 = proc.state().getParameter("c500");
    c500->setValueNotifyingHost(c500->convertTo0to1(-10.0f));
    double lowSens500 = measureReductionAt(500.0, 500.0, 0.05f, 0.0006f);
    c500->setValueNotifyingHost(c500->convertTo0to1(10.0f)); // host automates legacy sensitivity to +10
    double highSens500 = measureReductionAt(500.0, 500.0, 0.05f, 0.0006f);
    std::printf("Legacy \"c500\" swing -10 -> +10 @500Hz bin:\n");
    std::printf("  reduction at sens=-10: %.3f dB\n  reduction at sens=+10: %.3f dB\n", lowSens500, highSens500);
    std::printf("  band_sens_2 now reads: %.3f (should be ~10.0, mirrored live)\n", proc.state().getRawParameterValue("band_sens_2")->load());
    std::printf("  %s\n\n", (highSens500 < lowSens500 - 1.0) ? "PASS: legacy automation changed REAL processing." : "FAIL: legacy automation had no measurable effect.");

    // Reset for the frequency test.
    for (int i = 0; i < SpectralEngine::kMaxBands; ++i)
    {
        auto* sp = proc.state().getParameter("band_sens_" + juce::String(i));
        sp->setValueNotifyingHost(sp->convertTo0to1(0.0f));
    }
    c500->setValueNotifyingHost(c500->convertTo0to1(10.0f)); // re-arm strong sensitivity, still at legacy default freq (500Hz)

    double before2k = measureReductionAt(2000.0, 2000.0, 0.05f, 0.0006f);
    std::printf("Before moving \"freq_c500\" (band still at 500Hz): reduction @2000Hz bin: %.3f dB\n", before2k);

    auto* freqC500 = proc.state().getParameter("freq_c500");
    freqC500->setValueNotifyingHost(freqC500->convertTo0to1(2000.0f)); // host automates legacy FREQUENCY to 2kHz
    double after2k = measureReductionAt(2000.0, 2000.0, 0.05f, 0.0006f);
    std::printf("After host-automating legacy \"freq_c500\" to 2000Hz:\n");
    std::printf("  reduction @2000Hz bin: %.3f dB (was %.3f dB)\n", after2k, before2k);
    std::printf("  band_freq_2 now reads: %.1f (should be ~2000.0, mirrored live)\n", proc.state().getRawParameterValue("band_freq_2")->load());
    std::printf("  %s\n\n", (after2k < before2k - 0.3) ? "PASS: legacy frequency automation moved REAL processing." : "FAIL: legacy frequency automation had no measurable effect.");

    std::printf("================================================================\n");
    std::printf("Check complete. No production defaults changed by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
