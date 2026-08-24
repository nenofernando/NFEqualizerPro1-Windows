#include "PluginProcessor.h"
#include "PluginEditor.h"

NFStressorAudioProcessor::NFStressorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
NFStressorAudioProcessor::createParameterLayout()
{
    using FloatParameter = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(
        std::make_unique<FloatParameter>(
            juce::ParameterID("input", 1), "Input", -20.0f, 20.0f, 0.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            juce::ParameterID("attack", 2), "Attack", 0.0f, 10.0f, 5.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            juce::ParameterID("release", 3), "Release", 0.0f, 10.0f, 5.0f));

    parameters.push_back(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("ratio", 4), "Ratio",
            juce::StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1" }, 3));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("hp", 5), "Detector HP", false));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("link", 6), "Stereo Link", true));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("dist2", 7), "Dist 2", false));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("dist3", 8), "Dist 3", false));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            juce::ParameterID("mix", 11), "Mix", 0.0f, 100.0f, 100.0f));

    parameters.push_back(
        std::make_unique<FloatParameter>(
            juce::ParameterID("output", 12), "Output", -20.0f, 20.0f, 0.0f));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("bypass", 13), "Bypass", false));

    parameters.push_back(
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("nuke", 14), "Nuke", false));

    parameters.push_back(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("outHp", 15), "Output HP",
            juce::StringArray { "Off", "70 Hz", "120 Hz" }, 0));

    return { parameters.begin(), parameters.end() };
}

void NFStressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax(1, getTotalNumOutputChannels())
    };

    stressorEngine.prepare(spec);
}

void NFStressorAudioProcessor::releaseResources()
{
    stressorEngine.reset();
}

bool NFStressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void NFStressorAudioProcessor::updateEngineParameters()
{
    auto get = [this](const char* id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    NF::StressorParameters params;
    params.inputDb = get("input");
    params.attackAmount = get("attack");
    params.releaseAmount = get("release");
    params.ratioIndex = (int) get("ratio");
    params.hpEnabled = get("hp") > 0.5f;
    params.linkEnabled = get("link") > 0.5f;
    params.dist2Enabled = get("dist2") > 0.5f;
    params.dist3Enabled = get("dist3") > 0.5f;
    params.mixPct = get("mix");
    params.outputDb = get("output");
    params.bypass = get("bypass") > 0.5f;
    params.nukeMode = get("nuke") > 0.5f;
    params.outHpMode = (int) get("outHp");

    stressorEngine.setParameters(params);
}

void NFStressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalInput = getTotalNumInputChannels();
    const int totalOutput = getTotalNumOutputChannels();

    for (int channel = totalInput; channel < totalOutput; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    updateEngineParameters();
    stressorEngine.process(buffer);
}

void NFStressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void NFStressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* NFStressorAudioProcessor::createEditor()
{
    return new NFStressorAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NFStressorAudioProcessor();
}
