// NF Resonance -- final validation of C2 (true left-padded STFT) +
// algorithmic latency validity boundary, ahead of the production checkpoint.
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
    for (int c=0;c<b.getNumChannels();++c) for (int i=0;i<b.getNumSamples();++i)
        if (! std::isfinite(b.getSample(c,i))) return true;
    return false;
}
static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi; const int total = buf.getNumSamples();
    for (int pos=0; pos<total; pos+=blockSize)
    {
        int n = juce::jmin(blockSize, total-pos);
        AudioBuffer<float> block(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, n);
        proc.processBlock(block, midi);
    }
}
static AudioBuffer<float> genImpulse(int n, int pos, float amp)
{
    AudioBuffer<float> b(2,n); b.clear(); for(int c=0;c<2;++c) b.setSample(c,pos,amp); return b;
}
static double rms(const AudioBuffer<float>& b, int start, int len)
{
    double sum=0; int n=0;
    for (int c=0;c<b.getNumChannels();++c) for(int i=start;i<start+len && i<b.getNumSamples() && i>=0;++i){ double v=b.getSample(c,i); sum+=v*v; ++n; }
    return n?std::sqrt(sum/n):0.0;
}
static double peak(const AudioBuffer<float>& b, int start, int len)
{
    double pk=0; for (int c=0;c<b.getNumChannels();++c) for(int i=start;i<start+len && i<b.getNumSamples() && i>=0;++i) pk=juce::jmax(pk,(double)std::abs(b.getSample(c,i)));
    return pk;
}
static double toDb(double lin){ return 20.0*std::log10(juce::jmax(lin,1e-15)); }

static std::unique_ptr<NFResonanceAudioProcessor> makeProc(double sr, int blockSize)
{
    auto proc = std::make_unique<NFResonanceAudioProcessor>();
    proc->prepareToPlay(sr, blockSize);
    return proc;
}

