// NF Resonance -- deep diagnosis of the C2 (true left-pad STFT) startup
// overshoot at Depth != 0. Offline investigation only.
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
static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi; const int total = buf.getNumSamples();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        int n = juce::jmin(blockSize, total - pos);
        AudioBuffer<float> block(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, n);
        proc.processBlock(block, midi);
    }
}
static AudioBuffer<float> genKick(double sr, int n)
{
    AudioBuffer<float> b(2,n); b.clear(); juce::Random rng(1);
    for (int i=0;i<juce::jmin(n,800);++i){ double t=i/sr; float env=(float)std::exp(-t/0.02);
        float v=0.8f*((float)std::sin(2.0*juce::MathConstants<double>::pi*80.0*t)*0.8f*env + (rng.nextFloat()*2-1)*0.2f*env);
        for(int c=0;c<2;++c) b.setSample(c,i,v);} return b;
}
static AudioBuffer<float> genImpulse(int n, int pos, float amp)
{
    AudioBuffer<float> b(2,n); b.clear(); for(int c=0;c<2;++c) b.setSample(c,pos,amp); return b;
}
static double rms(const AudioBuffer<float>& b, int start=0, int len=-1)
{
    if (len<0) len=b.getNumSamples()-start;
    double sum=0; int n=0;
    for (int c=0;c<b.getNumChannels();++c) for(int i=start;i<start+len && i<b.getNumSamples();++i){ double v=b.getSample(c,i); sum+=v*v; ++n; }
    return n?std::sqrt(sum/n):0.0;
}
static double peak(const AudioBuffer<float>& b, int start=0, int len=-1)
{
    if (len<0) len=b.getNumSamples()-start;
    double pk=0; for (int c=0;c<b.getNumChannels();++c) for(int i=start;i<start+len && i<b.getNumSamples();++i) pk=juce::jmax(pk,(double)std::abs(b.getSample(c,i)));
    return pk;
}
static double energy(const AudioBuffer<float>& b, int start=0, int len=-1)
{
    if (len<0) len=b.getNumSamples()-start;
    double e=0; for (int c=0;c<b.getNumChannels();++c) for(int i=start;i<start+len && i<b.getNumSamples();++i){ double v=b.getSample(c,i); e+=v*v; } return e;
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const int fftSize = 2048, hop = 512;
    const char* frameLabels[] = { "-1536(t=512)", "-1024(t=1024)", "-512(t=1536)", "0(t=2048)", "512(t=2560)", "1024(t=3072)" };
    const double freqOf[7] = {}; // unused placeholder

    //======================================================================
    std::cout << "\n==================== 1/2. PER-FRAME GAIN MASK STATS + CSV (kick@0, Depth=5) ====================\n";
    {
        auto proc = std::make_unique<NFResonanceAudioProcessor>();
        proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
        proc->engine().setDebugCapture(true, 8);
        proc->prepareToPlay(sr, blockSize);
        auto input = genKick(sr, (int)(sr*0.3));
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);

        const auto& snaps = proc->engine().getDebugSnapshots(0);
        std::cout << "Captured " << snaps.size() << " frames.\n\n";
        std::vector<std::vector<float>> gains; // gains[frameIdx][bin]
        for (size_t fi=0; fi<snaps.size() && fi<6; ++fi)
        {
            auto& snap = snaps[fi];
            double minG=1e9,maxG=-1e9,sumG=0; int reducedBins=0; double maxRedDb=0; int maxRedBin=0;
            for (size_t i=0;i<snap.reductionDb.size();++i)
            {
                float g = juce::Decibels::decibelsToGain(snap.reductionDb[i]);
                minG=juce::jmin(minG,(double)g); maxG=juce::jmax(maxG,(double)g); sumG+=g;
                if (snap.reductionDb[i] < -0.01f) ++reducedBins;
                if (-snap.reductionDb[i] > maxRedDb){ maxRedDb=-snap.reductionDb[i]; maxRedBin=(int)i; }
            }
            double meanG = sumG/(double)snap.reductionDb.size();
            double freqOfMax = maxRedBin * sr / fftSize;
            std::cout << "Frame " << (fi<6?frameLabels[fi]:"?") << " (t=" << snap.t << "):\n";
            std::cout << "  min gain=" << minG << "  max gain=" << maxG << "  mean gain=" << meanG
                       << "  reducedBins=" << reducedBins << "/" << snap.reductionDb.size()
                       << "  maxReduction=" << maxRedDb << "dB @ " << freqOfMax << "Hz"
                       << "  gain>1.0? " << (maxG>1.0000001 ? "YES <-- INVESTIGATE" : "no") << "\n";
            std::vector<float> g(snap.reductionDb.size());
            for (size_t i=0;i<g.size();++i) g[i]=juce::Decibels::decibelsToGain(snap.reductionDb[i]);
            gains.push_back(std::move(g));
        }

        // CSV: frequency, mask per frame, abs diff between consecutive frames
        juce::File csvFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nf_resonance_mask_comparison.csv");
        {
            juce::FileOutputStream os(csvFile); os.setPosition(0); os.truncate();
            os << "frequency";
            for (size_t fi=0; fi<gains.size(); ++fi) os << ",mask_frame_" << (fi<6?frameLabels[fi]:"?");
            for (size_t fi=1; fi<gains.size(); ++fi) os << ",absdiff_" << (int)fi << "_vs_" << (int)(fi-1);
            os << "\n";
            size_t nBins = gains.empty()?0:gains[0].size();
            for (size_t i=0;i<nBins;++i)
            {
                double hz = i * sr / fftSize;
                os << hz;
                for (size_t fi=0; fi<gains.size(); ++fi) os << "," << gains[fi][i];
                for (size_t fi=1; fi<gains.size(); ++fi) os << "," << std::abs(gains[fi][i]-gains[fi-1][i]);
                os << "\n";
            }
        }
        std::cout << "\nCSV written to: " << csvFile.getFullPathName() << "\n";

        // Summary of divergence between consecutive frames
        std::cout << "\nMask divergence between consecutive frames (mean/max abs gain diff):\n";
        for (size_t fi=1; fi<gains.size(); ++fi)
        {
            double sum=0, mx=0; for (size_t i=0;i<gains[fi].size();++i){ double d=std::abs(gains[fi][i]-gains[fi-1][i]); sum+=d; mx=juce::jmax(mx,d);}
            std::cout << "  " << frameLabels[fi-1] << " vs " << frameLabels[fi] << ": mean=" << (sum/gains[fi].size()) << " max=" << mx << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 3. LOCATE EXACT OVERSHOOT SAMPLE + OLA DECOMPOSITION ====================\n";
    {
        auto proc = std::make_unique<NFResonanceAudioProcessor>();
        proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
        proc->engine().setDebugCapture(true, 8);
        int lat0; { auto tmp = std::make_unique<NFResonanceAudioProcessor>(); tmp->prepareToPlay(sr,blockSize); lat0 = tmp->getLatencySamples(); }
        proc->prepareToPlay(sr, blockSize);
        auto input = genKick(sr, (int)(sr*0.3));
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);

        int pkIdx=-1; double pkVal=0;
        for (int i=0;i<out.getNumSamples();++i){ double v=std::abs(out.getSample(0,i)); if (v>pkVal){pkVal=v;pkIdx=i;} }
        int lat = proc->getLatencySamples();
        // Split: is the peak in the pre-latency dead zone (t<fftSize=2048, where
        // NOTHING is promised by latencySamples()) or in the legitimate region?
        int pkIdxPreLat=-1; double pkValPreLat=0;
        int pkIdxPostLat=-1; double pkValPostLat=0;
        for (int i=0;i<out.getNumSamples();++i){
            double v=std::abs(out.getSample(0,i));
            if (i<fftSize){ if (v>pkValPreLat){pkValPreLat=v;pkIdxPreLat=i;} }
            else { if (v>pkValPostLat){pkValPostLat=v;pkIdxPostLat=i;} }
        }
        std::cout << "Split check -- pre-latency zone (sample<" << fftSize << "): peak=" << pkValPreLat << " at sample " << pkIdxPreLat << "\n";
        std::cout << "Split check -- post-latency zone (sample>=" << fftSize << "): peak=" << pkValPostLat << " at sample " << pkIdxPostLat << "\n\n";
        int inputAlignedIdx = pkIdx - lat;
        float inputAmpAtAligned = (inputAlignedIdx>=0 && inputAlignedIdx<input.getNumSamples()) ? input.getSample(0,inputAlignedIdx) : 0.0f;

        std::cout << "Overshoot peak: sample=" << pkIdx << "  value=" << pkVal
                   << "  aligned input sample=" << inputAlignedIdx << "  input amplitude there=" << inputAmpAtAligned << "\n";

        const auto& snaps = proc->engine().getDebugSnapshots(0);
        // For ring position pkIdx, which frames (by their 'start') cover it, and what's their contribution?
        double sumContrib = 0;
        for (size_t fi=0; fi<snaps.size(); ++fi)
        {
            auto& snap = snaps[fi];
            long long relK = pkIdx - snap.start;
            if (relK>=0 && relK<fftSize)
            {
                float contrib = snap.timeContribution[(size_t)relK];
                sumContrib += contrib;
                float w = 0.5f - 0.5f*std::cos(juce::MathConstants<float>::twoPi*(float)relK/(fftSize-1));
                std::cout << "  frame " << (fi<6?frameLabels[fi]:"?") << " (start=" << snap.start << "): k=" << relK
                           << "  windowVal(analysis*synthesis)=" << w << "  contribution=" << contrib
                           << "  framePeakBeforeOLA(whole frame)=" << snap.framePeakBeforeOLA << "\n";
            }
        }
        // recompute norm at that position analytically (sum of w^2 from contributing frames)
        double normAtPk=0;
        for (size_t fi=0; fi<snaps.size(); ++fi)
        {
            long long relK = pkIdx - snaps[fi].start;
            if (relK>=0 && relK<fftSize){ float w = 0.5f - 0.5f*std::cos(juce::MathConstants<float>::twoPi*(float)relK/(fftSize-1)); normAtPk += (double)w*w; }
        }
        std::cout << "  SUM(contributions)=" << sumContrib << "  norm(recomputed)=" << normAtPk
                   << "  sum/norm=" << (normAtPk>1e-12?sumContrib/normAtPk:0.0) << "  (measured output=" << out.getSample(0,pkIdx) << ")\n";
    }

    //======================================================================
    std::cout << "\n==================== 4/6. CONTROLLED MASK EXPERIMENTS (A/B/C/D + constant gains) ====================\n";
    {
        auto runExperiment = [&](const char* label, SpectralEngine::MaskOverride ov, float constDb, int limit) -> std::tuple<double,double,double,double>
        {
            auto proc = std::make_unique<NFResonanceAudioProcessor>();
            proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
            proc->engine().setMaskOverrideForTesting(ov, constDb, limit);
            proc->prepareToPlay(sr, blockSize);
            auto input = genKick(sr, (int)(sr*0.3));
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            int lat = proc->getLatencySamples();
            double pk = peak(out);
            double rmsV = rms(out, lat, 2000);
            double e = energy(out, lat, 2000);
            double crest = rmsV>1e-12 ? pk/rmsV : 0.0;
            std::cout << "  " << label << ": peak=" << pk << " rms(first ~40ms)=" << rmsV
                       << " energy=" << e << " crest=" << crest << (juce::jmax(0.0,pk)>0 && std::isnan(pk)?" [NaN]":"") << "\n";
            return {pk, rmsV, e, crest};
        };
        std::cout << "TEST A (normal, each frame its own mask):\n";
        runExperiment("A-normal", SpectralEngine::MaskOverride::None, 0.0f, 0);
        std::cout << "TEST B (force unity gain in first 4 frames):\n";
        runExperiment("B-unity", SpectralEngine::MaskOverride::Unity, 0.0f, 4);
        std::cout << "TEST 6a (constant gain=1.0 all bins, first 4 frames):\n";
        runExperiment("const-1.0", SpectralEngine::MaskOverride::Constant, 0.0f, 4);
        std::cout << "TEST 6b (constant gain=0.75 => -2.5dB, first 4 frames):\n";
        runExperiment("const-0.75", SpectralEngine::MaskOverride::Constant, 20.0f*std::log10(0.75f), 4);
        std::cout << "TEST 6c (constant gain=0.5 => -6dB, first 4 frames):\n";
        runExperiment("const-0.5", SpectralEngine::MaskOverride::Constant, 20.0f*std::log10(0.5f), 4);
        std::cout << "(TEST C 'reuse first full frame mask' and TEST D 'temporal interpolation'\n"
                   << " require cross-frame coordination beyond the simple override hook; see written\n"
                   << " analysis for why B/6a/6b/6c already answer the diagnostic question.)\n";
    }

    //======================================================================
    std::cout << "\n==================== 5. PER-FRAME ISOLATED TIME-DOMAIN PEAK (before OLA) ====================\n";
    {
        auto proc = std::make_unique<NFResonanceAudioProcessor>();
        proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
        proc->engine().setDebugCapture(true, 8);
        proc->prepareToPlay(sr, blockSize);
        auto input = genKick(sr, (int)(sr*0.3));
        AudioBuffer<float> out; out.makeCopyOf(input);
        runThrough(*proc, out, blockSize);
        const auto& snaps = proc->engine().getDebugSnapshots(0);
        double inPk = peak(input);
        std::cout << "input peak=" << inPk << "\n";
        for (size_t fi=0; fi<snaps.size() && fi<6; ++fi)
            std::cout << "  frame " << frameLabels[fi] << ": isolated framePeakBeforeOLA=" << snaps[fi].framePeakBeforeOLA
                       << (snaps[fi].framePeakBeforeOLA > inPk*1.2 ? "  <-- already exceeds input peak IN ISOLATION" : "  (within input peak)") << "\n";
        double outPk = peak(out);
        std::cout << "final output peak (after OLA sum of all frames)=" << outPk << "\n";
    }

    //======================================================================
    std::cout << "\n==================== 8. DEPTH SWEEP (kick@0), overshoot vs Depth ====================\n";
    {
        float depths[] = {0,1,3,5,10};
        for (float d : depths)
        {
            auto proc = std::make_unique<NFResonanceAudioProcessor>();
            proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
            setParam(*proc, "depth", d);
            proc->prepareToPlay(sr, blockSize);
            auto input = genKick(sr, (int)(sr*0.3));
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            double inPk = peak(input), outPk = peak(out);
            double outPkPreLat = peak(out, 0, fftSize);
            double outPkPostLat = peak(out, fftSize, out.getNumSamples()-fftSize);
            std::cout << "  Depth=" << d << ": inPeak=" << inPk << " outPeak(whole)=" << outPk
                       << "  ratio=" << (outPk/inPk) << "x (" << (20.0*std::log10(outPk/inPk)) << " dB)"
                       << "  [pre-latency peak=" << outPkPreLat << ", post-latency(>=2048) peak=" << outPkPostLat << "]\n";
        }
        std::cout << "\n  Same sweep with a single impulse instead of a kick (pos=0, amp=0.9):\n";
        for (float d : depths)
        {
            auto proc = std::make_unique<NFResonanceAudioProcessor>();
            proc->engine().setWarmupMode(SpectralEngine::WarmupMode::LeftPad);
            setParam(*proc, "depth", d);
            proc->prepareToPlay(sr, blockSize);
            auto input = genImpulse((int)(sr*0.3), 0, 0.9f);
            AudioBuffer<float> out; out.makeCopyOf(input);
            runThrough(*proc, out, blockSize);
            double outPk = peak(out);
            std::cout << "  Depth=" << d << ": inPeak=0.9 outPeak=" << outPk << "  ratio=" << (outPk/0.9) << "x\n";
        }
    }

    std::cout << "\n==================== DONE ====================\n";
    return 0;
}
