#pragma once
#include <JuceHeader.h>

// The section LEDs (LOW/MID/HIGH/NF CHARACTER) doubling as real
// enable/bypass toggles for their section, attached to an APVTS bool param.
// Lit colour is skin-dependent (set via setAccentColours), since it sits
// directly on the main face and needs to contrast against whichever colour
// that face currently is.
class NFSectionEnableButton : public juce::ToggleButton
{
public:
    NFSectionEnableButton()
    {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
    }

    void setAccentColours(juce::Colour top, juce::Colour bottom, juce::Colour ring)
    {
        litTop = top;
        litBottom = bottom;
        litRing = ring;
        repaint();
    }

    void paintButton(juce::Graphics& g,
                     bool isMouseOverButton,
                     bool isButtonDown) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.5f);

        const bool enabled = getToggleState();

        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.fillEllipse(area.translated(1.5f, 2.0f));

        g.setColour(juce::Colour::fromRGB(5, 5, 5));
        g.fillEllipse(area);

        auto inner = area.reduced(2.5f);

        if (enabled)
        {
            juce::ColourGradient gradient(
                litTop, inner.getCentreX(), inner.getY(),
                litBottom, inner.getCentreX(), inner.getBottom(),
                false);

            g.setGradientFill(gradient);
            g.fillEllipse(inner);

            g.setColour(litRing.withAlpha(isMouseOverButton ? 0.45f : 0.28f));
            g.drawEllipse(area.expanded(1.0f), isMouseOverButton ? 2.0f : 1.2f);

            g.setColour(juce::Colours::white.withAlpha(0.45f));
            auto highlight = juce::Rectangle<float>(
                inner.getX() + inner.getWidth() * 0.25f,
                inner.getY() + inner.getHeight() * 0.16f,
                inner.getWidth() * 0.25f,
                inner.getHeight() * 0.20f);
            g.fillEllipse(highlight);
        }
        else
        {
            juce::ColourGradient gradient(
                juce::Colour::fromRGB(55, 35, 60),
                inner.getCentreX(), inner.getY(),
                juce::Colour::fromRGB(20, 15, 22),
                inner.getCentreX(), inner.getBottom(),
                false);

            g.setGradientFill(gradient);
            g.fillEllipse(inner);

            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.drawEllipse(inner, 1.0f);
        }

        if (isButtonDown)
        {
            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.fillEllipse(inner);
        }
    }

private:
    juce::Colour litTop { 215, 70, 255 };
    juce::Colour litBottom { 92, 0, 135 };
    juce::Colour litRing { 180, 35, 235 };
};
