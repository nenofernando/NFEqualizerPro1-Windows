#pragma once
#include <JuceHeader.h>
#include "ResonanceDetector.h"
#include "TransientGuard.h"
class SpectralEngine {
public:
 // Multiband Sensitivity Curve (0.1q): 32 pre-allocated slots (AU/VST3/AAX
 // need static parameters, so this is a fixed ceiling, never a runtime
 // allocation). Each slot is independently active/inactive; inactive slots
 // simply don't contribute to curveAt() -- they aren't destroyed.
 static constexpr int kMaxBands = 32;
 // Checkpoint A: bandWidth (octaves, how far this band's influence reaches
 // in log-frequency) + bandShape (0=Bell,1=LowShelf,2=HighShelf,3=LowFocus,
 // 4=HighFocus) alongside the existing freq/sens/active per slot.
 struct Params{ float depth=5,sharpness=4,selectivity=3.5f,attackMs=10,releaseMs=80,lowHz=20,highHz=20000,transient=5,biasDb=1.5f,
   bandFreq[kMaxBands]{}, bandSens[kMaxBands]{}, bandWidth[kMaxBands]{}, bandFocus[kMaxBands]{}; bool bandActive[kMaxBands]{}; int bandShape[kMaxBands]{}; int mode=0; bool delta=false;
   // lowEnabled/highEnabled OFF means that side of the range is fully open
   // for the detector's gate -- lowHz/highHz themselves are left untouched
   // (still whatever the user last set/automated), only the EFFECTIVE bound
   // passed to ResonanceDetector::compute() changes. See SpectralEngine::process().
   bool lowEnabled=true, highEnabled=true; };
 // Production path uses LeftPad (true left-padded/negative-time-framed STFT)
 // exclusively -- see below. TimeGate and NormGate remain only so the offline
 // test harness can still A/B them against LeftPad for historical comparison;
 // neither is reachable from the plugin UI and neither is the default.
 enum class WarmupMode { TimeGate, NormGate, PreRoll, LeftPad };
 void setWarmupMode(WarmupMode m){ warmupMode=m; }
 void prepare(double sr,int channels); void reset(); void setParams(const Params& p){params=p;} void process(juce::AudioBuffer<float>&); int latencySamples() const{return fftSize;}
 std::vector<float> getLastSpectrum() const; std::vector<float> getLastReduction() const;

