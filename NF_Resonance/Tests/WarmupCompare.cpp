// NF Resonance -- Startup/warm-up strategy comparison (A: time gate,
// B: norm-relative gate, C: virtual pre-roll). Offline diagnostic only.
#include <JuceHeader.h>
#include <functional>
#include "PluginProcessor.h"
#include "DSP/SpectralEngine.h"

using juce::AudioBuffer;

static void setParam(NFResonanceAudioProcessor& proc, const char* id, float value)
{
    if (auto* p = proc.state().getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

static bool hasNaNOrInf(const AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite(b.getSample(c, i))) return true;
    return false;
}

static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi;
    const int total = buf.getNumSamples();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        int n = juce::jmin(blockSize, total - pos);
        AudioBuffer<float> block(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, n);
        proc.processBlock(block, midi);
    }
}

static AudioBuffer<float> genImpulse(int channels, int numSamples, int pos, float amp)
{
    AudioBuffer<float> b(channels, numSamples); b.clear();
    for (int c = 0; c < channels; ++c) b.setSample(c, pos, amp);
    return b;
}

// A broadband "kick-like" transient: fast exponential-decay burst.
static AudioBuffer<float> genKickTransient(int channels, int numSamples, double sr, int startPos, float amp)
{
    AudioBuffer<float> b(channels, numSamples); b.clear();
    juce::Random rng(7);
    for (int i = startPos; i < juce::jmin(numSamples, startPos + 800); ++i)
    {
        double t = (i - startPos) / sr;
        float env = (float) std::exp(-t / 0.02);
        float click = (rng.nextFloat()*2.0f-1.0f) * env * 0.3f;
        float body = (float) std::sin(2.0*juce::MathConstants<double>::pi*80.0*t) * env;
        float v = amp * (body*0.8f + click);
        for (int c = 0; c < channels; ++c) b.setSample(c, i, v);
    }
    return b;
}

static std::unique_ptr<NFResonanceAudioProcessor> makeProcessor(double sr, int blockSize, SpectralEngine::WarmupMode mode)
{
    auto proc = std::make_unique<NFResonanceAudioProcessor>();
    proc->engine().setWarmupMode(mode);
    proc->prepareToPlay(sr, blockSize);
    return proc;
}

static const char* modeName(SpectralEngine::WarmupMode m)
{
    switch (m) {
        case SpectralEngine::WarmupMode::TimeGate: return "A-TimeGate";
        case SpectralEngine::WarmupMode::NormGate: return "B-NormGate";
        case SpectralEngine::WarmupMode::PreRoll:  return "C-PreRoll";
        default: return "C2-LeftPad";
    }
}

