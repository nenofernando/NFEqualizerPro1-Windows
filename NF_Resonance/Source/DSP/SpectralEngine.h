#pragma once
#include <JuceHeader.h>
#include "ResonanceDetector.h"
#include "TransientGuard.h"
class SpectralEngine {
public:
 struct Params{ float depth=5,sharpness=4,selectivity=3.5f,attackMs=10,releaseMs=80,lowHz=20,highHz=20000,transient=5,biasDb=1.5f,curveDb[7]{}; int mode=0; bool delta=false; };
 void prepare(double sr,int channels); void reset(); void setParams(const Params& p){params=p;} void process(juce::AudioBuffer<float>&); int latencySamples() const{return fftSize;}
 std::vector<float> getLastSpectrum() const; std::vector<float> getLastReduction() const;
private:
 static constexpr int order=11,fftSize=1<<order,hop=fftSize/4,ringSize=fftSize*4;
 // Deterministic OLA warm-up: a ring position only receives contributions from
 // all (fftSize/hop) overlapping analysis windows once the transport has run
 // for fftSize + (fftSize/hop - 1)*hop samples. Before that, fewer windows have
 // contributed and the sqrt-Hann taper near a single window's own edge can make
 // the normalization denominator (sum of w^2) numerically tiny even though the
 // exact-arithmetic ratio is bounded -- dividing there amplifies float32 FFT
 // round-trip error into an audible spike. Gate the output to silence until
 // full overlap is guaranteed, rather than dividing by a near-zero value.
 static constexpr int numOverlaps=fftSize/hop, fullOverlapAt=fftSize+(numOverlaps-1)*hop;
 struct Chan{ std::vector<float> history,ola,norm,fftData,magDb,reduction,lastWet; long long t=0; int histPos=0; ResonanceDetector det; TransientGuard trans; };
 std::unique_ptr<juce::dsp::FFT> fft; std::vector<float> window; std::vector<Chan> c; double sampleRate=48000; Params params;
 void frame(Chan& s,float transientFactor); float processOne(Chan& s,float x); };
