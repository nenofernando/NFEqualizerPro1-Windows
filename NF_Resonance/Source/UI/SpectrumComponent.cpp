#include "SpectrumComponent.h"
void SpectrumComponent::paint(juce::Graphics& g){auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0a0f14));g.fillRoundedRectangle(r,7);g.setColour(juce::Colour(0xff27313b));for(int i=1;i<8;++i){float x=r.getX()+r.getWidth()*i/8;g.drawVerticalLine((int)x,r.getY()+8,r.getBottom()-8);}for(int i=1;i<6;++i){float y=r.getY()+r.getHeight()*i/6;g.drawHorizontalLine((int)y,r.getX()+8,r.getRight()-8);}auto m=engine.getLastSpectrum(),red=engine.getLastReduction();if(m.size()<2)return;juce::Path p,q;for(size_t i=1;i<m.size();++i){float hz=(float)i*48000.0f/(2.0f*(m.size()-1));float xn=(std::log10(juce::jmax(20.0f,hz))-std::log10(20.0f))/(std::log10(20000.0f)-std::log10(20.0f));float x=r.getX()+juce::jlimit(0.0f,1.0f,xn)*r.getWidth();float y=r.getBottom()-juce::jlimit(0.0f,1.0f,(m[i]+90.0f)/102.0f)*r.getHeight();float yr=r.getCentreY()-red[i]*6.0f;if(i==1){p.startNewSubPath(x,y);q.startNewSubPath(x,yr);}else{p.lineTo(x,y);q.lineTo(x,yr);}}g.setColour(juce::Colour(0xffaeb5bd));g.strokePath(p,juce::PathStrokeType(1.0f));g.setColour(juce::Colour(0xff9a63eb));g.strokePath(q,juce::PathStrokeType(2.0f));

 // RESONANCES overlay -- architecture prep only, inert until V2-B/V2-C exist.
 // Draws nothing when no snapshot is attached (the default) or when it's
 // empty, which is the only state possible today: nothing in this codebase
 // calls ResonanceMapSnapshot::publish() yet, and this code must never
 // invent confidence from V1 or from raw V2-A5 prominence to fake a curve.
 // When real data exists, region.confidence (0..1, smooth, never a hard
 // threshold) is meant to drive this overlay's opacity/intensity -- e.g.
 // interpolating between a discreet low-alpha blue and a fuller neon-blue/
 // cyan as confidence rises -- kept as a comment here rather than code
 // because there is nothing real to drive it with yet.
 if(resonanceMap!=nullptr){
   std::array<ResonanceRegion,ResonanceMapSnapshot::maxRegions> regions;
   int n=resonanceMap->read(regions);
   for(int i=0;i<n;++i){
     // (unreachable today -- publish() is never called anywhere)
     juce::ignoreUnused(regions[(size_t)i]);
   }
 }
}
