// NF Resonance -- DAW Preview 0.1b isolated diagnostic battery.
// Offline console harness: instantiates the real AudioProcessor (checkpoint
// ee181d6) and quantifies the six issues reported from the first LUNA test.
// Read-only investigation -- makes no DSP/param changes to the Preview.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DSP/SpectralEngine.h"

using juce::AudioBuffer;

//==============================================================================
static void setParam(NFResonanceAudioProcessor& proc, const char* id, float value)
{
    if (auto* p = proc.state().getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

static std::unique_ptr<NFResonanceAudioProcessor> makeProcessor(double sr, int blockSize)
{
    auto proc = std::make_unique<NFResonanceAudioProcessor>();
    proc->prepareToPlay(sr, blockSize);
    return proc;
}

static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi;
    int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += blockSize)
    {
        int len = juce::jmin(blockSize, n - pos);
        AudioBuffer<float> blk(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, len);
        proc.processBlock(blk, midi);
    }
}

static double rmsOf(const AudioBuffer<float>& b, int start = 0, int len = -1)
{
    if (len < 0) len = b.getNumSamples() - start;
    double s = 0; long n = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i) { double v = b.getSample(c, i); s += v * v; ++n; }
    return n ? std::sqrt(s / (double) n) : 0.0;
}
static double peakOf(const AudioBuffer<float>& b, int start = 0, int len = -1)
{
    if (len < 0) len = b.getNumSamples() - start;
    double pk = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i) pk = juce::jmax(pk, (double) std::abs(b.getSample(c, i)));
    return pk;
}
static double toDb(double lin) { return 20.0 * std::log10(juce::jmax(lin, 1e-12)); }

// delayedInput[i] = input[i - lat], zero before that -- what an ideal, purely
// latency-delayed passthrough would look like, i.e. the null-test reference.
static AudioBuffer<float> delayBy(const AudioBuffer<float>& in, int lat)
{
    AudioBuffer<float> out(in.getNumChannels(), in.getNumSamples());
    out.clear();
    for (int c = 0; c < in.getNumChannels(); ++c)
        for (int i = lat; i < in.getNumSamples(); ++i)
            out.setSample(c, i, in.getSample(c, i - lat));
    return out;
}
static AudioBuffer<float> diffOf(const AudioBuffer<float>& a, const AudioBuffer<float>& b)
{
    AudioBuffer<float> out(a.getNumChannels(), a.getNumSamples());
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            out.setSample(c, i, a.getSample(c, i) - b.getSample(c, i));
    return out;
}

//==============================================================================
// Vocal-like synthetic signal: harmonic stack at f0=180Hz (18 harmonics,
// 1/h decay) + noise shaped through 3 parallel formant bandpass filters
// (~700/1200/2600Hz), the same class of broadband/formant content the V1
// detector is known to mis-treat as resonance.
static AudioBuffer<float> genVocalLike(int channels, int n, double sr, float amp = 0.3f)
{
    AudioBuffer<float> b(channels, n);
    b.clear();
    const double f0 = 180.0;
    const int nHarm = 18;
    for (int h = 1; h <= nHarm; ++h)
    {
        double freq = f0 * h;
        if (freq >= sr * 0.45) break;
        float hAmp = (float) (1.0 / h) * (h == 1 ? 1.0f : 0.85f);
        double inc = juce::MathConstants<double>::twoPi * freq / sr, ph = 0.0;
        for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * hAmp; for (int c = 0; c < channels; ++c) b.addSample(c, i, s); ph += inc; }
    }
    juce::Random rng(9001);
    AudioBuffer<float> noise(channels, n);
    for (int c = 0; c < channels; ++c) for (int i = 0; i < n; ++i) noise.setSample(c, i, rng.nextFloat() * 2.0f - 1.0f);
    struct Formant { double hz, q, level; };
    Formant formants[3] = { {700.0, 6.0, 0.22}, {1200.0, 7.0, 0.16}, {2600.0, 8.0, 0.10} };
    for (auto& fo : formants)
        for (int c = 0; c < channels; ++c)
        {
            juce::dsp::IIR::Filter<float> filt;
            filt.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, fo.hz, fo.q);
            for (int i = 0; i < n; ++i) { float x = noise.getSample(c, i); float y = filt.processSample(x); b.addSample(c, i, y * (float) fo.level); }
        }
    double pk = peakOf(b);
    if (pk > 1e-9) b.applyGain((float) (amp / pk));
    return b;
}

