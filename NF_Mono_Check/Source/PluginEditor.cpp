#include "PluginEditor.h"

namespace
{
    // Single source of truth for level-meter dB -> vertical-pixel mapping,
    // shared by the meter bars and their numeric labels so both always agree.
    float dbToMeterY (float db, const juce::Rectangle<float>& meterBounds,
                      float minDb = -60.0f, float maxDb = 0.0f)
    {
        const float clampedDb  = juce::jlimit (minDb, maxDb, db);
        const float normalized = juce::jmap (clampedDb, minDb, maxDb, 0.0f, 1.0f);

        return meterBounds.getBottom() - normalized * meterBounds.getHeight();
    }
}

NFMonoCheckAudioProcessorEditor::NFMonoCheckAudioProcessorEditor (NFMonoCheckAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    resizeConstrainer.setSizeLimits (500, 333, 1800, 1200);
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &resizeConstrainer);
    addAndMakeVisible (resizer.get());

    setResizable (true, true);
    setResizeLimits (500, 333, 1800, 1200);

    setSize (defaultWidth, defaultHeight);

    for (auto* button : { &leftButton, &monoButton, &rightButton })
    {
        addAndMakeVisible (*button);

        button->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    }

    addAndMakeVisible (aboutButton);

    aboutButton.setButtonText ("ABOUT");
    aboutButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff101216));
    aboutButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffdddddd));

    leftButton.onClick  = [this] { selectMode (0); };
    rightButton.onClick = [this] { selectMode (2); };

    // Double-clicking L or R (isolated channel) drops back to unprocessed Stereo.
    leftButton.addMouseListener (this, false);
    rightButton.addMouseListener (this, false);

    // M toggles: clicking it while Mono is already active drops back to
    // unprocessed Stereo (both L and R light up); clicking again re-engages Mono.
    monoButton.onClick = [this]
    {
        const int currentMode = static_cast<int> (processor.apvts.getRawParameterValue ("mode")->load());
        selectMode (currentMode == 1 ? 3 : 1);
    };

    aboutButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "NF Mono Check",
            "NF Mono Check v1.0.0\n\n"
            "NF Audio Tools\n\n"
            "Stereo / Mono / Channel monitoring utility.");
    };

    startTimerHz (30);
}

void NFMonoCheckAudioProcessorEditor::selectMode (int mode)
{
    auto* parameter = processor.apvts.getParameter ("mode");

    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (mode)));
    parameter->endChangeGesture();

    repaint();
}

void NFMonoCheckAudioProcessorEditor::timerCallback()
{
    repaint();
}

void NFMonoCheckAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Double-clicking an isolated channel (L or R) drops back to unprocessed Stereo.
    if (event.eventComponent == &leftButton || event.eventComponent == &rightButton)
    {
        selectMode (3);
        return;
    }

    // Use the fixed 1500x1000 design canvas (same one paint() scales from) to
    // invert the click position -- NOT defaultWidth/defaultHeight, which is
    // only the window's initial/reset *size* and would misalign the hit test.
    const float sx = getWidth()  / 1500.0f;
    const float sy = getHeight() / 1000.0f;

    // Clickable zone around the NF roundel in the top-left corner.
    const juce::Rectangle<float> logoBounds (50.0f, 35.0f, 140.0f, 115.0f);
    const juce::Point<float> local (event.position.x / sx, event.position.y / sy);

    if (logoBounds.contains (local))
        setSize (defaultWidth, defaultHeight);
}

void NFMonoCheckAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float sx = getWidth() / 1500.0f;
    const float sy = getHeight() / 1000.0f;

    g.addTransform (juce::AffineTransform::scale (sx, sy));

    juce::ColourGradient background (
        juce::Colour (0xff171b20), 750.0f, 0.0f,
        juce::Colour (0xff080b0e), 750.0f, 1000.0f,
        false);

    g.setGradientFill (background);
    g.fillRoundedRectangle (20.0f, 20.0f, 1460.0f, 960.0f, 28.0f);

    g.setColour (juce::Colour (0xff555a60));
    g.drawRoundedRectangle (20.0f, 20.0f, 1460.0f, 960.0f, 28.0f, 2.0f);

    g.setColour (juce::Colour (0xff55585d));
    g.drawLine (20.0f, 170.0f, 1480.0f, 170.0f, 1.0f);

    g.setColour (juce::Colour (0xff777b80));
    g.drawEllipse (75.0f, 60.0f, 80.0f, 80.0f, 3.0f);

    g.setFont (juce::FontOptions (50.0f).withStyle ("Bold"));
    g.drawText ("NF", 75, 70, 80, 60, juce::Justification::centred);

    g.setColour (juce::Colour (0xff777b80));
    g.drawLine (190.0f, 65.0f, 190.0f, 135.0f, 1.0f);

    // Centre "MONO CHECK" as one unit in the space between the logo divider
    // and the ABOUT button, using measured glyph widths rather than fixed boxes.
    g.setFont (juce::FontOptions (48.0f).withStyle ("Bold"));
    const juce::Font titleFont = g.getCurrentFont();

    const juce::String monoText  = "MONO";
    const juce::String checkText = "CHECK";
    constexpr float wordGap = 18.0f;

    const float monoWidth  = juce::GlyphArrangement::getStringWidth (titleFont, monoText);
    const float checkWidth = juce::GlyphArrangement::getStringWidth (titleFont, checkText);
    const float totalWidth = monoWidth + wordGap + checkWidth;

    // Aligned to the centre of the M button (635 + 240/2), so the title sits
    // exactly above the middle of the three mode buttons.
    constexpr float titleCentreX = 635.0f + 240.0f * 0.5f;

    const float titleStartX = titleCentreX - totalWidth * 0.5f;

    g.setColour (juce::Colour (0xfff0f0f2));
    g.drawText (monoText, juce::Rectangle<float> (titleStartX, 55.0f, monoWidth, 60.0f), juce::Justification::centred);

    g.setColour (juce::Colour (0xff55aaff));
    g.drawText (checkText, juce::Rectangle<float> (titleStartX + monoWidth + wordGap, 55.0f, checkWidth, 60.0f), juce::Justification::centred);

    g.setFont (juce::FontOptions (19.0f));
    g.setColour (juce::Colour (0xff888d94));
    g.drawText (juce::String::fromUTF8 ("\xe2\x80\xa2   C H E C K   Y O U R   M I X   \xe2\x80\xa2"),
                juce::Rectangle<float> (titleCentreX - 250.0f, 120.0f, 500.0f, 30.0f), juce::Justification::centred);

    g.setColour (juce::Colour (0x552daaff));
    g.fillEllipse (1400.0f, 85.0f, 35.0f, 35.0f);

    g.setColour (juce::Colour (0xff31b9ff));
    g.fillEllipse (1408.0f, 93.0f, 19.0f, 19.0f);

    const int mode = static_cast<int> (processor.apvts.getRawParameterValue ("mode")->load());

    // Stereo (M deselected) lights up L and R together to show unprocessed passthrough.
    drawModeButton (g, { 330.0f, 220.0f, 230.0f, 210.0f }, "L", "LEFT",  mode == 0 || mode == 3);
    drawModeButton (g, { 635.0f, 210.0f, 240.0f, 225.0f }, "M", "MONO",  mode == 1);
    drawModeButton (g, { 960.0f, 220.0f, 230.0f, 210.0f }, "R", "RIGHT", mode == 2 || mode == 3);

    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.setColour (juce::Colour (0xff35aaff));

    juce::String modeTitle;

    if (mode == 0)       modeTitle = "LEFT MODE";
    else if (mode == 1)  modeTitle = "MONO MODE";
    else if (mode == 2)  modeTitle = "RIGHT MODE";
    else                 modeTitle = "STEREO MODE";

    g.drawText (modeTitle, 550, 480, 400, 30, juce::Justification::centred);

    // drawLevelMeter always anchors the 45px-wide segment column at bounds.getX(),
    // so for a true mirror about the panel's centre (x=750) the right meter's
    // segment must start where the left one's *ends* up, reflected: the left
    // segment spans [180, 225]; mirrored that's [1500-225, 1500-180] = [1275, 1320].
    drawLevelMeter (g, { 180.0f, 340.0f, 80.0f, 430.0f }, processor.getLeftLevel(), true);
    drawLevelMeter (g, { 1275.0f, 340.0f, 80.0f, 430.0f }, processor.getRightLevel(), false);

    // Labels use the actual 45px bar width (not the 80px padded container
    // above) so the gap from bar edge to number matches on both sides.
    constexpr float meterBarWidth = 45.0f;
    const juce::Rectangle<float> leftMeterBounds  { 180.0f,  340.0f, meterBarWidth, 430.0f };
    const juce::Rectangle<float> rightMeterBounds { 1275.0f, 340.0f, meterBarWidth, 430.0f };

    drawMeterLabels (g, leftMeterBounds, true);
    drawMeterLabels (g, rightMeterBounds, false);

    // Enlarged (300 -> 330) and nudged up so it doesn't crowd the correlation
    // bar below; centre stays on the same x=750 axis as the correlation "0".
    drawVectorscope (g, { 585.0f, 520.0f, 330.0f, 330.0f });

    // Correlation bar's 0 mark shares the vectorscope's centre x (600 + 300/2)
    // so it lines up with the vertical crosshair line directly above it.
    drawCorrelationMeter (g, { 490.0f, 885.0f, 520.0f, 55.0f }, processor.getCorrelation());

    g.setFont (juce::FontOptions (25.0f).withStyle ("Bold"));
    g.setColour (juce::Colour (0xff248cff));
    g.drawText ("NF", 70, 915, 50, 35, juce::Justification::left);

    g.setColour (juce::Colour (0xffdddddf));
    g.setFont (juce::FontOptions (19.0f));
    g.drawText ("A U D I O   T O O L S", 115, 915, 260, 35, juce::Justification::left);

    g.setColour (juce::Colour (0xff8e9298));
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("v1.0.0", 1330, 915, 90, 35, juce::Justification::right);
}

