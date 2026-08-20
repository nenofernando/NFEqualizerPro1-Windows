#include "PluginProcessor.h"
#include "PluginEditor.h"

NFEqualizerAudioProcessor::NFEqualizerAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
NFEqualizerAudioProcessor::createParameterLayout()
{
    using FloatParameter = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    auto frequencyRange =
        [](float minimum, float maximum)
        {
            return juce::NormalisableRange<float>(
                minimum, maximum, 0.01f, 0.35f);
        };

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "input", "Input", -24.0f, 24.0f, 0.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "lowFreq", "Low Frequency",
            frequencyRange(30.0f, 500.0f), 100.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "lowGain", "Low Gain", -15.0f, 15.0f, 0.0f));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "lowShelf", "Low Shelf", true));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "midFreq", "Mid Frequency",
            frequencyRange(200.0f, 8000.0f), 1200.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "midGain", "Mid Gain", -15.0f, 15.0f, 0.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "midQ", "Mid Q", 0.25f, 4.0f, 0.70f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "highFreq", "High Frequency",
            frequencyRange(2000.0f, 20000.0f), 8000.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "highGain", "High Gain", -15.0f, 15.0f, 0.0f));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "highShelf", "High Shelf", true));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "drive", "Drive", 0.0f, 1.0f, 0.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "character", "Character", 0.0f, 1.0f, 0.35f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "mix", "Mix", 0.0f, 1.0f, 1.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            "output", "Output", -24.0f, 24.0f, 0.0f));

    parameters.push_back(
        std::make_unique<juce::AudioParameterChoice>(
            "oversampling", "Oversampling",
            juce::StringArray { "1x", "2x", "4x", "8x" }, 2));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "bypass", "Bypass", false));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "lowEnabled", "Low Enabled", true));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "midEnabled", "Mid Enabled", true));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "highEnabled", "High Enabled", true));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            "characterEnabled", "NF Character Enabled", true));

    return { parameters.begin(), parameters.end() };
}

void NFEqualizerAudioProcessor::prepareToPlay(
    double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax(1, getTotalNumOutputChannels())
    };

    dsp.prepare(spec);
}

void NFEqualizerAudioProcessor::releaseResources()
{
    dsp.reset();
}

bool NFEqualizerAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() &&
        output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void NFEqualizerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalInput = getTotalNumInputChannels();
    const int totalOutput = getTotalNumOutputChannels();

    for (int channel = totalInput; channel < totalOutput; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto get = [this](const char* id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    if (get("bypass") > 0.5f)
    {
        dsp.processBypassed(buffer);
        return;
    }

    dsp.setParameters(
        get("input"),
        get("lowFreq"), get("lowGain"), get("lowShelf") > 0.5f,
        get("midFreq"), get("midGain"), get("midQ"),
        get("highFreq"), get("highGain"), get("highShelf") > 0.5f,
        get("drive"), get("character"),
        get("mix"), get("output"),
        (int) get("oversampling"),
        get("lowEnabled") > 0.5f, get("midEnabled") > 0.5f,
        get("highEnabled") > 0.5f, get("characterEnabled") > 0.5f);

    dsp.process(buffer);
}

void NFEqualizerAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void NFEqualizerAudioProcessor::setStateInformation(
    const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(
                juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor*
NFEqualizerAudioProcessor::createEditor()
{
    return new NFEqualizerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NFEqualizerAudioProcessor();
}
