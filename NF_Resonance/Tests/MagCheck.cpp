#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <vector>
#include <cstdio>
#include <cmath>

int main()
{
    double sr = 192000.0; int fftSize = 2048;
    int n = (int)(sr*0.5);
    std::vector<float> sig((size_t)n, 0.0f);
    double f=120.0, Q=10.0; double bwHz=f/Q;
    juce::Random rng(5);
    for (int k=0;k<9;++k){ double t=(double)k/8.0-0.5; double ff=f+t*bwHz; double ph=rng.nextDouble()*juce::MathConstants<double>::twoPi, inc=juce::MathConstants<double>::twoPi*ff/sr;
        float amp=(float)juce::Decibels::decibelsToGain(4.0f)*0.4f;
        for(int i=0;i<n;++i){ sig[(size_t)i]+=(float)std::sin(ph)*(amp/3.0f); ph+=inc; } }

    SpectralProminenceEngineV5 prom; prom.prepare(fftSize/2+1, sr, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t)fftSize);
    for(int i=0;i<fftSize;++i) window[(size_t)i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(fftSize-1));
    std::vector<float> scratch((size_t)fftSize*2), magDb((size_t)(fftSize/2+1)), promOut((size_t)(fftSize/2+1));
    int hop=512;
    int frame=0;
    for(int i=0;i+fftSize<=n;i+=hop){
        for(int k=0;k<fftSize;++k) scratch[(size_t)k]=sig[(size_t)(i+k)]*window[(size_t)k];
        std::fill(scratch.begin()+fftSize, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins=fftSize/2+1;
        for(int b=0;b<bins;++b){ float re=scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)];
            magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
        prom.computeProminence(magDb, 4.0f, promOut);
        if (frame==20) {
            std::printf("frame %d:\n", frame);
            for (int b=0;b<=5;++b) std::printf("  bin=%d hz=%.2f magDb=%.2f promDb=%.2f\n", b, b*sr/fftSize, magDb[(size_t)b], promOut[(size_t)b]);
        }
        ++frame;
    }
    return 0;
}
