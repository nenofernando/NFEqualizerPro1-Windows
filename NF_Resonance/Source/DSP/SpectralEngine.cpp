#include "SpectralEngine.h"
void SpectralEngine::prepare(double sr,int channels){sampleRate=sr;fft=std::make_unique<juce::dsp::FFT>(order);window.resize(fftSize);for(int i=0;i<fftSize;++i)window[(size_t)i]=std::sqrt(0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(fftSize-1)));c.clear();c.resize((size_t)juce::jmax(1,channels));for(auto& s:c){s.history.assign(fftSize,0);s.ola.assign(ringSize,0);s.norm.assign(ringSize,0);s.fftData.assign(fftSize*2,0);s.magDb.assign(fftSize/2+1,-120);s.reduction.assign(fftSize/2+1,0);s.lastWet.assign(fftSize/2+1,0);s.det.prepare(fftSize/2+1,sr,fftSize);s.mask.prepare(sr,fftSize,hop);
   s.scHistory.assign(fftSize,0);s.scFftData.assign(fftSize*2,0);s.scMagDb.assign(fftSize/2+1,-120);}
 // Compute the true steady-state norm value for this window/hop by running the
 // same accumulation logic as frame() would, with silence, so this is exactly
 // what the real algorithm converges to (not a guessed constant).
 { std::vector<float> tmpNorm((size_t)ringSize,0.0f); for(int f=0; f<numOverlaps*2; ++f){ long long start=fftSize+(long long)f*hop; for(int k=0;k<fftSize;++k){ int ri=(int)((start+k)%ringSize); float w=window[(size_t)k]; tmpNorm[(size_t)ri]+=w*w; } } steadyNormRef=tmpNorm[(size_t)(ringSize/2)]; }
 // Option C: precompute what (numOverlaps-1) virtual silent frames -- fired
 // at T = fftSize-(numOverlaps-1)*hop, ..., fftSize-hop, i.e. the same hop
 // cadence real frames use, ending exactly where the first real frame (at
 // T=fftSize) would begin -- leave behind in the norm ring. Content-free, so
 // this only ever affects the norm denominator, never the ola numerator.
 { preRollNorm.assign((size_t)ringSize,0.0f); for(int f=1;f<numOverlaps;++f){ long long start=fftSize-(long long)f*hop; for(int k=0;k<fftSize;++k){ int ri=(int)((start+k)%ringSize); float w=window[(size_t)k]; preRollNorm[(size_t)ri]+=w*w; } } }
 reset();}
void SpectralEngine::reset(){for(auto& s:c){std::fill(s.history.begin(),s.history.end(),0);std::fill(s.ola.begin(),s.ola.end(),0);
   if(warmupMode==WarmupMode::PreRoll) s.norm=preRollNorm; else std::fill(s.norm.begin(),s.norm.end(),0);
   s.t=0;s.histPos=0;s.det.reset();s.trans.reset();s.mask.reset();
   std::fill(s.scHistory.begin(),s.scHistory.end(),0);std::fill(s.scMagDb.begin(),s.scMagDb.end(),-120.0f);}
   frameCountPerChan.assign(c.size(),0);
   if(debugCapture){ debugPerChan.assign(c.size(),{}); } }
float SpectralEngine::processOne(Chan& s,float x,int channelIndex,float scX){ long long t=s.t; int out=(int)(t%ringSize); float nrm=s.norm[(size_t)out];
 bool ok; switch(warmupMode){
   case WarmupMode::TimeGate: ok=(t>=fullOverlapAt && nrm>1e-7f); break;
   case WarmupMode::PreRoll:  ok=(nrm>1e-7f); break;
   case WarmupMode::LeftPad:  ok=(t>=algorithmicLatencyValidityBoundary && nrm>1e-7f); break;
   default: /* NormGate */    ok=(nrm>steadyNormRef*normFloorFraction); break; }
 float y=ok?s.ola[(size_t)out]/nrm:0.0f; s.ola[(size_t)out]=0;s.norm[(size_t)out]=0;
 // Sidechain history ring: same histPos as the main ring below (never a
 // separate index), so the two are always exactly phase-aligned -- no
 // extra synchronization logic needed anywhere else.
 s.history[(size_t)s.histPos]=x; s.scHistory[(size_t)s.histPos]=scX; s.histPos=(s.histPos+1)%fftSize; float tf=s.trans.process(x,params.transient); ++s.t;
 bool frameDue = (warmupMode==WarmupMode::LeftPad) ? (s.t>0 && (s.t%hop)==0) : (s.t>=fftSize && (s.t%hop)==0);
 if(frameDue) frame(s,tf,channelIndex); return y; }
