#pragma once
#include <JuceHeader.h>

// Real preset system, independent of the host's own generic preset selector.
// Factory presets are baked into the binary (not files -- nothing to
// accidentally delete or corrupt); user presets are plain XML files (the
// same binary APVTS state produced by AudioProcessor::getStateInformation)
// saved under the user's own persistent folder, so they survive across
// plugin runs/reinstalls of the same version.
class PresetManager
{
public:
    struct FactoryPreset { juce::String name; juce::String description; std::function<void(juce::AudioProcessorValueTreeState&)> apply; };

    explicit PresetManager(juce::AudioProcessorValueTreeState& s) : state(s) { userFolder().createDirectory(); }

    static juce::File presetsRootFolder()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("NF Audio Tools").getChildFile("NF Resonance").getChildFile("Presets");
    }
    static juce::File userFolder() { return presetsRootFolder().getChildFile("User"); }

    // Resets EVERY parameter to its own compiled-in default value -- generic
    // and safe for any parameter that exists now or is added later, so
    // "Default" always matches the true factory state exactly, with no
    // hand-maintained list to fall out of sync.
    void resetToDefault()
    {
        for (auto* param : state.processor.getParameters())
            param->setValueNotifyingHost(param->getDefaultValue());
    }

    const std::vector<FactoryPreset>& factoryPresets() const { return factory; }

    void applyFactoryPreset(int index)
    {
        if (index < 0 || index >= (int) factory.size()) return;
        resetToDefault();
        factory[(size_t) index].apply(state);
    }

    bool saveUserPreset(const juce::String& name)
    {
        if (name.isEmpty()) return false;
        auto file = userFileFor(name);
        juce::MemoryBlock block;
        if (auto xml = state.copyState().createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, block);
        return file.replaceWithData(block.getData(), block.getSize());
    }

    bool loadUserPreset(const juce::String& name)
    {
        auto file = userFileFor(name);
        if (! file.existsAsFile()) return false;
        juce::MemoryBlock block;
        if (! file.loadFileAsData(block)) return false;
        if (auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), (int) block.getSize()))
        {
            state.replaceState(juce::ValueTree::fromXml(*xml));
            return true;
        }
        return false;
    }

    bool deleteUserPreset(const juce::String& name) { return userFileFor(name).deleteFile(); }

    juce::StringArray listUserPresets() const
    {
        juce::StringArray names;
        for (const auto& f : juce::RangedDirectoryIterator(userFolder(), false, "*.nfrpreset"))
            names.add(f.getFile().getFileNameWithoutExtension());
        names.sort(true);
        return names;
    }

private:
    juce::AudioProcessorValueTreeState& state;
    static juce::File userFileFor(const juce::String& name) { return userFolder().getChildFile(juce::File::createLegalFileName(name) + ".nfrpreset"); }

    // Safe, conservative starting points ONLY -- no aggressive settings
    // until Detector V2 exists (V1's low-frequency aggressiveness is a
    // known, separately-tracked issue, not something these presets should
    // paper over). Each preset just nudges Depth/Sharpness/Selectivity/
    // Attack/Release/LOW/HIGH from the factory default -- no bands
    // pre-created, so the curve still opens clean for the user to shape.
    std::vector<FactoryPreset> factory
    {
        { "Default", "Factory default: neutral, LOW=100Hz/HIGH=16kHz, no bands.", [](juce::AudioProcessorValueTreeState&) {} },
        { "Vocal - Gentle", "Light, conservative smoothing for vocals.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(2.5f));
            s.getParameter("selectivity")->setValueNotifyingHost(s.getParameter("selectivity")->convertTo0to1(5.0f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(90.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(14000.0f)); } },
        { "Vocal - De-Harsh", "Mild focus toward sibilance/harshness range, still conservative.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(3.0f));
            s.getParameter("sharpness")->setValueNotifyingHost(s.getParameter("sharpness")->convertTo0to1(5.5f));
            s.getParameter("selectivity")->setValueNotifyingHost(s.getParameter("selectivity")->convertTo0to1(4.5f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(150.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(16000.0f)); } },
        { "Vocal - De-Nasal", "Mild focus toward the nasal/honk range, still conservative.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(3.0f));
            s.getParameter("sharpness")->setValueNotifyingHost(s.getParameter("sharpness")->convertTo0to1(3.0f));
            s.getParameter("selectivity")->setValueNotifyingHost(s.getParameter("selectivity")->convertTo0to1(4.5f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(200.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(4000.0f)); } },
        { "Guitar", "Conservative full-range setting for guitar.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(3.0f));
            s.getParameter("attack")->setValueNotifyingHost(s.getParameter("attack")->convertTo0to1(8.0f));
            s.getParameter("release")->setValueNotifyingHost(s.getParameter("release")->convertTo0to1(60.0f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(80.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(12000.0f)); } },
        { "Drums", "Faster response, conservative depth, wide range.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(2.5f));
            s.getParameter("attack")->setValueNotifyingHost(s.getParameter("attack")->convertTo0to1(3.0f));
            s.getParameter("release")->setValueNotifyingHost(s.getParameter("release")->convertTo0to1(40.0f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(50.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(18000.0f)); } },
        { "Mix Bus", "Very gentle, wide, slow -- a light safety net across the mix.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(1.5f));
            s.getParameter("selectivity")->setValueNotifyingHost(s.getParameter("selectivity")->convertTo0to1(6.0f));
            s.getParameter("attack")->setValueNotifyingHost(s.getParameter("attack")->convertTo0to1(20.0f));
            s.getParameter("release")->setValueNotifyingHost(s.getParameter("release")->convertTo0to1(150.0f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(60.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(18000.0f)); } },
        { "Mastering - Gentle", "The gentlest setting -- minimal depth, high selectivity, slow.", [](juce::AudioProcessorValueTreeState& s) {
            s.getParameter("depth")->setValueNotifyingHost(s.getParameter("depth")->convertTo0to1(1.0f));
            s.getParameter("selectivity")->setValueNotifyingHost(s.getParameter("selectivity")->convertTo0to1(7.0f));
            s.getParameter("attack")->setValueNotifyingHost(s.getParameter("attack")->convertTo0to1(30.0f));
            s.getParameter("release")->setValueNotifyingHost(s.getParameter("release")->convertTo0to1(200.0f));
            s.getParameter("lowHz")->setValueNotifyingHost(s.getParameter("lowHz")->convertTo0to1(40.0f));
            s.getParameter("highHz")->setValueNotifyingHost(s.getParameter("highHz")->convertTo0to1(19000.0f)); } },
    };
};
