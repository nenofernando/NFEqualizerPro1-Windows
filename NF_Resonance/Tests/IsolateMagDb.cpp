#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph=0.0, inc=juce::MathConstants<double>::twoPi*freq/sr; for (auto& s : b) { s += (float)std::sin(ph)*amp; ph+=inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb=3.0f) { auto b=genSilence(n); for(int h=1;h<=numH;++h) addTone(b,sr,f0*h,amp*(float)juce::Decibels::decibelsToGain(-rolloffDb*(h-1))); return b; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz=freqHz/Q; int n=(int)b.size();
    for(int k=0;k<9;++k){ double t=(double)k/8.0-0.5, f=freqHz+t*bwHz; double ph=rng.nextDouble()*juce::MathConstants<double>::twoPi, inc=juce::MathConstants<double>::twoPi*f/sr;
        for(int i=0;i<n;++i){ b[(size_t)i]+=(float)std::sin(ph)*(amp/3.0f); ph+=inc; } }
}

int main()
{
    double sr = 44100.0; int fftSize=2048, hop=512;
    int n = (int)(sr*1.5);
    auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
    addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);

    for (int useMagDb = 0; useMagDb <= 1; ++useMagDb)
    {
        SpectralProminenceEngineV5 prom; prom.prepare(fftSize/2+1, sr, fftSize);
        prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
        ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);
        juce::dsp::FFT fft(11);
        std::vector<float> window((size_t)fftSize);
        for(int i=0;i<fftSize;++i) window[(size_t)i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(fftSize-1));
        std::vector<float> scratch((size_t)fftSize*2), magDb((size_t)(fftSize/2+1)), promOut((size_t)(fftSize/2+1));

        for (int i = 0; i + fftSize <= n; i += hop)
        {
            for(int k=0;k<fftSize;++k) scratch[(size_t)k]=sig[(size_t)(i+k)]*window[(size_t)k];
            std::fill(scratch.begin()+fftSize, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            int bins=fftSize/2+1;
            for(int b=0;b<bins;++b){ float re=scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)];
                magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
            prom.computeProminence(magDb, 4.0f, promOut);
            aux.pushSamples(sig.data()+i, hop);
            conf.applyAuxProminenceAssist(promOut, magDb, aux);
            conf.process(promOut, &aux, useMagDb ? &magDb : nullptr);
        }
        std::printf("=== useMagDb=%d ===\n", useMagDb);
        for (auto& r : conf.regions())
            if (r.active) std::printf("  freq=%.1f prom=%.2f persist=%.3f stab=%.3f harmLike=%.3f effRel=%.3f effProt=%.3f conf=%.3f\n",
                r.centerHz, r.peakProminenceDb, r.persistence, r.stability, r.effectiveLikelihood, r.effectiveReliability, r.effectiveHarmonicProtection, r.confidence);
    }
    return 0;
}