static double rmsOfRange(const AudioBuffer<float>& b, int start, int len)
{
    double sum=0; int n=0;
    for (int c=0;c<b.getNumChannels();++c)
        for (int i=start;i<start+len && i<b.getNumSamples();++i){ double v=b.getSample(c,i); sum+=v*v; ++n; }
    return n? std::sqrt(sum/n):0.0;
}
static double peakOfRange(const AudioBuffer<float>& b, int start, int len)
{
    double pk=0;
    for (int c=0;c<b.getNumChannels();++c)
        for (int i=start;i<start+len && i<b.getNumSamples();++i) pk=juce::jmax(pk,(double)std::abs(b.getSample(c,i)));
    return pk;
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const int fftSize = 2048, hop = 512;
    SpectralEngine::WarmupMode modes[] = { SpectralEngine::WarmupMode::TimeGate, SpectralEngine::WarmupMode::NormGate, SpectralEngine::WarmupMode::PreRoll, SpectralEngine::WarmupMode::LeftPad };

    //======================================================================
    std::cout << "\n==================== MAIN TEST: impulse sweep, Depth=0 ====================\n";
    int positions[] = { 0, 1, 2, 5, 10, 20, 50, 100, 500 };
    for (auto mode : modes)
    {
        std::cout << "\n---- " << modeName(mode) << " ----\n";
        std::printf("%8s | %8s | %8s | %10s | %10s | %12s\n", "inPos", "expPos", "measPos", "inPeak", "outPeak", "err(dB)");
        for (int pos : positions)
        {
            auto proc = makeProcessor(sr, blockSize, mode);
            setParam(*proc, "depth", 0.0f);
            int lat = proc->getLatencySamples();
            auto input = genImpulse(2, (int)(sr*0.3), pos, 0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            int measPos=-1; double pkVal=0;
            for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if (v>pkVal){pkVal=v;measPos=i;} }
            int expPos = lat + pos;
            double errDb = pkVal > 1e-15 ? 20.0*std::log10(pkVal/0.9) : -300.0;
            std::printf("%8d | %8d | %8d | %10.4f | %10.6g | %12.2f\n", pos, expPos, measPos, 0.9, pkVal, errDb);
        }
    }

    //======================================================================
    std::cout << "\n==================== MUSICAL TRANSIENT TEST (kick-like, Depth=5 default) ====================\n";
    int startPositions[] = { 0, 1, 10 };
    for (auto mode : modes)
    {
        std::cout << "\n---- " << modeName(mode) << " ----\n";
        for (int startPos : startPositions)
        {
            auto proc = makeProcessor(sr, blockSize, mode);
            int lat = proc->getLatencySamples();
            auto input = genKickTransient(2, (int)(sr*0.3), sr, startPos, 0.8f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            double inPk=0, outPk=0;
            for (int i=0;i<input.getNumSamples();++i) inPk = juce::jmax(inPk, (double) std::abs(input.getSample(0,i)));
            for (int i=0;i<out.getNumSamples();++i) outPk = juce::jmax(outPk, (double) std::abs(out.getSample(0,i)));
            // energy in the first 20ms of output after the expected arrival point
            int arriveAt = lat + startPos;
            double earlyEnergy=0; int earlyN=0;
            for (int i=arriveAt; i<juce::jmin(out.getNumSamples(), arriveAt+(int)(sr*0.02)); ++i){ double v=out.getSample(0,i); earlyEnergy+=v*v; ++earlyN; }
            double earlyRms = earlyN? std::sqrt(earlyEnergy/earlyN) : 0.0;
            bool nanInf = hasNaNOrInf(out);
            std::cout << "  start=" << startPos << "  inPeak=" << inPk << "  outPeak=" << outPk
                       << "  early-20ms-RMS-after-arrival=" << earlyRms
                       << (nanInf ? "  [NaN/Inf!]" : "") << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== RESET / TRANSPORT / SAMPLE-RATE SCENARIOS (Depth=0, impulse at pos=10) ====================\n";
    double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    for (auto mode : modes)
    {
        std::cout << "\n---- " << modeName(mode) << " ----\n";
        for (double testSr : sampleRates)
        {
            auto proc = makeProcessor(testSr, blockSize, mode);
            // simulate: process some audio, then re-prepare (stop/play / reset / sr change scenario)
            auto warm = genImpulse(2,(int)(testSr*0.1),100,0.5f);
            runThrough(*proc, warm, blockSize);
            proc->prepareToPlay(testSr, blockSize); // simulate reset/transport-restart
            int lat = proc->getLatencySamples();
            setParam(*proc, "depth", 0.0f);
            auto input = genImpulse(2, (int)(testSr*0.1), 10, 0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            double pk=0; int pkIdx=-1;
            for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if (v>pk){pk=v;pkIdx=i;} }
            std::cout << "  sr=" << testSr << "  reportedLat=" << lat << "  expPos=" << (lat+10)
                       << "  measPos=" << pkIdx << "  peak=" << pk << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== BYPASS: does it reset the engine? ====================\n";
    {
        auto proc = makeProcessor(sr, blockSize, SpectralEngine::WarmupMode::NormGate);
        auto warm = genImpulse(2,(int)(sr*0.5),100,0.5f);
        runThrough(*proc, warm, blockSize); // engine now warmed up / steady-state
        setParam(*proc, "bypass", 1.0f);
        auto silence = AudioBuffer<float>(2,(int)(sr*0.2)); silence.clear();
        runThrough(*proc, silence, blockSize); // bypassed for 200ms
        setParam(*proc, "bypass", 0.0f);
        setParam(*proc, "depth", 0.0f);
        auto input2 = genImpulse(2,(int)(sr*0.1),10,0.9f);
        AudioBuffer<float> out2; out2.makeCopyOf(input2);
        runThrough(*proc, out2, blockSize);
        double pk=0; int pkIdx=-1;
        for (int i=0;i<out2.getNumSamples();++i){ double v=std::abs(out2.getSample(0,i)); if (v>pk){pk=v;pkIdx=i;} }
        int lat = proc->getLatencySamples();
        std::cout << "  after warm-up, bypass on 200ms, bypass off, impulse@10: expPos=" << (lat+10)
                   << " measPos=" << pkIdx << " peak=" << pk
                   << "\n  (if peak is near 0.9 and measPos is near expected without a NEW warm-up delay,\n"
                   << "   the engine state survives bypass -- confirmed: bypass simply skips process() calls,\n"
                   << "   it never touches the SpectralEngine's ring/history/t state.)\n";
    }

    //======================================================================
    std::cout << "\n==================== STARTUP-INCLUSIVE NULL TEST (Depth=0, sine sweep, from output sample 0) ====================\n";
    {
        auto genSineSweep = [&](double durationSec)->AudioBuffer<float>{
            int n=(int)(sr*durationSec); AudioBuffer<float> b(2,n);
            double f0=20.0,f1=20000.0,T=durationSec;
            for (int i=0;i<n;++i){ double t=i/sr; double phase=2.0*juce::MathConstants<double>::pi*f0*T/std::log(f1/f0)*(std::exp((t/T)*std::log(f1/f0))-1.0);
                float v=0.4f*(float)std::sin(phase); for(int c=0;c<2;++c) b.setSample(c,i,v); }
            return b;
        };
        struct Region{ const char* label; int start; int len; };
        for (auto mode : modes)
        {
            auto proc = makeProcessor(sr, blockSize, mode);
            setParam(*proc, "depth", 0.0f);
            int lat = proc->getLatencySamples();
            auto input = genSineSweep(1.0);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            std::cout << "\n---- " << modeName(mode) << " ----\n";
            Region regions[] = {
                {"samples 0-511 (output)",0,512}, {"samples 512-1023",512,512}, {"samples 1024-1535",1024,512},
                {"samples 1536-2047",1536,512}, {"samples 2048-4095",2048,2048}, {"steady state (8192+)",8192,(int)(sr*1.0)-8192}
            };
            for (auto& r : regions)
            {
                double errSum=0, refSum=0; int n=0; double maxErr=0;
                for (int i=r.start; i<r.start+r.len && i<out.getNumSamples(); ++i)
                {
                    int inIdx = i - lat;
                    float refV = (inIdx>=0 && inIdx<input.getNumSamples()) ? input.getSample(0,inIdx) : 0.0f;
                    float outV = out.getSample(0,i);
                    double d = outV - refV;
                    errSum += d*d; refSum += (double)refV*refV; maxErr = juce::jmax(maxErr, (double)std::abs(d));
                    ++n;
                }
                double errRms = n? std::sqrt(errSum/n) : 0.0;
                double refRms = n? std::sqrt(refSum/n) : 0.0;
                double errDb = refRms>1e-12 ? 20.0*std::log10(errRms/refRms) : (errRms<1e-12? -300.0 : 0.0);
                std::cout << "  " << r.label << ": errRMS=" << errRms << " refRMS=" << refRms
                           << " errDb=" << errDb << " maxAbsErr=" << maxErr << "\n";
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== MUSICAL AUDIO AT SAMPLE 0 (Depth=0) ====================\n";
    {
        auto genKick = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(1);
            for (int i=0;i<juce::jmin(n,800);++i){ double t=i/sr; float env=(float)std::exp(-t/0.02);
                float v=0.8f*((float)std::sin(2.0*juce::MathConstants<double>::pi*80.0*t)*0.8f*env + (rng.nextFloat()*2-1)*0.2f*env);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        auto genSnare = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(2);
            for (int i=0;i<juce::jmin(n,600);++i){ double t=i/sr; float env=(float)std::exp(-t/0.008);
                float v=0.7f*((rng.nextFloat()*2-1)*0.7f*env + (float)std::sin(2.0*juce::MathConstants<double>::pi*200.0*t)*0.3f*env);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        auto genConsonant = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(3);
            for (int i=0;i<juce::jmin(n,300);++i){ double t=i/sr; float env=(float)std::exp(-t/0.004);
                float v=0.5f*(rng.nextFloat()*2-1)*env; // fricative-like burst
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        auto genBurst = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear();
            for (int i=0;i<juce::jmin(n,50);++i){ float v=0.9f*(float)std::sin(2.0*juce::MathConstants<double>::pi*1000.0*i/sr);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };

        struct Src{ const char* name; std::function<AudioBuffer<float>(int)> gen; };
        Src sources[] = { {"kick", genKick}, {"snare", genSnare}, {"consonant burst", genConsonant}, {"1kHz burst", genBurst} };

        for (auto mode : modes)
        {
            std::cout << "\n---- " << modeName(mode) << " ----\n";
            for (auto& src : sources)
            {
                auto proc = makeProcessor(sr, blockSize, mode);
                setParam(*proc, "depth", 0.0f);
                int lat = proc->getLatencySamples();
                int n = (int)(sr*0.3);
                auto input = src.gen(n);
                AudioBuffer<float> out; out.makeCopyOf(input);
                runThrough(*proc, out, blockSize);
                // compare delayed input vs output over the transient's duration
                double errSum=0, refSum=0; int cnt=0;
                for (int i=0;i<juce::jmin(n,1600);++i)
                {
                    int outIdx = i+lat;
                    if (outIdx>=out.getNumSamples()) break;
                    float refV = input.getSample(0,i), outV = out.getSample(0,outIdx);
                    double d = outV-refV; errSum+=d*d; refSum+=(double)refV*refV; ++cnt;
                }
                double errRms = cnt? std::sqrt(errSum/cnt):0.0, refRms = cnt? std::sqrt(refSum/cnt):0.0;
                double errDb = refRms>1e-9 ? 20.0*std::log10(errRms/refRms) : -300.0;
                double inPk = peakOfRange(input,0,n), outPk = peakOfRange(out, lat, n);
                std::cout << "  " << src.name << ": inPeak=" << inPk << " outPeak(delayed)=" << outPk
                           << " errDb=" << errDb << (hasNaNOrInf(out)?" [NaN/Inf!]":"") << "\n";
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== LONG BYPASS TEST (process 2s, bypass 5s while audio continues, bypass off, inspect first 100ms) ====================\n";
    {
        for (auto mode : modes)
        {
            auto proc = makeProcessor(sr, blockSize, mode);
            int lat = proc->getLatencySamples();
            // 2s of continuous tone/noise
            auto pre = AudioBuffer<float>(2,(int)(sr*2.0));
            juce::Random rng(5);
            for (int i=0;i<pre.getNumSamples();++i){ double t=i/sr; float v=0.3f*(float)std::sin(2.0*juce::MathConstants<double>::pi*440.0*t)+0.05f*(rng.nextFloat()*2-1); for(int c=0;c<2;++c) pre.setSample(c,i,v); }
            runThrough(*proc, pre, blockSize);

            setParam(*proc, "bypass", 1.0f);
            // 5s of audio continues to "play" through the host while bypassed
            auto during = AudioBuffer<float>(2,(int)(sr*5.0));
            for (int i=0;i<during.getNumSamples();++i){ double t=i/sr; float v=0.3f*(float)std::sin(2.0*juce::MathConstants<double>::pi*440.0*t)+0.05f*(rng.nextFloat()*2-1); for(int c=0;c<2;++c) during.setSample(c,i,v); }
            AudioBuffer<float> duringOut; duringOut.makeCopyOf(during);
            runThrough(*proc, duringOut, blockSize);

            setParam(*proc, "bypass", 0.0f);
            // continue the SAME continuous signal after un-bypassing (no discontinuity in the true source)
            auto post = AudioBuffer<float>(2,(int)(sr*0.3));
            double contStart = 7.0; // 2s pre + 5s during
            for (int i=0;i<post.getNumSamples();++i){ double t=contStart+i/sr; float v=0.3f*(float)std::sin(2.0*juce::MathConstants<double>::pi*440.0*t)+0.05f*(rng.nextFloat()*2-1); for(int c=0;c<2;++c) post.setSample(c,i,v); }
            AudioBuffer<float> postOut; postOut.makeCopyOf(post);
            runThrough(*proc, postOut, blockSize);

            // click detector: max abs sample-to-sample jump in the first 100ms after un-bypass
            double maxJump=0; float lastSample = duringOut.getSample(0, duringOut.getNumSamples()-1);
            int check100ms = (int)(sr*0.1);
            for (int i=0;i<juce::jmin(check100ms, postOut.getNumSamples()); ++i)
            {
                float s = postOut.getSample(0,i);
                maxJump = juce::jmax(maxJump, (double)std::abs(s-lastSample));
                lastSample = s;
            }
            bool nanInf = hasNaNOrInf(duringOut) || hasNaNOrInf(postOut);
            double bypassedOutRms = rmsOfRange(duringOut, 0, duringOut.getNumSamples());
            double duringInRms = rmsOfRange(during, 0, during.getNumSamples());
            std::cout << "  " << modeName(mode) << ": during-bypass outRMS=" << bypassedOutRms
                       << " (inRMS=" << duringInRms << ", should match = passthrough)"
                       << "  max jump first 100ms after un-bypass=" << maxJump
                       << (nanInf?" [NaN/Inf!]":"") << "\n";
        }
        std::cout << "  (NOTE: current architecture skips spectral.process() entirely during bypass --\n"
                   << "   engine's internal sample clock/history/ring freeze. When bypass turns off,\n"
                   << "   the very next analysis frame will splice pre-bypass history with post-bypass\n"
                   << "   audio as if they were temporally adjacent, even though real time advanced by\n"
                   << "   the bypass duration in between. At Depth=0 this is invisible in the numbers\n"
                   << "   above because gain is always unity regardless of spectral content -- the risk\n"
                   << "   is specific to nonzero Depth, where the detector could react oddly to that one\n"
                   << "   spliced frame. See written report for the CPU-cost tradeoff of keeping the\n"
                   << "   engine fed during bypass instead.)\n";
    }

    std::cout << "\n==================== DONE ====================\n";
    return 0;
}