void SpectralEngine::frame(Chan& s,float transientFactor,int channelIndex){ for(int k=0;k<fftSize;++k){int idx=(s.histPos+k)%fftSize;s.fftData[(size_t)k]=s.history[(size_t)idx]*window[(size_t)k];} std::fill(s.fftData.begin()+fftSize,s.fftData.end(),0); fft->performRealOnlyForwardTransform(s.fftData.data());
 for(int i=0;i<=fftSize/2;++i){float re=s.fftData[(size_t)2*i],im=(i==0||i==fftSize/2)?0.0f:s.fftData[(size_t)2*i+1];s.magDb[(size_t)i]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f);}
 // Sonic Alpha V2 gain mask (PHYSICAL C/D-driven) -- REPLACES
 // ResonanceDetector::compute() as what fills s.reduction. Extracts this
 // channel's own raw time-domain samples for the hop that just completed
 // (LowFrequencyHarmonicAnalyzer, inside GainMaskEngine, is causal and
 // needs real audio, not just the spectral frame) from the SAME history
 // ring the STFT itself reads -- no separate buffering, no extra latency.
 for(int k=0;k<hop;++k){ int idx=(s.histPos-hop+k+fftSize)%fftSize; s.hopScratch[(size_t)k]=s.history[(size_t)idx]; }
 // EXTERNAL SIDECHAIN: an independent FFT of the SAME time-aligned ring
 // (scHistory, kept in lockstep with history via the shared histPos in
 // processOne()), same window/fftSize -- only computed when a real
 // sidechain buffer was actually handed to process() this block, so the
 // extra FFT cost is paid only while the feature is in active use.
 // Detection-only: scMagDb/scHopScratch never reach fftData/ola/output.
 if(sidechainAvailable){
   for(int k=0;k<fftSize;++k){int idx=(s.histPos+k)%fftSize;s.scFftData[(size_t)k]=s.scHistory[(size_t)idx]*window[(size_t)k];} std::fill(s.scFftData.begin()+fftSize,s.scFftData.end(),0);
   fft->performRealOnlyForwardTransform(s.scFftData.data());
   for(int i=0;i<=fftSize/2;++i){float re=s.scFftData[(size_t)2*i],im=(i==0||i==fftSize/2)?0.0f:s.scFftData[(size_t)2*i+1];s.scMagDb[(size_t)i]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f);}
   for(int k=0;k<hop;++k){ int idx=(s.histPos-hop+k+fftSize)%fftSize; s.scHopScratch[(size_t)k]=s.scHistory[(size_t)idx]; }
 }
 // Safe fallback: SIDECHAIN is only actually used when both the user
 // selected it AND a real buffer was handed in this block -- otherwise
 // (bus disabled, host never routed anything, or INTERNAL selected)
 // transparently reads the main signal's own magDb/hopScratch, identical
 // to today's behaviour. Never NaN/uninitialized: scMagDb/scHopScratch are
 // always fully sized and zero/silence-initialized even when unused.
 const bool useSidechain = sidechainAvailable && params.detectorSource==1;
 const std::vector<float>& detectorMagDb = useSidechain ? s.scMagDb : s.magDb;
 const float* detectorHop = useSidechain ? s.scHopScratch.data() : s.hopScratch.data();
 s.mask.setParams(params.depth,params.selectivity,params.attackMs,params.releaseMs,params.lowEnabled?params.lowHz:0.0f,params.highEnabled?params.highHz:1.0e9f);
 s.mask.setDetail(params.detail);
 s.mask.setMaxReduction(params.maxReductionEnabled,params.maxReductionDb);
 // White Sensitivity Curve -- same band arrays the UI curve already reads/
 // draws (params.bandFreq/.../bandActive), now also modulating local action
 // authority inside GainMaskEngine (see setSensitivityCurve()).
 s.mask.setSensitivityCurve(params.bandFreq,params.bandSens,params.bandWidth,params.bandShape,params.bandFocus,params.bandActive);
 // Gain Mask ALWAYS applies to the MAIN signal's own bins below (s.fftData,
 // never touched by the sidechain path) -- only the detection INPUT above
 // changes with detectorSource. No sidechain sample ever reaches the output.
 s.mask.process(detectorMagDb,detectorHop,hop,s.reduction);
 (void) transientFactor; // V1's TransientGuard-derived scalar is superseded by PHYSICAL D's own per-band transientProtection inside GainMaskEngine; s.trans itself is left running (unused for gain) rather than removed, see header comment.
 int frameIdx = channelIndex>=0 && channelIndex<(int)frameCountPerChan.size() ? frameCountPerChan[(size_t)channelIndex]++ : -1;
 // Testing-only mask override (never engaged unless the harness explicitly sets it).
 if(maskOverride!=MaskOverride::None && frameIdx>=0 && frameIdx<maskOverrideFrameLimit){
   if(maskOverride==MaskOverride::Unity){ std::fill(s.reduction.begin(),s.reduction.end(),0.0f); }
   else if(maskOverride==MaskOverride::Constant){ std::fill(s.reduction.begin(),s.reduction.end(),maskOverrideConstantDb); }
   // ReuseFirstFull is applied by the harness itself (it needs the captured mask from
   // a later frame that doesn't exist yet at this point in a single forward pass).
 }
 if(debugCapture && channelIndex>=0 && channelIndex<(int)debugPerChan.size() && (int)debugPerChan[(size_t)channelIndex].size()<debugCaptureLimit){
   FrameDebugSnapshot snap; snap.t=s.t; snap.start=s.t; snap.magDb=s.magDb; snap.reductionDb=s.reduction;
   snap.timeContribution.assign((size_t)fftSize,0.0f);
   // computed below after gain is applied and inverse-FFT'd, filled in at the end of this function.
   debugPerChan[(size_t)channelIndex].push_back(std::move(snap));
 }
 for(int i=0;i<=fftSize/2;++i){float g=juce::Decibels::decibelsToGain(s.reduction[(size_t)i]); if(params.delta)g=1.0f-g; s.fftData[(size_t)2*i]*=g; if(i>0&&i<fftSize/2)s.fftData[(size_t)2*i+1]*=g; }
 fft->performRealOnlyInverseTransform(s.fftData.data()); long long start=s.t;
 if(debugCapture && channelIndex>=0 && channelIndex<(int)debugPerChan.size()){
   auto& log = debugPerChan[(size_t)channelIndex];
   if(! log.empty() && log.back().start==start && log.back().timeContribution.size()==(size_t)fftSize){
     float pk=0; for(int k=0;k<fftSize;++k){ float v=s.fftData[(size_t)k]*window[(size_t)k]; log.back().timeContribution[(size_t)k]=v; pk=juce::jmax(pk,std::abs(v)); }
     log.back().framePeakBeforeOLA=pk;
   }
 }
 for(int k=0;k<fftSize;++k){int ri=(int)((start+k)%ringSize);float w=window[(size_t)k];s.ola[(size_t)ri]+=s.fftData[(size_t)k]*w;s.norm[(size_t)ri]+=w*w;} }
