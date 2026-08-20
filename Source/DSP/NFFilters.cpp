#include "NFFilters.h"

namespace NF
{
void ThreeBandEQ::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    channels = (int) spec.numChannels;
    reset();
}

void ThreeBandEQ::reset()
{
    lowL.reset();
    lowR.reset();
    midL.reset();
    midR.reset();
    highL.reset();
    highR.reset();
}

void ThreeBandEQ::update(float lowFreq, float lowGain, bool lowShelf,
                         float midFreq, float midGain, float midQ,
                         float highFreq, float highGain, bool highShelf)
{
    lowFreq  = juce::jlimit(20.0f, 20000.0f, lowFreq);
    midFreq  = juce::jlimit(20.0f, 20000.0f, midFreq);
    highFreq = juce::jlimit(20.0f, 20000.0f, highFreq);
    midQ     = juce::jlimit(0.1f, 10.0f, midQ);

    const auto low = lowShelf
        ? juce::dsp::IIR::Coefficients<float>::makeLowShelf(
              sampleRate, lowFreq, 0.7071f,
              juce::Decibels::decibelsToGain(lowGain))
        : juce::dsp::IIR::Coefficients<float>::makePeakFilter(
              sampleRate, lowFreq, 0.7071f,
              juce::Decibels::decibelsToGain(lowGain));

    const auto mid = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, midFreq, midQ,
        juce::Decibels::decibelsToGain(midGain));

    const auto high = highShelf
        ? juce::dsp::IIR::Coefficients<float>::makeHighShelf(
              sampleRate, highFreq, 0.7071f,
              juce::Decibels::decibelsToGain(highGain))
        : juce::dsp::IIR::Coefficients<float>::makePeakFilter(
              sampleRate, highFreq, 0.7071f,
              juce::Decibels::decibelsToGain(highGain));

    *lowL.coefficients = *low;
    *lowR.coefficients = *low;
    *midL.coefficients = *mid;
    *midR.coefficients = *mid;
    *highL.coefficients = *high;
    *highR.coefficients = *high;
}

void ThreeBandEQ::process(juce::AudioBuffer<float>& buffer,
                          bool lowEnabled, bool midEnabled, bool highEnabled)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels);

    if (numChannels > 0)
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                           1, numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);

        if (lowEnabled) lowL.process(context);
        if (midEnabled) midL.process(context);
        if (highEnabled) highL.process(context);
    }

    if (numChannels > 1)
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers() + 1,
                                           1, numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);

        if (lowEnabled) lowR.process(context);
        if (midEnabled) midR.process(context);
        if (highEnabled) highR.process(context);
    }
}
}
