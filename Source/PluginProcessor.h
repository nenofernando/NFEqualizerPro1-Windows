#pragma once
#include <JuceHeader.h>
#include "DSP/NFEqualizerDSP.h"
#include "PresetManager.h"
#include "License/NFLicenseManager.h"

class NFEqualizerAudioProcessor : public juce::AudioProcessor
{
public:
    NFEqualizerAudioProcessor();
    ~NFEqualizerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&,
                      juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NF Pro Eq"; }

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

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager { apvts };
    NFLicenseManager licenseManager { "NF_PRO_EQ" };

    float getInputLevelL() const { return dsp.getInputLevelL(); }
    float getInputLevelR() const { return dsp.getInputLevelR(); }
    float getOutputLevelL() const { return dsp.getOutputLevelL(); }
    float getOutputLevelR() const { return dsp.getOutputLevelR(); }

    // Pure UI preference, not an automatable parameter — persisted
    // alongside the APVTS state so it survives project/preset reload.
    std::atomic<int> skinIndex { 0 };

private:
    NF::EqualizerDSP dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        NFEqualizerAudioProcessor)
};
