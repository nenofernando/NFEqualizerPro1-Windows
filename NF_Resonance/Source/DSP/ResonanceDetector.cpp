#include "ResonanceDetector.h"
#include <algorithm>
void ResonanceDetector::prepare(int bins,double sr,int fft){sampleRate=sr;fftSize=fft;smoothGainDb.assign((size_t)bins,0);prefix.assign((size_t)bins+1,0);}
void ResonanceDetector::reset(){std::fill(smoothGainDb.begin(),smoothGainDb.end(),0);}

// Per-band shaped contribution at a given frequency. u = signed distance
// from the band's own centre, in units of the band's OWN width (octaves) --
// this is what makes a band genuinely self-localized: its influence is
// defined entirely by its own (freq, width), never by neighbouring bands or
// by fixed 20Hz/20kHz anchors. All 6 shapes decay to ~0 far from the band's
// centre (Bell/WideBell/Focus both sides; Shelf saturates to `sens` on one
// side and decays to ~0 on the other), so a single isolated band naturally
// stays local -- the virtual-anchor mechanism used before Width existed is
// no longer needed and has been removed (see compute()). Width's meaning is
// coherent per shape by construction: for Bell/WideBell it directly opens/
// closes the central bump; for Shelf/Focus it's the transition's softness,
// since `widthOct` is the only thing u is scaled by in every branch.
// Shapes: 0=Bell 1=WideBell 2=LowShelf 3=HighShelf 4=LowFocus 5=HighFocus.
float ResonanceDetector::bandContribution(float hz,float freq,float sens,float widthOct,int shape,float focus){
 float u=(std::log2(juce::jmax(1.0f,hz))-std::log2(juce::jmax(1.0f,freq)))/juce::jmax(0.02f,widthOct);
 // focus (0..1) reshapes the falloff's CONCENTRATION: 0.5 is the original
 // plain Gaussian/logistic (unchanged from before focus existed); below 0.5
 // it plateaus wider/more uniform, above 0.5 it peaks narrower/sharper --
 // real effect on the actual curve, not a cosmetic-only control.
 float sharp=juce::jmap(juce::jlimit(0.0f,1.0f,focus),0.0f,1.0f,0.6f,2.2f);
 auto gauss=[&](float uu){ return std::exp(-0.5f*std::pow(std::abs(uu),2.0f*sharp)); };
 switch(shape){
  case 1: { float uu=u/1.8f; return sens*gauss(uu); }                                            // Wide Bell: same shape, naturally wider (fixed multiplier on top of the band's own width)
  case 2: return sens/(1.0f+std::exp(u*4.0f*sharp));                                             // Low Shelf: full sens below freq, decays above
  case 3: return sens/(1.0f+std::exp(-u*4.0f*sharp));                                            // High Shelf: full sens above freq, decays below
  case 4: { float uu=u<0.0f?u:u*2.5f; return sens*gauss(uu); }                                   // Low Focus: narrower above freq (favours content below)
  case 5: { float uu=u>0.0f?u:u*2.5f; return sens*gauss(uu); }                                   // High Focus: narrower below freq (favours content above)
  default: return sens*gauss(u);                                                                 // Bell: symmetric
 }
}
// Combination rule: SUM every active band's contribution, then CLAMP to a
// single band's own [-12,+12] range. Summing lets nearby bands reinforce or
// cancel each other (positive+positive genuinely adds, positive+negative
// genuinely partially cancels -- both musically expected), while the clamp
// guarantees the result can never run away regardless of how many bands
// overlap (10, 32, all at the same frequency, etc.) -- continuous, bounded,
// deterministic, independent of overlap count.
float ResonanceDetector::combinedSensitivityAt(float hz,const float bandFreq[kMaxBands],const float bandSens[kMaxBands],const float bandWidth[kMaxBands],const int bandShape[kMaxBands],const float bandFocus[kMaxBands],const bool bandActive[kMaxBands]){
 float sum=0.0f;
 for(int k=0;k<kMaxBands;++k) if(bandActive[k]) sum+=bandContribution(hz,bandFreq[k],bandSens[k],bandWidth[k],bandShape[k],bandFocus[k]);
 return juce::jlimit(-12.0f,12.0f,sum);
}
void ResonanceDetector::compute(const std::vector<float>& m,std::vector<float>& r,float depth,float sharp,float sel,float atk,float rel,float lo,float hi,float bias,const float bandFreq[kMaxBands],const float bandSens[kMaxBands],const float bandWidth[kMaxBands],const int bandShape[kMaxBands],const float bandFocus[kMaxBands],const bool bandActive[kMaxBands],float transient){
 const int n=(int)m.size(); if((int)r.size()!=n)r.resize(n); prefix[0]=0; for(int i=0;i<n;++i)prefix[(size_t)i+1]=prefix[(size_t)i]+m[(size_t)i];
 const int radius=juce::jlimit(2,48,(int)std::round(34.0f-sharp*2.8f));
 const float maxRed=depth*2.4f; const float thr=juce::jmap(sel,0.0f,10.0f,5.5f,0.7f)+bias;
 const float frameMs=1000.0f*512.0f/(float)sampleRate; float aa=std::exp(-frameMs/juce::jmax(0.1f,atk)), rr=std::exp(-frameMs/juce::jmax(1.0f,rel));
 for(int i=0;i<n;++i){ float hz=(float)i*(float)sampleRate/(float)fftSize; int a=juce::jmax(0,i-radius),b=juce::jmin(n-1,i+radius); float mean=(prefix[(size_t)b+1]-prefix[(size_t)a])/(float)(b-a+1); float prominence=m[(size_t)i]-mean; float target=0; if(hz>=lo&&hz<=hi){ float sens=combinedSensitivityAt(juce::jmax(20.0f,hz),bandFreq,bandSens,bandWidth,bandShape,bandFocus,bandActive); float over=prominence-(thr-sens*0.18f); if(over>0) target=-juce::jmin(maxRed,over*juce::jmap(sel,0.0f,10.0f,0.65f,1.65f))*transient; }
   float &g=smoothGainDb[(size_t)i]; float coeff=(target<g)?aa:rr; g=target+(g-target)*coeff; r[(size_t)i]=g; }
}
