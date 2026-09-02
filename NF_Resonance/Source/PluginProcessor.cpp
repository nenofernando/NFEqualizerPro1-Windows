#include "PluginProcessor.h"
#include "PluginEditor.h"

NFResonanceAudioProcessor::NFResonanceAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                 .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                 // EXTERNAL SIDECHAIN: optional second input bus, OFF by default
                                 // (starts disabled -- most hosts require the user to explicitly
                                 // enable a sidechain bus in routing). Purely a detector input --
                                 // never mixed into the output, never widens the main channel
                                 // count. See isBusesLayoutSupported() for the accepted shapes.
                                 .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
  apvts(*this, nullptr, "PARAMETERS", makeLayout())
{
    // Cache band parameter pointers once here (string concatenation is fine
    // off the audio thread) so processBlock() never builds a String or does
    // a name lookup -- see the member declarations in the header.
    for (int i = 0; i < SpectralEngine::kMaxBands; ++i)
    {
        bandActiveParams[i] = apvts.getRawParameterValue("band_active_" + juce::String(i));
        bandFreqParams[i] = apvts.getRawParameterValue("band_freq_" + juce::String(i));
        bandSensParams[i] = apvts.getRawParameterValue("band_sens_" + juce::String(i));
        bandWidthParams[i] = apvts.getRawParameterValue("band_width_" + juce::String(i));
        bandShapeParams[i] = apvts.getRawParameterValue("band_shape_" + juce::String(i));
        bandFocusParams[i] = apvts.getRawParameterValue("band_focus_" + juce::String(i));
    }
    lowEnabledParam = apvts.getRawParameterValue("lowEnabled");
    highEnabledParam = apvts.getRawParameterValue("highEnabled");
    // Legacy automation compatibility: bidirectional live link between the 7
    // old freq_c*/c* IDs and band slots 0-6 -- see parameterChanged() and the
    // header comment.
    const char* legacySensIds[7]={"c20","c100","c500","c1k","c5k","c10k","c20k"};
    const char* legacyFreqIds[7]={"freq_c20","freq_c100","freq_c500","freq_c1k","freq_c5k","freq_c10k","freq_c20k"};
    for (int i=0;i<7;++i)
    {
        apvts.addParameterListener(legacySensIds[i], this);
        apvts.addParameterListener(legacyFreqIds[i], this);
        apvts.addParameterListener("band_sens_"+juce::String(i), this);
        apvts.addParameterListener("band_freq_"+juce::String(i), this);
    }
}

void NFResonanceAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (syncingLegacy) return; // reentrancy guard -- breaks the A->B->A loop
    static const char* legacySensIds[7]={"c20","c100","c500","c1k","c5k","c10k","c20k"};
    static const char* legacyFreqIds[7]={"freq_c20","freq_c100","freq_c500","freq_c1k","freq_c5k","freq_c10k","freq_c20k"};
    for (int i=0;i<7;++i)
    {
        juce::String bandFreqId="band_freq_"+juce::String(i), bandSensId="band_sens_"+juce::String(i);
        juce::RangedAudioParameter* target=nullptr;
        bool fromLegacy=false;
        if (parameterID==legacyFreqIds[i]) { target=apvts.getParameter(bandFreqId); fromLegacy=true; }
        else if (parameterID==legacySensIds[i]) { target=apvts.getParameter(bandSensId); fromLegacy=true; }
        else if (parameterID==bandFreqId) target=apvts.getParameter(legacyFreqIds[i]);
        else if (parameterID==bandSensId) target=apvts.getParameter(legacySensIds[i]);
        else continue;
        if (target)
        {
            syncingLegacy=true;
            target->setValueNotifyingHost(target->convertTo0to1(newValue));
            // A brand-new instance opens with slots 0-6 inactive (empty
            // canvas). Old hosts/automation lanes on the legacy c*/freq_c*
            // IDs have no notion of an "active" flag at all -- for them the
            // band was always conceptually on -- so driving a legacy ID must
            // still produce a real, audible change even without ever going
            // through the setStateInformation migration path.
            if (fromLegacy)
                if (auto* activeParam = apvts.getParameter("band_active_"+juce::String(i)))
                    if (activeParam->getValue() < 0.5f)
                        activeParam->setValueNotifyingHost(1.0f);
            syncingLegacy=false;
        }
        return;
    }
}

