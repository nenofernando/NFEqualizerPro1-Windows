#pragma once

#include <JuceHeader.h>
#include <atomic>

class NFMonoCheckAudioProcessor : public juce::AudioProcessor
{
public:
    NFMonoCheckAudioProcessor();
    ~NFMonoCheckAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    enum class MonitorMode
    {
        Left = 0,
        Mono = 1,
        Right = 2,
        Stereo = 3
    };

    float getLeftLevel() const noexcept       { return leftLevel.load(); }
    float getRightLevel() const noexcept      { return rightLevel.load(); }
    float getCorrelation() const noexcept     { return correlation.load(); }

    // Lock-free snapshot of the most recent original stereo samples (pre L/M/R
    // conversion), for the editor to draw a real goniometer. Copies up to
    // maxPoints samples, most recent first, into destL/destR. Safe to call
    // from the message thread; never blocks or allocates.
    int getScopeSnapshot (float* destL, float* destR, int maxPoints) const noexcept;

private:

    static constexpr int scopeBufferSize = 4096; // power of two

    std::atomic<float> leftLevel   { 0.0f };
    std::atomic<float> rightLevel  { 0.0f };
    std::atomic<float> correlation { 1.0f };

    std::atomic<float> scopeBufferL[scopeBufferSize];
    std::atomic<float> scopeBufferR[scopeBufferSize];
    std::atomic<int> scopeWriteIndex { 0 };

    double currentSampleRate = 44100.0;

    float leftMeterSmoothed  = 0.0f;
    float rightMeterSmoothed = 0.0f;

    std::atomic<float>* modeParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NFMonoCheckAudioProcessor)
};
