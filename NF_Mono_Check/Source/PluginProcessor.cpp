#include "PluginProcessor.h"
#include "PluginEditor.h"

NFMonoCheckAudioProcessor::NFMonoCheckAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParameter = apvts.getRawParameterValue ("mode");

    for (auto& sample : scopeBufferL)
        sample.store (0.0f, std::memory_order_relaxed);

    for (auto& sample : scopeBufferR)
        sample.store (0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout NFMonoCheckAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 },
        "Monitor Mode",
        juce::StringArray { "Left", "Mono", "Right", "Stereo" },
        1));

    return { params.begin(), params.end() };
}

void NFMonoCheckAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    leftMeterSmoothed  = 0.0f;
    rightMeterSmoothed = 0.0f;

    leftLevel.store (0.0f);
    rightLevel.store (0.0f);
    correlation.store (1.0f);
}

bool NFMonoCheckAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void NFMonoCheckAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const int numSamples = buffer.getNumSamples();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    double sumLL = 0.0;
    double sumRR = 0.0;
    double sumLR = 0.0;

    float peakL = 0.0f;
    float peakR = 0.0f;

    constexpr int scopeMask = scopeBufferSize - 1;
    int scopeWriteIdx = scopeWriteIndex.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i];
        const float r = right[i];

        peakL = juce::jmax (peakL, std::abs (l));
        peakR = juce::jmax (peakR, std::abs (r));

        sumLL += static_cast<double> (l) * l;
        sumRR += static_cast<double> (r) * r;
        sumLR += static_cast<double> (l) * r;

        scopeBufferL[scopeWriteIdx].store (l, std::memory_order_relaxed);
        scopeBufferR[scopeWriteIdx].store (r, std::memory_order_relaxed);
        scopeWriteIdx = (scopeWriteIdx + 1) & scopeMask;
    }

    scopeWriteIndex.store (scopeWriteIdx, std::memory_order_release);

    const double denominator = std::sqrt (sumLL * sumRR);

    float corr = 0.0f;

    if (denominator > 1.0e-12)
        corr = static_cast<float> (sumLR / denominator);

    corr = juce::jlimit (-1.0f, 1.0f, corr);

    correlation.store (corr);

    constexpr float meterRelease = 0.92f;

    if (peakL > leftMeterSmoothed)
        leftMeterSmoothed = peakL;
    else
        leftMeterSmoothed *= meterRelease;

    if (peakR > rightMeterSmoothed)
        rightMeterSmoothed = peakR;
    else
        rightMeterSmoothed *= meterRelease;

    leftLevel.store (leftMeterSmoothed);
    rightLevel.store (rightMeterSmoothed);

    const int mode = static_cast<int> (modeParameter->load());

    if (mode == 3) // Stereo: unprocessed passthrough
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputL = left[i];
        const float inputR = right[i];

        switch (mode)
        {
            case 0: // Left: only the left channel plays, hard left -- right side is silent
                left[i]  = inputL;
                right[i] = 0.0f;
                break;

            case 2: // Right: only the right channel plays, hard right -- left side is silent
                left[i]  = 0.0f;
                right[i] = inputR;
                break;

            case 1:
            default: // Mono: true mono sum, identical on both outputs
            {
                const float mono = 0.5f * (inputL + inputR);
                left[i]  = mono;
                right[i] = mono;
                break;
            }
        }
    }
}

juce::AudioProcessorEditor* NFMonoCheckAudioProcessor::createEditor()
{
    return new NFMonoCheckAudioProcessorEditor (*this);
}

void NFMonoCheckAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    copyXmlToBinary (*xml, destData);
}

void NFMonoCheckAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

int NFMonoCheckAudioProcessor::getScopeSnapshot (float* destL, float* destR, int maxPoints) const noexcept
{
    const int numPoints = juce::jmin (maxPoints, scopeBufferSize);
    const int writeIdx = scopeWriteIndex.load (std::memory_order_acquire);

    for (int i = 0; i < numPoints; ++i)
    {
        const int idx = ((writeIdx - 1 - i) % scopeBufferSize + scopeBufferSize) % scopeBufferSize;

        destL[i] = scopeBufferL[idx].load (std::memory_order_relaxed);
        destR[i] = scopeBufferR[idx].load (std::memory_order_relaxed);
    }

    return numPoints;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NFMonoCheckAudioProcessor();
}
