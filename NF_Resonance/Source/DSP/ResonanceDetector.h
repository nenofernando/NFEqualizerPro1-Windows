#pragma once
#include <JuceHeader.h>
class ResonanceDetector {
public:
 void prepare(int bins,double sr,int fft); void reset();
 void compute(const std::vector<float>& magDb,std::vector<float>& reductionDb,float depth,float sharpness,float selectivity,float attackMs,float releaseMs,float lowHz,float highHz,float biasDb,const float curveDb[7],float transientFactor);
private:
 std::vector<float> smoothGainDb, prefix; double sampleRate=48000; int fftSize=2048;
 float curveAt(float hz,const float c[7]) const;
};