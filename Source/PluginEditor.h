#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFEqualizerPanel.h"
#include "License/LicenseActivationComponent.h"

// Thin host window: keeps the panel laid out at a fixed design resolution
// and scales it as a whole to fill whatever size the host gives us, so
// resizing shrinks/grows every control together instead of just the frame.
class NFEqualizerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NFEqualizerAudioProcessorEditor(
        NFEqualizerAudioProcessor&);

    ~NFEqualizerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NFEqualizerPanel panel;
    LicenseActivationComponent licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        NFEqualizerAudioProcessorEditor)
};