void SpectralEngine::process(juce::AudioBuffer<float>& b, const juce::AudioBuffer<float>* sidechain){
 int ch=juce::jmin((int)c.size(),b.getNumChannels());
 const int scCh = sidechain ? sidechain->getNumChannels() : 0;
 sidechainAvailable = scCh > 0;
 // Channel mapping: stereo sidechain -> matching main channel index; mono
 // sidechain -> the same single channel feeds every main channel's own
 // detector (the usual sidechain convention -- one detector signal,
 // applied per-channel). In Mid/Side mode the two "channels" processed
 // below are M and S, not L/R -- the sidechain's own raw channel 0/1 (or
 // its single mono channel) is fed positionally the same way, since
 // Stereo Link/Mid/Side matrixing is explicitly out of scope here.
 auto scSample = [&](int cc, int i) -> float {
   if (scCh <= 0) return 0.0f;
   int idx = scCh == 1 ? 0 : juce::jmin(cc, scCh - 1);
   return sidechain->getSample(idx, i);
 };
 if(ch==2&&params.mode==2){for(int i=0;i<b.getNumSamples();++i){float l=b.getSample(0,i),r=b.getSample(1,i),m=(l+r)*0.70710678f,s=(l-r)*0.70710678f;m=processOne(c[0],m,0,scSample(0,i));s=processOne(c[1],s,1,scSample(1,i));b.setSample(0,i,(m+s)*0.70710678f);b.setSample(1,i,(m-s)*0.70710678f);}} else {for(int cc=0;cc<ch;++cc)for(int i=0;i<b.getNumSamples();++i)b.setSample(cc,i,processOne(c[(size_t)cc],b.getSample(cc,i),cc,scSample(cc,i)));}}
std::vector<float> SpectralEngine::getLastSpectrum() const{return c.empty()?std::vector<float>{}:c[0].magDb;} std::vector<float> SpectralEngine::getLastReduction() const{return c.empty()?std::vector<float>{}:c[0].reduction;}
