#pragma once
#include <JuceHeader.h>
#include "DSP/NFTapeEngine.h"

class NFTapeMachineAudioProcessor : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    NFTapeMachineAudioProcessor();
    ~NFTapeMachineAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NF Tape Machine"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override { loadPreset(index); }
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static int getNumFactoryPresets();
    static juce::String getFactoryPresetName(int index);
    void loadPreset(int index);
    void loadNextPreset();
    void loadPreviousPreset();
    int getCurrentPresetIndex() const { return currentPresetIndex; }

    juce::AudioProcessorValueTreeState apvts;

    float getOutputLevelL() const { return tapeEngine.getOutputLevelL(); }
    float getOutputLevelR() const { return tapeEngine.getOutputLevelR(); }

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Host automation can deliver parameter changes on the audio thread;
    // setValueNotifyingHost() calls back into the host (e.g. VST3's
    // performEdit) and must only ever happen on the message thread, so the
    // Gain Link's cross-parameter write is deferred here instead of being
    // made directly from parameterChanged().
    void handleAsyncUpdate() override;

    NF::TapeEngine tapeEngine;
    int currentPresetIndex = 0;
    std::atomic<float> gainLinkSumDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFTapeMachineAudioProcessor)
};
