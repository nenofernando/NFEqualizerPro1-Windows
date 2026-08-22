#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct FactoryPreset
    {
        juce::String name;
        std::map<juce::String, float> values;
    };

    const std::vector<FactoryPreset>& getFactoryPresets()
    {
        static const std::vector<FactoryPreset> presets =
        {
            {
                "Default",
                {
                    { "input", 0.0f }, { "hpf", 20.0f }, { "tapeType", 0.0f },
                    { "drive", 4.5f }, { "satEnabled", 1.0f },
                    { "bias", 5.0f }, { "biasCal", 1.0f },
                    { "wowRate", 0.6f }, { "wowDepth", 8.0f }, { "wowFlutterEnabled", 0.0f },
                    { "noise", 3.0f }, { "noiseEnabled", 0.0f },
                    { "eqLf", 0.0f }, { "eqHf", 0.0f },
                    { "output", 0.0f }, { "lpf", 20000.0f },
                    { "tapeSpeed", 1.0f }, { "reproHead", 0.0f },
                    { "tapeAge", 12.0f },
                    { "dropout", 2.0f }, { "dropoutEnabled", 0.0f },
                    { "mix", 100.0f }, { "bypass", 0.0f }, { "gainLink", 0.0f }
                }
            },
            {
                "Warm Bus Glue",
                {
                    { "input", 0.0f }, { "hpf", 25.0f }, { "tapeType", 1.0f },
                    { "drive", 5.5f }, { "satEnabled", 1.0f },
                    { "bias", 6.0f }, { "biasCal", 1.0f },
                    { "wowRate", 0.5f }, { "wowDepth", 10.0f }, { "wowFlutterEnabled", 1.0f },
                    { "noise", 3.0f }, { "noiseEnabled", 0.0f },
                    { "eqLf", 1.5f }, { "eqHf", -0.5f },
                    { "output", 0.0f }, { "lpf", 18000.0f },
                    { "tapeSpeed", 1.0f }, { "reproHead", 0.0f },
                    { "tapeAge", 15.0f },
                    { "dropout", 2.0f }, { "dropoutEnabled", 0.0f },
                    { "mix", 100.0f }, { "bypass", 0.0f }, { "gainLink", 0.0f }
                }
            },
            {
                "Vintage Slap",
                {
                    { "input", 0.0f }, { "hpf", 30.0f }, { "tapeType", 3.0f },
                    { "drive", 6.5f }, { "satEnabled", 1.0f },
                    { "bias", 6.5f }, { "biasCal", 0.0f },
                    { "wowRate", 0.8f }, { "wowDepth", 18.0f }, { "wowFlutterEnabled", 1.0f },
                    { "noise", 4.0f }, { "noiseEnabled", 1.0f },
                    { "eqLf", 0.5f }, { "eqHf", -1.0f },
                    { "output", 0.0f }, { "lpf", 14000.0f },
                    { "tapeSpeed", 0.0f }, { "reproHead", 0.0f },
                    { "tapeAge", 45.0f },
                    { "dropout", 3.0f }, { "dropoutEnabled", 1.0f },
                    { "mix", 100.0f }, { "bypass", 0.0f }, { "gainLink", 0.0f }
                }
            },
            {
                "Mastering Sheen",
                {
                    { "input", 0.0f }, { "hpf", 20.0f }, { "tapeType", 2.0f },
                    { "drive", 3.0f }, { "satEnabled", 1.0f },
                    { "bias", 5.0f }, { "biasCal", 1.0f },
                    { "wowRate", 0.4f }, { "wowDepth", 3.0f }, { "wowFlutterEnabled", 1.0f },
                    { "noise", 1.0f }, { "noiseEnabled", 0.0f },
                    { "eqLf", 0.0f }, { "eqHf", 0.8f },
                    { "output", 0.0f }, { "lpf", 20000.0f },
                    { "tapeSpeed", 2.0f }, { "reproHead", 1.0f },
                    { "tapeAge", 5.0f },
                    { "dropout", 0.0f }, { "dropoutEnabled", 0.0f },
                    { "mix", 100.0f }, { "bypass", 0.0f }, { "gainLink", 0.0f }
                }
            },
            {
                "Lo-Fi Worn",
                {
                    { "input", 0.0f }, { "hpf", 60.0f }, { "tapeType", 3.0f },
                    { "drive", 7.0f }, { "satEnabled", 1.0f },
                    { "bias", 7.5f }, { "biasCal", 0.0f },
                    { "wowRate", 1.2f }, { "wowDepth", 30.0f }, { "wowFlutterEnabled", 1.0f },
                    { "noise", 7.0f }, { "noiseEnabled", 1.0f },
                    { "eqLf", -1.0f }, { "eqHf", -3.0f },
                    { "output", 0.0f }, { "lpf", 9000.0f },
                    { "tapeSpeed", 0.0f }, { "reproHead", 0.0f },
                    { "tapeAge", 80.0f },
                    { "dropout", 6.0f }, { "dropoutEnabled", 1.0f },
                    { "mix", 100.0f }, { "bypass", 0.0f }, { "gainLink", 0.0f }
                }
            }
        };

        return presets;
    }
}

