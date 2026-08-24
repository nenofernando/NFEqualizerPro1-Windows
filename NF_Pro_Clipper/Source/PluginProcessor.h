#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class NFProClipperAudioProcessor : public juce::AudioProcessor
{
public:
    NFProClipperAudioProcessor();
    ~NFProClipperAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&,
                       juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
        return "NF Pro Clipper";
    }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override
    {
        return 0.0;
    }

    int getNumPrograms() override
    {
        return 1;
    }

    int getCurrentProgram() override
    {
        return 0;
    }

    void setCurrentProgram (int) override {}

    const juce::String getProgramName (int) override
    {
        return "Default";
    }

    void changeProgramName (int,
                            const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;

    void setStateInformation (const void*,
                              int) override;

    // ========================================================
    // APVTS
    // ========================================================

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

    // ========================================================
    // METERS
    // ========================================================

    float getInputPeakDb (int channel) const noexcept;
    float getOutputPeakDb (int channel) const noexcept;

    float getReductionDb() const noexcept
    {
        return reductionDb.load();
    }

    float getClipAmount() const noexcept
    {
        return clipAmount.load();
    }

    // ========================================================
    // TRANSFER CURVE (used by the editor so the graph always
    // matches exactly what processBlock is doing, including
    // the knee)
    // ========================================================

    float computeTransferCurveSample (float x) const noexcept;

    // ========================================================
    // PRESETS
    // ========================================================

    static juce::File getPresetsDirectory();

    // ========================================================
    // SKIN (preferência de UI, não é um parâmetro de áudio -
    // guardada como propriedade solta no ValueTree do APVTS,
    // que já é salva/restaurada inteira via
    // getStateInformation/setStateInformation)
    // ========================================================

    int getSkinIndex() const noexcept
    {
        return (int) apvts.state.getProperty ("skinIndex", 0);
    }

    void setSkinIndex (int index)
    {
        apvts.state.setProperty ("skinIndex", index, nullptr);
    }

private:

    enum ClipMode
    {
        SoftClip = 0,
        MediumClip,
        HardClip
    };

    // ========================================================
    // OVERSAMPLING
    //
    // 0 = 1x
    // 1 = 2x
    // 2 = 4x
    // 3 = 8x
    // 4 = 16x
    // ========================================================

    std::array<
        std::unique_ptr<juce::dsp::Oversampling<float>>,
        5> oversamplers;

    int activeOversamplingIndex = 2;
    int previousOversamplingIndexForCrossfade = 2;
    int crossfadeSamplesRemaining = 0;
    int crossfadeSamplesTotal = 0;

    // buffers pré-alocados (sem alocação dentro de processBlock)
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> monitorInputBuffer;
    juce::AudioBuffer<float> preClipBuffer;
    juce::AudioBuffer<float> clipDeltaBuffer;
    juce::AudioBuffer<float> crossfadeBuffer;

    std::vector<float> ceilingPerSample;
    std::vector<float> kneePerSample;
    std::vector<float> tonePerSample;

    // ========================================================
    // SMOOTHING
    // ========================================================

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Multiplicative>
        inputGainSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Multiplicative>
        driveGainSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Multiplicative>
        outputGainSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Multiplicative>
        ceilingSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear>
        mixSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear>
        toneSmooth;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear>
        kneeSmooth;

    // ========================================================
    // METERS THREAD SAFE
    // ========================================================

    std::array<std::atomic<float>, 2>
        inputPeakDb { -100.0f, -100.0f };

    std::array<std::atomic<float>, 2>
        outputPeakDb { -100.0f, -100.0f };

    std::atomic<float> reductionDb { 0.0f };
    std::atomic<float> clipAmount { 0.0f };

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;

    // ========================================================
    // DSP
    // ========================================================

    float processClipperSample (
        float sample,
        float ceiling,
        float kneeDb,
        int mode) const noexcept;

    float processTone (
        float sample,
        float toneValue) const noexcept;

    void renderThroughClipper (
        juce::AudioBuffer<float>& ioBuffer,
        int oversamplingIndex,
        const float* ceilingPerBaseSample,
        const float* kneePerBaseSample,
        const float* toneValuePerBaseSample,
        int clipMode,
        int numBaseSamples,
        float& maxBeforeOut,
        float& maxAfterOut,
        float& clippingAmountOut) noexcept;

    static float linearToDb (
        float linear) noexcept;

    static float calculatePeak (
        const juce::AudioBuffer<float>&,
        int channel) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        NFProClipperAudioProcessor)
};
