#include "NFStressorLookAndFeel.h"

using namespace NFStressorColours;

NFStressorLookAndFeel::NFStressorLookAndFeel()
{
    setColour(juce::Label::textColourId, textLight);
    setColour(juce::TextButton::buttonColourId, panelDarker);
    setColour(juce::TextButton::buttonOnColourId, amber);
    setColour(juce::TextButton::textColourOffId, textDim);
    setColour(juce::TextButton::textColourOnId, panelDarker);
}

void NFStressorLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPosProportional,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider&)
{
    // The whole numbered disc rotates as one piece — it's part of the knob,
    // not a fixed panel scale. A fixed outer frame + red index mark at the
    // top shows which number is currently selected as the disc turns
    // underneath it. Geometry intentionally fixed — do not genericise this
    // for other knobs.
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Cast shadow, lit from upper-left so the shadow falls lower-right.
    {
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), (int) (radius * 0.24f), { (int) (radius * 0.06f), (int) (radius * 0.12f) });
        juce::Path shadowPath;
        shadowPath.addEllipse(centre.x - radius * 0.9f, centre.y - radius * 0.9f, radius * 1.8f, radius * 1.8f);
        shadow.drawForPath(g, shadowPath);
    }

    // Large light silver/grey disc — the whole rotating body of the knob.
    const float discRadius = radius * 0.9f;
    juce::ColourGradient discGradient(knobDiscLight, centre.x - discRadius * 0.4f, centre.y - discRadius * 0.6f,
                                      knobDiscDark, centre.x + discRadius * 0.4f, centre.y + discRadius * 0.6f, false);
    g.setGradientFill(discGradient);
    g.fillEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f, 1.0f);

    // 3D bevel: a bright rim catching the light along the upper-left edge and
    // a dark shadowed rim along the lower-right edge, so the disc reads as a
    // domed, machined metal surface rather than a flat sticker.
    {
        juce::Path rimLight, rimShadow;
        const auto rimBounds = juce::Rectangle<float>(centre.x - discRadius, centre.y - discRadius,
                                                       discRadius * 2.0f, discRadius * 2.0f);
        rimLight.addArc(rimBounds.getX(), rimBounds.getY(), rimBounds.getWidth(), rimBounds.getHeight(),
                        juce::MathConstants<float>::pi * 1.05f, juce::MathConstants<float>::pi * 1.85f, true);
        rimShadow.addArc(rimBounds.getX(), rimBounds.getY(), rimBounds.getWidth(), rimBounds.getHeight(),
                         juce::MathConstants<float>::pi * -0.05f, juce::MathConstants<float>::pi * 0.85f, true);

        juce::PathStrokeType stroke(juce::jmax(1.0f, discRadius * 0.055f),
                                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(juce::Colours::white.withAlpha(0.32f));
        g.strokePath(rimLight, stroke);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.strokePath(rimShadow, stroke);
    }

    // Soft specular dome highlight, upper-left, so the disc looks curved
    // rather than flat.
    {
        const float specR = discRadius * 0.62f;
        const auto specCentre = centre.translated(-discRadius * 0.32f, -discRadius * 0.42f);
        juce::ColourGradient specular(juce::Colours::white.withAlpha(0.22f), specCentre.x, specCentre.y,
                                      juce::Colours::white.withAlpha(0.0f), specCentre.x, specCentre.y + specR, true);
        g.setGradientFill(specular);
        g.fillEllipse(specCentre.x - specR, specCentre.y - specR, specR * 2.0f, specR * 2.0f);
    }

    // 41 marks from 0 to 10 in 0.25 steps, rotating with the disc: each
    // mark's screen angle is its own fixed angle on the dial minus the
    // current rotation, so the mark matching the live value always lands
    // under the fixed index at the top. Every integer 0-10 gets a printed
    // number, bold and heavy-stroked like the numbered dial on a real
    // Distressor knob; the finer marks in between are unlabelled ticks.
    for (int i = 0; i <= 40; ++i)
    {
        const float value = (float) i * 0.25f;
        const float dialAngle = rotaryStartAngle + ((float) i / 40.0f) * (rotaryEndAngle - rotaryStartAngle);
        const float screenAngle = dialAngle - angle;
        const bool isMajor = std::fmod(value, 1.0f) < 0.01f;

        // Ticks live in a short band right at the rim; numbers sit in their
        // own ring well inside it, so the two never cross and the digits
        // stay crisp instead of looking scratched-through.
        const float innerR = discRadius * (isMajor ? 0.83f : 0.88f);
        const float outerR = discRadius * 0.95f;
        const auto p1 = centre.getPointOnCircumference(innerR, screenAngle);
        const auto p2 = centre.getPointOnCircumference(outerR, screenAngle);
        g.setColour(isMajor ? tick : tick.withAlpha(0.6f));
        g.drawLine({ p1, p2 }, isMajor ? 2.5f : 1.0f);

        if (isMajor)
        {
            // Numbers are rotated to follow the radial direction at their
            // position, like the engraved dial on a real Distressor knob,
            // rather than sitting perfectly upright everywhere.
            const auto labelPos = centre.getPointOnCircumference(discRadius * 0.65f, screenAngle);
            g.setColour(tick);
            g.setFont(juce::Font(juce::FontOptions(juce::jmax(10.5f, radius * 0.21f)).withStyle("Bold")));
            // Float-precision drawText (not drawFittedText, which snaps its
            // rectangle to integer pixels) — rounding to int here would make
            // the label jump a pixel at a time as it rotates continuously,
            // reading as a wobble instead of a smooth turn.
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(screenAngle, labelPos.x, labelPos.y));
            g.drawText(juce::String((int) value), juce::Rectangle<float>(19.0f, 16.0f).withCentre(labelPos),
                      juce::Justification::centred, false);
            g.restoreState();
        }
    }

    // Wide dark separator ring, rotating with the disc.
    const float ringRadius = discRadius * 0.4f;
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawEllipse(centre.x - ringRadius, centre.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f,
                 juce::jmax(2.2f, radius * 0.07f));

    // Light-grey centre circle, inside the ring — plain, no pointer mark.
    const float centreRadius = ringRadius * 0.78f;
    juce::ColourGradient centreGradient(knobCentreLight, centre.x - centreRadius * 0.3f, centre.y - centreRadius,
                                        knobCentreDark, centre.x + centreRadius * 0.3f, centre.y + centreRadius, false);
    g.setGradientFill(centreGradient);
    g.fillEllipse(centre.x - centreRadius, centre.y - centreRadius, centreRadius * 2.0f, centreRadius * 2.0f);

    // Small specular dot so the centre cap reads as a raised dome too.
    {
        const float capSpecR = centreRadius * 0.55f;
        const auto capSpecCentre = centre.translated(-centreRadius * 0.28f, -centreRadius * 0.32f);
        juce::ColourGradient capSpecular(juce::Colours::white.withAlpha(0.35f), capSpecCentre.x, capSpecCentre.y,
                                         juce::Colours::white.withAlpha(0.0f), capSpecCentre.x, capSpecCentre.y + capSpecR, true);
        g.setGradientFill(capSpecular);
        g.fillEllipse(capSpecCentre.x - capSpecR, capSpecCentre.y - capSpecR, capSpecR * 2.0f, capSpecR * 2.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(centre.x - centreRadius, centre.y - centreRadius, centreRadius * 2.0f, centreRadius * 2.0f, 1.0f);

    // Fixed outer frame ring — static reference the numbered disc spins
    // inside of.
    const float frameRadius = radius * 0.98f;
    g.setColour(bezelInner);
    g.drawEllipse(centre.x - frameRadius, centre.y - frameRadius, frameRadius * 2.0f, frameRadius * 2.0f, 1.6f);

    // Fixed red index mark at the top of the frame, pointing in at whichever
    // number the rotating disc currently shows.
    {
        juce::Path indexMark;
        const float notchWidth = radius * 0.122f;
        const float notchLength = radius * 0.28f;
        indexMark.addTriangle(-notchWidth * 0.5f, 0.0f, notchWidth * 0.5f, 0.0f, 0.0f, notchLength);
        g.setColour(pointerTip);
        g.fillPath(indexMark, juce::AffineTransform::translation(centre.x, centre.y - frameRadius));
    }
}

void NFStressorLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour&,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = button.getToggleState();

    // Preset menu — a plain hamburger icon sitting directly on the chassis,
    // no button box, just three bars that brighten slightly on hover.
    if (button.getButtonText() == "MENU")
    {
        const bool hovered = shouldDrawButtonAsHighlighted;
        if (hovered)
        {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }

        const float barWidth = bounds.getWidth() * 0.7f;
        const float barHeight = 2.2f;
        const float gap = bounds.getHeight() * 0.34f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        g.setColour(textLight.withAlpha(hovered ? 0.95f : 0.75f));
        for (int i = -1; i <= 1; ++i)
        {
            const float y = cy + (float) i * gap;
            g.fillRoundedRectangle(cx - barWidth * 0.5f, y - barHeight * 0.5f, barWidth, barHeight, barHeight * 0.5f);
        }
        return;
    }

    // NUKE and BYPASS are both plain square buttons — no LED, the whole
    // face lights up solid when engaged, like a hardware push switch with a
    // lit cap. NUKE lights blue and stays solidly lit; BYPASS lights red and
    // blinks (see PluginEditor::timerCallback) so it's impossible to miss
    // that the plugin is doing nothing while it's on.
    if (button.getButtonText() == "NUKE" || button.getButtonText() == "BYPASS")
    {
        const bool isBypass = button.getButtonText() == "BYPASS";
        const bool lit = isBypass ? (isOn && button.getProperties().getWithDefault("blinkVisible", true)) : isOn;
        const juce::Colour litColour = isBypass ? red : nukeBlue;
        const bool hovered = shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown && !lit;
        const float corner = 6.0f;

        if (lit)
        {
            for (float grow = 6.0f; grow > 0.0f; grow -= 2.0f)
            {
                g.setColour(litColour.withAlpha(0.06f));
                g.fillRoundedRectangle(bounds.expanded(grow), corner + grow * 0.5f);
            }

            juce::ColourGradient onGradient(litColour.brighter(0.3f), bounds.getX(), bounds.getY(),
                                            litColour.darker(0.2f), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(onGradient);
            g.fillRoundedRectangle(bounds, corner);

            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.drawLine(bounds.getX() + 2, bounds.getY() + 1.2f, bounds.getRight() - 2, bounds.getY() + 1.2f, 1.0f);
        }
        else
        {
            juce::Colour top = panelDark.darker(shouldDrawButtonAsDown ? 0.15f : 0.0f);
            juce::Colour bottom = panelDarker.darker(0.2f);
            if (hovered)
            {
                top = top.brighter(0.4f);
                bottom = bottom.brighter(0.15f);
            }

            juce::ColourGradient offGradient(top, bounds.getX(), bounds.getY(),
                                             bottom, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(offGradient);
            g.fillRoundedRectangle(bounds, corner);

            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawLine(bounds.getX() + 2, bounds.getY() + 0.8f, bounds.getRight() - 2, bounds.getY() + 0.8f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawLine(bounds.getX() + 2, bounds.getBottom() - 0.8f, bounds.getRight() - 2, bounds.getBottom() - 0.8f, 1.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds, corner, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(hovered ? 0.55f : 0.3f));
        g.drawRoundedRectangle(bounds.reduced(0.75f), corner - 0.75f, hovered ? 1.5f : 1.1f);
        return;
    }

    if (!button.isEnabled())
        g.beginTransparencyLayer(0.4f);

    // RATIO is a mutually-exclusive segment selector (clickingTogglesState
    // is off for those buttons) — it keeps the flat highlighted-fill look.
    // Everything else is a real hardware-style toggle switch: illuminated
    // LED, panel colour never changes.
    if (!button.getClickingTogglesState())
    {
        const float corner = 3.5f;

        if (isOn)
        {
            for (float grow = 6.0f; grow > 0.0f; grow -= 2.0f)
            {
                g.setColour(amber.withAlpha(0.05f));
                g.fillRoundedRectangle(bounds.expanded(grow), corner + grow * 0.5f);
            }

            juce::ColourGradient onGradient(amber.brighter(0.25f), bounds.getX(), bounds.getY(),
                                            amber.darker(0.25f), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(onGradient);
            g.fillRoundedRectangle(bounds, corner);

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawLine(bounds.getX() + 2, bounds.getY() + 1.2f, bounds.getRight() - 2, bounds.getY() + 1.2f, 1.0f);
        }
        const bool hovered = shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown && !isOn;
        if (!isOn)
        {
            juce::Colour top = panelDark.darker(shouldDrawButtonAsDown ? 0.15f : 0.0f);
            juce::Colour bottom = panelDarker.darker(0.2f);
            if (hovered)
            {
                top = top.brighter(0.4f);
                bottom = bottom.brighter(0.15f);
            }

            juce::ColourGradient offGradient(top, bounds.getX(), bounds.getY(),
                                             bottom, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(offGradient);
            g.fillRoundedRectangle(bounds, corner);

            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawLine(bounds.getX() + 2, bounds.getY() + 0.8f, bounds.getRight() - 2, bounds.getY() + 0.8f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawLine(bounds.getX() + 2, bounds.getBottom() - 0.8f, bounds.getRight() - 2, bounds.getBottom() - 0.8f, 1.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds, corner, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(hovered ? 0.55f : 0.3f));
        g.drawRoundedRectangle(bounds.reduced(0.75f), corner - 0.75f, hovered ? 1.5f : 1.1f);

        if (!button.isEnabled())
            g.endTransparencyLayer();
        return;
    }

    const float corner = bounds.getHeight() * 0.5f; // hardware-style pill switch
    const bool hovered = shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown;

    juce::Colour top = panelDark.darker(shouldDrawButtonAsDown ? 0.15f : 0.0f);
    juce::Colour bottom = panelDarker.darker(0.25f);
    if (hovered)
    {
        top = top.brighter(0.4f);
        bottom = bottom.brighter(0.15f);
    }

    juce::ColourGradient bodyGradient(top, bounds.getX(), bounds.getY(),
                                      bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bodyGradient);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(bounds.getX() + corner, bounds.getY() + 0.8f, bounds.getRight() - corner, bounds.getY() + 0.8f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(bounds.getX() + corner, bounds.getBottom() - 0.8f, bounds.getRight() - corner, bounds.getBottom() - 0.8f, 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds, corner, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(hovered ? 0.55f : 0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.75f), corner - 0.75f, hovered ? 1.5f : 1.1f);

    // LED indicator, inset at the left of the switch.
    const float ledDiameter = bounds.getHeight() * 0.34f;
    const auto ledCentre = juce::Point<float>(bounds.getX() + bounds.getHeight() * 0.5f, bounds.getCentreY());
    juce::Rectangle<float> led(ledDiameter, ledDiameter);
    led.setCentre(ledCentre);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(led.expanded(ledDiameter * 0.22f));

    if (isOn)
    {
        // Tight rim glow only, hugging the LED edge — no soft bloom
        // spreading out into the switch body around it.
        g.setColour(red.withAlpha(0.45f));
        g.fillEllipse(led.expanded(ledDiameter * 0.1f));
    }

    juce::ColourGradient ledGradient(isOn ? red.withMultipliedSaturation(1.25f).withMultipliedBrightness(1.4f) : ledOff.brighter(0.1f), led.getX(), led.getY(),
                                     isOn ? red.withMultipliedSaturation(1.15f) : ledOff.darker(0.2f), led.getX(), led.getBottom(), false);
    g.setGradientFill(ledGradient);
    g.fillEllipse(led);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(led, 0.7f);

    if (isOn)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillEllipse(led.reduced(ledDiameter * 0.62f).translated(-ledDiameter * 0.08f, -ledDiameter * 0.1f));
    }

    if (!button.isEnabled())
        g.endTransparencyLayer();
}

void NFStressorLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool)
{
    if (button.getButtonText() == "MENU")
        return; // icon-only, fully drawn in drawButtonBackground

    if (!button.isEnabled())
        g.beginTransparencyLayer(0.4f);

    auto bounds = button.getLocalBounds().toFloat();

    if (button.getButtonText() == "NUKE" || button.getButtonText() == "BYPASS")
    {
        // Centred in the plain square face — the whole button lights up
        // solid when engaged, so the text just needs enough contrast to
        // read against either state. BYPASS blinks in sync with its fill.
        const bool isBypass = button.getButtonText() == "BYPASS";
        const bool lit = isBypass
                             ? (button.getToggleState() && button.getProperties().getWithDefault("blinkVisible", true))
                             : button.getToggleState();
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.setColour(lit ? panelDarker.darker(0.3f) : textLight.withAlpha(0.75f));
        g.drawFittedText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);

        if (!button.isEnabled())
            g.endTransparencyLayer();
        return;
    }

    if (button.getClickingTogglesState())
        bounds.removeFromLeft(bounds.getHeight() * 0.85f); // clears the LED

    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour(button.getClickingTogglesState() ? (button.getToggleState() ? textLight : textLight.withAlpha(0.75f))
                                                 : (button.getToggleState() ? panelDarker.darker(0.3f) : textLight.withAlpha(0.75f)));
    g.drawFittedText(button.getButtonText(), bounds.reduced(2, 0).toNearestInt(), juce::Justification::centred, 1);

    if (!button.isEnabled())
        g.endTransparencyLayer();
}

juce::Font NFStressorLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    if (button.getButtonText() == "NUKE")
        return juce::Font(juce::FontOptions(15.5f).withStyle("Bold"));

    if (button.getButtonText() == "BYPASS")
        return juce::Font(juce::FontOptions(11.0f).withStyle("Bold"));

    return juce::Font(juce::FontOptions(juce::jlimit(9.0f, 16.0f, (float) buttonHeight * 0.46f)).withStyle("Bold"));
}

void NFStressorLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(label.getFont());
    g.drawFittedText(label.getText(), label.getLocalBounds(), label.getJustificationType(), 1);
}
