#pragma once
#include <JuceHeader.h>

namespace NFColours
{
    inline const juce::Colour fluorescentGreen { 0xffAAFF00 };
    inline const juce::Colour fluorescentGreenBright { 0xffD5FF38 };
    inline const juce::Colour limeDark { 0xff5F9100 };
    inline const juce::Colour purple { 0xff8217B8 };
    inline const juce::Colour purpleBright { 0xffB52DE8 };
    inline const juce::Colour purpleShadow { 0xff24002F };
    inline const juce::Colour purpleDark { 0xff4A1268 };
    inline const juce::Colour black { 0xff070907 };
    inline const juce::Colour panelBlack { 0xff090D09 };
    inline const juce::Colour graphite { 0xff121512 };
    inline const juce::Colour white { 0xffffffff };
    inline const juce::Colour grey { 0xffAFAFAF };
}

namespace NFGraphics
{
    // Plain fitted text. Stamping fake-bold copies with pixel offsets was
    // tried here and made everything read as blurry: the whole panel is
    // drawn through an AffineTransform scale (see PluginEditor), so any
    // "1 pixel" offset lands on a fractional screen pixel once scaled and
    // just smears under anti-aliasing. Real weight comes from Font::bold
    // and picking a large-enough size, not from stamping.
    inline void drawBoldText(juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<int> area, juce::Justification justification,
                             int maxLines = 1)
    {
        g.drawFittedText(text, area, justification, maxLines);
    }

    // Genuinely thickens strokes by filling AND stroking the glyph outlines
    // as a single vector path — unlike stamping copies with a pixel offset,
    // this stays crisp under the panel's AffineTransform scale because it's
    // real vector geometry, not overlapping anti-aliased raster copies.
    inline void drawThickText(juce::Graphics& g, const juce::String& text,
                              juce::Rectangle<float> area, const juce::Font& font,
                              juce::Justification justification, float strokeWidth = 1.0f)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText(font, text, area.getX(), area.getY(),
                             area.getWidth(), area.getHeight(), justification, 1);

        juce::Path path;
        glyphs.createPath(path);
        g.fillPath(path);
        g.strokePath(path, juce::PathStrokeType(strokeWidth));
    }
}

class NFLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NFLookAndFeel();

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

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
};