void NFMonoCheckAudioProcessorEditor::drawModeButton (juce::Graphics& g,
                                                       juce::Rectangle<float> bounds,
                                                       const juce::String& largeText,
                                                       const juce::String& smallText,
                                                       bool active)
{
    const auto fullBounds = bounds; // keep the un-mutated rect for the indicator light below

    g.setColour (juce::Colour (0x99000000));
    g.fillRoundedRectangle (bounds.translated (0.0f, 8.0f), 25.0f);

    juce::ColourGradient gradient (
        juce::Colour (0xff24282d), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff090c0f), bounds.getCentreX(), bounds.getBottom(),
        false);

    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 25.0f);

    // Bevelled edge: the border is lit from above (lighter at the top, darker
    // at the bottom) instead of a flat single colour, so the button reads as
    // a raised hardware key rather than a flat rectangle.
    if (active)
    {
        // Neon-tube look: a thin bright core line with a strong soft bloom
        // built from several wider, fainter strokes stacked outward.
        struct GlowLayer { float expand, alpha; };
        constexpr GlowLayer glowLayers[] = {
            { 2.0f,  0.30f }, { 4.0f,  0.22f }, { 6.0f,  0.16f }, { 8.0f,  0.11f },
            { 10.0f, 0.075f }, { 13.0f, 0.05f }, { 16.0f, 0.03f }, { 20.0f, 0.018f }
        };

        for (const auto& layer : glowLayers)
        {
            g.setColour (juce::Colour (0xff33aaff).withAlpha (layer.alpha));
            g.drawRoundedRectangle (bounds.expanded (layer.expand), 25.0f + layer.expand, 3.5f);
        }

        g.setColour (juce::Colour (0xff9fe6ff));
        g.drawRoundedRectangle (bounds, 25.0f, 1.5f);
    }
    else
    {
        juce::ColourGradient borderGradient (
            juce::Colour (0xff868b92), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff2e3136), bounds.getCentreX(), bounds.getBottom(),
            false);

        g.setGradientFill (borderGradient);
        g.drawRoundedRectangle (bounds, 25.0f, 2.0f);
    }

    // Crisp "catch light" along just the top edge -- clip to the upper slice
    // of the button so only the top of the rounded-rect stroke is visible,
    // like light falling on the button from directly above.
    {
        juce::Graphics::ScopedSaveState save (g);

        g.reduceClipRegion (juce::Rectangle<int> (
            static_cast<int> (bounds.getX() - 10.0f),
            static_cast<int> (bounds.getY() - 4.0f),
            static_cast<int> (bounds.getWidth() + 20.0f),
            static_cast<int> (bounds.getHeight() * 0.3f)));

        g.setColour (juce::Colours::white.withAlpha (active ? 0.55f : 0.30f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 25.0f, 1.5f);
    }

    g.setFont (juce::FontOptions (70.0f).withStyle ("Bold"));
    g.setColour (active ? juce::Colour (0xff36bfff) : juce::Colour (0xffeeeeef));
    g.drawText (largeText, bounds.removeFromTop (135.0f), juce::Justification::centredBottom);

    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.setColour (active ? juce::Colour (0xff39aaff) : juce::Colour (0xffeeeeef));
    g.drawText (smallText, bounds, juce::Justification::centredTop);

    // Small indicator light under the button, lit blue when this mode is active.
    constexpr float barWidth  = 70.0f;
    constexpr float barHeight = 6.0f;
    constexpr float barGap    = 18.0f;

    juce::Rectangle<float> bar (fullBounds.getCentreX() - barWidth * 0.5f,
                                fullBounds.getBottom() + barGap,
                                barWidth, barHeight);

    if (active)
    {
        g.setColour (juce::Colour (0x402f9cff));
        g.fillRoundedRectangle (bar.expanded (10.0f, 6.0f), barHeight);

        g.setColour (juce::Colour (0x8033aaff));
        g.fillRoundedRectangle (bar.expanded (4.0f, 2.0f), barHeight);

        g.setColour (juce::Colour (0xff9fe6ff));
        g.fillRoundedRectangle (bar, barHeight * 0.5f);
    }
    else
    {
        g.setColour (juce::Colour (0xffd6d8db));
        g.fillRoundedRectangle (bar, barHeight * 0.5f);
    }
}

