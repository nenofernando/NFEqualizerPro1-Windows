#pragma once
#include <JuceHeader.h>
#include "NFFilters.h"
#include "NFSaturation.h"

namespace NF
{
class EqualizerDSP
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(float inputDb,
                       float lowFreq, float lowGain, bool lowShelf,
                       float midFreq, float midGain, float midQ,
                       float highFreq, float highGain, bool highShelf,
                       float drive, float character,
                       float mix, float outputDb,
                       int oversamplingFactor,
                       bool lowEnabled, bool midEnabled,
                       bool highEnabled, bool characterEnabled);

    void process(juce::AudioBuffer<float>& buffer);
    void processBypassed(juce::AudioBuffer<float>& buffer);

    float getInputLevelL() const { return inputLevelL.load(); }
    float getInputLevelR() const { return inputLevelR.load(); }
    float getOutputLevelL() const { return outputLevelL.load(); }
    float getOutputLevelR() const { return outputLevelR.load(); }

private:
    static void measureLevels(const juce::AudioBuffer<float>& buffer,
                              std::atomic<float>& left, std::atomic<float>& right);

    ThreeBandEQ eq;
    Saturation saturation;

    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float mix = 1.0f;

    bool lowEnabled = true;
    bool midEnabled = true;
    bool highEnabled = true;
    bool characterEnabled = true;

    std::atomic<float> inputLevelL { 0.0f };
    std::atomic<float> inputLevelR { 0.0f };
    std::atomic<float> outputLevelL { 0.0f };
    std::atomic<float> outputLevelR { 0.0f };

    juce::AudioBuffer<float> dryBuffer;
};
}
