#include "NFTapeReels.h"
#include "NFTapeLookAndFeel.h"

namespace
{
    struct ReelFinish
    {
        juce::Colour rimBright, rimDark;
        juce::Colour flangeBright, flangeDark;
    };

    // GP9 = warm gold, 456 = brushed silver, 499 = deep red anodised,
    // 250 = matte black — a distinct finish per tape stock selection.
    const ReelFinish& getReelFinish(int typeIndex)
    {
        static const ReelFinish finishes[] = {
            // GP9 matches the VU meter's cream-gold face (slightly lighter).
            { juce::Colour(0xfff8ecc8), juce::Colour(0xffddc090), juce::Colour(0xfffbf3de), juce::Colour(0xffe8cc98) },
            { juce::Colour(0xffe8e6e0), juce::Colour(0xff6c6a64), juce::Colour(0xfff5f4f0), juce::Colour(0xff9a988e) },
            { juce::Colour(0xffe0a090), juce::Colour(0xff7a2a1e), juce::Colour(0xfff0c0b0), juce::Colour(0xffa83f2f) },
            { juce::Colour(0xff7a7a7a), juce::Colour(0xff1a1a1a), juce::Colour(0xff9a9a9a), juce::Colour(0xff2a2a2a) },
        };

        return finishes[juce::jlimit(0, 3, typeIndex)];
    }
}

void NFTapeReels::setTapeType(int typeIndex)
{
    if (tapeTypeIndex == typeIndex)
        return;

    tapeTypeIndex = typeIndex;
    repaint();
}

void NFTapeReels::setSpeedIndex(int speedIndex)
{
    const float multiplier = speedIndex <= 0 ? 0.55f : (speedIndex == 1 ? 1.0f : 1.85f);
    speedMultiplier = multiplier;
}

NFTapeReels::NFTapeReels(bool mirrored)
    : mirroredFlange(mirrored)
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(30);
}

NFTapeReels::~NFTapeReels()
{
    stopTimer();
}

void NFTapeReels::setSpinning(bool shouldSpin)
{
    if (spinning == shouldSpin)
        return;

    spinning = shouldSpin;

    // No callback at all while frozen — cheaper than a running timer that
    // just checks a flag and does nothing every tick.
    if (spinning)
        startTimerHz(30);
    else
        stopTimer();
}

void NFTapeReels::mouseDown(const juce::MouseEvent&)
{
    setSpinning(! spinning);

    if (onToggle)
        onToggle();
}

void NFTapeReels::timerCallback()
{
    rotationAngle += (mirroredFlange ? -1.0f : 1.0f) * 0.012f * speedMultiplier;
    if (rotationAngle > juce::MathConstants<float>::twoPi)
        rotationAngle -= juce::MathConstants<float>::twoPi;
    if (rotationAngle < 0.0f)
        rotationAngle += juce::MathConstants<float>::twoPi;

    repaint();
}

