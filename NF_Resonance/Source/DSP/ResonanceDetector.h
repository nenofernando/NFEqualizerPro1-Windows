#pragma once
#include <JuceHeader.h>
#include "SpectralProminenceEngine.h"
class ResonanceDetector {
public:
 void prepare(int bins,double sr,int fft); void reset();
 void compute(const std::vector<float>& magDb,std::vector<float>& reductionDb,float depth,float sharpness,float selectivity,float attackMs,float releaseMs,float lowHz,float highHz,float biasDb,const float curveDb[7],float transientFactor);
private:
 std::vector<float> smoothGainDb, prominenceBuf; double sampleRate=48000; int fftSize=2048;
 // V2-A: multi-scale, log-frequency-aware, FFT-resolution-respecting prominence
 // estimation, replacing the old box-car linear-bin mean. Winner of the V2-A
 // baseline benchmark (lowest combined MAE+CPU across flat/pink/tilted/smooth
 // baselines and 120Hz/1kHz/10kHz, with a 70%-radius exclusion gap so a
 // resonance's own energy doesn't bias its own baseline).
 SpectralProminenceEngine prominenceEngine;
 float curveAt(float hz,const float c[7]) const;
};