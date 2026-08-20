#pragma once
#include <JuceHeader.h>
#include "NFLookAndFeel.h"

class NFKnob : public juce::Slider
{
public:
    NFKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f,
                           true);
        setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
        setColour(juce::Slider::textBoxTextColourId, NFColours::white);
        setColour(juce::Slider::textBoxBackgroundColourId, NFColours::black);
        setColour(juce::Slider::textBoxOutlineColourId, NFColours::purple);
    }
};