NFTapeMachineAudioProcessor::NFTapeMachineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    apvts.addParameterListener("input", this);
    apvts.addParameterListener("output", this);
    apvts.addParameterListener("gainLink", this);
}

NFTapeMachineAudioProcessor::~NFTapeMachineAudioProcessor()
{
    apvts.removeParameterListener("input", this);
    apvts.removeParameterListener("output", this);
    apvts.removeParameterListener("gainLink", this);
}

// While Gain Link is on, INPUT drives OUTPUT inversely so the total gain
// stays constant (dB for dB) — turning input up attenuates output by the
// same amount. The reference sum is recaptured each time the link is
// switched on, so it always starts from the knobs' current positions
// rather than some stale value from the last time it was engaged.
void NFTapeMachineAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "gainLink")
    {
        if (newValue > 0.5f)
            gainLinkSumDb = apvts.getRawParameterValue("input")->load()
                          + apvts.getRawParameterValue("output")->load();
        return;
    }

    if (apvts.getRawParameterValue("gainLink")->load() <= 0.5f)
        return;

    if (parameterID == "input")
    {
        const float targetOutputDb = juce::jlimit(-24.0f, 24.0f, gainLinkSumDb - newValue);
        if (auto* outputParam = apvts.getParameter("output"))
            outputParam->setValueNotifyingHost(outputParam->convertTo0to1(targetOutputDb));
    }
    else if (parameterID == "output")
    {
        // A direct nudge of OUTPUT while linked rebases the coupling point
        // instead of being fought on the next INPUT move.
        gainLinkSumDb = apvts.getRawParameterValue("input")->load() + newValue;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
NFTapeMachineAudioProcessor::createParameterLayout()
{
    using FloatParameter = juce::AudioParameterFloat;
    using BoolParameter = juce::AudioParameterBool;
    using ChoiceParameter = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<FloatParameter>("input", "Input", -24.0f, 24.0f, 0.0f));
    parameters.push_back(std::make_unique<FloatParameter>("hpf", "HPF",
        juce::NormalisableRange<float>(20.0f, 200.0f, 0.1f, 0.4f), 20.0f));

    parameters.push_back(std::make_unique<ChoiceParameter>("tapeType", "Tape Type",
        juce::StringArray { "GP9", "456", "499", "250" }, 0));

    parameters.push_back(std::make_unique<FloatParameter>("drive", "Drive", 0.0f, 10.0f, 4.5f));
    parameters.push_back(std::make_unique<BoolParameter>("satEnabled", "Saturation", true));

    parameters.push_back(std::make_unique<FloatParameter>("bias", "Bias", 0.0f, 10.0f, 5.0f));
    parameters.push_back(std::make_unique<BoolParameter>("biasCal", "Bias Calibrated", true));

    parameters.push_back(std::make_unique<FloatParameter>("wowRate", "Wow/Flutter Rate",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.3f), 0.6f));
    parameters.push_back(std::make_unique<FloatParameter>("wowDepth", "Wow/Flutter Depth", 0.0f, 100.0f, 8.0f));
    parameters.push_back(std::make_unique<BoolParameter>("wowFlutterEnabled", "Wow/Flutter Enabled", false));

    parameters.push_back(std::make_unique<FloatParameter>("noise", "Noise", 0.0f, 10.0f, 3.0f));
    parameters.push_back(std::make_unique<BoolParameter>("noiseEnabled", "Noise Enabled", false));

    parameters.push_back(std::make_unique<FloatParameter>("eqLf", "EQ Low", -12.0f, 12.0f, 0.0f));
    parameters.push_back(std::make_unique<FloatParameter>("eqHf", "EQ High", -12.0f, 12.0f, 0.0f));

    parameters.push_back(std::make_unique<FloatParameter>("output", "Output", -24.0f, 24.0f, 0.0f));
    parameters.push_back(std::make_unique<FloatParameter>("lpf", "LPF",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.4f), 20000.0f));

    parameters.push_back(std::make_unique<ChoiceParameter>("tapeSpeed", "Tape Speed",
        juce::StringArray { "7.5 IPS", "15 IPS", "30 IPS" }, 1));
    parameters.push_back(std::make_unique<ChoiceParameter>("reproHead", "Repro Head",
        juce::StringArray { "NAB", "IEC" }, 0));

    parameters.push_back(std::make_unique<FloatParameter>("tapeAge", "Tape Age", 0.0f, 100.0f, 12.0f));

    parameters.push_back(std::make_unique<FloatParameter>("dropout", "Dropouts", 0.0f, 10.0f, 2.0f));
    parameters.push_back(std::make_unique<BoolParameter>("dropoutEnabled", "Dropouts Enabled", false));

    parameters.push_back(std::make_unique<FloatParameter>("mix", "Mix", 0.0f, 100.0f, 100.0f));

    parameters.push_back(std::make_unique<BoolParameter>("bypass", "Bypass", false));

    parameters.push_back(std::make_unique<BoolParameter>("gainLink", "Gain Link", false));

    return { parameters.begin(), parameters.end() };
}

