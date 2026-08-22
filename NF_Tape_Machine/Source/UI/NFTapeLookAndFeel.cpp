#include "NFTapeLookAndFeel.h"

NFTapeLookAndFeel::NFTapeLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, NFTapeColours::white);
    setColour(juce::Slider::textBoxTextColourId, NFTapeColours::white);
    setColour(juce::Slider::textBoxBackgroundColourId, NFTapeColours::bezelBlack);
    setColour(juce::Slider::textBoxOutlineColourId, NFTapeColours::amber);
    setColour(juce::Label::textColourId, NFTapeColours::white);
    setColour(juce::PopupMenu::backgroundColourId, NFTapeColours::chassisPanel);
    setColour(juce::PopupMenu::textColourId, NFTapeColours::white);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, NFTapeColours::amberDim);
    setColour(juce::PopupMenu::highlightedTextColourId, NFTapeColours::amberBright);
}

void NFTapeLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(7.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    // Deeper, softer cast shadow — the single biggest cue that a knob
    // actually sits proud of the panel rather than being printed on it.
    for (int i = 6; i >= 1; --i)
    {
        const float grow = (float) i * 1.7f;
        g.setColour(juce::Colours::black.withAlpha(0.055f));
        g.fillEllipse(centre.x - radius - grow, centre.y - radius - grow + 3.0f,
                      (radius + grow) * 2.0f, (radius + grow) * 2.0f);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.fillEllipse(centre.x - radius - 4.5f, centre.y - radius - 4.5f,
                  (radius + 4.5f) * 2.0f, (radius + 4.5f) * 2.0f);

    {
        juce::ColourGradient lip(
            NFTapeColours::brassBright, centre.x, centre.y - radius - 2.5f,
            NFTapeColours::brass.darker(0.5f), centre.x, centre.y + radius + 2.5f, false);
        g.setGradientFill(lip);
        g.fillEllipse(centre.x - radius - 2.5f, centre.y - radius - 2.5f,
                      (radius + 2.5f) * 2.0f, (radius + 2.5f) * 2.0f);

        // Thin ambient-occlusion ring where the bronze lip meets the body.
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawEllipse(centre.x - radius - 0.5f, centre.y - radius - 0.5f, (radius + 0.5f) * 2.0f, (radius + 0.5f) * 2.0f, 1.4f);
    }

    juce::ColourGradient bodyGradient(
        NFTapeColours::knobBodyLight.brighter(0.15f), centre.x - radius * 0.4f, centre.y - radius * 0.55f,
        NFTapeColours::knobBodyDark.darker(0.35f), centre.x + radius * 0.6f, centre.y + radius * 0.8f, true);
    bodyGradient.addColour(0.55, NFTapeColours::knobBody);
    g.setGradientFill(bodyGradient);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(centre.x - radius + 0.7f, centre.y - radius + 0.7f,
                  (radius - 0.7f) * 2.0f, (radius - 0.7f) * 2.0f, 1.0f);

    // Bright, tight specular hotspot plus a wider soft glow underneath it —
    // two very different sizes/opacities read as glossy rather than flat.
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillEllipse(centre.x - radius * 0.55f, centre.y - radius * 0.62f, radius * 0.85f, radius * 0.42f);

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.fillEllipse(centre.x - radius * 0.32f, centre.y - radius * 0.52f, radius * 0.34f, radius * 0.18f);

    constexpr int numTicks = 25;
    const float tickInner = radius + 6.0f;
    const float tickOuter = radius + 11.0f;

    for (int i = 0; i < numTicks; ++i)
    {
        const float proportion = (float) i / (float) (numTicks - 1);
        const float angle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
        const bool major = (i % 4 == 0);

        const float inner = major ? tickInner - 1.5f : tickInner;
        const float outer = major ? tickOuter + 1.0f : tickOuter;

        const float x1 = centre.x + std::cos(angle) * inner;
        const float y1 = centre.y + std::sin(angle) * inner;
        const float x2 = centre.x + std::cos(angle) * outer;
        const float y2 = centre.y + std::sin(angle) * outer;

        g.setColour(NFTapeColours::textDim.withAlpha(major ? 0.9f : 0.55f));
        g.drawLine(x1, y1, x2, y2, major ? 1.6f : 1.0f);
    }

    const juce::String tickLabelsCsv = slider.getProperties()[NFTapeProps::tickLabels].toString();
    if (tickLabelsCsv.isNotEmpty())
    {
        const auto labels = juce::StringArray::fromTokens(tickLabelsCsv, "|", "");
        const int count = labels.size();
        const float labelR = tickOuter + 11.0f;

        g.setColour(NFTapeColours::textDim);
        g.setFont(juce::Font(juce::FontOptions(juce::jmax(9.5f, radius * 0.30f), juce::Font::bold)));

        for (int i = 0; i < count; ++i)
        {
            const float proportion = count > 1 ? (float) i / (float) (count - 1) : 0.0f;
            const float labelAngle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);

            const float lx = centre.x + std::cos(labelAngle) * labelR;
            const float ly = centre.y + std::sin(labelAngle) * labelR;

            juce::Rectangle<float> labelArea(lx - 16.0f, ly - 7.0f, 32.0f, 14.0f);
            g.drawFittedText(labels[i], labelArea.toNearestInt(), juce::Justification::centred, 1);
        }
    }

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float pointerStart = radius * 0.12f;
    const float pointerEnd = radius * 0.82f;

    const float px1 = centre.x + std::cos(angle) * pointerStart;
    const float py1 = centre.y + std::sin(angle) * pointerStart;
    const float px2 = centre.x + std::cos(angle) * pointerEnd;
    const float py2 = centre.y + std::sin(angle) * pointerEnd;

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(px1, py1 + 1.2f, px2, py2 + 1.2f, 3.4f);

    g.setColour(NFTapeColours::amberBright);
    g.drawLine(px1, py1, px2, py2, 2.4f);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(centre.x - 3.5f, centre.y - 3.0f, 7.0f, 7.0f);

    juce::ColourGradient capGradient(
        juce::Colour(0xffd8d4c8), centre.x - 1.2f, centre.y - 1.6f,
        juce::Colour(0xff5a5a54), centre.x + 1.2f, centre.y + 2.0f, true);
    g.setGradientFill(capGradient);
    g.fillEllipse(centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
}

void NFTapeLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&,
                                             bool highlighted, bool down)
{
    if ((bool) button.getProperties()[NFTapeProps::segmented])
    {
        drawSegmentedButton(g, button, highlighted, down);
        return;
    }

    if ((bool) button.getProperties()[NFTapeProps::ledPill])
    {
        drawLedPillButton(g, button, highlighted, down);
        return;
    }

    if ((bool) button.getProperties()[NFTapeProps::powerSquare])
    {
        drawPowerSquareButton(g, button, highlighted, down);
        return;
    }

    if ((bool) button.getProperties()[NFTapeProps::powerCircle])
    {
        drawPowerCircleButton(g, button, highlighted, down);
        return;
    }

    // Plain utility button (preset arrows, menu, etc.)
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 4.0f;

    if (!down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(outer.translated(0.0f, 1.2f), corner);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(outer, corner);

    auto face = outer.reduced(1.4f);
    auto base = down ? NFTapeColours::chassisDark : (highlighted ? NFTapeColours::chassisLight : NFTapeColours::chassisPanel);

    juce::ColourGradient faceGradient = down
        ? juce::ColourGradient(base.darker(0.2f), face.getX(), face.getY(), base.brighter(0.06f), face.getX(), face.getBottom(), false)
        : juce::ColourGradient(base.brighter(0.18f), face.getX(), face.getY(), base.darker(0.25f), face.getX(), face.getBottom(), false);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(face, corner - 1.0f);

    g.setColour(NFTapeColours::bezelBlack);
    g.drawRoundedRectangle(outer, corner, 1.4f);

    if ((bool) button.getProperties()[NFTapeProps::hamburger])
    {
        auto iconArea = face.reduced(face.getWidth() * 0.24f, face.getHeight() * 0.3f);
        g.setColour(NFTapeColours::white);
        const float step = iconArea.getHeight() / 2.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float y = iconArea.getY() + step * (float) i;
            g.drawLine(iconArea.getX(), y, iconArea.getRight(), y, 1.8f);
        }
    }
}