// AU compliance fix: auval flagged the legacy<->band-0-6 bidirectional live
// sync as invalid ("Parameter values are different since last set -- a Meta
// Param Flag is NOT set on a parameter that will change values of other
// parameters"), because parameterChanged() genuinely does mutate a SECOND
// parameter as a side effect of the first. AU has a sanctioned mechanism for
// exactly this (kAudioUnitParameterFlag_IsGlobalMeta, surfaced in JUCE via
// AudioProcessorParameter::isMetaParameter()) -- this thin subclass is the
// only way to set it, since the plain AudioParameterFloat/Bool constructors
// don't expose it. Only the 28 parameters actually involved in the
// bidirectional sync (the 14 legacy IDs + band_freq_0-6/band_sens_0-6) need
// it; nothing else in the plugin changes another parameter's value.
class MetaAudioParameterFloat : public juce::AudioParameterFloat
{
public:
    using juce::AudioParameterFloat::AudioParameterFloat;
    bool isMetaParameter() const override { return true; }
};

juce::AudioProcessorValueTreeState::ParameterLayout NFResonanceAudioProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f=[&](const char* id,const char* name,float mn,float mx,float def,float step=0.001f){
        p.push_back(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(mn,mx,step),def));};
    auto fMeta=[&](const char* id,const char* name,float mn,float mx,float def,float step=0.001f){
        p.push_back(std::make_unique<MetaAudioParameterFloat>(id,name,juce::NormalisableRange<float>(mn,mx,step),def));};
    // Depth default = 0 (Preview Safe Default): Detector V1's aggressiveness
    // is a known, separately-tracked issue (see the LOW/HIGH neutrality
    // investigation) -- a new instance must never audibly darken/lower the
    // mix just by existing. This is explicitly temporary: once Detector V2
    // is finished, Depth gets recalibrated into a real musical default.
    f("depth","Depth",0,10,0); f("sharpness","Sharpness",0,10,4); f("selectivity","Selectivity",0,10,3.5f);
    f("detail","Detail",0,10,5);
    f("attack","Attack",0.1f,200,10,0.1f); f("release","Release",5,1000,80,0.5f);
    f("lowHz","Low Frequency",20,1000,100,0.1f); f("highHz","High Frequency",2000,22000,16000,0.1f); // fine step: now also draggable as analyzer handles, not just knobs
    // Independent ON/OFF for the LOW/HIGH range boundary -- OFF means that
    // side of the detector's active region is fully open, WITHOUT touching
    // the saved lowHz/highHz value (see SpectralEngine::process()). Real,
    // save/recall/automatable parameters -- clicking the LOW/HIGH handle in
    // the UI (or turning the knob while it's off) toggles these.
    p.push_back(std::make_unique<juce::AudioParameterBool>("lowEnabled","Low Cut Enabled",true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("highEnabled","High Cut Enabled",true));
    f("output","Output",-12,12,0,0.01f); f("mix","Mix",0,100,100,0.1f);
    f("transient","Transient Protect",0,10,5); f("threshold","Detector Bias",-6,12,1.5f,0.01f);
    // DETAIL -- Sonic Alpha V2. A third, independent concept alongside
    // SHARPNESS (broad/medium/narrow structure preference) and SELECTIVITY
    // (how much evidence/confidence is required to call something a
    // problem). DETAIL controls the granularity/resolution of the FINAL
    // reduction mask itself, applied AFTER the full decision pipeline
    // (Problem Confidence -> Selectivity -> Transient Protection -> White
    // Sensitivity Curve -> gamma/action shaping -> raw local gain mask) and
    // BEFORE temporal gain smoothing -- see GainMaskEngine::process(). Low
    // Detail groups nearby resonances into a smoother, broader/aggregated
    // mask (fewer independent small reductions); High Detail preserves
    // small spectral structures, letting narrow resonances close together
    // be acted on individually. Default 5 reproduces the Sonic Alpha
    // Calibration 1 baseline (matching sharpness/selectivity's 0-10
    // convention); Detail itself never changes how MUCH is reduced
    // (Depth's job), only how spatially granular that reduction is.
    // LEGACY (0.1h): the original 7 fixed-ID sensitivity + frequency params.
    // Kept permanently, unchanged, for old-session/automation-lane
    // compatibility -- setStateInformation() migrates their loaded values
    // into band slots 0-6 on first load of a pre-multiband session (see
    // below). They are never read by processBlock() any more; the 32-slot
    // band arrays are the DSP's only source of truth going forward.
    for (auto id : {"c20","c100","c500","c1k","c5k","c10k","c20k"}) fMeta(id,id,-12,12,0,0.01f);
    { const char* freqIds[7]={"freq_c20","freq_c100","freq_c500","freq_c1k","freq_c5k","freq_c10k","freq_c20k"};
      const float freqDefaults[7]={20.0f,100.0f,500.0f,1000.0f,5000.0f,10000.0f,20000.0f};
      for (int i=0;i<7;++i) fMeta(freqIds[i],freqIds[i],20.0f,20000.0f,freqDefaults[i],0.01f); }
    // Multiband Sensitivity Curve (0.1q): 32 statically pre-allocated slots
    // (AU/VST3/AAX require static parameter lists -- this is a fixed
    // ceiling, never created at runtime). Slots 0-6 default to mirror the
    // legacy 7 points exactly (active, same frequencies, 0 sensitivity);
    // slots 7-31 default inactive. curveAt() reads ONLY these.
    {
        const float legacyFreqDefaults[7]={20.0f,100.0f,500.0f,1000.0f,5000.0f,10000.0f,20000.0f};
        for (int i=0;i<SpectralEngine::kMaxBands;++i)
        {
            // A brand-new instance opens with an EMPTY curve (no bands, free
            // canvas) -- only an old-format session loaded through
            // setStateInformation's legacy migration explicitly activates
            // slots 0-6 (see the isLegacySession branch below).
            bool defaultActive = false;
            float defaultFreq = i<7 ? legacyFreqDefaults[i] : 1000.0f;
            p.push_back(std::make_unique<juce::AudioParameterBool>("band_active_"+juce::String(i),"Band "+juce::String(i)+" Active",defaultActive));
            // Slots 0-6's freq/sens are the OTHER half of the bidirectional
            // legacy sync, so they need the meta flag too; slots 7-31 have
            // no legacy counterpart and use the plain (non-meta) param.
            if (i<7)
            {
                fMeta(("band_freq_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Frequency").toRawUTF8(),20.0f,20000.0f,defaultFreq,0.01f);
                fMeta(("band_sens_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Sensitivity").toRawUTF8(),-12.0f,12.0f,0.0f,0.01f);
            }
            else
            {
                f(("band_freq_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Frequency").toRawUTF8(),20.0f,20000.0f,defaultFreq,0.01f);
                f(("band_sens_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Sensitivity").toRawUTF8(),-12.0f,12.0f,0.0f,0.01f);
            }
            // Checkpoint A: Width/Q (octaves -- how far in log-frequency this
            // band's influence reaches) and Shape (the functional form of
            // that influence). Default width ~1.7oct approximates the old
            // fixed 7-point polyline's average spacing, so a migrated legacy
            // session isn't drastically different in feel even though the
            // underlying model (per-band shaped contribution, summed) is new.
            f(("band_width_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Width").toRawUTF8(),0.05f,4.0f,0.4f,0.01f);
            p.push_back(std::make_unique<juce::AudioParameterChoice>("band_shape_"+juce::String(i),"Band "+juce::String(i)+" Shape",
                juce::StringArray{"Bell","Wide Bell","Low Shelf","High Shelf","Low Focus","High Focus"},0));
            // Focus (0..1, default 0.5 = neutral/unchanged from before this
            // parameter existed): how CONCENTRATED vs UNIFORM the band's own
            // falloff is -- real effect on ResonanceDetector::bandContribution
            // (the exponent/transition-steepness term), not a cosmetic knob.
            f(("band_focus_"+juce::String(i)).toRawUTF8(),("Band "+juce::String(i)+" Focus").toRawUTF8(),0.0f,1.0f,0.5f,0.01f);
        }
    }
    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode","Mode",juce::StringArray{"Stereo","L/R","Mid/Side"},0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("delta","Delta",false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bypass","Bypass",false));
    // EXTERNAL SIDECHAIN (Etapa 1): which spectral content feeds the
    // detector (PHYSICAL C/D/ConfidenceEngine/GainMaskEngine's own
    // detection stage). INTERNAL (default) = the main signal, exactly
    // today's behaviour. SIDECHAIN = the optional second input bus's own
    // spectral content instead -- the Gain Mask itself is still applied to
    // the MAIN signal's own bins either way (see SpectralEngine::frame()).
    // Automatable/saved like any other choice parameter.
    p.push_back(std::make_unique<juce::AudioParameterChoice>("detectorSource","Detector Source",juce::StringArray{"Internal","Sidechain"},0));
    // MAX REDUCTION: a real ceiling on reduction magnitude, independent of
    // Depth/Selectivity/Detail/resonance strength -- see GainMaskEngine::
    // setMaxReduction(). OFF by default so the plugin's current sound is
    // unchanged unless the user explicitly opts in.
    p.push_back(std::make_unique<juce::AudioParameterBool>("maxReductionEnabled","Max Reduction Enabled",false));
    f("maxReductionDb","Max Reduction",0.5f,12.0f,3.0f,0.01f);
    p.push_back(std::make_unique<juce::AudioParameterBool>("showOriginalFft","Show Original FFT",false)); // 0.1p: OFF by default, per request
    return {p.begin(),p.end()};
}

