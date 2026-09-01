// Sonic Alpha V2 -- FIRST GAIN MASK validation. Exercises the REAL plugin
// path (PluginProcessor -> SpectralEngine -> GainMaskEngine, PHYSICAL C/D
// under the hood), not the offline PHYSICAL C/D harnesses directly.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <chrono>

using juce::AudioBuffer;

static AudioBuffer<float> genSilenceStereo(int n) { AudioBuffer<float> b(2, n); b.clear(); return b; }
static void addToneStereo(AudioBuffer<float>& b, double sr, double freq, float amp, int startSample = 0, int endSample = -1)
{
    if (endSample < 0) endSample = b.getNumSamples();
    double ph = 0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < b.getNumSamples(); ++i) { if (i >= startSample && i < endSample) { float s = (float) std::sin(ph) * amp; b.addSample(0, i, s); b.addSample(1, i, s); } ph += inc; }
}
static AudioBuffer<float> genHarmonicSeriesStereo(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{ auto b = genSilenceStereo(n); for (int h = 1; h <= numH; ++h) addToneStereo(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static void addDecayingResonanceStereo(AudioBuffer<float>& b, double sr, double freqHz, float amp, double decaySeconds, int startSample)
{
    double ph = 0, inc = juce::MathConstants<double>::twoPi * freqHz / sr; int n = b.getNumSamples();
    for (int i = startSample; i < n; ++i) { double t = (double) (i - startSample) / sr; float env = (float) std::exp(-t / decaySeconds); float s = (float) std::sin(ph) * amp * env; b.addSample(0, i, s); b.addSample(1, i, s); ph += inc; }
}
static void addClickStereo(AudioBuffer<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < b.getNumSamples(); ++i)
    { float env = (float) std::exp(-(double) i / (lenSamples * 0.3)); float s = (rng.nextFloat() * 2.0f - 1.0f) * amp * env; b.addSample(0, startSample + i, s); b.addSample(1, startSample + i, s); }
}
static void addNoiseBurstStereo(AudioBuffer<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < b.getNumSamples(); ++i) { float s = (rng.nextFloat() * 2.0f - 1.0f) * amp; b.addSample(0, startSample + i, s); b.addSample(1, startSample + i, s); }
}

static double rms(const AudioBuffer<float>& b, int startSample = 0, int endSample = -1)
{
    if (endSample < 0) endSample = b.getNumSamples();
    double sum = 0; long count = 0;
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = startSample; i < endSample; ++i) { double v = b.getSample(c, i); sum += v * v; ++count; }
    return count ? std::sqrt(sum / (double) count) : 0.0;
}

static void setParam(NFResonanceAudioProcessor& proc, const char* id, float v) { auto* p = proc.state().getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(v)); }

static AudioBuffer<float> runProcessor(NFResonanceAudioProcessor& proc, const AudioBuffer<float>& input, int blockSize)
{
    AudioBuffer<float> live(input);
    juce::MidiBuffer midi;
    int n = live.getNumSamples();
    for (int pos = 0; pos < n; pos += blockSize)
    {
        int len = juce::jmin(blockSize, n - pos);
        AudioBuffer<float> blk(live.getArrayOfWritePointers(), live.getNumChannels(), pos, len);
        proc.processBlock(blk, midi);
    }
    return live;
}

