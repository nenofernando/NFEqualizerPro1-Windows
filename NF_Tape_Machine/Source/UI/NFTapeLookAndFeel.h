#pragma once
#include <JuceHeader.h>

namespace NFTapeColours
{
    inline const juce::Colour chassisDark   { 0xff1a1c1f };
    inline const juce::Colour chassisPanel  { 0xff23262b };
    inline const juce::Colour chassisLight  { 0xff2e3238 };
    inline const juce::Colour bezelBlack    { 0xff0a0b0c };

    inline const juce::Colour woodDark      { 0xff3a2418 };
    inline const juce::Colour woodMid       { 0xff5a3a24 };
    inline const juce::Colour woodLight     { 0xff7a5232 };

    inline const juce::Colour brass         { 0xffb8925a };
    inline const juce::Colour brassBright   { 0xffe0b878 };

    inline const juce::Colour amber         { 0xffd88a2a };
    inline const juce::Colour amberBright   { 0xffffb84d };
    inline const juce::Colour amberDim      { 0xff5a3d1a };

    inline const juce::Colour creamFace     { 0xffe8dcc0 };
    inline const juce::Colour creamFaceDark { 0xffd0c2a0 };

    inline const juce::Colour ledGreen      { 0xff3fd15a };
    inline const juce::Colour ledAmber      { 0xffe8b62c };
    inline const juce::Colour ledRed        { 0xffe0303a };

    inline const juce::Colour knobBody      { 0xff34383e };
    inline const juce::Colour knobBodyLight { 0xff484d54 };
    inline const juce::Colour knobBodyDark  { 0xff17191c };

    inline const juce::Colour white         { 0xfff2ede0 };
    inline const juce::Colour textDim       { 0xffb8b2a4 };
}

// Every control property this LookAndFeel reacts to via
// Component::getProperties() (set from the editor when building controls).
namespace NFTapeProps
{
    inline const juce::Identifier segmented   { "nfSegmented" };  // TAPE TYPE / SPEED / REPRO HEAD buttons
    inline const juce::Identifier ledPill     { "nfLedPill" };    // small "IN"/"SAT"/"CAL" toggle
    inline const juce::Identifier powerSquare { "nfPowerSquare" };// big BYPASS square
    inline const juce::Identifier powerCircle { "nfPowerCircle" };// small round power button (top bar)
    inline const juce::Identifier tickLabels  { "nfTickLabels" }; // pipe-separated printed scale, e.g. "-24|-12|0|+12|+24"
    inline const juce::Identifier hamburger   { "nfHamburger" };  // preset menu button (drawn icon, no text)
    inline const juce::Identifier goldTitle   { "nfGoldTitle" };  // warm glowing gold-gradient title text
    inline const juce::Identifier customFont  { "nfCustomFont" }; // respect Label::setFont() instead of auto height-sizing
}

class NFTapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NFTapeLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Font getLabelFont(juce::Label&) override;

    // Stamps a printed scale ("-24|-12|0|+12|+24") around a knob's arc,
    // read back by drawRotarySlider — matches the silkscreened numbers on
    // real hardware faceplates instead of plain unlabelled tick marks.
    static void setTickLabels(juce::Slider& slider, const juce::StringArray& labels);

private:
    void drawSegmentedButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down);
    void drawLedPillButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down);
    void drawPowerSquareButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down);
    void drawPowerCircleButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down);
};
