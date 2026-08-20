#pragma once
#include <JuceHeader.h>

namespace NF
{
class Saturation
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void setDrive(float drive);
    void setCharacter(float character);

    // factor: 0 = 1x (off), 1 = 2x, 2 = 4x, 3 = 8x
    void setOversamplingFactor(int factor);

    void process(juce::AudioBuffer<float>& buffer);

private:
    void rebuildOversampling();

    float drive = 0.0f;
    float character = 0.0f;

    int oversamplingFactor = 2;
    int numChannels = 2;
    int blockSize = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};
}