bool NFResonanceAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != l.getMainOutputChannelSet()) return false;
    auto mainSet = l.getMainOutputChannelSet();
    if (mainSet != juce::AudioChannelSet::mono() && mainSet != juce::AudioChannelSet::stereo()) return false;
    // Sidechain (input bus index 1): disabled, mono, or stereo -- never
    // required, never changes the main output's own channel count.
    if (l.inputBuses.size() > 1)
    {
        auto sc = l.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
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
    // ch must be the MAIN bus's own channel count, never b.getNumChannels()
    // -- with the optional Sidechain input bus enabled, the combined buffer
    // JUCE hands processBlock() carries extra channels beyond the main
    // signal, and dry/output must only ever touch the main ones.
    const int ch=getTotalNumOutputChannels(), ns=b.getNumSamples(), lat=spectral.latencySamples(), ring=dryDelay.getNumSamples();
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
    p.detail=apvts.getRawParameterValue("detail")->load();
    p.lowHz=apvts.getRawParameterValue("lowHz")->load(); p.highHz=apvts.getRawParameterValue("highHz")->load(); p.transient=apvts.getRawParameterValue("transient")->load(); p.biasDb=apvts.getRawParameterValue("threshold")->load();
    p.lowEnabled=lowEnabledParam->load()>0.5f; p.highEnabled=highEnabledParam->load()>0.5f;
    p.mode=(int)apvts.getRawParameterValue("mode")->load(); p.delta=apvts.getRawParameterValue("delta")->load()>0.5f;
    p.detectorSource=(int)apvts.getRawParameterValue("detectorSource")->load();
    p.maxReductionEnabled=apvts.getRawParameterValue("maxReductionEnabled")->load()>0.5f;
    p.maxReductionDb=apvts.getRawParameterValue("maxReductionDb")->load();
    for(int i=0;i<SpectralEngine::kMaxBands;++i)
    {
        p.bandActive[i]=bandActiveParams[i]->load()>0.5f;
        p.bandFreq[i]=bandFreqParams[i]->load();
        p.bandSens[i]=bandSensParams[i]->load();
        p.bandWidth[i]=bandWidthParams[i]->load();
        p.bandShape[i]=(int)bandShapeParams[i]->load();
        p.bandFocus[i]=bandFocusParams[i]->load();
    }
    // EXTERNAL SIDECHAIN: only ever a detector INPUT (see
    // SpectralEngine::process()'s own sidechain parameter) -- never mixed
    // into the main signal. Safe fallback: if the bus doesn't exist, isn't
    // enabled, or has zero channels (SIDECHAIN selected but the host never
    // routed anything to it), scAvailable stays false and SpectralEngine
    // transparently falls back to the main signal for detection, exactly
    // like INTERNAL -- no special-casing needed downstream, no NaN risk
    // (an unfed/zeroed sidechain ring just reads as silence, which the
    // detector already handles safely).
    juce::AudioBuffer<float> scBuf;
    bool scAvailable = false;
    if (auto* scBus = getBus(true, 1))
        if (scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        { scBuf = getBusBuffer(b, true, 1); scAvailable = true; }
    spectral.setParams(p); spectral.process(b, scAvailable ? &scBuf : nullptr);

    const float mix=apvts.getRawParameterValue("mix")->load()/100.0f, out=juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());
    const bool bypassed = apvts.getRawParameterValue("bypass")->load()>0.5f;
    const float bypassTarget = bypassed ? 1.0f : 0.0f;
    for(int i=0;i<ns;++i){
        bypassMix = bypassTarget + (bypassMix-bypassTarget)*bypassSmoothCoeff;
        for(int c=0;c<ch;++c){
            float wet=b.getSample(c,i);
            float processed = p.delta?wet:(dry.getSample(c,i)*(1.0f-mix)+wet*mix);
            // Output gain belongs ONLY to the processed path -- applying it
            // after the blend (the old `(dry*bypassMix + processed*(1-
            // bypassMix)) * out` form) let Output keep changing the audible
            // level even at bypassMix==1 (fully bypassed), which is exactly
            // the bug: bypass must be dry, latency-aligned, and completely
            // deaf to Output/Depth/Sharpness/Selectivity/Attack/Release/Mix/
            // bands/LOW-HIGH -- all of which only ever reach `processed`,
            // never `dry`.
            float y = dry.getSample(c,i)*bypassMix + processed*out*(1.0f-bypassMix);
            b.setSample(c,i,y);
        }
    }
}
void NFResonanceAudioProcessor::getStateInformation(juce::MemoryBlock& d){ if(auto xml=apvts.copyState().createXml()) copyXmlToBinary(*xml,d); }
void NFResonanceAudioProcessor::setStateInformation(const void* d,int s)
{
    auto xml=getXmlFromBinary(d,s);
    if(!xml) return;
    // A session saved before the multiband Sensitivity Curve existed has no
    // "band_active_0" entry at all -- detect that BEFORE replaceState()
    // overwrites everything with the loaded values (band_* will simply keep
    // their compiled-in defaults from a legacy file, since JUCE leaves
    // unmentioned parameters alone).
    const bool isLegacySession = xml->getChildByAttribute("id","band_active_0")==nullptr;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    if(isLegacySession)
    {
        // Migrate the 7 legacy points' CURRENT (just-loaded) values into
        // band slots 0-6 and activate them, so an old preset's actual curve
        // shape is preserved exactly -- the legacy c*/freq_c* params
        // themselves stay loaded too (for any automation lanes bound to
        // them) but are otherwise inert from here on.
        const char* sensIds[7]={"c20","c100","c500","c1k","c5k","c10k","c20k"};
        const char* freqIds[7]={"freq_c20","freq_c100","freq_c500","freq_c1k","freq_c5k","freq_c10k","freq_c20k"};
        for(int i=0;i<7;++i)
        {
            float freq=apvts.getRawParameterValue(freqIds[i])->load();
            float sens=apvts.getRawParameterValue(sensIds[i])->load();
            if(auto* ap=apvts.getParameter("band_active_"+juce::String(i))) ap->setValueNotifyingHost(1.0f);
            if(auto* fp=apvts.getParameter("band_freq_"+juce::String(i))) fp->setValueNotifyingHost(fp->convertTo0to1(freq));
            if(auto* sp=apvts.getParameter("band_sens_"+juce::String(i))) sp->setValueNotifyingHost(sp->convertTo0to1(sens));
        }
    }
}
juce::AudioProcessorEditor* NFResonanceAudioProcessor::createEditor(){ return new NFResonanceAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new NFResonanceAudioProcessor(); }
