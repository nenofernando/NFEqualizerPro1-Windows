#pragma once
#include <JuceHeader.h>

namespace NF
{
class ThreeBandEQ
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void update(float lowFreq, float lowGain, bool lowShelf,
                float midFreq, float midGain, float midQ,
                float highFreq, float highGain, bool highShelf);

    void process(juce::AudioBuffer<float>& buffer,
                bool lowEnabled, bool midEnabled, bool highEnabled);

private:
    juce::dsp::IIR::Filter<float> lowL, lowR;
    juce::dsp::IIR::Filter<float> midL, midR;
    juce::dsp::IIR::Filter<float> highL, highR;

    double sampleRate = 44100.0;
    int channels = 2;
};
}