void NFMonoCheckAudioProcessorEditor::drawLevelMeter (juce::Graphics& g,
                                                      juce::Rectangle<float> bounds,
                                                      float level,
                                                      bool leftSide)
{
    constexpr int segments = 20;
    constexpr float segmentWidth = 45.0f;

    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float normalized = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const int activeSegments = juce::roundToInt (normalized * segments);

    const float segmentHeight = bounds.getHeight() / static_cast<float> (segments);

    for (int i = 0; i < segments; ++i)
    {
        const bool active = i >= (segments - activeSegments);
        const float y = bounds.getY() + i * segmentHeight;

        juce::Rectangle<float> segment (bounds.getX(), y + 2.0f, segmentWidth, segmentHeight - 4.0f);

        if (active)
        {
            const float position = static_cast<float> (i) / static_cast<float> (segments);

            g.setColour (juce::Colour::fromHSV (0.55f, 0.75f, 0.95f, 1.0f)
                             .interpolatedWith (juce::Colour (0xff44ddff), position));
        }
        else
        {
            g.setColour (juce::Colour (0xff11161b));
        }

        g.fillRoundedRectangle (segment, 2.0f);

        g.setColour (juce::Colour (0xff050709));
        g.drawRoundedRectangle (segment, 2.0f, 1.0f);
    }

    g.setColour (juce::Colour (0xff258eff));
    g.setFont (juce::FontOptions (25.0f).withStyle ("Bold"));
    g.drawText (leftSide ? "L" : "R", bounds.getX() - 10.0f, bounds.getBottom() + 15.0f, 70.0f, 40.0f, juce::Justification::centred);
}

void NFMonoCheckAudioProcessorEditor::drawMeterLabels (juce::Graphics& g,
                                                       juce::Rectangle<float> meterBounds,
                                                       bool labelsOnLeft)
{
    static constexpr std::array<float, 10> labelValues
    {
         0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f, -42.0f, -48.0f, -60.0f
    };

    constexpr float labelGap    = 7.0f;
    constexpr float labelWidth  = 28.0f;
    constexpr float labelHeight = 14.0f;

    // Preserve the meter numbers' existing font (monospaced, so every
    // digit/hyphen has identical advance width) and colour.
    g.setFont (juce::FontOptions (13.0f).withName (juce::Font::getDefaultMonospacedFontName()));
    g.setColour (juce::Colour (0xff9da1a7));

    const float labelX = labelsOnLeft
        ? meterBounds.getX() - labelGap - labelWidth
        : meterBounds.getRight() + labelGap;

    const auto justification = labelsOnLeft
        ? juce::Justification::centredRight
        : juce::Justification::centredLeft;

    for (const float db : labelValues)
    {
        // Pixel-snapped so labels stay crisp and on-grid at any window size.
        const float yCenter = std::round (dbToMeterY (db, meterBounds));

        const juce::Rectangle<float> textBounds (
            labelX, yCenter - labelHeight * 0.5f, labelWidth, labelHeight);

        g.drawText (juce::String (juce::roundToInt (db)), textBounds, justification, false);
    }
}

