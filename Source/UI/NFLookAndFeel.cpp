#include "NFLookAndFeel.h"

namespace
{
    void drawBypassLed(juce::Graphics& g, juce::Rectangle<float> area, bool active)
    {
        auto led = area.reduced(1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillEllipse(led.translated(1.0f, 1.5f));

        g.setColour(juce::Colours::black);
        g.fillEllipse(led);

        auto inner = led.reduced(2.0f);

        if (active)
        {
            juce::ColourGradient grad(
                juce::Colour::fromRGB(215, 80, 255),
                inner.getCentreX(),
                inner.getY(),

                juce::Colour::fromRGB(85, 0, 120),
                inner.getCentreX(),
                inner.getBottom(),
                false);

            g.setGradientFill(grad);
            g.fillEllipse(inner);

            g.setColour(
                juce::Colours::white.withAlpha(0.45f));

            g.fillEllipse(
                inner.getX() + inner.getWidth() * 0.24f,
                inner.getY() + inner.getHeight() * 0.18f,
                inner.getWidth() * 0.26f,
                inner.getHeight() * 0.20f);
        }
        else
        {
            g.setColour(
                juce::Colour::fromRGB(45, 25, 50));

            g.fillEllipse(inner);
        }
    }

    void drawKnobTicks(juce::Graphics& g, juce::Point<float> centre, float radius,
                       float rotaryStartAngle, float rotaryEndAngle, bool lightTicks,
                       juce::Colour onFaceColour)
    {
        constexpr int numTicks = 37;
        const float tickInner = radius + 8.0f;
        const float tickOuter = radius + 13.0f;

        const auto tickColour = lightTicks ? juce::Colours::white : onFaceColour;

        for (int i = 0; i < numTicks; ++i)
        {
            const float proportion = (float) i / (float) (numTicks - 1);
            const float angle =
                rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);

            const bool major = (i % 6 == 0);
            const float inner = major ? tickInner - 2.0f : tickInner;
            const float outer = major ? tickOuter + 1.5f : tickOuter;

            const float x1 = centre.x + std::cos(angle) * inner;
            const float y1 = centre.y + std::sin(angle) * inner;
            const float x2 = centre.x + std::cos(angle) * outer;
            const float y2 = centre.y + std::sin(angle) * outer;

            // Same solid colour for every tick — a uniform ring all the way
            // from the first mark to the last, only major ticks read a
            // little longer/thicker, no partly-transparent minors.
            g.setColour(tickColour);
            g.drawLine(x1, y1, x2, y2, major ? 1.8f : 1.2f);
        }
    }
}

NFLookAndFeel::NFLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, NFColours::white);
    setColour(juce::Slider::textBoxTextColourId, NFColours::white);
    setColour(juce::Slider::textBoxBackgroundColourId, NFColours::black);
    setColour(juce::Slider::textBoxOutlineColourId, NFColours::purple);
    setColour(juce::Label::textColourId, NFColours::black);
}

void NFLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                     int x, int y, int width, int height,
                                     float sliderPosProportional,
                                     float rotaryStartAngle,
                                     float rotaryEndAngle,
                                     juce::Slider& slider)
{
    const bool lightTicks = slider.getProperties()["lightTicks"];

    auto bounds = juce::Rectangle<float>((float) x, (float) y,
                                         (float) width, (float) height)
                      .reduced(9.0f);

    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    // Soft ambient shadow, layered to fake a blur since JUCE has no
    // native gaussian blur for vector fills.
    for (int i = 4; i >= 1; --i)
    {
        const float grow = (float) i * 1.6f;
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillEllipse(centre.x - radius - grow, centre.y - radius - grow + 2.5f,
                      (radius + grow) * 2.0f, (radius + grow) * 2.0f);
    }

    // Outer black bezel
    g.setColour(NFColours::black);
    g.fillEllipse(centre.x - radius - 5.5f, centre.y - radius - 5.5f,
                  (radius + 5.5f) * 2.0f, (radius + 5.5f) * 2.0f);

    // Machined metal lip: a thin gradient ring between the black bezel
    // and the graphite ring, lighter at the top where light would catch it.
    {
        juce::ColourGradient lip(
            juce::Colour(0xff5A5A5A), centre.x, centre.y - radius - 3.0f,
            juce::Colour(0xff141414), centre.x, centre.y + radius + 3.0f,
            false);
        g.setGradientFill(lip);
        juce::Path lipPath;
        lipPath.addEllipse(centre.x - radius - 3.5f, centre.y - radius - 3.5f,
                           (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f);
        lipPath.setUsingNonZeroWinding(false);
        lipPath.addEllipse(centre.x - radius - 1.0f, centre.y - radius - 1.0f,
                           (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);
        g.fillPath(lipPath);
    }

    // Second graphite ring
    g.setColour(NFColours::graphite);
    g.fillEllipse(centre.x - radius - 1.0f, centre.y - radius - 1.0f,
                  (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);

    // Purple gradient body — off-centre highlight to suggest a dome,
    // darker fresnel-style rim at the very edge.
    juce::ColourGradient bodyGradient(
        NFColours::purpleBright, centre.x - radius * 0.42f, centre.y - radius * 0.55f,
        NFColours::purpleShadow, centre.x + radius * 0.65f, centre.y + radius * 0.85f,
        true);
    bodyGradient.addColour(0.4, NFColours::purple);
    bodyGradient.addColour(0.82, NFColours::purple.darker(0.15f));
    g.setGradientFill(bodyGradient);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Fresnel rim — a slim darker stroke right at the body edge
    g.setColour(NFColours::purpleShadow.withAlpha(0.55f));
    g.drawEllipse(centre.x - radius + 0.8f, centre.y - radius + 0.8f,
                  (radius - 0.8f) * 2.0f, (radius - 0.8f) * 2.0f, 1.2f);

    // Bottom-right rim light — faint bounce light opposite the highlight
    {
        juce::Path rimLight;
        rimLight.addCentredArc(centre.x, centre.y, radius - 1.0f, radius - 1.0f,
                               0.0f, juce::MathConstants<float>::pi * 0.15f,
                               juce::MathConstants<float>::pi * 0.85f, true);
        g.setColour(NFColours::purpleBright.withAlpha(0.30f));
        g.strokePath(rimLight, juce::PathStrokeType(1.4f));
    }

    // Layered glossy specular highlight (soft blur fake)
    for (int i = 3; i >= 1; --i)
    {
        const float scale = (float) i / 3.0f;
        g.setColour(juce::Colours::white.withAlpha(0.10f * (4 - i)));
        g.fillEllipse(centre.x - radius * 0.55f * scale - radius * 0.05f,
                      centre.y - radius * 0.78f * scale,
                      radius * 0.95f * scale,
                      radius * 0.5f * scale);
    }

    // Black inner contour
    g.setColour(NFColours::black);
    g.drawEllipse(centre.x - radius, centre.y - radius,
                  radius * 2.0f, radius * 2.0f, 2.0f);

    drawKnobTicks(g, centre, radius, rotaryStartAngle, rotaryEndAngle, lightTicks, theme.onFaceText);

    // Position indicator — bevelled: dark drop shadow, bright top face
    const float angle =
        rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const float pointerStart = radius * 0.16f;
    const float pointerEnd = radius * 0.78f;

    const float px1 = centre.x + std::cos(angle) * pointerStart;
    const float py1 = centre.y + std::sin(angle) * pointerStart;
    const float px2 = centre.x + std::cos(angle) * pointerEnd;
    const float py2 = centre.y + std::sin(angle) * pointerEnd;

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawLine(px1, py1 + 1.4f, px2, py2 + 1.4f, 4.2f);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawLine(px1, py1, px2, py2, 4.4f);

    juce::ColourGradient pointerGradient(
        juce::Colours::white, px1, py1,
        juce::Colour(0xffD8D8E8), px2, py2, false);
    g.setGradientFill(pointerGradient);
    g.drawLine(px1, py1, px2, py2, 3.0f);

    // Centre cap — small metallic dome
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(centre.x - 4.0f, centre.y - 3.5f, 8.0f, 8.0f);

    juce::ColourGradient capGradient(
        juce::Colour(0xffF0F0F0), centre.x - 1.5f, centre.y - 2.0f,
        juce::Colour(0xff8A8A8A), centre.x + 1.5f, centre.y + 2.5f, true);
    g.setGradientFill(capGradient);
    g.fillEllipse(centre.x - 3.5f, centre.y - 3.5f, 7.0f, 7.0f);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawEllipse(centre.x - 3.5f, centre.y - 3.5f, 7.0f, 7.0f, 0.8f);
}

void NFLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                          juce::Button& button,
                                          const juce::Colour& backgroundColour,
                                          bool highlighted,
                                          bool down)
{
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool outlined = backgroundColour == NFColours::black;
    constexpr float corner = 5.0f;

    // Drop shadow, sinks in when pressed
    if (!down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(outer.translated(0.0f, 1.6f), corner);
    }

    // Dark bezel
    g.setColour(NFColours::black);
    g.fillRoundedRectangle(outer, corner);

    auto face = outer.reduced(1.6f);

    auto base = down ? backgroundColour.darker(0.15f)
                      : (highlighted ? backgroundColour.brighter(0.10f) : backgroundColour);

    // Raised face: lighter at the top, darker at the bottom: a real bevel
    // rather than a flat fill. Pressed state flips the gradient so the
    // button visually sinks instead of bulging.
    juce::ColourGradient faceGradient = down
        ? juce::ColourGradient(base.darker(0.25f), face.getX(), face.getY(),
                               base.brighter(0.08f), face.getX(), face.getBottom(), false)
        : juce::ColourGradient(base.brighter(0.28f), face.getX(), face.getY(),
                               base.darker(0.32f), face.getX(), face.getBottom(), false);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(face, corner - 1.0f);

    if (!down)
    {
        // Top bevel highlight
        juce::Path topEdge;
        topEdge.addRoundedRectangle(face, corner - 1.0f);
        g.saveState();
        g.reduceClipRegion(topEdge);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawLine(face.getX() + 2.0f, face.getY() + 0.8f, face.getRight() - 2.0f, face.getY() + 0.8f, 1.2f);
        g.restoreState();
    }

    // Bottom bevel shadow
    juce::Path bottomEdge;
    bottomEdge.addRoundedRectangle(face, corner - 1.0f);
    g.saveState();
    g.reduceClipRegion(bottomEdge);
    g.setColour(juce::Colours::black.withAlpha(down ? 0.5f : 0.30f));
    g.drawLine(face.getX() + 2.0f, face.getBottom() - 0.8f, face.getRight() - 2.0f, face.getBottom() - 0.8f, 1.4f);
    g.restoreState();

    g.setColour(outlined ? theme.accentBorder.withAlpha(0.85f) : NFColours::black);
    g.drawRoundedRectangle(outer, corner, outlined ? 1.4f : 1.8f);
}

void NFLookAndFeel::drawToggleButton(juce::Graphics& g,
                                     juce::ToggleButton& button,
                                     bool highlighted,
                                     bool down)
{
    const bool showLed = button.getProperties()["led"];

    auto backgroundColour = showLed ? NFColours::black : theme.accentFill;
    drawButtonBackground(g, button, backgroundColour, highlighted, down);

    auto textArea = button.getLocalBounds().reduced(4);

    if (showLed)
    {
        auto ledArea = textArea.removeFromLeft(18);
        drawBypassLed(g, ledArea.withSizeKeepingCentre(11, 11).toFloat(), button.getToggleState());
    }

    g.setColour(NFColours::white);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    NFGraphics::drawBoldText(g, button.getButtonText(), textArea,
                             juce::Justification::centred, 1);
}

void NFLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const auto font = getLabelFont(label);
        const auto textColour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha);

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

        // The halo (see NFTheme) keeps small labels crisp when the skin's
        // face colour doesn't give the text much luminance contrast to
        // grab onto — a no-op on skins/labels that don't need it.
        NFGraphics::drawThickText(g, label.getText(), textArea.toFloat(), font,
                                  label.getJustificationType(), textColour, 0.4f,
                                  theme.haloColour, theme.haloWidth);

        g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
    }
    else if (label.isEnabled())
    {
        g.setColour(label.findColour(juce::Label::outlineColourId));
    }

    g.drawRect(label.getLocalBounds());
}

void NFLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                 int, int, int, int, juce::ComboBox&)
{
    // No background/border painted here — the panel draws one shared pill
    // behind the "OVERSAMPLING" caption and this box so the two read as a
    // single control, matching a stepper rather than a plain dropdown.
    auto arrowZone = juce::Rectangle<float>((float) width - 17.0f, 0.0f, 13.0f, (float) height);
    const float cx = arrowZone.getCentreX();
    const float cy = arrowZone.getCentreY();

    juce::Path up, down;
    up.addTriangle(cx - 4.0f, cy - 1.5f, cx + 4.0f, cy - 1.5f, cx, cy - 6.0f);
    down.addTriangle(cx - 4.0f, cy + 1.5f, cx + 4.0f, cy + 1.5f, cx, cy + 6.0f);

    g.setColour(NFColours::white);
    g.fillPath(up);
    g.fillPath(down);
}

juce::Font NFLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(12.5f, juce::Font::bold));
}

void NFLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // The default LookAndFeel_V4 reserves 30px for its dropdown arrow;
    // our custom stepper arrow (drawComboBox above) is only ~17px, so
    // reusing the default here left too little room and clipped "4x" etc.
    label.setBounds(1, 1, box.getWidth() - 19, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}
