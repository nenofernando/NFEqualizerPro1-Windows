#pragma once
#include <JuceHeader.h>
#include "DSP/StressorEngine.h"

class NFStressorAudioProcessor : public juce::AudioProcessor
{
public:
    NFStressorAudioProcessor();
    ~NFStressorAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NF - Stressor"; }

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    float getGainReductionDb() const { return stressorEngine.getGainReductionDb(); }

private:
    void updateEngineParameters();

    NF::StressorEngine stressorEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFStressorAudioProcessor)
};