int main()
{
    bool allOk = true;
    std::printf("=== Sonic Alpha V2 -- FIRST GAIN MASK Validation ===\n");
    const double sr = 48000.0; const int blockSize = 512; // == hop, so each block triggers exactly one frame()

    // ---- item: Depth 0 unity/null ----
    std::printf("\n-- Depth=0 unity/null (bit-exact against Depth=0 dry-equivalent) --\n");
    {
        NFResonanceAudioProcessor procA; setParam(procA, "depth", 0.0f); setParam(procA, "mix", 100.0f); procA.prepareToPlay(sr, blockSize);
        NFResonanceAudioProcessor procB; setParam(procB, "bypass", 1.0f); procB.prepareToPlay(sr, blockSize); // dry, latency-aligned reference
        auto sig = genHarmonicSeriesStereo(sr, (int) (sr * 1.0), 220.0, 0.3f, 8); addClickStereo(sig, 0, (int) (sr * 0.003), 0.5f, 1);
        auto outA = runProcessor(procA, sig, blockSize);
        auto outB = runProcessor(procB, sig, blockSize);
        double maxDiff = 0;
        int lat = procA.engine().latencySamples();
        for (int c = 0; c < 2; ++c) for (int i = lat + blockSize; i < sig.getNumSamples(); ++i) maxDiff = juce::jmax((double) maxDiff, (double) std::abs(outA.getSample(c, i) - outB.getSample(c, i)));
        bool pass = maxDiff < 1.0e-5;
        std::printf("  Depth=0 vs Bypass, max sample diff = %.2e  %s\n", maxDiff, pass ? "PASS" : "FAIL");
        allOk &= pass;
    }

    // ---- item: Delta polarity/math ----
    std::printf("\n-- Delta polarity/math (Delta output + processed output ~= dry) --\n");
    {
        NFResonanceAudioProcessor procWet; setParam(procWet, "depth", 5.0f); setParam(procWet, "selectivity", 3.0f); setParam(procWet, "mix", 100.0f); procWet.prepareToPlay(sr, blockSize);
        NFResonanceAudioProcessor procDelta; setParam(procDelta, "depth", 5.0f); setParam(procDelta, "selectivity", 3.0f); setParam(procDelta, "mix", 100.0f); setParam(procDelta, "delta", 1.0f); procDelta.prepareToPlay(sr, blockSize);
        auto sig = genHarmonicSeriesStereo(sr, (int) (sr * 1.0), 120.0, 0.3f, 8); addDecayingResonanceStereo(sig, sr, 170.0, 0.4f, 0.3, 0);
        auto wet = runProcessor(procWet, sig, blockSize);
        auto delta = runProcessor(procDelta, sig, blockSize);
        int lat = procWet.engine().latencySamples();
        double maxDiff = 0;
        for (int c = 0; c < 2; ++c) for (int i = lat + blockSize; i < sig.getNumSamples(); ++i)
        { double expectedDry = sig.getSample(c, i - lat); double sum = wet.getSample(c, i) + delta.getSample(c, i); maxDiff = juce::jmax(maxDiff, std::abs(sum - expectedDry)); }
        bool pass = maxDiff < 1.0e-3; // small FFT/OLA roundtrip tolerance, not a null test
        std::printf("  max |wet+delta - dry| = %.2e  %s\n", maxDiff, pass ? "PASS" : "FAIL (wet+delta should reconstruct dry)");
        allOk &= pass;
    }

    // ---- item: attack + ringing tail (kick attack protected, ringing reduced) ----
    std::printf("\n-- Attack + decaying resonance: onset protected, tail reduced --\n");
    {
        NFResonanceAudioProcessor proc; setParam(proc, "depth", 5.0f); setParam(proc, "selectivity", 2.0f); setParam(proc, "mix", 100.0f); proc.prepareToPlay(sr, blockSize);
        int n = (int) (sr * 1.5); auto sig = genSilenceStereo(n);
        int attackStart = (int) (sr * 0.3);
        addClickStereo(sig, attackStart, (int) (sr * 0.004), 0.9f, 7);
        addDecayingResonanceStereo(sig, sr, 300.0, 0.5f, 0.6, attackStart);

        AudioBuffer<float> live(sig); juce::MidiBuffer midi;
        double hopMs = 1000.0 * blockSize / sr;
        int attackFrame = attackStart / blockSize;
        std::vector<float> reductionAt300;
        for (int pos = 0; pos < n; pos += blockSize)
        {
            int len = juce::jmin(blockSize, n - pos);
            AudioBuffer<float> blk(live.getArrayOfWritePointers(), 2, pos, len);
            proc.processBlock(blk, midi);
            auto red = proc.engine().getLastReduction();
            int bin = (int) std::round(300.0 * 2048.0 / sr);
            reductionAt300.push_back(bin < (int) red.size() ? red[(size_t) bin] : 0.0f);
        }
        float onsetRed = reductionAt300[(size_t) attackFrame];
        float tailRed = reductionAt300[(size_t) juce::jmin((size_t) reductionAt300.size() - 1, (size_t) (attackFrame + (int) (300.0 / hopMs)))];
        bool pass = onsetRed > -0.6f && tailRed < onsetRed - 0.3f; // onset: little/no reduction yet; tail: meaningfully more reduced
        std::printf("  reduction@300Hz onset=%.2fdB  +300ms-later=%.2fdB  %s\n", onsetRed, tailRed, pass ? "PASS (attack protected, tail reduced)" : "FAIL");
        allOk &= pass;
    }

    // ---- item: musical material at Depth 0/1/3/5 ----
    std::printf("\n-- Musical material, Depth 0/1/3/5 --\n");
    auto runMaterial = [&](const char* label, std::function<AudioBuffer<float>(double, int)> gen)
    {
        std::printf(" %s:\n", label);
        int n = (int) (sr * 1.5);
        for (float depth : { 0.0f, 1.0f, 3.0f, 5.0f })
        {
            NFResonanceAudioProcessor proc; setParam(proc, "depth", depth); setParam(proc, "selectivity", 3.0f); setParam(proc, "mix", 100.0f); proc.prepareToPlay(sr, blockSize);
            NFResonanceAudioProcessor procDelta; setParam(procDelta, "depth", depth); setParam(procDelta, "selectivity", 3.0f); setParam(procDelta, "mix", 100.0f); setParam(procDelta, "delta", 1.0f); procDelta.prepareToPlay(sr, blockSize);
            auto sig = gen(sr, n);
            auto out = runProcessor(proc, sig, blockSize);
            auto delta = runProcessor(procDelta, sig, blockSize);
            int lat = proc.engine().latencySamples();
            double inR = rms(sig, 0, n - lat), outR = rms(out, lat + blockSize, n), deltaR = rms(delta, lat + blockSize, n);
            double lossDb = 20.0 * std::log10(juce::jmax(1.0e-9, outR / juce::jmax(1.0e-9, inR)));
            // per-frame min reduction (deepest, most negative) sampled across a
            // fresh instance run (keeps the RMS-measuring instance above untouched)
            double sumMin = 0; double worstMin = 0; int frames = 0; int activeRegionsSum = 0;
            {
                NFResonanceAudioProcessor procStat; setParam(procStat, "depth", depth); setParam(procStat, "selectivity", 3.0f); setParam(procStat, "mix", 100.0f); procStat.prepareToPlay(sr, blockSize);
                AudioBuffer<float> live2(sig); juce::MidiBuffer midi2;
                for (int pos = 0; pos + blockSize <= n; pos += blockSize)
                {
                    AudioBuffer<float> blk(live2.getArrayOfWritePointers(), 2, pos, blockSize);
                    procStat.processBlock(blk, midi2);
                    auto red = procStat.engine().getLastReduction();
                    float mn = 0; int active = 0;
                    for (float v : red) { mn = juce::jmin(mn, v); if (v < -0.05f) ++active; }
                    sumMin += mn; worstMin = juce::jmin(worstMin, (double) mn); activeRegionsSum += active; ++frames;
                }
            }
            double meanMin = frames ? sumMin / frames : 0.0;
            double deltaRatio = inR > 1e-9 ? deltaR / inR : 0.0;
            std::printf("   Depth=%.0f: inRMS=%.4f outRMS=%.4f (%.2fdB) maxRed=%.2fdB meanRed=%.2fdB avgActiveBins=%.0f DeltaRMS/InRMS=%.3f\n",
                depth, inR, outR, lossDb, worstMin, meanMin, (double) activeRegionsSum / juce::jmax(1, frames), deltaRatio);
        }
    };
    runMaterial("Bass (55Hz pluck+sustain)", [](double s, int n){ auto b = genHarmonicSeriesStereo(s, n, 55.0, 0.3f, 8, 2.0f); addClickStereo(b, 0, (int)(s*0.004), 0.5f, 6); return b; });
    runMaterial("Kick (60Hz thump+click)", [](double s, int n){ auto b = genSilenceStereo(n); addClickStereo(b, (int)(s*0.05), (int)(s*0.003), 0.9f, 1); addDecayingResonanceStereo(b, s, 60.0, 0.8f, 0.25, (int)(s*0.05)); return b; });
    runMaterial("Vocal (140Hz + consonant)", [](double s, int n){ auto b = genHarmonicSeriesStereo(s, n, 140.0, 0.25f, 8, 2.5f); addNoiseBurstStereo(b, (int)(s*0.1), (int)(s*0.05), 0.3f, 4); return b; });
    runMaterial("Guitar (110Hz + pick attack)", [](double s, int n){ auto b = genHarmonicSeriesStereo(s, n, 110.0, 0.25f, 8, 2.5f); addClickStereo(b, 0, (int)(s*0.003), 0.5f, 5); return b; });
    runMaterial("Dense mix (bass+vocal+kick+snare)", [](double s, int n){
        auto b = genHarmonicSeriesStereo(s, n, 62.0, 0.18f, 6, 2.5f);
        auto vocal = genHarmonicSeriesStereo(s, n, 220.0, 0.13f, 8, 2.5f); for (int c=0;c<2;++c) for (int i=0;i<n;++i) b.addSample(c,i,vocal.getSample(c,i));
        addClickStereo(b, (int)(s*0.05), (int)(s*0.003), 0.6f, 9); addDecayingResonanceStereo(b, s, 60.0, 0.5f, 0.2, (int)(s*0.05));
        addNoiseBurstStereo(b, (int)(s*0.4), (int)(s*0.02), 0.35f, 10);
        return b; });

    // ---- item: cross-SR sanity ----
    std::printf("\n-- Cross-SR sanity (Depth=3, sustained + attack signal must process without NaN/Inf) --\n");
    for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        NFResonanceAudioProcessor proc; setParam(proc, "depth", 3.0f); setParam(proc, "selectivity", 3.0f); setParam(proc, "mix", 100.0f); proc.prepareToPlay(testSr, 512);
        int n = (int) (testSr * 1.0); auto sig = genHarmonicSeriesStereo(testSr, n, 110.0, 0.3f, 8); addClickStereo(sig, 0, (int)(testSr*0.003), 0.5f, 3);
        auto out = runProcessor(proc, sig, 512);
        bool finite = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) if (! std::isfinite(out.getSample(c, i))) finite = false;
        double outR = rms(out);
        std::printf("  sr=%.0f: finite=%s outRMS=%.4f %s\n", testSr, finite ? "yes" : "NO", outR, finite ? "PASS" : "FAIL");
        allOk &= finite;
    }

    // ---- item: CPU incremental (GainMaskEngine cost vs total processBlock) ----
    std::printf("\n-- CPU: incremental cost of the V2 gain mask (Depth=5 vs Depth=0, same signal) --\n");
    for (double testSr : { 48000.0, 192000.0 })
    {
        for (float depth : { 0.0f, 5.0f })
        {
            NFResonanceAudioProcessor proc; setParam(proc, "depth", depth); setParam(proc, "selectivity", 3.0f); setParam(proc, "mix", 100.0f); proc.prepareToPlay(testSr, 512);
            int n = (int) (testSr * 1.0); auto sig = genHarmonicSeriesStereo(testSr, n, 110.0, 0.3f, 8);
            AudioBuffer<float> live(sig); juce::MidiBuffer midi;
            std::vector<double> timesUs;
            for (int pos = 0; pos + 512 <= n; pos += 512)
            {
                AudioBuffer<float> blk(live.getArrayOfWritePointers(), 2, pos, 512);
                auto t0 = std::chrono::high_resolution_clock::now();
                proc.processBlock(blk, midi);
                auto t1 = std::chrono::high_resolution_clock::now();
                timesUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
            }
            std::sort(timesUs.begin(), timesUs.end());
            double med = timesUs[timesUs.size() / 2];
            double hopBudgetUs = 1.0e6 * 512 / testSr;
            std::printf("  sr=%.0f depth=%.0f: med=%.2fus (%.2f%% of hop budget)\n", testSr, depth, med, 100.0 * med / hopBudgetUs);
        }
    }

    std::printf("\n=== %s ===\n", allOk ? "ALL GAIN MASK CHECKS PASS" : "SOME GAIN MASK CHECKS FAILED");
    return allOk ? 0 : 1;
}