int gPass=0, gFail=0;
static void check(bool cond, const std::string& label){ if(cond){std::cout<<"  PASS - "<<label<<"\n";++gPass;} else {std::cout<<"  FAIL - "<<label<<"\n";++gFail;} }

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const int fftSize = 2048;

    //======================================================================
    std::cout << "\n==================== BOUNDARY TEST: output[2046..2050] ====================\n";
    {
        auto proc = makeProc(sr, blockSize);
        setParam(*proc, "depth", 0.0f);
        auto input = genImpulse((int)(sr*0.3), 0, 0.9f); // only tests sample 0; also check 1,2 via separate impulses below
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);
        for (int s : {2044,2045,2046,2047,2048,2049,2050,2051})
            std::cout << "  output[" << s << "] = " << out.getSample(0,s) << "\n";
        check(out.getSample(0,2046)==0.0f, "output[2046] == 0");
        check(out.getSample(0,2047)==0.0f, "output[2047] == 0");
        check(std::abs(out.getSample(0,2048)-0.9f) < 1e-4f, "output[2048] ~= 0.9 (input[0])");

        // input[1] -> output[2049], input[2] -> output[2050]
        for (int pos : {1,2})
        {
            auto p2 = makeProc(sr, blockSize);
            setParam(*p2, "depth", 0.0f);
            auto in2 = genImpulse((int)(sr*0.3), pos, 0.9f);
            AudioBuffer<float> o2; o2.makeCopyOf(in2);
            runThrough(*p2, o2, blockSize);
            int expected = 2048+pos;
            std::cout << "  input[" << pos << "] -> output[" << expected << "] = " << o2.getSample(0,expected) << "\n";
            check(std::abs(o2.getSample(0,expected)-0.9f) < 1e-4f, juce::String("output[" + juce::String(expected) + "] ~= 0.9 (input[" + juce::String(pos) + "])").toStdString());
            // and confirm no off-by-one: the sample BEFORE should not carry it
            check(std::abs(o2.getSample(0,expected-1)) < 1e-4f, juce::String("output[" + juce::String(expected-1) + "] does not carry input[" + juce::String(pos) + "] (no off-by-one)").toStdString());
        }
    }

    //======================================================================
    std::cout << "\n==================== IMPULSE TABLE (Depth=0, peak=0.9) ====================\n";
    {
        int positions[] = {0,1,2,5,10,20,50,100,500};
        std::printf("%8s | %10s | %10s | %10s | %12s\n","inPos","expPos","measPos","outPeak","err(dB)");
        for (int pos : positions)
        {
            auto proc = makeProc(sr, blockSize);
            setParam(*proc, "depth", 0.0f);
            int lat = proc->getLatencySamples();
            auto input = genImpulse((int)(sr*0.3), pos, 0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            int measPos=-1; double pkVal=0;
            for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if(v>pkVal){pkVal=v;measPos=i;} }
            int expPos = lat+pos;
            double errDb = toDb(pkVal/0.9);
            std::printf("%8d | %10d | %10d | %10.6f | %12.4f\n", pos, expPos, measPos, pkVal, errDb);
            check(measPos==expPos, "position exact for pos="+std::to_string(pos));
            check(std::abs(pkVal-0.9)<0.001, "amplitude ~=0.9 for pos="+std::to_string(pos));
        }
    }

    //======================================================================
    std::cout << "\n==================== STARTUP-INCLUSIVE NULL (Depth=0, no discard, sine sweep) ====================\n";
    {
        auto genSweep = [&](double dur)->AudioBuffer<float>{
            int n=(int)(sr*dur); AudioBuffer<float> b(2,n);
            double f0=20,f1=20000,T=dur;
            for (int i=0;i<n;++i){ double t=i/sr; double ph=2.0*juce::MathConstants<double>::pi*f0*T/std::log(f1/f0)*(std::exp((t/T)*std::log(f1/f0))-1.0);
                float v=0.4f*(float)std::sin(ph); for(int c=0;c<2;++c) b.setSample(c,i,v);} return b;
        };
        auto proc = makeProc(sr, blockSize);
        setParam(*proc, "depth", 0.0f);
        int lat = proc->getLatencySamples();
        auto input = genSweep(1.0);
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);
        struct Region{ const char* label; int start; int len; };
        Region regions[] = {
            {"0-511",0,512},{"512-1023",512,512},{"1024-1535",1024,512},{"1536-2047",1536,512},
            {"2048-4095",2048,2048},{"steady(8192+)",8192,(int)(sr*1.0)-8192}
        };
        for (auto& r : regions)
        {
            double errSum=0, refSum=0, maxErr=0, maxRef=0; int n=0;
            for (int i=r.start;i<r.start+r.len && i<out.getNumSamples();++i)
            {
                int inIdx=i-lat; float refV=(inIdx>=0&&inIdx<input.getNumSamples())?input.getSample(0,inIdx):0.0f;
                float outV=out.getSample(0,i);
                double d=outV-refV; errSum+=d*d; refSum+=(double)refV*refV;
                maxErr=juce::jmax(maxErr,(double)std::abs(d)); maxRef=juce::jmax(maxRef,(double)std::abs(refV));
                ++n;
            }
            double errRms=n?std::sqrt(errSum/n):0.0, refRms=n?std::sqrt(refSum/n):0.0;
            double errDb = refRms>1e-12? toDb(errRms/refRms) : (errRms<1e-12?-300.0:0.0);
            std::cout << "  " << r.label << ": RMSerr=" << errRms << " peakErr=" << maxErr << " refPeak=" << maxRef << " nullDb=" << errDb << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== DEPTH SWEEP: pre-latency=0? post-latency overshoot? ====================\n";
    {
        auto genKick = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(1);
            for (int i=0;i<juce::jmin(n,800);++i){ double t=i/sr; float env=(float)std::exp(-t/0.02);
                float v=0.8f*((float)std::sin(2.0*juce::MathConstants<double>::pi*80.0*t)*0.8f*env + (rng.nextFloat()*2-1)*0.2f*env);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        float depths[] = {0,1,3,5,10};
        for (float d : depths)
        {
            auto proc = makeProc(sr, blockSize);
            setParam(*proc, "depth", d);
            int lat = proc->getLatencySamples();
            auto input = genKick((int)(sr*0.3));
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            double preLatPeak = peak(out, 0, fftSize);
            double postLatPeak = peak(out, fftSize, out.getNumSamples()-fftSize);
            double inPk = peak(input, 0, input.getNumSamples());
            std::cout << "  Depth=" << d << ": pre-latency(<2048) peak=" << preLatPeak
                       << "  post-latency(>=2048) peak=" << postLatPeak << " (input peak=" << inPk << ")\n";
            check(preLatPeak==0.0, "Depth="+std::to_string((int)d)+": pre-latency output is exactly 0");
            check(postLatPeak <= inPk*1.15, "Depth="+std::to_string((int)d)+": no overshoot post-latency");
        }
    }

    //======================================================================
    std::cout << "\n==================== DELTA: boundary + identity ====================\n";
    {
        auto proc = makeProc(sr, blockSize);
        setParam(*proc, "delta", 1.0f);
        auto input = genImpulse((int)(sr*0.3), 10, 0.9f);
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);
        double preLatPeak = peak(out, 0, fftSize);
        std::cout << "  Delta pre-latency(<2048) peak=" << preLatPeak << "\n";
        check(preLatPeak==0.0, "Delta output is exactly 0 before latency boundary");

        // Identity: delayedOriginal ~= processed + delta
        auto genTwoTone = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); juce::Random rng(3);
            for(int i=0;i<n;++i){ double t=i/sr; float v=0.25f*(float)std::sin(2.0*juce::MathConstants<double>::pi*440.0*t)+0.2f*(float)std::sin(2.0*juce::MathConstants<double>::pi*3000.0*t)+0.05f*(rng.nextFloat()*2-1);
                for(int c=0;c<2;++c)b.setSample(c,i,v);} return b; };
        auto orig = genTwoTone((int)(sr*1.5));
        auto procWet = makeProc(sr, blockSize); AudioBuffer<float> wet; wet.makeCopyOf(orig); runThrough(*procWet, wet, blockSize);
        auto procDelta = makeProc(sr, blockSize); setParam(*procDelta,"delta",1.0f); AudioBuffer<float> delta; delta.makeCopyOf(orig); runThrough(*procDelta, delta, blockSize);
        int lat = procWet->getLatencySamples();
        double errSum=0, refSum=0; int n=0;
        for (int i=fftSize; i<wet.getNumSamples(); ++i) for (int c=0;c<2;++c)
        { double sum=wet.getSample(c,i)+delta.getSample(c,i); double d=sum-orig.getSample(c,i-lat); errSum+=d*d; refSum+=(double)orig.getSample(c,i-lat)*orig.getSample(c,i-lat); ++n; }
        double errDb = toDb(std::sqrt(errSum/n)/juce::jmax(std::sqrt(refSum/n),1e-12));
        std::cout << "  Delta identity error (from output[2048] onward): " << errDb << " dB\n";
        check(errDb < -60.0, "delayedOriginal ~= processed+delta holds from the boundary onward");

        // no ringing in delta pre-latency zone specifically (already checked ==0 above, but also check for the kick case)
        auto genKick = [&](int n)->AudioBuffer<float>{ AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(1);
            for (int i=0;i<juce::jmin(n,800);++i){ double t=i/sr; float env=(float)std::exp(-t/0.02);
                float v=0.8f*((float)std::sin(2.0*juce::MathConstants<double>::pi*80.0*t)*0.8f*env + (rng.nextFloat()*2-1)*0.2f*env);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        auto procDeltaKick = makeProc(sr, blockSize); setParam(*procDeltaKick,"delta",1.0f);
        auto kickIn = genKick((int)(sr*0.3)); AudioBuffer<float> kickOut; kickOut.makeCopyOf(kickIn); runThrough(*procDeltaKick, kickOut, blockSize);
        double kickPreLat = peak(kickOut, 0, fftSize);
        std::cout << "  Delta+kick pre-latency peak=" << kickPreLat << "\n";
        check(kickPreLat==0.0, "Delta+kick: no ringing before latency boundary");
    }

    //======================================================================
    std::cout << "\n==================== MIX ALIGNMENT (0/25/50/75/100%) ====================\n";
    {
        auto genSweep = [&](double dur)->AudioBuffer<float>{
            int n=(int)(sr*dur); AudioBuffer<float> b(2,n);
            double f0=20,f1=20000,T=dur;
            for (int i=0;i<n;++i){ double t=i/sr; double ph=2.0*juce::MathConstants<double>::pi*f0*T/std::log(f1/f0)*(std::exp((t/T)*std::log(f1/f0))-1.0);
                float v=0.4f*(float)std::sin(ph); for(int c=0;c<2;++c) b.setSample(c,i,v);} return b;
        };
        float mixes[] = {0,25,50,75,100};
        for (float m : mixes)
        {
            auto proc = makeProc(sr, blockSize);
            setParam(*proc,"depth",0.0f); setParam(*proc,"mix",m);
            int lat = proc->getLatencySamples();
            auto input = genSweep(1.0);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            // at Depth=0, wet==dry, so ANY mix should equal delayed input exactly (no comb filtering possible if aligned)
            int skip = lat+64;
            double errSum=0, refSum=0; int n=0;
            for (int i=skip;i<out.getNumSamples();++i) for (int c=0;c<2;++c)
            { double d=out.getSample(c,i)-input.getSample(c,i-lat); errSum+=d*d; refSum+=(double)input.getSample(c,i-lat)*input.getSample(c,i-lat); ++n; }
            double errDb = toDb(std::sqrt(errSum/n)/juce::jmax(std::sqrt(refSum/n),1e-12));
            std::cout << "  Mix=" << m << "%: alignment error=" << errDb << " dB\n";
            check(errDb < -60.0, "Mix="+std::to_string((int)m)+"%: dry/wet aligned, no comb filtering (Depth=0)");
        }
    }

    //======================================================================
    std::cout << "\n==================== BYPASS LONG TEST (new architecture: engine always fed) ====================\n";
    {
        auto proc = makeProc(sr, blockSize);
        juce::Random rng(5);
        auto makeTone = [&](int n, double startT)->AudioBuffer<float>{ AudioBuffer<float> b(2,n);
            for (int i=0;i<n;++i){ double t=startT+i/sr; float v=0.3f*(float)std::sin(2.0*juce::MathConstants<double>::pi*440.0*t)+0.05f*(rng.nextFloat()*2-1);
                for(int c=0;c<2;++c) b.setSample(c,i,v);} return b; };
        auto pre = makeTone((int)(sr*2.0), 0.0);
        runThrough(*proc, pre, blockSize);
        setParam(*proc, "bypass", 1.0f);
        auto during = makeTone((int)(sr*5.0), 2.0);
        AudioBuffer<float> duringOut; duringOut.makeCopyOf(during);
        runThrough(*proc, duringOut, blockSize);
        setParam(*proc, "bypass", 0.0f);
        auto post = makeTone((int)(sr*0.3), 7.0);
        AudioBuffer<float> postOut; postOut.makeCopyOf(post);
        runThrough(*proc, postOut, blockSize);

        double duringRmsOut = rms(duringOut,0,duringOut.getNumSamples());
        double duringRmsIn = rms(during,0,during.getNumSamples());
        bool nanInf = hasNaNOrInf(duringOut) || hasNaNOrInf(postOut);
        double maxJump=0; float last=duringOut.getSample(0,duringOut.getNumSamples()-1);
        int check200ms=(int)(sr*0.2);
        for (int i=0;i<juce::jmin(check200ms,postOut.getNumSamples());++i){ float s=postOut.getSample(0,i); maxJump=juce::jmax(maxJump,(double)std::abs(s-last)); last=s; }
        std::cout << "  during-bypass outRMS=" << duringRmsOut << " (inRMS=" << duringRmsIn << ")\n";
        std::cout << "  max jump in first 200ms after un-bypass=" << maxJump << (nanInf?" [NaN/Inf!]":"") << "\n";
        check(! nanInf, "no NaN/Inf across long bypass + transition");
        check(std::abs(duringRmsOut-duringRmsIn) < duringRmsIn*0.1, "during bypass, output tracks input (dry passthrough, no stale content)");
        check(maxJump < 0.1, "smooth transition back to processed, no click/spike");
    }

    //======================================================================
    std::cout << "\n==================== RESET / TRANSPORT / SAMPLE-RATE ====================\n";
    {
        double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
        for (double testSr : rates)
        {
            auto proc = makeProc(testSr, blockSize);
            auto warm = genImpulse((int)(testSr*0.1),50,0.5f);
            runThrough(*proc, warm, blockSize);
            proc->prepareToPlay(testSr, blockSize); // simulate reset / transport restart
            setParam(*proc, "depth", 0.0f);
            int lat = proc->getLatencySamples();
            auto input = genImpulse((int)(testSr*0.1),10,0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            int measPos=-1; double pkVal=0;
            for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if(v>pkVal){pkVal=v;measPos=i;} }
            std::cout << "  sr=" << testSr << " reportedLat=" << lat << " (=" << (lat/testSr*1000.0) << "ms) expPos=" << (lat+10) << " measPos=" << measPos << " peak=" << pkVal << "\n";
            check(lat==2048, "sr="+std::to_string((int)testSr)+": latency stays exactly 2048 samples");
            check(measPos==lat+10, "sr="+std::to_string((int)testSr)+": exact timing after reset");
            check(std::abs(pkVal-0.9)<0.001, "sr="+std::to_string((int)testSr)+": exact amplitude after reset");
        }

        // block-size change
        {
            auto proc = makeProc(sr, 256);
            auto warm = genImpulse((int)(sr*0.1),50,0.5f);
            runThrough(*proc, warm, 256);
            proc->prepareToPlay(sr, 1024); // block size change
            setParam(*proc, "depth", 0.0f);
            int lat = proc->getLatencySamples();
            auto input = genImpulse((int)(sr*0.1),10,0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, 1024);
            int measPos=-1; double pkVal=0;
            for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if(v>pkVal){pkVal=v;measPos=i;} }
            std::cout << "  block-size change 256->1024: reportedLat=" << lat << " expPos=" << (lat+10) << " measPos=" << measPos << " peak=" << pkVal << "\n";
            check(lat==2048, "block-size change: latency stays 2048");
            check(measPos==lat+10, "block-size change: exact timing");
        }
    }

    std::cout << "\n=========================================================\n";
    std::cout << gPass << " checks passed, " << gFail << " checks failed.\n";
    std::cout << "=========================================================\n";
    return gFail==0?0:1;
}
