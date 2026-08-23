#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFStressorLookAndFeel.h"
#include "UI/GRLadderMeter.h"

// Fixed-canvas editor: the whole panel is laid out at designWidth x
// designHeight and the host window scales it as one block, matching the
// approach used by the sibling NF Audio Tools plugins.
class NFStressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NFStressorAudioProcessorEditor(NFStressorAudioProcessor&);
    ~NFStressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 260;
    static constexpr int designHeight = 820;

private:
    void timerCallback() override;
    void layOutContent();

    juce::Slider& setupKnob(juce::Slider& knob);
    juce::TextButton& setupSegmentButton(juce::TextButton& button, const juce::String& text);
    juce::TextButton& setupToggleButton(juce::TextButton& button, const juce::String& text);
    void setupCaption(juce::Label& label, const juce::String& text, float pointSize, bool bold = false,
                      juce::Justification justification = juce::Justification::centred);

    struct BackgroundPanel : public juce::Component
    {
        void paint(juce::Graphics& g) override;
        void setInsetPanels(std::vector<juce::Rectangle<int>> panels) { insetPanels = std::move(panels); repaint(); }

    private:
        std::vector<juce::Rectangle<int>> insetPanels;
    };

    NFStressorAudioProcessor& audioProcessor;
    NFStressorLookAndFeel lookAndFeel;

    // Everything is added to `content` and laid out once at designWidth x
    // designHeight; resized() only rescales this single component, so
    // dragging the corner resizer scales the whole panel as one block
    // instead of reflowing it.
    juce::Component content;

    BackgroundPanel panel;

    // Top plate
    juce::Label titleLabel, taglineLabel;
    juce::TextButton powerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    // Main knobs
    juce::Slider inputKnob, attackKnob, releaseKnob, outputKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        inputAttachment, attackAttachment, releaseAttachment, outputAttachment;
    juce::Label inputCaption, attackCaption, releaseCaption, outputCaption;

    GRLadderMeter grMeter;

    // Ratio segmented row
    juce::Label ratioCaption;
    juce::OwnedArray<juce::TextButton> ratioButtons;
    std::unique_ptr<juce::ParameterAttachment> ratioAttachment;
    int currentRatioIndex = 3;

    // Character toggles (DETECTOR: hp/link, AUDIO: dist2/dist3)
    juce::Label detectorCaption, audioCaption;
    juce::TextButton hpButton, linkButton, dist2Button, dist3Button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hpAttachment, linkAttachment, dist2Attachment, dist3Attachment;

    // Mix
    juce::Slider mixKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    juce::Label mixCaption;

    // Bottom nameplate
    juce::Label footerLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFStressorAudioProcessorEditor)
};