void NFTapeReels::renderBaseLayer(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    // Soft contact shadow under the whole spool — several translucent
    // passes fake a blur since JUCE has no native gaussian blur.
    for (int i = 5; i >= 1; --i)
    {
        const float grow = (float) i * 2.2f;
        g.setColour(juce::Colours::black.withAlpha(0.06f));
        g.fillEllipse(centre.x - radius * 0.95f - grow, centre.y - radius * 0.9f + radius * 0.06f - grow * 0.3f,
                      radius * 1.9f + grow * 2.0f, radius * 1.9f + grow * 2.0f);
    }

    const auto& finish = getReelFinish(tapeTypeIndex);

    // Outer machined rim (fixed, doesn't rotate) — finish depends on the
    // selected tape stock.
    const float rimOuter = radius * 0.98f;
    const float rimInner = radius * 0.91f;

    juce::ColourGradient rimGrad(
        finish.rimBright, centre.x - rimOuter * 0.3f, centre.y - rimOuter * 0.3f,
        finish.rimDark, centre.x + rimOuter * 0.4f, centre.y + rimOuter * 0.4f, true);
    g.setGradientFill(rimGrad);

    juce::Path rimRing;
    rimRing.addEllipse(centre.x - rimOuter, centre.y - rimOuter, rimOuter * 2.0f, rimOuter * 2.0f);
    rimRing.setUsingNonZeroWinding(false);
    rimRing.addEllipse(centre.x - rimInner, centre.y - rimInner, rimInner * 2.0f, rimInner * 2.0f);
    g.fillPath(rimRing);

    // Wound tape pack — a real reel-to-reel spool is mostly a dark mass of
    // oxide tape filling nearly the whole disc; the flange on top of it is
    // just a thin skeleton with big open windows the tape shows through.
    const float packOuter = rimInner;
    const float packInner = radius * 0.24f;

    {
        juce::ColourGradient packGrad(
            juce::Colour(0xff3a2a1c), centre.x - packOuter * 0.3f, centre.y - packOuter * 0.3f,
            juce::Colour(0xff160f0a), centre.x + packOuter * 0.4f, centre.y + packOuter * 0.4f, true);
        g.setGradientFill(packGrad);

        juce::Path pack;
        pack.addEllipse(centre.x - packOuter, centre.y - packOuter, packOuter * 2.0f, packOuter * 2.0f);
        pack.setUsingNonZeroWinding(false);
        pack.addEllipse(centre.x - packInner, centre.y - packInner, packInner * 2.0f, packInner * 2.0f);
        g.fillPath(pack);

        g.saveState();
        g.reduceClipRegion(pack);
        for (float r = packInner; r < packOuter; r += 2.2f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.24f));
            g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 0.7f);
        }
        // A few brighter wind-lines catch the light like real tape layers.
        for (float t : { 0.2f, 0.45f, 0.7f, 0.9f })
        {
            const float r = packInner + (packOuter - packInner) * t;
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
        }
        g.restoreState();

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(centre.x - packOuter, centre.y - packOuter, packOuter * 2.0f, packOuter * 2.0f, 1.0f);
    }
}

