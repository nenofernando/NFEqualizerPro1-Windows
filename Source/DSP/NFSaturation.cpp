#include "NFSaturation.h"

namespace NF
{
void Saturation::prepare(double sampleRate, int maximumBlockSize, int channels)
{
    juce::ignoreUnused(sampleRate);

    numChannels = juce::jmax(1, channels);
    blockSize = juce::jmax(1, maximumBlockSize);

    rebuildOversampling();
}

void Saturation::rebuildOversampling()
{
    if (oversamplingFactor <= 0)
    {
        oversampling.reset();
        return;
    }

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) numChannels,
        (size_t) oversamplingFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);

    oversampling->initProcessing((size_t) blockSize);
    oversampling->reset();
}

void Saturation::setOversamplingFactor(int factor)
{
    factor = juce::jlimit(0, 3, factor);

    if (factor == oversamplingFactor)
        return;

    oversamplingFactor = factor;
    rebuildOversampling();
}

void Saturation::reset()
{
    if (oversampling)
        oversampling->reset();
}

void Saturation::setDrive(float value)
{
    drive = juce::jlimit(0.0f, 1.0f, value);
}

void Saturation::setCharacter(float value)
{
    character = juce::jlimit(0.0f, 1.0f, value);
}

void Saturation::process(juce::AudioBuffer<float>& buffer)
{
    if (drive <= 0.0001f && character <= 0.0001f)
        return;

    const float amount = 1.0f + drive * 7.0f;
    const float colour = 0.10f + character * 0.40f;
    const float normaliser = juce::jmax(0.001f, std::tanh(amount));

    auto saturate = [amount, colour, normaliser](float x)
    {
        const float soft = std::tanh(x * amount);
        const float coloured =
            std::tanh(x * amount * 0.65f) +
            0.08f * x * x * x;

        return ((1.0f - colour) * soft + colour * coloured) / normaliser;
    };

    juce::dsp::AudioBlock<float> block(buffer);

    if (!oversampling)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
                block.setSample(ch, sample, saturate(block.getSample(ch, sample)));

        return;
    }

    auto upsampled = oversampling->processSamplesUp(block);

    for (size_t ch = 0; ch < upsampled.getNumChannels(); ++ch)
        for (size_t sample = 0; sample < upsampled.getNumSamples(); ++sample)
            upsampled.setSample(ch, sample, saturate(upsampled.getSample(ch, sample)));

    oversampling->processSamplesDown(block);
}
}
