#pragma once
#include <JuceHeader.h>
#include "NFWhiteLookAndFeel.h"

// ============================================================
// FASE 2 -- premium LookAndFeel for official BinaryData knobs.
// Uses knob_large / knob_small (pointer baked at 12 o'clock) plus
// reference-matched cyan value arc, ticks and soft ground shadow.
// SYNC still uses button_on / button_off via drawButtonBackground.
// ============================================================

class NFPremiumLookAndFeel : public NFWhiteLookAndFeel
{
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