void NFTapeReels::renderFlangeLayer(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const auto& finish = getReelFinish(tapeTypeIndex);
    const float rimInner = radius * 0.91f;
    const float packOuter = rimInner;
    const float packInner = radius * 0.24f;

    // Bronze flange: four large open windows cut clean through it (evenodd
    // hole, not a dark wash) so the tape pack underneath shows through
    // directly, exactly like a real NAB reel. Rendered once at angle 0 —
    // paint() supplies the rotation as an image transform instead of this
    // being rebuilt from raw geometry every frame.
    const float flangeRadius = packOuter;
    constexpr int numWindows = 4;
    const float windowOuter = flangeRadius * 0.93f;
    const float windowInner = packInner * 1.7f;
    const float halfSpan = juce::MathConstants<float>::pi / (float) numWindows * 0.7f;

    juce::Path flange;
    flange.addEllipse(centre.x - flangeRadius, centre.y - flangeRadius, flangeRadius * 2.0f, flangeRadius * 2.0f);
    flange.setUsingNonZeroWinding(false);

    for (int i = 0; i < numWindows; ++i)
    {
        // The +pi/numWindows offset puts a solid SPOKE at 12 o'clock
        // (instead of a window there) so the engraved logo — which sits
        // at the top — always lands on metal, never on a tape window.
        const float baseAngle = juce::MathConstants<float>::twoPi * (float) i / (float) numWindows
                               - juce::MathConstants<float>::halfPi
                               + juce::MathConstants<float>::pi / (float) numWindows;
        flange.addPieSegment(centre.x - windowOuter, centre.y - windowOuter,
                             windowOuter * 2.0f, windowOuter * 2.0f,
                             baseAngle - halfSpan, baseAngle + halfSpan,
                             windowInner / windowOuter);
    }

    juce::ColourGradient flangeGrad(
        finish.flangeBright, centre.x - flangeRadius * 0.35f, centre.y - flangeRadius * 0.4f,
        finish.flangeDark, centre.x + flangeRadius * 0.5f, centre.y + flangeRadius * 0.55f, true);
    g.setGradientFill(flangeGrad);
    g.fillPath(flange);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.strokePath(flange, juce::PathStrokeType(1.0f));

    // Bevel highlight/shadow on each spoke edge catching the light.
    for (int i = 0; i < numWindows; ++i)
    {
        // Spoke centres are exactly where window centres used to be
        // before the +pi/numWindows shift above.
        const float spokeAngle = juce::MathConstants<float>::twoPi * (float) i / (float) numWindows
                                - juce::MathConstants<float>::halfPi;
        const auto p1 = centre + juce::Point<float>(std::cos(spokeAngle), std::sin(spokeAngle)) * windowInner;
        const auto p2 = centre + juce::Point<float>(std::cos(spokeAngle), std::sin(spokeAngle)) * windowOuter;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.4f);
        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.drawLine(p1.x + 1.1f, p1.y, p2.x + 1.1f, p2.y, 1.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawEllipse(centre.x - flangeRadius, centre.y - flangeRadius, flangeRadius * 2.0f, flangeRadius * 2.0f, 1.2f);

    // Ring of small rivets around the hub, like the real fasteners holding
    // a metal reel's two halves together — these spin with the flange.
    {
        constexpr int numRivets = 8;
        const float rivetRing = windowInner * 0.82f;
        const float rivetR = flangeRadius * 0.028f;

        for (int i = 0; i < numRivets; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) numRivets;
            const auto p = centre + juce::Point<float>(std::cos(a), std::sin(a)) * rivetRing;

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillEllipse(p.x - rivetR, p.y - rivetR, rivetR * 2.0f, rivetR * 2.0f);

            // Rivets stay plain steel regardless of the reel's finish —
            // real fasteners aren't anodised to match the tape stock.
            juce::ColourGradient rivetGrad(juce::Colour(0xffe8e6e0), p.x - rivetR * 0.4f, p.y - rivetR * 0.4f,
                                           juce::Colour(0xff5a5a5a), p.x + rivetR * 0.5f, p.y + rivetR * 0.5f, true);
            g.setGradientFill(rivetGrad);
            g.fillEllipse(p.x - rivetR * 0.75f, p.y - rivetR * 0.75f, rivetR * 1.5f, rivetR * 1.5f);
        }
    }

    // Brand mark engraved into the flange itself — spins along with it
    // (unlike the hub cap, this is part of the rotating metal, not a
    // fixed transport part), a dark recessed shadow plus a bright
    // highlight offset the other way reads as debossed metal.
    {
        const float markY = centre.y - radius * 0.56f;

        // Narrow enough to stay on the metal spoke at each row's radius
        // from centre — wider lower down, where the spoke is physically
        // wider (window cutouts flare outward), narrower right at the
        // top where the spoke is at its thinnest.
        const float nfW = radius * 0.26f;
        const float audioW = radius * 0.4f;

        auto drawEmbossed = [&g](const juce::String& text, juce::Rectangle<float> textArea, float fontSize, bool bold)
        {
            g.setFont(juce::Font(juce::FontOptions(fontSize, bold ? juce::Font::bold : juce::Font::plain)));

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawFittedText(text, textArea.translated(-0.6f, -0.6f).toNearestInt(), juce::Justification::centred, 1);

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawFittedText(text, textArea.translated(0.6f, 0.6f).toNearestInt(), juce::Justification::centred, 1);

            g.setColour(juce::Colour(0xff5a4322).withAlpha(0.85f));
            g.drawFittedText(text, textArea.toNearestInt(), juce::Justification::centred, 1);
        };

        drawEmbossed("NF", { centre.x - nfW * 0.5f, markY, nfW, radius * 0.2f }, radius * 0.13f, true);
        drawEmbossed("AUDIO TOOLS", { centre.x - audioW * 0.5f, markY + radius * 0.22f, audioW, radius * 0.1f }, radius * 0.05f, false);
    }
}

