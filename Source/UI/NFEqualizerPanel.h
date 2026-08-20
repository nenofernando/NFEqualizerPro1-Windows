#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "NFLookAndFeel.h"
#include "NFKnob.h"
#include "NFLevelMeter.h"
#include "NFSectionEnableButton.h"
#include "NFDesignMetrics.h"
#include "NFTheme.h"

// Fixed-resolution content panel, laid out at DesignMetrics::width x height.
// The editor scales this as a whole via AffineTransform so the entire UI
// resizes together instead of just the window frame.
class NFEqualizerPanel : public juce::Component,
                         private juce::Timer
{
public:
    static constexpr int designWidth = (int) DesignMetrics::width;
    static constexpr int designHeight = (int) DesignMetrics::height;

    explicit NFEqualizerPanel(NFEqualizerAudioProcessor&);
    ~NFEqualizerPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    NFEqualizerAudioProcessor& processor;
    NFLookAndFeel nfLookAndFeel;

    NFKnob input;
    NFKnob lowFreq, lowGain;
    NFKnob midFreq, midGain, midQ;
    NFKnob highFreq, highGain;
    NFKnob drive, character, mix;
    NFKnob output;

    juce::ToggleButton lowShelf { "SHELF" };
    juce::ToggleButton highShelf { "SHELF" };
    juce::ToggleButton bypass { "BYPASS" };

    NFSectionEnableButton lowEnableButton, midEnableButton,
                          highEnableButton, characterEnableButton;

    NFLevelMeter inputMeter, outputMeter;

    juce::TextButton prevPreset { "<" };
    juce::TextButton nextPreset { ">" };
    juce::Label presetName;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton loadButton { "LOAD" };

    juce::Label oversamplingCaption;
    juce::ComboBox oversamplingBox;

    juce::Label skinCaption;
    juce::TextButton skinButton1 { "1" }, skinButton2 { "2" };
    NFTheme currentTheme = NFTheme::classicGreen();

    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<ButtonAttachment> lowShelfAttachment;
    std::unique_ptr<ButtonAttachment> highShelfAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> lowEnableAttachment;
    std::unique_ptr<ButtonAttachment> midEnableAttachment;
    std::unique_ptr<ButtonAttachment> highEnableAttachment;
    std::unique_ptr<ButtonAttachment> characterEnableAttachment;
    std::unique_ptr<ComboBoxAttachment> oversamplingAttachment;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void configureKnob(NFKnob& knob,
                       const juce::String& parameterID,
                       const juce::String& labelText,
                       float cx, float cy, float diameter, float textBoxH = 18.0f,
                       float gapBeforeTextBox = 0.0f);

    void refreshPresetLabel();

    void showSaveDialog();
    void showLoadDialog();

    void applyTheme(const NFTheme& theme);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFEqualizerPanel)
};