void NFMonoCheckAudioProcessorEditor::drawVectorscope (juce::Graphics& g,
                                                       juce::Rectangle<float> bounds)
{
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.46f;

    g.setColour (juce::Colour (0xff5b6066));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    g.setColour (juce::Colour (0xff30363d));
    g.drawEllipse (centre.x - radius * 0.65f, centre.y - radius * 0.65f, radius * 1.3f, radius * 1.3f, 1.0f);

    g.setColour (juce::Colour (0xff777d83));
    g.drawLine (centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);
    g.drawLine (centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);

    g.setColour (juce::Colour (0xff2f98ff));
    g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));
    g.drawText ("L", centre.x - radius - 45.0f, centre.y - 15.0f, 30.0f, 30.0f, juce::Justification::centred);
    g.drawText ("R", centre.x + radius + 15.0f, centre.y - 15.0f, 30.0f, 30.0f, juce::Justification::centred);

    // Real goniometer: rotated Lissajous of the original stereo samples.
    // Fully correlated (mono) content collapses to a vertical line; phase
    // cancellation / stereo width spreads it horizontally.
    const int numPoints = processor.getScopeSnapshot (scopeSnapshotL.data(),
                                                       scopeSnapshotR.data(),
                                                       static_cast<int> (scopeSnapshotL.size()));

    constexpr float invSqrt2 = 0.70710678f;
    const float scale = radius * 0.95f;

    g.setColour (juce::Colour (0x5533aaff));

    for (int i = 0; i < numPoints; ++i)
    {
        const float l = scopeSnapshotL[static_cast<size_t> (i)];
        const float r = scopeSnapshotR[static_cast<size_t> (i)];

        const float side = (r - l) * invSqrt2;
        const float mid  = (l + r) * invSqrt2;

        const float x = centre.x + juce::jlimit (-1.0f, 1.0f, side) * scale;
        const float y = centre.y - juce::jlimit (-1.0f, 1.0f, mid) * scale;

        g.fillEllipse (x, y, 1.5f, 1.5f);
    }

    g.setColour (juce::Colour (0x6633bbff));
    g.fillEllipse (centre.x - 14.0f, centre.y - 14.0f, 28.0f, 28.0f);

    g.setColour (juce::Colour (0xff55ccff));
    g.fillEllipse (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);
}

void NFMonoCheckAudioProcessorEditor::drawCorrelationMeter (juce::Graphics& g,
                                                            juce::Rectangle<float> bounds,
                                                            float correlationValue)
{
    g.setColour (juce::Colour (0xffdddddd));
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("C O R R E L A T I O N", bounds.getX(), bounds.getY() - 30.0f, bounds.getWidth(), 25.0f, juce::Justification::centred);

    auto bar = bounds.withHeight (18.0f);

    juce::ColourGradient gradient (
        juce::Colour (0xffdd2037), bar.getX(), bar.getCentreY(),
        juce::Colour (0xff176bd1), bar.getRight(), bar.getCentreY(),
        false);

    gradient.addColour (0.5, juce::Colour (0xff22262b));

    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bar, 3.0f);

    g.setColour (juce::Colour (0xff050607));
    g.drawRoundedRectangle (bar, 3.0f, 1.0f);

    const float normalized = (juce::jlimit (-1.0f, 1.0f, correlationValue) + 1.0f) * 0.5f;
    const float x = bar.getX() + normalized * bar.getWidth();

    g.setColour (juce::Colours::white);
    g.fillRect (x - 2.0f, bar.getY() - 3.0f, 4.0f, bar.getHeight() + 6.0f);

    g.setFont (juce::FontOptions (14.0f));
    g.setColour (juce::Colour (0xffbbbbbb));

    g.drawText ("-1", bar.getX() - 10.0f, bar.getBottom() + 4.0f, 30.0f, 20.0f, juce::Justification::left);
    g.drawText ("0", bar.getCentreX() - 15.0f, bar.getBottom() + 4.0f, 30.0f, 20.0f, juce::Justification::centred);
    g.drawText ("+1", bar.getRight() - 20.0f, bar.getBottom() + 4.0f, 30.0f, 20.0f, juce::Justification::right);
}

void NFMonoCheckAudioProcessorEditor::resized()
{
    const float sx = getWidth() / 1500.0f;
    const float sy = getHeight() / 1000.0f;

    auto scaleRect = [sx, sy] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (
            juce::roundToInt (x * sx),
            juce::roundToInt (y * sy),
            juce::roundToInt (w * sx),
            juce::roundToInt (h * sy));
    };

    leftButton.setBounds  (scaleRect (330, 220, 230, 210));
    monoButton.setBounds  (scaleRect (635, 210, 240, 225));
    rightButton.setBounds (scaleRect (960, 220, 230, 210));
    aboutButton.setBounds (scaleRect (1190, 70, 150, 60));

    constexpr int handleSize = 18;
    resizer->setBounds (getWidth() - handleSize, getHeight() - handleSize, handleSize, handleSize);
}
