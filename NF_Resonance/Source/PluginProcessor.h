#pragma once
#include <JuceHeader.h>
#include "DSP/SpectralEngine.h"

class NFResonanceAudioProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    NFResonanceAudioProcessor(); ~NFResonanceAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "NF Resonance"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState& state() { return apvts; }
    SpectralEngine& engine() { return spectral; }
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
private:
    juce::AudioProcessorValueTreeState apvts;
    SpectralEngine spectral;
    juce::AudioBuffer<float> dryDelay;
    int dryWrite = 0;
    // Bypass keeps the STFT engine running continuously (never resets it),
    // so its internal timeline never goes stale relative to real time. What
    // changes is only the output blend: a smoothed per-sample crossfade
    // between processed and latency-compensated dry, avoiding both an
    // instant-switch click and a spectral-engine restart thump.
    float bypassMix = 0.0f; // 0 = fully processed/wet, 1 = fully dry
    float bypassSmoothCoeff = 0.0f;
    // Multiband Sensitivity Curve (0.1q): parameter pointers cached ONCE in
    // the constructor (juce::String concatenation to build "band_freq_17"
    // etc. is fine there) so processBlock() never does string work or a
    // getParameter() lookup by name -- pure atomic loads only, zero
    // allocation on the audio thread.
    std::atomic<float>* bandActiveParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* bandFreqParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* bandSensParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* bandWidthParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* bandShapeParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* bandFocusParams[SpectralEngine::kMaxBands]{};
    std::atomic<float>* lowEnabledParam=nullptr, *highEnabledParam=nullptr;
    // Legacy automation compatibility (Checkpoint A closure): the 7 old
    // freq_c*/c* IDs are wired as LIVE, bidirectional aliases of band slots
    // 0-6 via APVTS parameter listeners (message thread only -- processBlock
    // still reads ONLY the band_* atomics, never the legacy ones directly).
    // A host automation lane still bound to "c500" therefore keeps changing
    // real DSP behaviour, not just a cosmetic parameter value. syncingLegacy
    // guards against the obvious A->B->A feedback loop.
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    bool syncingLegacy = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFResonanceAudioProcessor)
};