// Mono broadband-noise-floor + a strong narrowband spike -- deliberately
// built so the spike sits far enough above the local mean to guarantee
// detector engagement (used for the M/S resonance-isolation test).
static AudioBuffer<float> genResonantMono(int n, double sr, float noiseAmp, float spikeAmp, double spikeHz)
{
    AudioBuffer<float> b(1, n);
    juce::Random rng(4242);
    for (int i = 0; i < n; ++i) b.setSample(0, i, (rng.nextFloat() * 2.0f - 1.0f) * noiseAmp);
    double ph = 0, inc = juce::MathConstants<double>::twoPi * spikeHz / sr;
    for (int i = 0; i < n; ++i) { b.addSample(0, i, (float) std::sin(ph) * spikeAmp); ph += inc; }
    return b;
}
static AudioBuffer<float> encodeMidOnly(const AudioBuffer<float>& mono)
{ AudioBuffer<float> b(2, mono.getNumSamples()); for (int i = 0; i < mono.getNumSamples(); ++i) { float x = mono.getSample(0, i); b.setSample(0, i, x); b.setSample(1, i, x); } return b; }
static AudioBuffer<float> encodeSideOnly(const AudioBuffer<float>& mono)
{ AudioBuffer<float> b(2, mono.getNumSamples()); for (int i = 0; i < mono.getNumSamples(); ++i) { float x = mono.getSample(0, i); b.setSample(0, i, x); b.setSample(1, i, -x); } return b; }
static AudioBuffer<float> decodeM(const AudioBuffer<float>& lr)
{ AudioBuffer<float> m(1, lr.getNumSamples()); for (int i = 0; i < lr.getNumSamples(); ++i) m.setSample(0, i, (lr.getSample(0, i) + lr.getSample(1, i)) * 0.70710678f); return m; }
static AudioBuffer<float> decodeS(const AudioBuffer<float>& lr)
{ AudioBuffer<float> s(1, lr.getNumSamples()); for (int i = 0; i < lr.getNumSamples(); ++i) s.setSample(0, i, (lr.getSample(0, i) - lr.getSample(1, i)) * 0.70710678f); return s; }

