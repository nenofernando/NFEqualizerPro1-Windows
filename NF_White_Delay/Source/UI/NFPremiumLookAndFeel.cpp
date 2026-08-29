#include "NFPremiumLookAndFeel.h"
#include <BinaryData.h>

void NFPremiumLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    // Single canonical knob renderer lives in NFWhiteLookAndFeel
    // (official BinaryData assets + reference arc/ticks/shadow).
    NFWhiteLookAndFeel::drawRotarySlider(g, x, y, width, height,
                                          sliderPosProportional, rotaryStartAngle,
                                          rotaryEndAngle, slider);
}

void NFPremiumLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                 bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool down = shouldDrawButtonAsDown && isEnabled;

    {
        const auto* data = isOn ? BinaryData::button_on_png : BinaryData::button_off_png;
        const int dataSize = isOn ? BinaryData::button_on_pngSize : BinaryData::button_off_pngSize;
        const auto asset = juce::ImageCache::getFromMemory(data, dataSize);
        if (asset.isValid())
        {
            auto drawBounds = down ? bounds.reduced(0.4f).translated(0.0f, 0.8f) : bounds;
            if (shouldDrawButtonAsHighlighted && isEnabled && ! down)
                drawBounds = drawBounds.expanded(0.5f);
            g.setOpacity(isEnabled ? 1.0f : 0.55f);
            g.drawImage(asset, drawBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
            g.setOpacity(1.0f);
            return;
        }
    }

    // Procedural fallback (identical behaviour to previous premium path).
    const float corner = juce::jmin(7.0f, bounds.getHeight() * 0.32f);
    const juce::Colour onColour = button.findColour(juce::TextButton::buttonOnColourId);
    juce::Colour fill = isOn ? onColour : button.findColour(juce::TextButton::buttonColourId);
    if (! isEnabled)
        fill = fill.withMultipliedSaturation(0.3f).withAlpha(0.55f);

    auto drawBounds = down ? bounds.reduced(0.4f).translated(0.0f, 0.8f) : bounds;

    if (isEnabled)
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(drawBounds, corner);
        juce::DropShadow outerShadow(juce::Colours::black.withAlpha(down ? 0.08f : 0.18f),
                                      down ? 2 : 4, juce::Point<int>(0, down ? 0 : 1));
        outerShadow.drawForPath(g, shadowPath);
    }

    if (isOn && isEnabled)
    {
        const float glowBoost = shouldDrawButtonAsHighlighted ? 1.25f : 1.0f;
        for (int i = 3; i >= 1; --i)
        {
            const float expand = (float) i * 2.4f;
            g.setColour(onColour.withAlpha((0.11f * glowBoost) / (float) i));
            g.fillRoundedRectangle(drawBounds.expanded(expand), corner + expand);
        }
    }

    g.setColour(kKnobOutline.darker(isOn ? 0.10f : 0.25f));
    g.fillRoundedRectangle(drawBounds, corner);

    auto innerBounds = drawBounds.reduced(1.1f);
    const float innerCorner = juce::jmax(0.0f, corner - 1.1f);

    juce::Colour topC = isOn ? onColour.brighter(down ? 0.05f : 0.22f) : fill.brighter(down ? 0.10f : 0.40f);
    juce::Colour botC = isOn ? onColour.darker(down ? 0.05f : 0.02f) : fill.darker(down ? 0.02f : 0.03f);
    if (! isEnabled) { topC = fill; botC = fill.darker(0.1f); }
    else if (shouldDrawButtonAsHighlighted && ! down) { topC = topC.brighter(isOn ? 0.06f : 0.04f); }

    juce::ColourGradient fillGradient(topC, innerBounds.getCentreX(), innerBounds.getY(),
                                       botC, innerBounds.getCentreX(), innerBounds.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillRoundedRectangle(innerBounds, innerCorner);

    if (isEnabled)
    {
        auto topArea = innerBounds.reduced(1.0f).withHeight(innerBounds.getHeight() * 0.42f);
        juce::Path topHighlight;
        topHighlight.addRoundedRectangle(topArea.getX(), topArea.getY(), topArea.getWidth(), topArea.getHeight(),
                                          innerCorner, innerCorner, true, true, false, false);
        g.setColour(juce::Colours::white.withAlpha(down ? (isOn ? 0.08f : 0.20f) : (isOn ? 0.20f : 0.55f)));
        g.strokePath(topHighlight, juce::PathStrokeType(1.0f));
    }

    {
        auto bottomArea = innerBounds.reduced(1.0f).withHeight(innerBounds.getHeight() * 0.35f)
                                      .withY(innerBounds.getBottom() - innerBounds.getHeight() * 0.35f - 1.0f);
        juce::ColourGradient insetShadow(juce::Colours::black.withAlpha(0.0f), bottomArea.getCentreX(), bottomArea.getY(),
                                          juce::Colours::black.withAlpha(isOn ? 0.12f : 0.07f), bottomArea.getCentreX(), bottomArea.getBottom(), false);
        g.setGradientFill(insetShadow);
        g.fillRoundedRectangle(bottomArea, juce::jmax(0.0f, innerCorner - 1.0f));
    }

    g.setColour(isOn ? onColour.darker(0.4f) : kKnobOutline.withAlpha(isEnabled ? 1.0f : 0.5f));
    g.drawRoundedRectangle(drawBounds, corner, 1.0f);
}

void NFPremiumLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool shouldDrawButtonAsDown)
{
    const bool isOn = button.getToggleState();
    juce::Colour textColour = isOn ? button.findColour(juce::TextButton::textColourOnId)
                                    : button.findColour(juce::TextButton::textColourOffId);
    if (! button.isEnabled())
        textColour = textColour.withAlpha(0.45f);

    auto bounds = button.getLocalBounds();
    if (shouldDrawButtonAsDown && button.isEnabled())
        bounds = bounds.translated(0, 1);

    g.setColour(textColour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), bounds.reduced(2, 0), juce::Justification::centred, 2);
}
