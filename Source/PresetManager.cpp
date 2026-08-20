#include "PresetManager.h"

namespace
{
    constexpr const char* presetFileExtension = ".nfpreset";
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    factoryPresets.push_back({
        "Default",
        {
            { "input", 0.0f }, { "output", 0.0f },
            { "lowFreq", 100.0f }, { "lowGain", 0.0f },
            { "midFreq", 1200.0f }, { "midGain", 0.0f }, { "midQ", 0.70f },
            { "highFreq", 8000.0f }, { "highGain", 0.0f },
            { "lowEnabled", 1.0f }, { "midEnabled", 1.0f }, { "highEnabled", 1.0f }, { "characterEnabled", 1.0f },
            { "drive", 0.0f }, { "character", 0.35f }, { "mix", 1.0f },
        }
    });

    factoryPresets.push_back({
        "Warm Boost",
        {
            { "input", 0.0f }, { "output", 0.0f },
            { "lowFreq", 90.0f }, { "lowGain", 3.5f },
            { "midFreq", 900.0f }, { "midGain", -1.5f }, { "midQ", 0.9f },
            { "highFreq", 9000.0f }, { "highGain", 1.5f },
            { "lowEnabled", 1.0f }, { "midEnabled", 1.0f }, { "highEnabled", 1.0f }, { "characterEnabled", 1.0f },
            { "drive", 0.20f }, { "character", 0.55f }, { "mix", 1.0f },
        }
    });

    factoryPresets.push_back({
        "Bright Air",
        {
            { "input", 0.0f }, { "output", 0.0f },
            { "lowFreq", 110.0f }, { "lowGain", -1.0f },
            { "midFreq", 1500.0f }, { "midGain", 1.0f }, { "midQ", 0.6f },
            { "highFreq", 11000.0f }, { "highGain", 4.5f },
            { "lowEnabled", 1.0f }, { "midEnabled", 1.0f }, { "highEnabled", 1.0f }, { "characterEnabled", 1.0f },
            { "drive", 0.10f }, { "character", 0.30f }, { "mix", 1.0f },
        }
    });

    factoryPresets.push_back({
        "Vocal Push",
        {
            { "input", 0.0f }, { "output", 0.0f },
            { "lowFreq", 150.0f }, { "lowGain", -2.0f },
            { "midFreq", 2500.0f }, { "midGain", 3.0f }, { "midQ", 1.1f },
            { "highFreq", 7500.0f }, { "highGain", 2.5f },
            { "lowEnabled", 1.0f }, { "midEnabled", 1.0f }, { "highEnabled", 1.0f }, { "characterEnabled", 1.0f },
            { "drive", 0.15f }, { "character", 0.45f }, { "mix", 1.0f },
        }
    });

    refreshUserPresets();
}

juce::File PresetManager::getPresetsFolder() const
{
    auto folder = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("NF Audio")
        .getChildFile("NF Equalizer")
        .getChildFile("Presets");

    folder.createDirectory();
    return folder;
}

void PresetManager::refreshUserPresets()
{
    presetNames.clear();

    for (auto& preset : factoryPresets)
        presetNames.add(preset.name);

    userPresetFiles.clear();

    auto folder = getPresetsFolder();
    for (auto& file : folder.findChildFiles(
             juce::File::findFiles, false, "*" + juce::String(presetFileExtension)))
    {
        userPresetFiles.add(file);
        presetNames.add(file.getFileNameWithoutExtension());
    }
}

void PresetManager::applyValues(const std::map<juce::String, float>& values)
{
    for (auto& [id, value] : values)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(value));
    }
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (currentIndex >= 0 && currentIndex < presetNames.size())
        return presetNames[currentIndex];

    return "Default";
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= presetNames.size())
        return;

    currentIndex = index;

    if (index < (int) factoryPresets.size())
    {
        applyValues(factoryPresets[(size_t) index].values);
        return;
    }

    const int userIndex = index - (int) factoryPresets.size();
    if (userIndex >= 0 && userIndex < userPresetFiles.size())
        loadFromFile(userPresetFiles[userIndex]);
}

void PresetManager::loadNext()
{
    if (presetNames.isEmpty())
        return;

    loadPreset((currentIndex + 1) % presetNames.size());
}

void PresetManager::loadPrevious()
{
    if (presetNames.isEmpty())
        return;

    loadPreset((currentIndex - 1 + presetNames.size()) % presetNames.size());
}

void PresetManager::loadFromFile(const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse(file))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::String PresetManager::saveCurrentAs(const juce::String& name)
{
    auto cleanName = name.trim().isEmpty() ? "Untitled" : name.trim();
    auto file = getPresetsFolder().getChildFile(
        cleanName + presetFileExtension);

    if (auto xml = apvts.copyState().createXml())
        xml->writeTo(file);

    refreshUserPresets();

    currentIndex = presetNames.indexOf(cleanName);
    return cleanName;
}