 // ---- Investigation-only instrumentation (never used by the real plugin path) ----
 struct FrameDebugSnapshot {
   long long t=0, start=0;
   std::vector<float> magDb;            // analyzed spectrum magnitude, dB
   std::vector<float> reductionDb;      // detector's per-bin mask, dB (<=0 always if untouched)
   std::vector<float> timeContribution; // fftSize samples: fftData[k]*window[k] (what gets added to ola[])
   float framePeakBeforeOLA=0;          // peak(|timeContribution|) -- this frame's own contribution alone
 };
 // Testing-only mask override, applied AFTER the real detector computes reductionDb,
 // for the first `forceMaskFrameLimit` frames of a run (post-reset). Never engaged
 // unless explicitly set by the test harness.
 enum class MaskOverride { None, Unity, Constant, ReuseFirstFull };
 void setDebugCapture(bool on, int limitPerChannel=8){ debugCapture=on; debugCaptureLimit=limitPerChannel; for(auto& s:debugPerChan) s.clear(); }
 const std::vector<FrameDebugSnapshot>& getDebugSnapshots(int channel) const { return debugPerChan[(size_t)channel]; }
 void setMaskOverrideForTesting(MaskOverride mode, float constantGainDb, int frameLimit){ maskOverride=mode; maskOverrideConstantDb=constantGainDb; maskOverrideFrameLimit=frameLimit; }
private:
 static constexpr int order=11,fftSize=1<<order,hop=fftSize/4,ringSize=fftSize*4;
 static constexpr int numOverlaps=fftSize/hop;
 // OLA normalization safety: dividing by norm=sum(w^2) is exact in real
 // arithmetic for ANY nonzero norm (numerator and denominator share the same
 // window factor), but near a single window's own zero-taper edge, norm can
 // be numerically tiny (~1e-6) while float32 FFT round-trip error is ~1e-7
 // absolute -- the ratio's relative error blows up. steadyNormRef is the
 // actual norm value the real accumulation converges to once fully
 // overlapped (computed once in prepare() by running the same accumulation
 // logic with silence, so it is exactly what this window/hop config
 // produces, not a guessed constant). Only samples whose norm is still a
 // tiny fraction of that reference are gated to silence; everything else is
 // reconstructed normally, so legitimate signal is not delayed beyond the
 // handful of samples that are genuinely ill-conditioned.
 static constexpr float normFloorFraction=1e-3f;
 // Deterministic time-gate threshold (Option A, superseded).
 static constexpr int fullOverlapAt=fftSize+(numOverlaps-1)*hop;
 float steadyNormRef=1.0f;
 // Option C (virtual pre-roll / left zero-padding): the norm ring value that
 // (numOverlaps-1) virtual all-silent frames -- fired at the same hop cadence
 // the real frames would use, ending exactly where the real stream begins --
 // would have left behind. Precomputed once in prepare() from the same
 // accumulation formula frame() uses, so it is exactly what a genuinely
 // continuous, correctly-zero-padded stream would have accumulated by t=0,
 // not a guessed constant. reset() seeds Chan::norm with this so real content
 // is well-conditioned from the first output sample without gating.
 std::vector<float> preRollNorm;
 // Production path: true left-padded STFT. Real frames fire starting at
 // t=hop instead of t=fftSize; Chan::history is zero-initialized and only
 // progressively overwritten by real samples, so an early frame's
 // un-overwritten ring slots are already correct zero-padding -- no separate
 // padding logic needed. Every ring position from t=fftSize onward then
 // receives contributions from all numOverlaps frames, each carrying genuine
 // signal wherever that position's absolute input time is >=0, so numerator
 // and denominator agree and reconstruction is exact -- including input
 // sample 0, which is zero-weighted only in the frame started at t=fftSize
 // but has substantial weight in the three earlier real frames.
 //
 // Algorithmic latency validity boundary: output samples 0..fftSize-1 are
 // never promised to carry meaningful content -- that IS what
 // latencySamples()==fftSize means. Frames that only partly overlap real
 // audio (the ones firing before t=fftSize) apply the detector's frequency-
 // selective gain mask across their whole span, including their still-zero-
 // padded portion; because a non-uniform per-bin gain is a filter, and
 // filtering is convolution, this can ring small amounts of energy into
 // samples that were originally exactly silent. That ringing is inaudible
 // everywhere else, but right at a frame's own zero-taper window edge the
 // normalization denominator is naturally tiny, so dividing amplifies it into
 // a real, audible spike -- confirmed by direct measurement to be confined
 // entirely to samples <fftSize, never appearing at or after fftSize.
 // Below is that boundary applied exactly, and only, where it belongs: an
 // explicit floor at t==fftSize, matching the latency contract already
 // reported to the host -- not one sample more.
 static constexpr long long algorithmicLatencyValidityBoundary = fftSize;
 WarmupMode warmupMode=WarmupMode::LeftPad;
 bool debugCapture=false; int debugCaptureLimit=8;
 std::vector<std::vector<FrameDebugSnapshot>> debugPerChan;
 MaskOverride maskOverride=MaskOverride::None; float maskOverrideConstantDb=0.0f; int maskOverrideFrameLimit=0;
 std::vector<int> frameCountPerChan; // how many frames fired since last reset, per channel
 struct Chan{ std::vector<float> history,ola,norm,fftData,magDb,reduction,lastWet; long long t=0; int histPos=0; ResonanceDetector det; TransientGuard trans; };
 std::unique_ptr<juce::dsp::FFT> fft; std::vector<float> window; std::vector<Chan> c; double sampleRate=48000; Params params;
 void frame(Chan& s,float transientFactor,int channelIndex); float processOne(Chan& s,float x,int channelIndex); };
