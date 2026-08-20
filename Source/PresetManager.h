#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);

    const juce::StringArray& getPresetNames() const { return presetNames; }
    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentPresetName() const;

    void loadPreset(int index);
    void loadNext();
    void loadPrevious();

    // Returns the new preset's display name.
    juce::String saveCurrentAs(const juce::String& name);
    void loadFromFile(const juce::File& file);

    juce::File getPresetsFolder() const;

private:
    struct FactoryPreset
    {
        juce::String name;
        std::map<juce::String, float> values;
    };

    void applyValues(const std::map<juce::String, float>& values);
    void refreshUserPresets();
    juce::File getFileForIndex(int index) const;

    juce::AudioProcessorValueTreeState& apvts;

    std::vector<FactoryPreset> factoryPresets;
    juce::Array<juce::File> userPresetFiles;
    juce::StringArray presetNames;

    int currentIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