int NFTapeMachineAudioProcessor::getNumFactoryPresets()
{
    return (int) getFactoryPresets().size();
}

juce::String NFTapeMachineAudioProcessor::getFactoryPresetName(int index)
{
    const auto& presets = getFactoryPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;
    return {};
}

const juce::String NFTapeMachineAudioProcessor::getProgramName(int index)
{
    return getFactoryPresetName(index);
}

void NFTapeMachineAudioProcessor::loadPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    currentPresetIndex = index;

    for (const auto& [paramId, value] : presets[(size_t) index].values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
}

void NFTapeMachineAudioProcessor::loadNextPreset()
{
    loadPreset((currentPresetIndex + 1) % getNumFactoryPresets());
}

void NFTapeMachineAudioProcessor::loadPreviousPreset()
{
    loadPreset((currentPresetIndex - 1 + getNumFactoryPresets()) % getNumFactoryPresets());
}

void NFTapeMachineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax(1, getTotalNumOutputChannels())
    };

    tapeEngine.prepare(spec);
}

void NFTapeMachineAudioProcessor::releaseResources()
{
    tapeEngine.reset();
}

bool NFTapeMachineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void NFTapeMachineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalInput = getTotalNumInputChannels();
    const int totalOutput = getTotalNumOutputChannels();

    for (int channel = totalInput; channel < totalOutput; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    if (get("bypass") > 0.5f)
        return;

    NF::TapeParameters params;
    params.inputDb = get("input");
    params.hpfHz = get("hpf");
    params.tapeType = (NF::TapeType) (int) get("tapeType");
    params.drive = get("drive");
    params.satEnabled = get("satEnabled") > 0.5f;
    params.bias = get("bias");
    params.biasCalEnabled = get("biasCal") > 0.5f;
    params.wowRateHz = get("wowRate");
    params.wowDepthPct = get("wowDepth");
    params.wowFlutterEnabled = get("wowFlutterEnabled") > 0.5f;
    params.noiseAmount = get("noise");
    params.noiseEnabled = get("noiseEnabled") > 0.5f;
    params.eqLfDb = get("eqLf");
    params.eqHfDb = get("eqHf");
    params.outputDb = get("output");
    params.lpfHz = get("lpf");
    params.tapeSpeed = (NF::TapeSpeed) (int) get("tapeSpeed");
    params.reproHead = (NF::ReproHead) (int) get("reproHead");
    params.tapeAgePct = get("tapeAge");
    params.dropoutAmount = get("dropout");
    params.dropoutsEnabled = get("dropoutEnabled") > 0.5f;
    params.mixPct = get("mix");

    tapeEngine.setParameters(params);
    tapeEngine.process(buffer);
}

void NFTapeMachineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute("nfPreset", currentPresetIndex);
        copyXmlToBinary(*xml, destData);
    }
}

void NFTapeMachineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            currentPresetIndex = xml->getIntAttribute("nfPreset", 0);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }
    }
}

juce::AudioProcessorEditor* NFTapeMachineAudioProcessor::createEditor()
{
    return new NFTapeMachineAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NFTapeMachineAudioProcessor();
}
