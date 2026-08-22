#pragma once
#include <JuceHeader.h>
#include "DSP/NFTapeEngine.h"

class NFTapeMachineAudioProcessor : public juce::AudioProcessor
{
public:
    NFTapeMachineAudioProcessor();
    ~NFTapeMachineAudioProcessor() override = default;

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
    NF::TapeEngine tapeEngine;
    int currentPresetIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFTapeMachineAudioProcessor)
};
