#include "NFVUComponent.h"
#include "NFTapeLookAndFeel.h"

namespace
{
    void drawMeterScrew(juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        juce::ColourGradient grad(juce::Colour(0xffc4c0b6), centre.x - radius * 0.3f, centre.y - radius * 0.3f,
                                  juce::Colour(0xff4a4842), centre.x + radius * 0.4f, centre.y + radius * 0.4f, true);
        g.setGradientFill(grad);
        g.fillEllipse(centre.x - radius * 0.85f, centre.y - radius * 0.85f, radius * 1.7f, radius * 1.7f);

        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawLine(centre.x - radius * 0.55f, centre.y, centre.x + radius * 0.55f, centre.y, radius * 0.26f);
        g.drawLine(centre.x, centre.y - radius * 0.55f, centre.x, centre.y + radius * 0.55f, radius * 0.26f);

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawLine(centre.x - radius * 0.5f, centre.y - radius * 0.5f, centre.x - radius * 0.15f, centre.y - radius * 0.15f, radius * 0.18f);
    }
}

const juce::Image& NFVUComponent::getPaperTexture()
{
    static const juce::Image texture = []
    {
        juce::Image img(juce::Image::ARGB, 200, 200, true);
        juce::Graphics tg(img);
        juce::Random rng(31415);

        for (int i = 0; i < 900; ++i)
        {
            const float x = rng.nextFloat() * 200.0f;
            const float y = rng.nextFloat() * 200.0f;
            const float r = 0.4f + rng.nextFloat() * 1.1f;
            const bool light = rng.nextFloat() > 0.5f;
            tg.setColour((light ? juce::Colours::white : juce::Colours::black).withAlpha(rng.nextFloat() * 0.05f));
            tg.fillEllipse(x, y, r, r);
        }

        for (int y = 0; y < 200; y += 2)
        {
            tg.setColour(juce::Colours::black.withAlpha(rng.nextFloat() * 0.02f));
            tg.drawLine(0.0f, (float) y, 200.0f, (float) y, 1.0f);
        }

        return img;
    }();

    return texture;
}

NFVUComponent::NFVUComponent()
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

NFVUComponent::~NFVUComponent()
{
    stopTimer();
}

void NFVUComponent::setLevels(float leftDb, float rightDb)
{
    // Calibration: 0 VU reads at -18 dBFS (common broadcast/mix reference),
    // so normally-mixed program material sits near the middle of the scale
    // instead of pinned at the bottom.
    constexpr float calibrationOffset = 18.0f;
    targetLeftDb = juce::jlimit(minDb, maxDb, leftDb + calibrationOffset);
    targetRightDb = juce::jlimit(minDb, maxDb, rightDb + calibrationOffset);
}

void NFVUComponent::timerCallback()
{
    // Real VU ballistics are asymmetric: ~300ms to swing up to a peak but
    // ~1.2-1.8s to fall back down.
    constexpr float attackCoeff = 0.18f;
    constexpr float releaseCoeff = 0.025f;

    const float leftCoeff = targetLeftDb > smoothedLeftDb ? attackCoeff : releaseCoeff;
    const float rightCoeff = targetRightDb > smoothedRightDb ? attackCoeff : releaseCoeff;

    smoothedLeftDb += (targetLeftDb - smoothedLeftDb) * leftCoeff;
    smoothedRightDb += (targetRightDb - smoothedRightDb) * rightCoeff;
    repaint();
}

float NFVUComponent::dbToAngle(float db)
{
    db = juce::jlimit(minDb, maxDb, db);

    // Each of the numScalePoints scale marks sits at an evenly-spaced
    // angle by index; find which pair of marks db falls between and
    // interpolate the angle linearly within that segment. This keeps the
    // needle's motion consistent with the (non-linear) printed scale
    // instead of a single dB-proportional formula that bunches every
    // number except -20 onto one side of the dial.
    for (int i = 0; i < numScalePoints - 1; ++i)
    {
        if (db <= scaleDb[i + 1] || i == numScalePoints - 2)
        {
            const float segmentT = (db - scaleDb[i]) / (scaleDb[i + 1] - scaleDb[i]);
            const float angleStart = juce::jmap((float) i, 0.0f, (float) (numScalePoints - 1), -0.95f, 0.95f);
            const float angleEnd = juce::jmap((float) (i + 1), 0.0f, (float) (numScalePoints - 1), -0.95f, 0.95f);
            return juce::jmap(juce::jlimit(0.0f, 1.0f, segmentT), 0.0f, 1.0f, angleStart, angleEnd);
        }
    }

    return 0.95f;
}

void NFVUComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(bounds, 8.0f);

    auto inner = bounds.reduced(6.0f);
    const float gap = 10.0f;
    const float dialWidth = (inner.getWidth() - gap) * 0.5f;

    auto leftArea = inner.removeFromLeft(dialWidth);
    inner.removeFromLeft(gap);
    auto rightArea = inner;

    drawDial(g, leftArea, smoothedLeftDb, "LEFT");
    drawDial(g, rightArea, smoothedRightDb, "RIGHT");

    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.4f);
}

void NFVUComponent::drawDial(juce::Graphics& g, juce::Rectangle<float> area,
                             float smoothedDb, const juce::String& channelLabel)
{
    // Amber backlight glow behind the cream face.
    juce::ColourGradient glow(
        NFTapeColours::amber.withAlpha(0.35f), area.getCentreX(), area.getY(),
        juce::Colours::transparentBlack, area.getCentreX(), area.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRect(area);

    auto faceArea = area.reduced(3.0f, 4.0f);
    faceArea.removeFromBottom(faceArea.getHeight() * 0.16f);

    // Lighter, slightly less saturated analogue-paper face — more of a
    // warm cream than a deep yellow-amber — with a subtle procedural
    // grain so it doesn't read as a flat digital rectangle.
    juce::ColourGradient faceGradient(
        juce::Colour(0xfff4e2ba), faceArea.getCentreX(), faceArea.getY(),
        juce::Colour(0xffd9bc85), faceArea.getCentreX(), faceArea.getBottom(), false);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(faceArea, 3.0f);

    g.saveState();
    juce::Path faceClip;
    faceClip.addRoundedRectangle(faceArea, 3.0f);
    g.reduceClipRegion(faceClip);
    g.setTiledImageFill(getPaperTexture(), (int) faceArea.getX(), (int) faceArea.getY(), 0.5f);
    g.fillRect(faceArea);
    g.restoreState();

    // Pivot sits right at the bottom of the visible face (not below it —
    // that pushed the whole arc out of the component's clip region on a
    // shorter box and cut the scale/needle in half). needleLength is then
    // capped by BOTH the available height and the available half-width,
    // so the full +-0.95 rad sweep always lands inside the face no matter
    // how wide/short the dial ends up being.
    const auto pivot = juce::Point<float>(faceArea.getCentreX(), faceArea.getBottom() - faceArea.getHeight() * 0.03f);

    constexpr float maxSweepAngle = 0.95f;
    const float availableHeight = faceArea.getHeight() * 0.82f;
    const float availableHalfWidth = faceArea.getWidth() * 0.47f;
    const float needleLength = juce::jmin(availableHeight, availableHalfWidth / std::sin(maxSweepAngle));

    // Scale ticks and numbers — the same evenly-spaced-by-index table
    // dbToAngle() uses, so the printed marks and the needle always agree.
    for (float tickDb : scaleDb)
    {
        const float angle = dbToAngle(tickDb);
        const auto dir = juce::Point<float>(std::sin(angle), -std::cos(angle));

        const float outerR = needleLength * 0.86f;
        const float innerR = needleLength * (tickDb >= 0.0f ? 0.72f : 0.78f);

        const auto p1 = pivot + dir * innerR;
        const auto p2 = pivot + dir * outerR;

        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawLine(p1.x, p1.y, p2.x, p2.y, tickDb == 0.0f ? 2.0f : 1.3f);

        const auto textPos = pivot + dir * (outerR + 8.0f);
        juce::Rectangle<float> textArea(textPos.x - 12.0f, textPos.y - 6.0f, 24.0f, 12.0f);

        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawFittedText(tickDb == 0.0f ? "0" : juce::String((int) tickDb),
                         textArea.toNearestInt(), juce::Justification::centred, 1);
    }

    // Red over-zone arc.
    {
        juce::Path redZone;
        const float r = needleLength * 0.86f;
        redZone.addCentredArc(pivot.x, pivot.y, r, r,
                              0.0f, dbToAngle(0.0f), dbToAngle(maxDb), true);
        g.setColour(NFTapeColours::ledRed.withAlpha(0.55f));
        g.strokePath(redZone, juce::PathStrokeType(2.4f));
    }

    // Plain "VU" lettering printed on the face (no badge plate).
    {
        auto vuArea = juce::Rectangle<float>(0.0f, 0.0f, faceArea.getWidth() * 0.4f, faceArea.getHeight() * 0.2f)
                        .withCentre({ faceArea.getCentreX(), faceArea.getY() + faceArea.getHeight() * 0.50f });

        g.setColour(juce::Colour(0xff2a1f14));
        g.setFont(juce::Font(juce::FontOptions(vuArea.getHeight() * 0.85f, juce::Font::bold)));
        g.drawFittedText("VU", vuArea.toNearestInt(), juce::Justification::centred, 1);
    }

    // Needle.
    const float needleAngle = dbToAngle(smoothedDb);
    const auto needleDir = juce::Point<float>(std::sin(needleAngle), -std::cos(needleAngle));
    const auto needleTip = pivot + needleDir * (needleLength * 0.88f);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawLine(pivot.x, pivot.y, needleTip.x, needleTip.y + 1.2f, 2.4f);

    g.setColour(juce::Colour(0xff241a10));
    g.drawLine(pivot.x, pivot.y, needleTip.x, needleTip.y, 1.6f);

    // Metallic pivot with a centre screw.
    juce::ColourGradient pivotGrad(juce::Colour(0xffb8b6ae), pivot.x - 2.0f, pivot.y - 2.0f,
                                   juce::Colour(0xff2c2c2a), pivot.x + 2.0f, pivot.y + 2.0f, true);
    g.setGradientFill(pivotGrad);
    g.fillEllipse(pivot.x - 4.0f, pivot.y - 4.0f, 8.0f, 8.0f);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(pivot.x - 4.0f, pivot.y - 4.0f, 8.0f, 8.0f, 0.8f);

    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawLine(pivot.x - 1.8f, pivot.y, pivot.x + 1.8f, pivot.y, 0.9f);

    // Channel label below the face.
    juce::Rectangle<float> labelArea(area.getX(), faceArea.getBottom() + 1.0f, area.getWidth(), area.getHeight() * 0.14f);
    g.setColour(NFTapeColours::textDim);
    g.setFont(juce::Font(juce::FontOptions(labelArea.getHeight() * 0.7f, juce::Font::bold)));
    g.drawFittedText(channelLabel, labelArea.toNearestInt(), juce::Justification::centred, 1);

    // Glass reflection — a soft diagonal band clipped to the face, as if
    // a protective glass sits over the dial and catches stray light.
    {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(faceArea, 3.0f);
        g.reduceClipRegion(clip);

        juce::Path sweep;
        const float w = faceArea.getWidth();
        const float h = faceArea.getHeight();
        sweep.startNewSubPath(faceArea.getX() - w * 0.2f, faceArea.getY());
        sweep.lineTo(faceArea.getX() + w * 0.25f, faceArea.getY());
        sweep.lineTo(faceArea.getX() - w * 0.35f, faceArea.getBottom());
        sweep.lineTo(faceArea.getX() - w * 0.7f, faceArea.getBottom());
        sweep.closeSubPath();

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillPath(sweep);

        sweep.applyTransform(juce::AffineTransform::translation(w * 0.55f, 0.0f));
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillPath(sweep);

        juce::ignoreUnused(h);
        g.restoreState();
    }

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(faceArea, 3.0f, 1.2f);

    // Dark inner border ring on the housing, between the black bezel and
    // the face — reads as real depth rather than a single flat edge.
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(area.reduced(1.5f), 3.5f, 1.2f);

    // Corner mounting screws on this meter's own housing — each dial is a
    // separate panel-mount unit on real gear, so each gets its own four.
    const float screwR = juce::jmin(area.getWidth(), area.getHeight()) * 0.05f;
    const float inset = screwR * 2.0f;
    for (float sx : { area.getX() + inset, area.getRight() - inset })
        for (float sy : { area.getY() + inset, area.getBottom() - inset })
            drawMeterScrew(g, { sx, sy }, screwR);
}
