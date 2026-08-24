#pragma once
#include <JuceHeader.h>

namespace NFStressorColours
{
    // Brushed-metal panel
    inline const juce::Colour panelTop { 0xff727278 };
    inline const juce::Colour panelBottom { 0xff2c2c30 };
    inline const juce::Colour panelDark { 0xff232324 };   // kept for call sites expecting a mid tone
    inline const juce::Colour panelDarker { 0xff141416 };
    inline const juce::Colour screwBody { 0xff3a3a3c };
    inline const juce::Colour screwShadow { 0xff0a0a0b };

    // Knobs — fixed scale printed on a light silver/grey disc, small light-grey
    // centre, thin dark separator ring, small black rotating pointer. The
    // numbers/ticks do NOT rotate; only the pointer does.
    inline const juce::Colour bezelOuter { 0xff3a3a3d };
    inline const juce::Colour bezelInner { 0xff0a0a0b };
    inline const juce::Colour knobDiscLight { 0xffefeff1 };
    inline const juce::Colour knobDiscDark { 0xffc9c9cd };
    inline const juce::Colour knobCentreLight { 0xffd9d9dc };
    inline const juce::Colour knobCentreDark { 0xffb9b9bd };
    inline const juce::Colour pointer { 0xff1a1a1b };
    inline const juce::Colour pointerTip { 0xffb3271e };
    inline const juce::Colour tick { 0xff1a1a1b }; // marks/numbers on the dial are black

    // LEDs / character glow
    inline const juce::Colour glowWhite { 0xfff2f0ea }; // faint knob underglow — near-white, barely warm
    inline const juce::Colour amber { 0xffffab3d };
    inline const juce::Colour amberDim { 0xff3a2a18 };
    inline const juce::Colour red { 0xffff4a36 };
    inline const juce::Colour redDim { 0xff33170f };
    inline const juce::Colour green { 0xff5fe06a };
    inline const juce::Colour greenDim { 0xff1c3320 };
    inline const juce::Colour ledOff { 0xff232326 };
    inline const juce::Colour nukeBlue { 0xff2f8fff }; // NUKE button's own lit colour, distinct from the red LEDs
    inline const juce::Colour optoGreen { 0xffccff00 }; // OPTO light's own lit colour — neon lime, distinct from the amber RATIO/GR-meter LEDs

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
