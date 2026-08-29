#include "NFPremiumLookAndFeel.h"
#include <BinaryData.h>

void NFPremiumLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    NFWhiteLookAndFeel::drawRotarySlider(g, x, y, width, height,
                                          sliderPosProportional, rotaryStartAngle,
                                          rotaryEndAngle, slider);
}

void NFPremiumLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                 bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Official ON/OFF shells (SYNC neon / PING PONG silver) — labels drawn in text pass.
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool down = shouldDrawButtonAsDown && isEnabled;
    const float corner = juce::jmin(bounds.getHeight() * 0.42f, bounds.getWidth() * 0.22f);

    auto drawBounds = down ? bounds.reduced(0.4f).translated(0.0f, 0.7f) : bounds;
    if (shouldDrawButtonAsHighlighted && isEnabled && ! down)
        drawBounds = drawBounds.expanded(0.4f);

    if (isEnabled)
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(drawBounds.reduced(0.5f), corner);
        juce::DropShadow(juce::Colours::black.withAlpha(down ? 0.10f : 0.18f),
                         down ? 2 : 4, { 0, down ? 0 : 1 }).drawForPath(g, shadowPath);
    }

    if (isOn && isEnabled)
    {
        const float glowBoost = shouldDrawButtonAsHighlighted ? 1.2f : 1.0f;
        for (int i = 4; i >= 1; --i)
        {
            const float expand = (float) i * 1.7f;
            g.setColour(kNeonGlow.withAlpha((0.12f * glowBoost) / (float) i));
            g.fillRoundedRectangle(drawBounds.expanded(expand), corner + expand * 0.5f);
        }
    }

    const auto* data = isOn ? BinaryData::button_on_png : BinaryData::button_off_png;
    const int dataSize = isOn ? BinaryData::button_on_pngSize : BinaryData::button_off_pngSize;
    const auto asset = juce::ImageCache::getFromMemory(data, dataSize);

    if (asset.isValid())
    {
        g.setOpacity(isEnabled ? 1.0f : 0.55f);
        g.drawImage(asset, drawBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
        return;
    }

    // Fallback
    juce::ColourGradient fill(isOn ? kAccent : kKnobLight, drawBounds.getCentreX(), drawBounds.getY(),
                               isOn ? kAccent.darker(0.2f) : kKnobDark, drawBounds.getCentreX(), drawBounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(drawBounds, corner);
}

void NFPremiumLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool shouldDrawButtonAsDown)
{
    // Labels only — shells are textless assets (no baked SYNC/PING PONG).
    const bool isOn = button.getToggleState();
    auto bounds = button.getLocalBounds();
    if (shouldDrawButtonAsDown && button.isEnabled())
        bounds = bounds.translated(0, 1);

    g.setFont(getTextButtonFont(button, button.getHeight()));
    const auto text = button.getButtonText();
    const auto textBounds = bounds.reduced(2, 0);

    if (isOn && button.isEnabled())
    {
        // Soft neon halo (glow only — single final glyph).
        g.setColour(kNeonGlow.withAlpha(0.35f));
        g.drawFittedText(text, textBounds.expanded(1), juce::Justification::centred, 2);
        g.setColour(juce::Colours::white.withAlpha(0.96f));
        g.drawFittedText(text, textBounds, juce::Justification::centred, 2);
        return;
    }

    juce::Colour textColour = button.findColour(juce::TextButton::textColourOffId);
    if (! button.isEnabled())
        textColour = textColour.withAlpha(0.45f);
    else
        textColour = kText.withAlpha(0.92f); // solid dark on silver shell
    g.setColour(textColour);
    g.drawFittedText(text, textBounds, juce::Justification::centred, 2);
}
