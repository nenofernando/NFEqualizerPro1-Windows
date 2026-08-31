#include "ResonanceDetector.h"
void ResonanceDetector::prepare(int bins,double sr,int fft){sampleRate=sr;fftSize=fft;smoothGainDb.assign((size_t)bins,0);prefix.assign((size_t)bins+1,0);}
void ResonanceDetector::reset(){std::fill(smoothGainDb.begin(),smoothGainDb.end(),0);}
float ResonanceDetector::curveAt(float hz,const float c[7]) const{ static const float f[7]={20,100,500,1000,5000,10000,20000}; if(hz<=f[0])return c[0]; for(int i=0;i<6;++i)if(hz<=f[i+1]){float t=(std::log(hz)-std::log(f[i]))/(std::log(f[i+1])-std::log(f[i]));return juce::jmap(t,c[i],c[i+1]);} return c[6]; }
void ResonanceDetector::compute(const std::vector<float>& m,std::vector<float>& r,float depth,float sharp,float sel,float atk,float rel,float lo,float hi,float bias,const float curve[7],float transient){
 const int n=(int)m.size(); if((int)r.size()!=n)r.resize(n); prefix[0]=0; for(int i=0;i<n;++i)prefix[(size_t)i+1]=prefix[(size_t)i]+m[(size_t)i];
 const int radius=juce::jlimit(2,48,(int)std::round(34.0f-sharp*2.8f));
 const float maxRed=depth*2.4f; const float thr=juce::jmap(sel,0.0f,10.0f,5.5f,0.7f)+bias;
 const float frameMs=1000.0f*512.0f/(float)sampleRate; float aa=std::exp(-frameMs/juce::jmax(0.1f,atk)), rr=std::exp(-frameMs/juce::jmax(1.0f,rel));
 for(int i=0;i<n;++i){ float hz=(float)i*(float)sampleRate/(float)fftSize; int a=juce::jmax(0,i-radius),b=juce::jmin(n-1,i+radius); float mean=(prefix[(size_t)b+1]-prefix[(size_t)a])/(float)(b-a+1); float prominence=m[(size_t)i]-mean; float target=0; if(hz>=lo&&hz<=hi){ float sens=curveAt(juce::jmax(20.0f,hz),curve); float over=prominence-(thr-sens*0.18f); if(over>0) target=-juce::jmin(maxRed,over*juce::jmap(sel,0.0f,10.0f,0.65f,1.65f))*transient; }
   float &g=smoothGainDb[(size_t)i]; float coeff=(target<g)?aa:rr; g=target+(g-target)*coeff; r[(size_t)i]=g; }
}