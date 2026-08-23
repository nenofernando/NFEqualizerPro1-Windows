#pragma once
#include <JuceHeader.h>

namespace NFStressorColours
{
    // Brushed-metal panel
    inline const juce::Colour panelTop { 0xff2e2e30 };
    inline const juce::Colour panelBottom { 0xff141416 };
    inline const juce::Colour panelDark { 0xff232324 };   // kept for call sites expecting a mid tone
    inline const juce::Colour panelDarker { 0xff141416 };
    inline const juce::Colour screwBody { 0xff3a3a3c };
    inline const juce::Colour screwShadow { 0xff0a0a0b };

    // Knobs — silver knurled dial with a printed 0-10 scale, matching the
    // hardware reference photo (metal grip ring, black separator, dark hub).
    inline const juce::Colour bezelOuter { 0xff3a3a3d };
    inline const juce::Colour bezelInner { 0xff0a0a0b };
    inline const juce::Colour knurlLight { 0xffdedee2 };
    inline const juce::Colour knurlDark { 0xff6b6b70 };
    inline const juce::Colour knobWhiteLight { 0xffffffff };
    inline const juce::Colour knobWhiteDark { 0xffd8d8da };
    inline const juce::Colour knobPointer { 0xffc9c9cd };
    inline const juce::Colour knobHighlight { 0xffffffff };
    inline const juce::Colour pointer { 0xff232323 };
    inline const juce::Colour pointerTip { 0xffb3271e };
    inline const juce::Colour tick { 0xff0a0a0b }; // marks/numbers on the dial are black

    // LEDs / character glow
    inline const juce::Colour amber { 0xffffab3d };
    inline const juce::Colour amberDim { 0xff3a2a18 };
    inline const juce::Colour red { 0xffff4a36 };
    inline const juce::Colour redDim { 0xff33170f };
    inline const juce::Colour green { 0xff5fe06a };
    inline const juce::Colour greenDim { 0xff1c3320 };
    inline const juce::Colour ledOff { 0xff232326 };

    // Text
    inline const juce::Colour textLight { 0xfff2efe6 };
    inline const juce::Colour textDim { 0xff86868c };
}

class NFStressorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NFStressorLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;
};
