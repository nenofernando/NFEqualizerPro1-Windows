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
    // The whole numbered dial (ticks + numbers) is one big rotating knob —
    // it turns as a single piece when dragged, like a safe/tuning dial. A
    // FIXED ring frames it from outside, with a fixed red index mark at the
    // top showing which number is currently "selected" as the dial spins
    // underneath it. Geometry intentionally fixed — do not genericise this
    // for other knobs.
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Cast shadow, lit from upper-left so the shadow falls lower-right.
    {
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.6f), (int) (radius * 0.28f), { (int) (radius * 0.08f), (int) (radius * 0.14f) });
        juce::Path shadowPath;
        shadowPath.addEllipse(centre.x - radius * 0.86f, centre.y - radius * 0.86f, radius * 1.72f, radius * 1.72f);
        shadow.drawForPath(g, shadowPath);
    }

    // One continuous white disc — this is the whole rotating body. The only
    // black on it is the engraved ticks/numbers and a thin ring near the
    // centre; everything else, including inside that ring, stays white.
    const float discRadius = radius * 0.94f;
    juce::ColourGradient discGradient(knobWhiteLight, centre.x - discRadius * 0.5f, centre.y - discRadius * 0.6f,
                                      knobWhiteDark, centre.x + discRadius * 0.5f, centre.y + discRadius * 0.6f, false);
    g.setGradientFill(discGradient);
    g.fillEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f);

    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path discClip;
        discClip.addEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f);
        g.reduceClipRegion(discClip);

        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.35f), centre.x - discRadius * 0.4f, centre.y - discRadius,
                                   juce::Colours::white.withAlpha(0.0f), centre.x - discRadius * 0.4f, centre.y + discRadius * 0.3f, false);
        g.setGradientFill(sheen);
        g.fillEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(centre.x - discRadius, centre.y - discRadius, discRadius * 2.0f, discRadius * 2.0f, 1.0f);

    // 21 marks from 0 to 10 in 0.5 steps, engraved black on the grey disc and
    // rotating with it: each mark's screen angle is its own fixed angle on
    // the dial minus the current rotation, so the mark matching the live
    // value always lands under the fixed index at the top (angle 0). Only
    // even integers get a printed number.
    for (int i = 0; i <= 20; ++i)
    {
        const float value = (float) i * 0.5f;
        const float dialAngle = rotaryStartAngle + ((float) i / 20.0f) * (rotaryEndAngle - rotaryStartAngle);
        const float screenAngle = dialAngle - angle;
        const bool isMajor = std::fmod(value, 2.0f) < 0.01f;

        const float innerR = discRadius * (isMajor ? 0.62f : 0.68f);
        const float outerR = discRadius * 0.9f;
        const auto p1 = centre.getPointOnCircumference(innerR, screenAngle);
        const auto p2 = centre.getPointOnCircumference(outerR, screenAngle);
        g.setColour(isMajor ? tick : tick.withAlpha(0.6f));
        g.drawLine({ p1, p2 }, isMajor ? 1.4f : 0.8f);

        if (isMajor)
        {
            const auto labelPos = centre.getPointOnCircumference(discRadius * 0.76f, screenAngle);
            g.setColour(tick);
            g.setFont(juce::Font(juce::FontOptions(juce::jmax(7.0f, radius * 0.16f)).withStyle("Bold")));
            g.drawFittedText(juce::String((int) value), juce::Rectangle<float>(14.0f, 11.0f).withCentre(labelPos).toNearestInt(),
                             juce::Justification::centred, 1);
        }
    }

    // Thin black ring near the centre — just an outline, not a filled hub;
    // the disc stays white both outside and inside it.
    const float innerRingRadius = discRadius * 0.34f;
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawEllipse(centre.x - innerRingRadius, centre.y - innerRingRadius, innerRingRadius * 2.0f, innerRingRadius * 2.0f,
                 juce::jmax(1.2f, radius * 0.035f));

    // Fixed outer frame ring — the second, larger circle beyond the marks —
    // static reference the numbered dial spins inside of.
    const float frameRadius = radius * 1.0f;
    g.setColour(bezelInner);
    g.drawEllipse(centre.x - frameRadius, centre.y - frameRadius, frameRadius * 2.0f, frameRadius * 2.0f, 1.6f);

    // Fixed red index mark at the top of the frame, pointing in at whichever
    // number the rotating dial currently shows.
    {
        juce::Path indexMark;
        const float notchWidth = radius * 0.09f;
        const float notchLength = radius * 0.14f;
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
        else
        {
            juce::Colour top = panelDark.darker(shouldDrawButtonAsDown ? 0.15f : 0.0f);
            juce::Colour bottom = panelDarker.darker(0.2f);
            if (shouldDrawButtonAsHighlighted)
                top = top.brighter(0.08f);

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

        if (!button.isEnabled())
            g.endTransparencyLayer();
        return;
    }

    const float corner = bounds.getHeight() * 0.5f; // hardware-style pill switch

    juce::Colour top = panelDark.darker(shouldDrawButtonAsDown ? 0.15f : 0.0f);
    juce::Colour bottom = panelDarker.darker(0.25f);
    if (shouldDrawButtonAsHighlighted)
        top = top.brighter(0.08f);

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

    // LED indicator, inset at the left of the switch.
    const float ledDiameter = bounds.getHeight() * 0.34f;
    const auto ledCentre = juce::Point<float>(bounds.getX() + bounds.getHeight() * 0.5f, bounds.getCentreY());
    juce::Rectangle<float> led(ledDiameter, ledDiameter);
    led.setCentre(ledCentre);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(led.expanded(ledDiameter * 0.22f));

    if (isOn)
    {
        g.setColour(red.withAlpha(0.35f));
        g.fillEllipse(led.expanded(ledDiameter * 0.55f));
    }

    juce::ColourGradient ledGradient(isOn ? red.brighter(0.3f) : ledOff.brighter(0.1f), led.getX(), led.getY(),
                                     isOn ? red.darker(0.3f) : ledOff.darker(0.2f), led.getX(), led.getBottom(), false);
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
    if (!button.isEnabled())
        g.beginTransparencyLayer(0.4f);

    auto bounds = button.getLocalBounds().toFloat();
    if (button.getClickingTogglesState())
        bounds.removeFromLeft(bounds.getHeight() * 0.85f); // clears the LED

    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour(button.getClickingTogglesState() ? (button.getToggleState() ? textLight : textDim)
                                                 : (button.getToggleState() ? panelDarker.darker(0.3f) : textDim));
    g.drawFittedText(button.getButtonText(), bounds.reduced(2, 0).toNearestInt(), juce::Justification::centred, 1);

    if (!button.isEnabled())
        g.endTransparencyLayer();
}

juce::Font NFStressorLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jlimit(9.0f, 13.0f, (float) buttonHeight * 0.42f)).withStyle("Bold"));
}

void NFStressorLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(label.getFont());
    g.drawFittedText(label.getText(), label.getLocalBounds(), label.getJustificationType(), 1);
}