void NFTapeReels::renderOverlayLayer(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const float packOuter = radius * 0.91f;
    const float flangeRadius = packOuter;

    // Fixed specular sweep — a constant light source hitting a spinning
    // glossy disc puts the highlight at the same screen position no
    // matter how the flange is rotated underneath it.
    {
        juce::Path sweep;
        sweep.addPieSegment(centre.x - flangeRadius, centre.y - flangeRadius, flangeRadius * 2.0f, flangeRadius * 2.0f,
                            juce::MathConstants<float>::pi * 1.08f, juce::MathConstants<float>::pi * 1.42f, 0.15f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.fillPath(sweep);
    }

    // Fixed (non-rotating) centre hub — black moulded collet with
    // concentric grooves and a raised chrome dome, like the reel-lock
    // spindle adapter on a real NAB hub.
    const float hubRadius = radius * 0.22f;

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(centre.x - hubRadius - 1.5f, centre.y - hubRadius - 1.5f, (hubRadius + 1.5f) * 2.0f, (hubRadius + 1.5f) * 2.0f);

    juce::ColourGradient hubGrad(
        juce::Colour::fromRGB(46, 44, 42), centre.x - hubRadius * 0.3f, centre.y - hubRadius * 0.3f,
        juce::Colour::fromRGB(10, 10, 10), centre.x + hubRadius * 0.4f, centre.y + hubRadius * 0.4f, true);
    g.setGradientFill(hubGrad);
    g.fillEllipse(centre.x - hubRadius, centre.y - hubRadius, hubRadius * 2.0f, hubRadius * 2.0f);

    // Concentric groove rings moulded into the black collet.
    for (float t : { 0.62f, 0.78f })
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(centre.x - hubRadius * t, centre.y - hubRadius * t, hubRadius * t * 2.0f, hubRadius * t * 2.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawEllipse(centre.x - hubRadius * t + 0.6f, centre.y - hubRadius * t + 0.6f,
                      hubRadius * t * 2.0f, hubRadius * t * 2.0f, 0.8f);
    }

    // Raised chrome dome (the reel-lock collet).
    juce::ColourGradient domeGrad(
        juce::Colours::white, centre.x - hubRadius * 0.18f, centre.y - hubRadius * 0.22f,
        juce::Colour::fromRGB(90, 92, 96), centre.x + hubRadius * 0.3f, centre.y + hubRadius * 0.35f, true);
    g.setGradientFill(domeGrad);
    g.fillEllipse(centre.x - hubRadius * 0.46f, centre.y - hubRadius * 0.46f,
                 hubRadius * 0.92f, hubRadius * 0.92f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(centre.x - hubRadius * 0.46f, centre.y - hubRadius * 0.46f,
                 hubRadius * 0.92f, hubRadius * 0.92f, 0.8f);

    // Three small hub pins around the dome (fixed, don't spin).
    for (int i = 0; i < 3; ++i)
    {
        const float boltAngle = juce::MathConstants<float>::twoPi * (float) i / 3.0f + juce::MathConstants<float>::halfPi;
        const auto boltPos = centre + juce::Point<float>(std::cos(boltAngle), std::sin(boltAngle)) * hubRadius * 0.68f;

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(boltPos.x - 2.2f, boltPos.y - 2.2f, 4.4f, 4.4f);
        g.setColour(juce::Colour(0xffcfd0d2));
        g.fillEllipse(boltPos.x - 1.6f, boltPos.y - 1.6f, 3.2f, 3.2f);
    }

    // Outer chrome bezel ring framing the whole spool.
    g.setColour(juce::Colour(0xff45484c));
    g.drawEllipse(centre.x - radius * 0.97f, centre.y - radius * 0.97f, radius * 1.94f, radius * 1.94f, 1.6f);
}

void NFTapeReels::rebuildCacheIfNeeded()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    if (! cachedBase.isNull() && bounds == cachedForBounds && tapeTypeIndex == cachedForTapeType)
        return;

    cachedForBounds = bounds;
    cachedForTapeType = tapeTypeIndex;

    const auto boundsF = bounds.toFloat();
    const auto centre = boundsF.getCentre();
    const float radius = juce::jmin(boundsF.getWidth(), boundsF.getHeight()) * 0.5f;

    cachedBase = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    cachedFlange = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    cachedOverlay = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);

    {
        juce::Graphics g(cachedBase);
        renderBaseLayer(g, centre, radius);
    }
    {
        juce::Graphics g(cachedFlange);
        renderFlangeLayer(g, centre, radius);
    }
    {
        juce::Graphics g(cachedOverlay);
        renderOverlayLayer(g, centre, radius);
    }
}

void NFTapeReels::paint(juce::Graphics& g)
{
    rebuildCacheIfNeeded();
    if (cachedBase.isNull())
        return;

    const auto centre = getLocalBounds().toFloat().getCentre();

    g.drawImageAt(cachedBase, 0, 0);
    g.drawImageTransformed(cachedFlange, juce::AffineTransform::rotation(rotationAngle, centre.x, centre.y), false);
    g.drawImageAt(cachedOverlay, 0, 0);
}
