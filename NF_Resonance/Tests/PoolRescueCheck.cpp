#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <atomic>
#include <cstdlib>
#include <new>

static std::atomic<bool> gTrackAllocs{false};
static std::atomic<long long> gAllocCount{0};
void* operator new(std::size_t sz) { if (gTrackAllocs.load()) gAllocCount.fetch_add(1); void* p = std::malloc(sz==0?1:sz); if(!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph=0.0, inc=juce::MathConstants<double>::twoPi*freq/sr; for (auto& s : b) { s += (float)std::sin(ph)*amp; ph+=inc; } }
static std::vector<float> genWhiteNoise(int n, float amp) { std::vector<float> b((size_t) n); juce::Random rng(31); for (auto& s : b) s = (rng.nextFloat()*2.0f-1.0f)*amp; return b; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz=freqHz/Q; int n=(int)b.size();
    for(int k=0;k<9;++k){ double t=(double)k/8.0-0.5, f=freqHz+t*bwHz; double ph=rng.nextDouble()*juce::MathConstants<double>::twoPi, inc=juce::MathConstants<double>::twoPi*f/sr;
        for(int i=0;i<n;++i){ b[(size_t)i]+=(float)std::sin(ph)*(amp/3.0f); ph+=inc; } }
}

int main()
{
    double sr = 96000.0; int fftSize=2048, hop=512;
    // pool saturation: 1s of white noise, then a real resonance introduced
    int nPre = (int)(sr*1.0);
    auto noise = genWhiteNoise(nPre, 0.2f);
    int nBurst = (int)(sr*0.6);
    std::vector<float> sig = genSilence(nPre+nBurst);
    for (int i=0;i<nPre;++i) sig[(size_t)i]=noise[(size_t)i];
    std::vector<float> burstOnly = genSilence(nBurst);
    addBurst(burstOnly, sr, 300.0, (float)juce::Decibels::decibelsToGain(6.0f)*0.4f, 8.0, 13);
    for (int i=0;i<nBurst;++i) sig[(size_t)(nPre+i)] += burstOnly[(size_t)i];

    SpectralProminenceEngineV5 prom; prom.prepare(fftSize/2+1, sr, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t)fftSize);
    for(int i=0;i<fftSize;++i) window[(size_t)i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(fftSize-1));
    std::vector<float> scratch((size_t)fftSize*2), magDb((size_t)(fftSize/2+1)), promOut((size_t)(fftSize/2+1));

    int n = (int)sig.size();
    int preFrames = nPre/hop;
    int occBeforeIntro = -1; int framesToAdmit300 = -1; bool admitted300 = false;
    int mergeCount=0, rescueCount=0;

    gTrackAllocs.store(true);
    for (int i=0, frame=0; i+fftSize<=n; i+=hop, ++frame)
    {
        for(int k=0;k<fftSize;++k) scratch[(size_t)k]=sig[(size_t)(i+k)]*window[(size_t)k];
        std::fill(scratch.begin()+fftSize, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins=fftSize/2+1;
        for(int b=0;b<bins;++b){ float re=scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)];
            magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
        prom.computeProminence(magDb, 4.0f, promOut);
        aux.pushSamples(sig.data()+i, hop);
        conf.process(promOut, &aux, &magDb);

        if (frame == preFrames-1) occBeforeIntro = conf.activeRegionCount();
        if (frame >= preFrames && !admitted300)
        {
            for (auto& r : conf.regions()) if (r.active && std::abs(std::log2(juce::jmax(1.0f,r.centerHz))-std::log2(300.0f))<0.25f) { admitted300=true; framesToAdmit300=frame-preFrames; break; }
        }
        for (auto& r : conf.regions()) if (r.active) { if (r.lastCandidateSource==ConfidenceEngine::CandidateSource::AuxRescue) ++rescueCount; else if (r.lastAuxRescueAuthority>0.0f) ++mergeCount; }
    }
    gTrackAllocs.store(false);

    std::printf("pool occupancy before 300Hz intro = %d/32\n", occBeforeIntro);
    std::printf("300Hz admitted = %s, frames after intro = %d\n", admitted300?"YES":"NO", framesToAdmit300);
    std::printf("final active regions = %d/32\n", conf.activeRegionCount());
    std::printf("allocations during whole run = %lld\n", gAllocCount.load());
    std::printf("(rescue/merge tag observations over run: rescueCount=%d mergeCount=%d -- just confirms the tagging mechanism fired at least once)\n", rescueCount, mergeCount);
    return 0;
}
