#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFTapeLookAndFeel.h"
#include "UI/NFVUComponent.h"
#include "UI/NFTapeReels.h"

// Fixed-canvas editor: the whole panel is laid out at designWidth x
// designHeight and the host window scales it as one block (see resized()),
// exactly like NFEqualizerPanel does in the sibling NF Pro Eq plugin.
class NFTapeMachineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit NFTapeMachineAudioProcessorEditor(NFTapeMachineAudioProcessor&);
    ~NFTapeMachineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;

private:
    void timerCallback() override;
    void showPresetMenu();
    void refreshPresetLabel();

    juce::Slider& setupKnob(juce::Slider& knob);
    juce::TextButton& setupLedPill(juce::TextButton& button, const juce::String& text);
    juce::Label& addCaption(const juce::String& text, bool bold, float relativeSize,
                            juce::Justification justification = juce::Justification::centred);

    struct ChoiceGroup
    {
        juce::String paramId;
        juce::OwnedArray<juce::TextButton> buttons;
    };

    void buildChoiceGroup(ChoiceGroup& group, const juce::StringArray& names);
    void syncChoiceGroup(ChoiceGroup& group);

    // A slim LED-ladder bar for the final output level, styled to match
    // the chassis rather than the cream VU dials above it.
    struct OutputMeterBar : public juce::Component
    {
        void setLevels(float leftDb, float rightDb);
        void paint(juce::Graphics& g) override;

    private:
        void drawColumn(juce::Graphics& g, juce::Rectangle<float> bar, float levelDb);
        static juce::Colour zoneColour(float segmentDb);

        float smoothedLeft = -60.0f, smoothedRight = -60.0f;
    };

    // Fixed-canvas root: draws the whole chassis (brushed metal face, wood
    // side panels, screws, tube lamps, tape path/rollers) as a static
    // background; every interactive control is added as its child on top.
    struct BackgroundPanel : public juce::Component
    {
        void paint(juce::Graphics& g) override;

        // Set from resized() so the plate always tracks the VU meter's
        // position instead of duplicating its layout math here.
        juce::Rectangle<float> nameplateArea;

        // 0-1, mirrors the DRIVE knob — the tube lamps glow hotter as this
        // rises. Set from the editor's timer; only repaints when changed.
        void setDriveAmount(float amount)
        {
            amount = juce::jlimit(0.0f, 1.0f, amount);
            if (! juce::approximatelyEqual(driveAmount, amount))
            {
                driveAmount = amount;
                repaint();
            }
        }

        // While bypassed (or SAT disabled) the tape isn't actually being
        // driven, so the tubes hold their vivid resting glow instead of
        // tracking the knob.
        void setBypassed(bool shouldBeBypassed)
        {
            if (bypassed != shouldBeBypassed)
            {
                bypassed = shouldBeBypassed;
                repaint();
            }
        }

        void setSatEnabled(bool shouldBeEnabled)
        {
            if (satEnabled != shouldBeEnabled)
            {
                satEnabled = shouldBeEnabled;
                repaint();
            }
        }

        float getTubeDriveAmount() const { return (bypassed || ! satEnabled) ? 0.0f : driveAmount; }

    private:
        float driveAmount = 0.0f;
        bool bypassed = false;
        bool satEnabled = true;
    };

    NFTapeMachineAudioProcessor& audioProcessor;
    NFTapeLookAndFeel lookAndFeel;

    BackgroundPanel panel;

    // Top bar
    juce::Label logoLabel, titleLabel, subtitleLabel, presetNameLabel;
    juce::TextButton prevPresetButton, nextPresetButton, menuButton, powerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    // Reels + VU + tube lamps
    NFTapeReels leftReel { false };
    NFTapeReels rightReel { true };
    NFVUComponent vuMeter;

    // Knobs
    juce::Slider inputKnob, hpfKnob, driveKnob, biasKnob, wowRateKnob, wowDepthKnob,
                 noiseKnob, eqLfKnob, eqHfKnob, outputKnob, lpfKnob, tapeAgeKnob,
                 dropoutKnob, mixKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        inputAttachment, hpfAttachment, driveAttachment, biasAttachment,
        wowRateAttachment, wowDepthAttachment, noiseAttachment, eqLfAttachment,
        eqHfAttachment, outputAttachment, lpfAttachment, tapeAgeAttachment,
        dropoutAttachment, mixAttachment;

    // LED-pill toggles
    juce::TextButton satButton, calButton, wowInButton, noiseInButton, dropoutInButton, bypassButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        satAttachment, calAttachment, wowInAttachment, noiseInAttachment,
        dropoutInAttachment, bypassAttachment;

    // Segmented choice groups
    ChoiceGroup tapeTypeGroup { "tapeType" };
    ChoiceGroup tapeSpeedGroup { "tapeSpeed" };
    ChoiceGroup reproHeadGroup { "reproHead" };

    OutputMeterBar outputMeterBar;

    juce::OwnedArray<juce::Label> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFTapeMachineAudioProcessorEditor)
};
