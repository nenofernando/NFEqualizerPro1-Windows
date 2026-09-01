#pragma once
#include <JuceHeader.h>
class ResonanceDetector {
public:
 static constexpr int kMaxBands = 32;
 void prepare(int bins,double sr,int fft); void reset();
 // Checkpoint A: bandWidth (octaves) + bandShape (0=Bell,1=LowShelf,
 // 2=HighShelf,3=LowFocus,4=HighFocus) added alongside freq/sens/active.
 // Sensitivity at a bin is now the SUM of every active band's own shaped
 // contribution (evaluated directly, no sort/interpolation needed -- each
 // band decays with distance from its own centre, so unlike the old
 // point-to-point model there's no ordering dependency at all).
 void compute(const std::vector<float>& magDb,std::vector<float>& reductionDb,float depth,float sharpness,float selectivity,float attackMs,float releaseMs,float lowHz,float highHz,float biasDb,const float bandFreq[kMaxBands],const float bandSens[kMaxBands],const float bandWidth[kMaxBands],const int bandShape[kMaxBands],const float bandFocus[kMaxBands],const bool bandActive[kMaxBands],float transientFactor);
 // Per-bin, per-band shaped sensitivity contribution -- pure function of
 // (hz, band params), no state, no allocation. Public+static so the UI
 // (ControlCurveComponent) can render the ACTUAL mathematical curve instead
 // of an interpolated approximation of it -- guarantees the drawn shape and
 // the real DSP behaviour can never disagree, by construction.
 // focus (0..1): how CONCENTRATED vs UNIFORM the band's own falloff is --
 // 0 plateaus wider/gentler, 1 peaks narrower/sharper, 0.5 is the plain
 // Gaussian/logistic curve the shape used before focus existed. Applies to
 // every shape (Bell family via the exponent, Shelf family via the
 // transition steepness), never touches freq/sens/width.
 static float bandContribution(float hz,float freq,float sens,float widthOct,int shape,float focus);
 // Same clamp-to-[-12,12] combination rule compute() uses internally,
 // exposed for the same reason -- the UI's rendered curve is the exact
 // function the detector evaluates, not a look-alike.
 static float combinedSensitivityAt(float hz,const float bandFreq[kMaxBands],const float bandSens[kMaxBands],const float bandWidth[kMaxBands],const int bandShape[kMaxBands],const float bandFocus[kMaxBands],const bool bandActive[kMaxBands]);
private:
 std::vector<float> smoothGainDb, prefix; double sampleRate=48000; int fftSize=2048;
};
