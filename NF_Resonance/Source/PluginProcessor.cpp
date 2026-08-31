#include "PluginProcessor.h"
#include "PluginEditor.h"

NFResonanceAudioProcessor::NFResonanceAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                 .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMETERS", makeLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout NFResonanceAudioProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f=[&](const char* id,const char* name,float mn,float mx,float def,float step=0.001f){
        p.push_back(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(mn,mx,step),def));};
    f("depth","Depth",0,10,5); f("sharpness","Sharpness",0,10,4); f("selectivity","Selectivity",0,10,3.5f);
    f("attack","Attack",0.1f,200,10,0.1f); f("release","Release",5,1000,80,0.5f);
    f("lowHz","Low Frequency",20,1000,20,1); f("highHz","High Frequency",2000,22000,20000,1);
    f("output","Output",-12,12,0,0.01f); f("mix","Mix",0,100,100,0.1f);
    f("transient","Transient Protect",0,10,5); f("threshold","Detector Bias",-6,12,1.5f,0.01f);
    for (auto id : {"c20","c100","c500","c1k","c5k","c10k","c20k"}) f(id,id,-12,12,0,0.01f);
    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode","Mode",juce::StringArray{"Stereo","L/R","Mid/Side"},0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("delta","Delta",false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bypass","Bypass",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("quality","Quality",juce::StringArray{"Eco","Balanced","High"},1));
    return {p.begin(),p.end()};
}

bool NFResonanceAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet()==l.getMainOutputChannelSet() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void NFResonanceAudioProcessor::prepareToPlay(double sr,int spb)
{
    spectral.prepare(sr,getTotalNumOutputChannels());
    setLatencySamples(spectral.latencySamples());
    dryDelay.setSize(getTotalNumOutputChannels(), spectral.latencySamples()+spb+8); dryDelay.clear(); dryWrite=0;
    bypassMix = apvts.getRawParameterValue("bypass")->load()>0.5f ? 1.0f : 0.0f;
    const float bypassSmoothMs = 15.0f;
    bypassSmoothCoeff = std::exp(-1.0f/(0.001f*bypassSmoothMs*(float)sr));
}
void NFResonanceAudioProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals n;
    const int ch=b.getNumChannels(), ns=b.getNumSamples(), lat=spectral.latencySamples(), ring=dryDelay.getNumSamples();
    juce::AudioBuffer<float> dry(ch,ns);
    for(int i=0;i<ns;++i){
        for(int c=0;c<ch;++c){ float x=b.getSample(c,i); dryDelay.setSample(c,dryWrite,x); int rp=(dryWrite-lat+ring)%ring; dry.setSample(c,i,dryDelay.getSample(c,rp)); }
        dryWrite=(dryWrite+1)%ring;
    }
    // The STFT engine always runs, even while bypassed: its internal ring/
    // history/detector timeline stays continuously synced to real time, so
    // turning bypass off never restarts the algorithmic-latency boundary or
    // splices stale pre-bypass history against fresh post-bypass audio.
    SpectralEngine::Params p;
    p.depth=apvts.getRawParameterValue("depth")->load(); p.sharpness=apvts.getRawParameterValue("sharpness")->load(); p.selectivity=apvts.getRawParameterValue("selectivity")->load();
    p.attackMs=apvts.getRawParameterValue("attack")->load(); p.releaseMs=apvts.getRawParameterValue("release")->load();
    p.lowHz=apvts.getRawParameterValue("lowHz")->load(); p.highHz=apvts.getRawParameterValue("highHz")->load(); p.transient=apvts.getRawParameterValue("transient")->load(); p.biasDb=apvts.getRawParameterValue("threshold")->load();
    p.mode=(int)apvts.getRawParameterValue("mode")->load(); p.delta=apvts.getRawParameterValue("delta")->load()>0.5f;
    const char* ids[]={"c20","c100","c500","c1k","c5k","c10k","c20k"}; for(int i=0;i<7;++i)p.curveDb[i]=apvts.getRawParameterValue(ids[i])->load();
    spectral.setParams(p); spectral.process(b);

    const float mix=apvts.getRawParameterValue("mix")->load()/100.0f, out=juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());
    const bool bypassed = apvts.getRawParameterValue("bypass")->load()>0.5f;
    const float bypassTarget = bypassed ? 1.0f : 0.0f;
    for(int i=0;i<ns;++i){
        bypassMix = bypassTarget + (bypassMix-bypassTarget)*bypassSmoothCoeff;
        for(int c=0;c<ch;++c){
            float wet=b.getSample(c,i);
            float processed = p.delta?wet:(dry.getSample(c,i)*(1.0f-mix)+wet*mix);
            float y = dry.getSample(c,i)*bypassMix + processed*(1.0f-bypassMix);
            b.setSample(c,i,y*out);
        }
    }
}
void NFResonanceAudioProcessor::getStateInformation(juce::MemoryBlock& d){ if(auto xml=apvts.copyState().createXml()) copyXmlToBinary(*xml,d); }
void NFResonanceAudioProcessor::setStateInformation(const void* d,int s){ if(auto xml=getXmlFromBinary(d,s)) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessorEditor* NFResonanceAudioProcessor::createEditor(){ return new NFResonanceAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new NFResonanceAudioProcessor(); }