void NFTapeLookAndFeel::drawPowerCircleButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down)
{
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto centre = outer.getCentre();
    const float radius = juce::jmin(outer.getWidth(), outer.getHeight()) * 0.5f;
    const bool on = ! button.getToggleState();

    if (!down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(centre.x - radius, centre.y - radius + 1.6f, radius * 2.0f, radius * 2.0f);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    const float faceRadius = radius - 2.0f;

    if (on)
    {
        juce::ColourGradient grad(NFTapeColours::amberBright, centre.x - faceRadius * 0.3f, centre.y - faceRadius * 0.4f,
                                  NFTapeColours::ledRed.darker(0.2f), centre.x + faceRadius * 0.4f, centre.y + faceRadius * 0.5f, true);
        g.setGradientFill(grad);
        g.fillEllipse(centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.fillEllipse(centre.x - faceRadius * 0.45f, centre.y - faceRadius * 0.55f, faceRadius * 0.5f, faceRadius * 0.32f);
    }
    else
    {
        auto base = highlighted ? NFTapeColours::chassisLight : NFTapeColours::chassisDark;
        juce::ColourGradient grad(base.brighter(0.1f), centre.x, centre.y - faceRadius,
                                  base.darker(0.3f), centre.x, centre.y + faceRadius, false);
        g.setGradientFill(grad);
        g.fillEllipse(centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
    }

    // Power glyph: a broken ring with a short vertical tick through the top.
    g.setColour(on ? juce::Colours::black.withAlpha(0.75f) : NFTapeColours::textDim);
    juce::Path ring;
    ring.addCentredArc(centre.x, centre.y, faceRadius * 0.42f, faceRadius * 0.42f,
                       0.0f, juce::MathConstants<float>::pi * 0.28f,
                       juce::MathConstants<float>::twoPi - juce::MathConstants<float>::pi * 0.28f, true);
    g.strokePath(ring, juce::PathStrokeType(1.8f));
    g.drawLine(centre.x, centre.y - faceRadius * 0.55f, centre.x, centre.y - faceRadius * 0.05f, 1.8f);

    g.setColour(juce::Colours::black);
    g.drawEllipse(centre.x - radius + 0.6f, centre.y - radius + 0.6f, (radius - 0.6f) * 2.0f, (radius - 0.6f) * 2.0f, 1.2f);
}

void NFTapeLookAndFeel::drawSegmentedButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down)
{
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 3.0f;
    const bool selected = button.getToggleState();

    if (!down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRoundedRectangle(outer.translated(0.0f, 1.2f), corner);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(outer, corner);

    auto face = outer.reduced(1.2f);

    if (selected)
    {
        juce::ColourGradient grad(NFTapeColours::amberBright, face.getX(), face.getY(),
                                  NFTapeColours::amber.darker(0.3f), face.getX(), face.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(face, corner - 0.6f);

        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawRoundedRectangle(face, corner - 0.6f, 1.0f);
    }
    else
    {
        auto base = highlighted ? NFTapeColours::chassisLight : NFTapeColours::chassisPanel;
        juce::ColourGradient grad(base.brighter(0.1f), face.getX(), face.getY(),
                                  base.darker(0.3f), face.getX(), face.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(face, corner - 0.6f);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.drawRoundedRectangle(outer, corner, 1.2f);
}

void NFTapeLookAndFeel::drawLedPillButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool)
{
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 4.0f;
    const bool on = button.getToggleState();

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(outer.translated(0.0f, 1.0f), corner);

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(outer, corner);

    auto face = outer.reduced(1.2f);
    auto base = highlighted ? NFTapeColours::chassisLight : NFTapeColours::chassisPanel;
    juce::ColourGradient grad(base.brighter(0.12f), face.getX(), face.getY(),
                              base.darker(0.28f), face.getX(), face.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(face, corner - 0.6f);

    g.setColour(NFTapeColours::bezelBlack);
    g.drawRoundedRectangle(outer, corner, 1.2f);

    auto ledArea = face.removeFromLeft(juce::jmin(16.0f, face.getWidth() * 0.3f))
                       .withSizeKeepingCentre(9.0f, 9.0f);

    g.setColour(juce::Colours::black);
    g.fillEllipse(ledArea.expanded(1.2f));

    if (on)
    {
        juce::ColourGradient ledGrad(NFTapeColours::amberBright, ledArea.getCentreX(), ledArea.getY(),
                                     NFTapeColours::amber.darker(0.4f), ledArea.getCentreX(), ledArea.getBottom(), false);
        g.setGradientFill(ledGrad);
        g.fillEllipse(ledArea);
    }
    else
    {
        g.setColour(juce::Colour(0xff2a2015));
        g.fillEllipse(ledArea);
    }
}

void NFTapeLookAndFeel::drawPowerSquareButton(juce::Graphics& g, juce::Button& button, bool highlighted, bool down)
{
    auto outer = button.getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 5.0f;
    // toggleState mirrors the "bypass" parameter (true = bypassed), so the
    // lamp should light when it's FALSE (i.e. the tape path is engaged).
    const bool on = ! button.getToggleState();

    if (!down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(outer.translated(0.0f, 1.6f), corner);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(outer, corner);

    auto face = outer.reduced(1.6f);

    if (on)
    {
        juce::ColourGradient grad(NFTapeColours::amberBright, face.getCentreX(), face.getY(),
                                  NFTapeColours::amber.darker(0.35f), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(face, corner - 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.fillRoundedRectangle(face.removeFromTop(face.getHeight() * 0.35f).reduced(3.0f, 0.0f), corner - 1.5f);
    }
    else
    {
        auto base = highlighted ? NFTapeColours::chassisLight : NFTapeColours::chassisDark;
        juce::ColourGradient grad(base.brighter(0.1f), face.getCentreX(), face.getY(),
                                  base.darker(0.3f), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(face, corner - 1.0f);
    }

    g.setColour(NFTapeColours::bezelBlack);
    g.drawRoundedRectangle(outer, corner, 1.6f);
}

void NFTapeLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                       bool, bool)
{
    if ((bool) button.getProperties()[NFTapeProps::powerCircle]
        || (bool) button.getProperties()[NFTapeProps::hamburger])
        return;

    const bool selected = button.getToggleState();
    const bool isSegmentedOrPill = (bool) button.getProperties()[NFTapeProps::segmented]
                                 || (bool) button.getProperties()[NFTapeProps::ledPill]
                                 || (bool) button.getProperties()[NFTapeProps::powerSquare];

    g.setColour(isSegmentedOrPill && selected ? juce::Colours::black : NFTapeColours::white);
    g.setFont(juce::Font(juce::FontOptions(button.getHeight() * 0.42f, juce::Font::bold)));

    auto textArea = button.getLocalBounds();

    if ((bool) button.getProperties()[NFTapeProps::ledPill])
        textArea.removeFromLeft(juce::jmin(16, textArea.getWidth() / 3));

    g.drawFittedText(button.getButtonText(), textArea, juce::Justification::centred, 1);
}

void NFTapeLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (label.isBeingEdited())
        return;

    const float alpha = label.isEnabled() ? 1.0f : 0.5f;
    const bool isGoldTitle = (bool) label.getProperties()[NFTapeProps::goldTitle];
    const bool useOwnFont = isGoldTitle || (bool) label.getProperties()[NFTapeProps::customFont];

    // The title/subtitle set their own font explicitly (size, bold,
    // kerning) rather than relying on the generic height-based auto-size
    // every other caption uses.
    const auto font = useOwnFont ? label.getFont() : getLabelFont(label);
    auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

    if (isGoldTitle)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText(font, label.getText(), (float) textArea.getX(), (float) textArea.getY(),
                             (float) textArea.getWidth(), (float) textArea.getHeight(),
                             label.getJustificationType(), 1);
        juce::Path path;
        glyphs.createPath(path);
        const auto pathBounds = path.getBounds();

        // Small, tight glow — just enough to read as "lit from within",
        // not a thick fog. Crisp letters matter more than a big halo here.
        for (int i = 3; i >= 1; --i)
        {
            g.setColour(NFTapeColours::amberBright.withAlpha(0.06f * (float) i));
            g.strokePath(path, juce::PathStrokeType((float) i * 0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::ColourGradient goldGrad(
            juce::Colour(0xfff9ecc4), pathBounds.getX(), pathBounds.getY(),
            juce::Colour(0xffe8b85c), pathBounds.getX(), pathBounds.getBottom(), false);
        goldGrad.addColour(0.5, juce::Colour(0xfff2d59a));
        g.setGradientFill(goldGrad);
        g.fillPath(path);
        return;
    }

    g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
    g.setFont(font);
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(), 2);
}

juce::Font NFTapeLookAndFeel::getLabelFont(juce::Label& label)
{
    return juce::Font(juce::FontOptions(juce::jmax(10.0f, (float) label.getHeight() * 0.62f), juce::Font::bold));
}

void NFTapeLookAndFeel::setTickLabels(juce::Slider& slider, const juce::StringArray& labels)
{
    slider.getProperties().set(NFTapeProps::tickLabels, labels.joinIntoString("|"));
}