//==============================================================================
int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const double durationSec = 4.0;
    const int n = (int) (sr * durationSec);

    std::printf("================================================================\n");
    std::printf("NF Resonance -- DAW Preview 0.1b isolated diagnostic (ee181d6)\n");
    std::printf("sr=%.0f block=%d duration=%.1fs (%d samples)\n", sr, blockSize, durationSec, n);
    std::printf("================================================================\n\n");

    // Reference signal + latency, established once.
    auto probe = makeProcessor(sr, blockSize);
    const int lat = probe->getLatencySamples();
    std::printf("Reported latency: %d samples (%.2f ms)\n\n", lat, 1000.0 * lat / sr);

    AudioBuffer<float> vocalOrig = genVocalLike(2, n, sr);
    // Measurement window: skip latency + 0.6s settle (attack/release + AR smoothing).
    const int measStart = lat + (int) (0.6 * sr);
    const int measLen = n - measStart;
    AudioBuffer<float> vocalDelayed = delayBy(vocalOrig, lat);
    const double inputRmsSteady = rmsOf(vocalOrig, measStart - lat, measLen);
    const double inputPeakSteady = peakOf(vocalOrig, measStart - lat, measLen);

    //==========================================================================
    std::printf("---- 1. UNITY GAIN TEST (Depth=0, Mix=100, Delta=OFF, Output=0dB) ----\n");
    {
        auto proc = makeProcessor(sr, blockSize);
        setParam(*proc, "depth", 0); setParam(*proc, "mix", 100); setParam(*proc, "delta", 0);
        setParam(*proc, "output", 0); setParam(*proc, "bypass", 0);
        AudioBuffer<float> active = vocalOrig; runThrough(*proc, active, blockSize);

        auto procByp = makeProcessor(sr, blockSize);
        setParam(*procByp, "depth", 0); setParam(*procByp, "mix", 100); setParam(*procByp, "delta", 0);
        setParam(*procByp, "output", 0); setParam(*procByp, "bypass", 1);
        AudioBuffer<float> bypassed = vocalOrig; runThrough(*procByp, bypassed, blockSize);

        double outRms = rmsOf(active, measStart, measLen), outPeak = peakOf(active, measStart, measLen);
        double bypRms = rmsOf(bypassed, measStart, measLen), bypPeak = peakOf(bypassed, measStart, measLen);
        double idealRms = rmsOf(vocalDelayed, measStart, measLen), idealPeak = peakOf(vocalDelayed, measStart, measLen);

        AudioBuffer<float> nullVsIdeal = diffOf(active, vocalDelayed);
        AudioBuffer<float> nullVsBypass = diffOf(active, bypassed);
        double nullIdealRms = rmsOf(nullVsIdeal, measStart, measLen);
        double nullBypassRms = rmsOf(nullVsBypass, measStart, measLen);

        std::printf("  input RMS (steady)          : %.6f (%.2f dBFS)\n", inputRmsSteady, toDb(inputRmsSteady));
        std::printf("  input peak (steady)         : %.6f (%.2f dBFS)\n", inputPeakSteady, toDb(inputPeakSteady));
        std::printf("  output RMS  active           : %.6f (%.2f dBFS)\n", outRms, toDb(outRms));
        std::printf("  output peak active           : %.6f (%.2f dBFS)\n", outPeak, toDb(outPeak));
        std::printf("  output RMS  plugin-bypass    : %.6f (%.2f dBFS)\n", bypRms, toDb(bypRms));
        std::printf("  output RMS  ideal delayed-in : %.6f (%.2f dBFS)\n", idealRms, toDb(idealRms));
        std::printf("  gain diff  active vs ideal   : %+.4f dB\n", toDb(outRms) - toDb(idealRms));
        std::printf("  gain diff  active vs bypass  : %+.4f dB\n", toDb(outRms) - toDb(bypRms));
        std::printf("  NULL active vs ideal delayed input : %.6f (%.2f dBFS, %.2f dB below input)\n",
                     nullIdealRms, toDb(nullIdealRms), toDb(inputRmsSteady) - toDb(nullIdealRms));
        std::printf("  NULL active vs plugin bypass        : %.6f (%.2f dBFS, %.2f dB below input)\n",
                     nullBypassRms, toDb(nullBypassRms), toDb(inputRmsSteady) - toDb(nullBypassRms));
        std::printf("\n");
    }

    //==========================================================================
    std::printf("---- 2. DEPTH GAIN-LOSS TEST (Mix=100, Delta=OFF, Output=0dB) ----\n");
    std::printf("  %-6s | %-14s | %-14s | %-12s | %-10s\n", "Depth", "input RMS", "processed RMS", "delta RMS", "gain diff");
    {
        const float depths[] = { 0, 1, 3, 5, 7, 10 };
        for (float d : depths)
        {
            auto proc = makeProcessor(sr, blockSize);
            setParam(*proc, "depth", d); setParam(*proc, "mix", 100); setParam(*proc, "delta", 0);
            setParam(*proc, "output", 0); setParam(*proc, "bypass", 0);
            AudioBuffer<float> out = vocalOrig; runThrough(*proc, out, blockSize);
            double outRms = rmsOf(out, measStart, measLen);
            AudioBuffer<float> removed = diffOf(vocalDelayed, out);
            double removedRms = rmsOf(removed, measStart, measLen);
            double gainDiffDb = toDb(outRms) - toDb(inputRmsSteady);
            std::printf("  %-6.1f | %-14.6f | %-14.6f | %-12.6f | %+7.4f dB\n", d, inputRmsSteady, outRms, removedRms, gainDiffDb);
        }
        std::printf("\n");
    }

    //==========================================================================
    std::printf("---- 3. DELTA SEMANTICS: Delta=ON, sweep Mix (Depth=5, Output=0dB) ----\n");
    std::printf("  %-6s | %-14s | %-14s | %-10s\n", "Mix%", "output RMS", "output peak", "vs Mix=0");
    {
        double refRms = -1;
        const float mixes[] = { 0, 25, 50, 75, 100 };
        for (float mx : mixes)
        {
            auto proc = makeProcessor(sr, blockSize);
            setParam(*proc, "depth", 5); setParam(*proc, "mix", mx); setParam(*proc, "delta", 1);
            setParam(*proc, "output", 0); setParam(*proc, "bypass", 0);
            AudioBuffer<float> out = vocalOrig; runThrough(*proc, out, blockSize);
            double outRms = rmsOf(out, measStart, measLen), outPeak = peakOf(out, measStart, measLen);
            if (refRms < 0) refRms = outRms;
            std::printf("  %-6.0f | %-14.6f | %-14.6f | %+8.4f dB\n", mx, outRms, outPeak, toDb(outRms) - toDb(refRms));
        }
        std::printf("  (PluginProcessor::processBlock: when params.delta==true, \"processed\" is\n");
        std::printf("   assigned directly from the engine's wet buffer -- \"mix\" is only read inside\n");
        std::printf("   the else-branch of that ternary, so at the code level Mix cannot reach the\n");
        std::printf("   Delta path. See Source/PluginProcessor.cpp, processBlock(), the\n");
        std::printf("   \"processed = p.delta ? wet : ...\" line.)\n\n");
    }

    //==========================================================================
    std::printf("---- 4. DELTA CONTENT: energy fraction into Delta at Depth=3 / Depth=5 ----\n");
    std::printf("  %-6s | %-14s | %-14s | %-12s | %-10s\n", "Depth", "orig RMS", "delta RMS", "fraction", "delta dB");
    {
        const float depths[] = { 3, 5 };
        for (float d : depths)
        {
            auto proc = makeProcessor(sr, blockSize);
            setParam(*proc, "depth", d); setParam(*proc, "mix", 100); setParam(*proc, "delta", 1);
            setParam(*proc, "output", 0); setParam(*proc, "bypass", 0);
            AudioBuffer<float> out = vocalOrig; runThrough(*proc, out, blockSize);
            double deltaRms = rmsOf(out, measStart, measLen);
            double fraction = (deltaRms * deltaRms) / (inputRmsSteady * inputRmsSteady);
            std::printf("  %-6.1f | %-14.6f | %-14.6f | %-12.4f | %+7.4f dB\n", d, inputRmsSteady, deltaRms, fraction, toDb(deltaRms) - toDb(inputRmsSteady));
        }
        std::printf("\n");
    }

    //==========================================================================
    std::printf("---- 5. MID/SIDE MATRIX CORRECTNESS ----\n");
    {
        // 5a/5b: pure central / pure side tone, Depth=0 (matrix-only check).
        AudioBuffer<float> toneMono(1, n);
        { double ph = 0, inc = juce::MathConstants<double>::twoPi * 1000.0 / sr;
          for (int i = 0; i < n; ++i) { toneMono.setSample(0, i, 0.3f * (float) std::sin(ph)); ph += inc; } }
        AudioBuffer<float> centralIn = encodeMidOnly(toneMono);
        AudioBuffer<float> sideIn = encodeSideOnly(toneMono);

        auto runMS = [&](AudioBuffer<float> in, float depth) {
            auto proc = makeProcessor(sr, blockSize);
            setParam(*proc, "mode", 2); setParam(*proc, "depth", depth); setParam(*proc, "mix", 100);
            setParam(*proc, "delta", 0); setParam(*proc, "output", 0); setParam(*proc, "bypass", 0);
            runThrough(*proc, in, blockSize);
            return in;
        };

        auto centralOut = runMS(centralIn, 0.0f);
        auto sOfCentral = decodeS(centralOut);
        double sideLeakPeak = peakOf(sOfCentral, measStart, measLen);
        std::printf("  5a. Central-only in, mode=M/S, Depth=0:\n");
        std::printf("      decoded Side leakage peak: %.8f (%.1f dBFS) -- should be ~ -inf / numerical floor\n",
                     sideLeakPeak, toDb(sideLeakPeak));

        auto sideOut = runMS(sideIn, 0.0f);
        auto mOfSide = decodeM(sideOut);
        double midLeakPeak = peakOf(mOfSide, measStart, measLen);
        std::printf("  5b. Side-only in, mode=M/S, Depth=0:\n");
        std::printf("      decoded Mid leakage peak : %.8f (%.1f dBFS) -- should be ~ -inf / numerical floor\n",
                     midLeakPeak, toDb(midLeakPeak));

        // 5c/5d: resonance-bearing content isolated to Mid-only or Side-only,
        // depth=6 so the reduction is clearly measurable; compare against the
        // same content at depth=0 to quantify how much reduction actually engaged.
        AudioBuffer<float> resMono = genResonantMono(n, sr, 0.03f, 0.3f, 2500.0);
        AudioBuffer<float> midResIn = encodeMidOnly(resMono);
        AudioBuffer<float> midResIn0 = midResIn; // copy before processing
        auto midResOut0 = runMS(midResIn0, 0.0f);
        auto midResOut6 = runMS(midResIn, 6.0f);
        double midRefRms = rmsOf(decodeM(midResOut0), measStart, measLen);
        double midProcRms = rmsOf(decodeM(midResOut6), measStart, measLen);
        double sideLeakPeak2 = peakOf(decodeS(midResOut6), measStart, measLen);
        std::printf("  5c. Resonance ONLY in Mid, Depth=6 vs Depth=0 reference:\n");
        std::printf("      Mid RMS  depth=0: %.6f | depth=6: %.6f | reduction: %+.4f dB\n",
                     midRefRms, midProcRms, toDb(midProcRms) - toDb(midRefRms));
        std::printf("      Side leakage peak while Mid is being reduced: %.8f (%.1f dBFS)\n", sideLeakPeak2, toDb(sideLeakPeak2));

        AudioBuffer<float> sideResIn = encodeSideOnly(resMono);
        AudioBuffer<float> sideResIn0 = sideResIn;
        auto sideResOut0 = runMS(sideResIn0, 0.0f);
        auto sideResOut6 = runMS(sideResIn, 6.0f);
        double sideRefRms = rmsOf(decodeS(sideResOut0), measStart, measLen);
        double sideProcRms = rmsOf(decodeS(sideResOut6), measStart, measLen);
        double midLeakPeak2 = peakOf(decodeM(sideResOut6), measStart, measLen);
        std::printf("  5d. Resonance ONLY in Side, Depth=6 vs Depth=0 reference:\n");
        std::printf("      Side RMS depth=0: %.6f | depth=6: %.6f | reduction: %+.4f dB\n",
                     sideRefRms, sideProcRms, toDb(sideProcRms) - toDb(sideRefRms));
        std::printf("      Mid leakage peak while Side is being reduced: %.8f (%.1f dBFS)\n", midLeakPeak2, toDb(midLeakPeak2));
        std::printf("\n");
    }

    std::printf("================================================================\n");
    std::printf("Diagnostic complete. No production/DSP files were modified by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
