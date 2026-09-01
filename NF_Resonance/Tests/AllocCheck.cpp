#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <atomic>
#include <cstdlib>
#include <new>

static std::atomic<bool> gTrackAllocs{false};
static std::atomic<long long> gAllocCount{0};
void* operator new(std::size_t sz) { if (gTrackAllocs.load()) gAllocCount.fetch_add(1); void* p = std::malloc(sz==0?1:sz); if(!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t=(double)k/8.0-0.5; double f=freqHz+t*bwHz; double ph=rng.nextDouble()*juce::MathConstants<double>::twoPi, inc=juce::MathConstants<double>::twoPi*f/sr;
        for (int i=0;i<n;++i){ b[(size_t)i]+=(float)std::sin(ph)*(amp/3.0f); ph+=inc; } }
}
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph=0.0, inc=juce::MathConstants<double>::twoPi*freq/sr; for (auto& s : b) { s += (float)std::sin(ph)*amp; ph+=inc; } }

int main()
{
    double sr = 48000.0; int fftSize=2048, hop=512;
    int n = (int)(sr*2.0);
    // dense-ish signal: many harmonics + noise -> stresses pool churn/eviction paths
    auto sig = genSilence(n);
    for (int h=1;h<=8;++h) addTone(sig, sr, 62.0*h, 0.3f*(float)juce::Decibels::decibelsToGain(-2.5f*(h-1)));
    addBurst(sig, sr, 300.0, 0.4f, 8.0, 3);
    juce::Random rng(9); for (auto& s : sig) s += (rng.nextFloat()*2.0f-1.0f)*0.05f;

    SpectralProminenceEngineV5 prom; prom.prepare(fftSize/2+1, sr, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t)fftSize);
    for(int i=0;i<fftSize;++i) window[(size_t)i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(fftSize-1));
    std::vector<float> scratch((size_t)fftSize*2), magDb((size_t)(fftSize/2+1)), promOut((size_t)(fftSize/2+1));

    gTrackAllocs.store(true);
    for(int i=0;i+fftSize<=n;i+=hop)
    {
        for(int k=0;k<fftSize;++k) scratch[(size_t)k]=sig[(size_t)(i+k)]*window[(size_t)k];
        std::fill(scratch.begin()+fftSize, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins=fftSize/2+1;
        for(int b=0;b<bins;++b){ float re=scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)];
            magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
        prom.computeProminence(magDb, 4.0f, promOut);
        conf.process(promOut, nullptr);
    }
    gTrackAllocs.store(false);
    std::printf("allocations during process() loop: %lld\n", gAllocCount.load());
    return 0;
}
