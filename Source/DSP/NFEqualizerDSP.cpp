#include "NFEqualizerDSP.h"

namespace NF
{
void EqualizerDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    eq.prepare(spec);
    saturation.prepare(spec.sampleRate,
                        (int) spec.maximumBlockSize,
                        (int) spec.numChannels);

    dryBuffer.setSize((int) spec.numChannels,
                      (int) spec.maximumBlockSize);
}

void EqualizerDSP::reset()
{
    eq.reset();
    saturation.reset();
}

void EqualizerDSP::setParameters(float inputDb,
                                  float lowFreq, float lowGain, bool lowShelf,
                                  float midFreq, float midGain, float midQ,
                                  float highFreq, float highGain, bool highShelf,
                                  float drive, float character,
                                  float newMix, float outputDb,
                                  int oversamplingFactor,
                                  bool newLowEnabled, bool newMidEnabled,
                                  bool newHighEnabled, bool newCharacterEnabled)
{
    inputGain = juce::Decibels::decibelsToGain(inputDb);
    outputGain = juce::Decibels::decibelsToGain(outputDb);
    mix = juce::jlimit(0.0f, 1.0f, newMix);

    lowEnabled = newLowEnabled;
    midEnabled = newMidEnabled;
    highEnabled = newHighEnabled;
    characterEnabled = newCharacterEnabled;

    eq.update(lowFreq, lowGain, lowShelf,
              midFreq, midGain, midQ,
              highFreq, highGain, highShelf);

    saturation.setDrive(drive);
    saturation.setCharacter(character);
    saturation.setOversamplingFactor(oversamplingFactor);
}

void EqualizerDSP::measureLevels(const juce::AudioBuffer<float>& buffer,
                                 std::atomic<float>& left, std::atomic<float>& right)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    left.store(numChannels > 0 ? buffer.getMagnitude(0, 0, numSamples) : 0.0f);
    right.store(numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples)
                                : left.load());
}

void EqualizerDSP::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    measureLevels(buffer, inputLevelL, inputLevelR);

    if (dryBuffer.getNumChannels() != numChannels ||
        dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize(numChannels, numSamples, false, false, true);
    }

    dryBuffer.makeCopyOf(buffer, true);

    buffer.applyGain(inputGain);

    eq.process(buffer, lowEnabled, midEnabled, highEnabled);

    if (characterEnabled)
        saturation.process(buffer);

    buffer.applyGain(outputGain);

    if (mix < 0.9999f)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            buffer.applyGain(channel, 0, numSamples, mix);
            buffer.addFrom(channel, 0, dryBuffer, channel, 0,
                           numSamples, 1.0f - mix);
        }
    }

    measureLevels(buffer, outputLevelL, outputLevelR);
}

void EqualizerDSP::processBypassed(juce::AudioBuffer<float>& buffer)
{
    measureLevels(buffer, inputLevelL, inputLevelR);
    outputLevelL.store(inputLevelL.load());
    outputLevelR.store(inputLevelR.load());
}
}
