#pragma once
#include <JuceHeader.h>

// ============================================================
// FASE 2 / 7 -- LookAndFeel. Chassis almost-monochrome; neon blue as
// accent. Rotary knobs use official BinaryData assets (knob_large /
// knob_small) with reference-matched cyan value arc and ticks.
// ============================================================

class NFWhiteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NFWhiteLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                            bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    static const juce::Colour kBackground;
    static const juce::Colour kPanelBackground;
    static const juce::Colour kDisplayBackground;
    static const juce::Colour kDisplayBackgroundEdge;
    static const juce::Colour kDisplayText;
    static const juce::Colour kDisplayAccent;
    static const juce::Colour kNeonGlow;
    static const juce::Colour kText;
    static const juce::Colour kTextMuted;
    static const juce::Colour kAccent;
    static const juce::Colour kKnobLight;
    static const juce::Colour kKnobDark;
    static const juce::Colour kKnobOutline;
    static const juce::Colour kKnobValueArc;
    static const juce::Colour kKnobValueArcCore; // nucleo brilhante do arco (FASE 7.2B)
    static const juce::Colour kTrackBackground;
    static const juce::Colour kBypassActive;
};
