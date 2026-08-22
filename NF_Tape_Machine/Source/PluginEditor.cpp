#include "PluginEditor.h"

namespace
{
    void drawScrew(juce::Graphics& g, juce::Point<float> centre, float radius, bool dark = false)
    {
        // Recessed socket the screw head sits in.
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillEllipse(centre.x - radius * 1.3f, centre.y - radius * 1.3f + 1.0f, radius * 2.6f, radius * 2.6f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(centre.x - radius * 1.15f, centre.y - radius * 1.15f, radius * 2.3f, radius * 2.3f, 1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(centre.x - radius, centre.y - radius + 0.6f, radius * 2.0f, radius * 2.0f);

        // Black screws (framing the reels) get a dark gunmetal finish
        // instead of bright steel, to visually set them apart from the
        // rest of the chassis hardware.
        juce::ColourGradient grad = dark
            ? juce::ColourGradient(juce::Colour(0xff646260), centre.x - radius * 0.35f, centre.y - radius * 0.4f,
                                   juce::Colour(0xff1c1c1a), centre.x + radius * 0.45f, centre.y + radius * 0.5f, true)
            : juce::ColourGradient(juce::Colour(0xffeeece2), centre.x - radius * 0.35f, centre.y - radius * 0.4f,
                                   juce::Colour(0xff3e3e3a), centre.x + radius * 0.45f, centre.y + radius * 0.5f, true);
        g.setGradientFill(grad);
        g.fillEllipse(centre.x - radius * 0.86f, centre.y - radius * 0.86f, radius * 1.72f, radius * 1.72f);

        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawEllipse(centre.x - radius * 0.86f, centre.y - radius * 0.86f, radius * 1.72f, radius * 1.72f, radius * 0.12f);

        // Phillips-style cross slot, bevelled (dark groove + bright edge).
        const float armLen = radius * 0.62f;
        const float thickness = radius * 0.24f;
        for (float angleDeg : { 20.0f, 110.0f })
        {
            const float a = juce::degreesToRadians(angleDeg);
            const auto dir = juce::Point<float>(std::cos(a), std::sin(a));
            const auto p1 = centre - dir * armLen;
            const auto p2 = centre + dir * armLen;

            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawLine(p1.x, p1.y, p2.x, p2.y, thickness);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawLine(p1.x - dir.y * 0.6f, p1.y + dir.x * 0.6f, p2.x - dir.y * 0.6f, p2.y + dir.x * 0.6f, thickness * 0.35f);
        }

        // Small specular highlight catching the light off the head.
        g.setColour(juce::Colours::white.withAlpha(dark ? 0.18f : 0.35f));
        g.fillEllipse(centre.x - radius * 0.5f, centre.y - radius * 0.6f, radius * 0.32f, radius * 0.2f);
    }

    // driveAmount (0-1, from the DRIVE knob) makes the tube glow hotter —
    // brighter, more saturated, and taller — the harder the tape is driven,
    // like a real valve pulled harder into saturation.
    void drawTube(juce::Graphics& g, juce::Rectangle<float> area, float driveAmount)
    {
        driveAmount = juce::jlimit(0.0f, 1.0f, driveAmount);

        juce::ColourGradient glass(juce::Colour(0xff2a2620).withAlpha(0.9f), area.getCentreX(), area.getY(),
                                   juce::Colour(0xff141210).withAlpha(0.9f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill(glass);
        g.fillRoundedRectangle(area, 5.0f);

        auto glow = area.reduced(area.getWidth() * 0.22f, 0.0f);

        // Strong and vivid even at rest/disabled (never reads as "off"),
        // but with real headroom left so pushing DRIVE up still reads as
        // a dramatic, obvious change — taller, hotter, and haloed much
        // further out at full drive.
        const float glowHeightFrac = juce::jmap(driveAmount, 0.0f, 1.0f, 0.62f, 1.0f);
        const float litHeight = glow.getHeight() * glowHeightFrac;
        const juce::Rectangle<float> litGlow(glow.getX(), glow.getBottom() - litHeight, glow.getWidth(), litHeight);

        // Vivid amber at rest; as drive climbs toward saturation the glow
        // bleaches out toward white-hot, like a valve being driven hard.
        const juce::Colour restColour = NFTapeColours::amberBright;
        const juce::Colour hotColour = juce::Colours::white;
        const juce::Colour topColour = restColour.interpolatedWith(hotColour, driveAmount);

        // Soft outer bloom bleeding past the tube's own glass — grows both
        // brighter AND physically wider with drive, reading as actual
        // light being cast rather than a static painted rectangle.
        const float bloomExpand = juce::jmap(driveAmount, 0.0f, 1.0f, 3.0f, 14.0f);
        juce::ColourGradient bloom(topColour.withAlpha(juce::jmap(driveAmount, 0.0f, 1.0f, 0.4f, 0.9f)),
                                  area.getCentreX(), area.getBottom(),
                                  juce::Colours::transparentBlack, area.getCentreX(), area.getY(), false);
        g.setGradientFill(bloom);
        g.fillRoundedRectangle(area.expanded(bloomExpand, 0.0f), 8.0f);

        juce::ColourGradient amberGlow(topColour.withAlpha(juce::jmap(driveAmount, 0.0f, 1.0f, 0.85f, 1.0f)),
                                       litGlow.getCentreX(), litGlow.getBottom(),
                                       NFTapeColours::amber.withAlpha(0.08f),
                                       litGlow.getCentreX(), litGlow.getY(), false);
        g.setGradientFill(amberGlow);
        g.fillRoundedRectangle(litGlow, 4.0f);

        g.setColour(juce::Colours::white.withAlpha(juce::jmap(driveAmount, 0.0f, 1.0f, 0.2f, 0.5f)));
        g.fillRoundedRectangle(area.getX() + area.getWidth() * 0.16f, area.getY() + 2.0f,
                               area.getWidth() * 0.18f, area.getHeight() - 4.0f, 3.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(area, 5.0f, 1.2f);
    }

    // Cached procedural textures — generated once (they're deterministic,
    // seeded) and tiled with Graphics::setTiledImageFill rather than redrawn
    // stroke-by-stroke every repaint, so real per-pixel grain stays cheap.
    const juce::Image& getBrushedMetalTexture()
    {
        static const juce::Image texture = []
        {
            juce::Image img(juce::Image::ARGB, 256, 256, true);
            juce::Graphics tg(img);
            juce::Random rng(90210);

            for (int y = 0; y < 256; ++y)
            {
                const bool light = rng.nextFloat() > 0.5f;
                const float alpha = rng.nextFloat() * 0.05f;
                tg.setColour((light ? juce::Colours::white : juce::Colours::black).withAlpha(alpha));
                const float wobble = (rng.nextFloat() - 0.5f) * 1.5f;
                tg.drawLine(0.0f, (float) y, 256.0f, (float) y + wobble, 1.0f);
            }

            for (int i = 0; i < 700; ++i)
            {
                const float y = rng.nextFloat() * 256.0f;
                const float x = rng.nextFloat() * 256.0f;
                const float len = 4.0f + rng.nextFloat() * 26.0f;
                tg.setColour(juce::Colours::black.withAlpha(rng.nextFloat() * 0.05f));
                tg.drawLine(x, y, x + len, y, 1.0f);
            }

            return img;
        }();

        return texture;
    }

    const juce::Image& getWoodGrainTexture()
    {
        static const juce::Image texture = []
        {
            juce::Image img(juce::Image::ARGB, 64, 512, true);
            juce::Graphics tg(img);
            juce::Random rng(4477);

            for (int y = 0; y < 512; ++y)
            {
                const float wobble = std::sin((float) y * 0.045f) * 5.0f
                                   + std::sin((float) y * 0.11f + 1.3f) * 2.5f;
                const float alpha = 0.05f + rng.nextFloat() * 0.07f;
                tg.setColour(juce::Colours::black.withAlpha(alpha));
                tg.drawLine(-8.0f + wobble, (float) y, 72.0f + wobble, (float) y, 1.0f);
            }

            for (int i = 0; i < 5; ++i)
            {
                const float cx = rng.nextFloat() * 64.0f;
                const float cy = rng.nextFloat() * 512.0f;
                const float r = 3.0f + rng.nextFloat() * 5.0f;
                tg.setColour(juce::Colours::black.withAlpha(0.30f));
                tg.drawEllipse(cx - r, cy - r * 1.6f, r * 2.0f, r * 3.2f, 1.0f);
            }

            return img;
        }();

        return texture;
    }
}

void NFTapeMachineAudioProcessorEditor::BackgroundPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(NFTapeColours::bezelBlack);

    constexpr float woodWidth = 18.0f;

    // Wood side panels (outermost edges).
    auto leftWood = bounds.removeFromLeft(woodWidth);
    auto rightWood = bounds.removeFromRight(woodWidth);

    for (auto* woodArea : { &leftWood, &rightWood })
    {
        juce::ColourGradient woodGrad(NFTapeColours::woodLight, woodArea->getX(), woodArea->getY(),
                                      NFTapeColours::woodDark, woodArea->getRight(), woodArea->getBottom(), false);
        g.setGradientFill(woodGrad);
        g.fillRect(*woodArea);

        g.setTiledImageFill(getWoodGrainTexture(), (int) woodArea->getX(), (int) woodArea->getY(), 0.55f);
        g.fillRect(*woodArea);

        // Bevelled edge where the wood meets the metal chassis.
        const bool isLeft = woodArea->getX() < bounds.getCentreX();
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawLine(isLeft ? woodArea->getRight() : woodArea->getX(), woodArea->getY(),
                  isLeft ? woodArea->getRight() : woodArea->getX(), woodArea->getBottom(), 1.5f);
        g.setColour(NFTapeColours::brassBright.withAlpha(0.18f));
        g.drawLine(isLeft ? woodArea->getX() + 1.0f : woodArea->getX(), woodArea->getY(),
                  isLeft ? woodArea->getX() + 1.0f : woodArea->getX(), woodArea->getBottom(), 1.0f);
    }

    // Main brushed-metal chassis.
    juce::ColourGradient chassisGrad(NFTapeColours::chassisLight, bounds.getCentreX(), bounds.getY(),
                                     NFTapeColours::chassisDark, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillRect(bounds);

    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

    g.setTiledImageFill(getBrushedMetalTexture(), (int) bounds.getX(), (int) bounds.getY(), 0.9f);
    g.fillRect(bounds);

    // Vignette: subtle radial darkening toward the corners gives the flat
    // vector fill some real depth instead of reading as a solid colour.
    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                  juce::Colours::black.withAlpha(0.38f), bounds.getX(), bounds.getY(), true);
    vignette.isRadial = true;
    vignette.addColour(0.72, juce::Colours::transparentBlack);
    g.setGradientFill(vignette);
    g.fillRect(bounds);

    // Soft light spill from the top edge, as if the unit is lit from above.
    juce::ColourGradient topSheen(juce::Colours::white.withAlpha(0.05f), bounds.getCentreX(), bounds.getY(),
                                  juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.35f, false);
    g.setGradientFill(topSheen);
    g.fillRect(bounds);

    g.restoreState();

    g.setColour(juce::Colours::black);
    g.drawRect(bounds, 2.0f);

    // Bright bevel along the chassis' top edge, dark along the bottom —
    // reinforces the sense of a machined metal slab with real thickness.
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(bounds.getX() + 2.0f, bounds.getY() + 1.5f, bounds.getRight() - 2.0f, bounds.getY() + 1.5f, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawLine(bounds.getX() + 2.0f, bounds.getBottom() - 1.5f, bounds.getRight() - 2.0f, bounds.getBottom() - 1.5f, 1.2f);

    // Decorative flanking lines either side of "ANALOG TAPE EMULATOR",
    // each capped with a small diamond, framing the subtitle like a
    // hardware badge rather than plain floating text.
    {
        const float midY = 74.0f;
        const float centreX = 768.0f;
        const float gap = 128.0f;
        const float lineLen = 145.0f;

        for (float dir : { -1.0f, 1.0f })
        {
            const float x1 = centreX + dir * gap;
            const float x2 = centreX + dir * (gap + lineLen);

            const auto lineColour = NFTapeColours::amber.brighter(0.35f);
            juce::ColourGradient lineGrad(lineColour.withAlpha(0.75f), x1, midY,
                                          juce::Colours::transparentBlack, x2, midY, false);
            g.setGradientFill(lineGrad);
            g.drawLine(x1, midY, x2, midY, 1.2f);

            juce::Path diamond;
            diamond.addQuadrilateral(x1 - dir * 3.0f, midY, x1, midY - 3.0f, x1 + dir * 3.0f, midY, x1, midY + 3.0f);
            g.setColour(lineColour.withAlpha(0.8f));
            g.fillPath(diamond);
        }
    }

    // Divider lines between the top bar / reel-VU stage / control rows.
    const float dividerY[] = { 112.0f, 562.0f, 832.0f };
    for (float y : dividerY)
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawLine(bounds.getX() + 10.0f, y, bounds.getRight() - 10.0f, y, 1.4f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawLine(bounds.getX() + 10.0f, y + 1.5f, bounds.getRight() - 10.0f, y + 1.5f, 1.0f);
    }

    // Corner + mid-span screws.
    const float screwR = 10.5f;
    // The four screws framing the reels (2 top, 2 bottom) are black, to
    // set them apart from the rest of the chassis hardware.
    drawScrew(g, { 46.0f, 40.0f }, screwR, true);
    drawScrew(g, { (float) getWidth() - 46.0f, 40.0f }, screwR, true);
    drawScrew(g, { 46.0f, (float) getHeight() - 30.0f }, screwR, true);
    drawScrew(g, { (float) getWidth() - 46.0f, (float) getHeight() - 30.0f }, screwR, true);
    drawScrew(g, { 46.0f, 585.0f }, screwR);
    drawScrew(g, { (float) getWidth() - 46.0f, 585.0f }, screwR);

    // Vacuum-tube lamps flanking the reel stage.
    drawTube(g, { 55.0f, 284.0f, 45.0f, 154.0f }, getTubeDriveAmount());
    drawTube(g, { 1434.0f, 284.0f, 45.0f, 155.0f }, getTubeDriveAmount());

    // Tape path: a recessed guide channel threading from the left reel,
    // past the head block, to the right reel — no roller wheels, just a
    // machined groove in the chassis.
    {
        juce::Path belt;
        belt.startNewSubPath(167.0f, 508.0f);
        belt.lineTo(310.0f, 486.0f);
        belt.lineTo(445.0f, 484.0f);
        belt.lineTo(552.0f, 464.0f);
        belt.lineTo(612.5f, 477.5f);
        belt.lineTo(940.5f, 477.5f);
        belt.lineTo(1010.0f, 464.0f);
        belt.lineTo(1098.0f, 484.0f);
        belt.lineTo(1232.0f, 486.0f);
        belt.lineTo(1348.0f, 508.0f);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.strokePath(belt, juce::PathStrokeType(3.4f));
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.strokePath(belt, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Head block: the dark plate the tape rides across between the two
    // head-flanking rollers, with a few playback/erase head slits.
    {
        juce::Rectangle<float> headBlock(625.0f, 456.0f, 284.0f, 86.0f);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(headBlock.expanded(2.0f), 8.0f);

        juce::ColourGradient headGrad(
            juce::Colour(0xff34302c), headBlock.getX(), headBlock.getY(),
            juce::Colour(0xff17140f), headBlock.getX(), headBlock.getBottom(), false);
        g.setGradientFill(headGrad);
        g.fillRoundedRectangle(headBlock, 7.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(headBlock, 7.0f, 1.4f);

        for (int i = 0; i < 3; ++i)
        {
            const float hx = headBlock.getX() + headBlock.getWidth() * (0.28f + 0.22f * (float) i);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillRoundedRectangle(hx - 6.0f, headBlock.getCentreY() - 18.0f, 12.0f, 36.0f, 3.0f);
            g.setColour(juce::Colour(0xff5a5248).withAlpha(0.6f));
            g.drawRoundedRectangle(hx - 6.0f, headBlock.getCentreY() - 18.0f, 12.0f, 36.0f, 3.0f, 1.0f);
        }
    }

    // Nameplate badge behind the "NF TAPE MACHINE / STUDIO SERIES / ..."
    // captions — an actual engraved metal plate rather than plain text
    // floating on the chassis.
    if (! nameplateArea.isEmpty())
    {
        constexpr float corner = 9.0f;
        auto plate = nameplateArea;

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(plate.expanded(2.5f), corner + 2.0f);

        juce::ColourGradient plateGrad(
            juce::Colour(0xff2c2a26), plate.getCentreX(), plate.getY(),
            juce::Colour(0xff151412), plate.getCentreX(), plate.getBottom(), false);
        g.setGradientFill(plateGrad);
        g.fillRoundedRectangle(plate, corner);

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(plate.getX() + 6.0f, plate.getY() + 1.4f, plate.getRight() - 6.0f, plate.getY() + 1.4f, 1.0f);

        // Single clean brass contour (not a double outline).
        g.setColour(NFTapeColours::brass.withAlpha(0.65f));
        g.drawRoundedRectangle(plate.reduced(0.5f), corner, 1.2f);

        // Thin divider between "STUDIO SERIES" and the "15 IPS..." line.
        const float dividerY = plate.getY() + plate.getHeight() * 0.52f;
        g.setColour(NFTapeColours::brass.withAlpha(0.35f));
        g.drawLine(plate.getX() + 18.0f, dividerY, plate.getRight() - 18.0f, dividerY, 1.0f);

        // Four corner screws, like the plate is actually bolted to the
        // chassis rather than just printed on it.
        const float screwR = 4.0f;
        const float inset = 11.0f;
        drawScrew(g, { plate.getX() + inset, plate.getY() + inset }, screwR);
        drawScrew(g, { plate.getRight() - inset, plate.getY() + inset }, screwR);
        drawScrew(g, { plate.getX() + inset, plate.getBottom() - inset }, screwR);
        drawScrew(g, { plate.getRight() - inset, plate.getBottom() - inset }, screwR);
    }
}

//==============================================================================
NFTapeMachineAudioProcessorEditor::NFTapeMachineAudioProcessorEditor(NFTapeMachineAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    panel.setBounds(0, 0, designWidth, designHeight);
    addAndMakeVisible(panel);

    auto& apvts = audioProcessor.apvts;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // ---- Top bar ---------------------------------------------------
    // The top-left "NF / AUDIO TOOLS" wordmark is retired now that the
    // logo is engraved directly into the reels instead — kept as hidden
    // components rather than deleted so the caption-index scheme below
    // (captions.getUnchecked(N)) doesn't have to shift.
    logoLabel.setText("NF", juce::dontSendNotification);
    logoLabel.setJustificationType(juce::Justification::centredLeft);
    logoLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    logoLabel.setColour(juce::Label::textColourId, NFTapeColours::white);
    logoLabel.setVisible(false);

    auto& logoSub = addCaption("AUDIO TOOLS", false, 12.0f, juce::Justification::centredLeft);
    logoSub.setColour(juce::Label::textColourId, NFTapeColours::textDim);
    logoSub.setVisible(false);

    titleLabel.setText("NF TAPE MACHINE", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    {
        auto titleFont = juce::Font(juce::FontOptions(39.0f, juce::Font::bold));
        titleFont.setExtraKerningFactor(0.06f);
        titleLabel.setFont(titleFont);
    }
    titleLabel.getProperties().set(NFTapeProps::goldTitle, true);
    panel.addAndMakeVisible(titleLabel);

    subtitleLabel.setText("ANALOG TAPE EMULATOR", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    {
        auto subtitleFont = juce::Font(juce::FontOptions(19.0f, juce::Font::plain));
        subtitleFont.setExtraKerningFactor(0.18f);
        subtitleLabel.setFont(subtitleFont);
    }
    subtitleLabel.getProperties().set(NFTapeProps::customFont, true);
    subtitleLabel.setColour(juce::Label::textColourId, NFTapeColours::amber.brighter(0.35f));
    panel.addAndMakeVisible(subtitleLabel);

    prevPresetButton.setButtonText("<");
    nextPresetButton.setButtonText(">");
    menuButton.getProperties().set(NFTapeProps::hamburger, true);
    for (auto* b : { &prevPresetButton, &nextPresetButton, &menuButton })
        panel.addAndMakeVisible(*b);

    prevPresetButton.onClick = [this] { audioProcessor.loadPreviousPreset(); refreshPresetLabel(); };
    nextPresetButton.onClick = [this] { audioProcessor.loadNextPreset(); refreshPresetLabel(); };
    menuButton.onClick = [this] { showPresetMenu(); };

    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setColour(juce::Label::backgroundColourId, NFTapeColours::chassisPanel);
    presetNameLabel.setColour(juce::Label::textColourId, NFTapeColours::white);
    panel.addAndMakeVisible(presetNameLabel);
    refreshPresetLabel();

    powerButton.getProperties().set(NFTapeProps::powerCircle, true);
    powerButton.setClickingTogglesState(true);
    panel.addAndMakeVisible(powerButton);
    powerAttachment = std::make_unique<ButtonAttachment>(apvts, "bypass", powerButton);

    // ---- Reels / VU / tube glow -------------------------------------
    panel.addAndMakeVisible(leftReel);
    panel.addAndMakeVisible(rightReel);

    // Both reels are threaded on the same tape, so clicking either one
    // stops/starts the whole transport rather than just that spool.
    leftReel.onToggle = [this] { rightReel.setSpinning(leftReel.isSpinning()); };
    rightReel.onToggle = [this] { leftReel.setSpinning(rightReel.isSpinning()); };
    panel.addAndMakeVisible(vuMeter);

    addCaption("NF TAPE MACHINE", true, 16.0f).setJustificationType(juce::Justification::centred);
    addCaption("STUDIO SERIES", false, 12.0f).setColour(juce::Label::textColourId, NFTapeColours::amber);
    addCaption(juce::String(juce::CharPointer_UTF8("15 IPS  \xc2\xb7  NAB  \xc2\xb7  2 TRACK")), false, 12.0f);
    addCaption("MADE IN ANALOG", false, 11.0f).setColour(juce::Label::textColourId, NFTapeColours::textDim);

    // ---- Knobs --------------------------------------------------------
    for (auto* knob : { &inputKnob, &hpfKnob, &driveKnob, &biasKnob, &wowRateKnob, &wowDepthKnob,
                        &noiseKnob, &eqLfKnob, &eqHfKnob, &outputKnob, &lpfKnob, &tapeAgeKnob,
                        &dropoutKnob, &mixKnob })
        setupKnob(*knob);

    // Printed scales, like the silkscreen numbers on a real channel strip.
    NFTapeLookAndFeel::setTickLabels(inputKnob, { "-24", "-12", "0", "+12", "+24" });
    NFTapeLookAndFeel::setTickLabels(hpfKnob, { "20", "200" });
    NFTapeLookAndFeel::setTickLabels(driveKnob, { "0", "4", "6", "8", "10" });
    NFTapeLookAndFeel::setTickLabels(biasKnob, { "-", "0", "+" });
    NFTapeLookAndFeel::setTickLabels(wowRateKnob, { "0.1", "10" });
    NFTapeLookAndFeel::setTickLabels(wowDepthKnob, { "0", "100" });
    NFTapeLookAndFeel::setTickLabels(noiseKnob, { "0", "4", "6", "10" });
    NFTapeLookAndFeel::setTickLabels(eqLfKnob, { "-", "0", "+" });
    NFTapeLookAndFeel::setTickLabels(eqHfKnob, { "-", "0", "+" });
    NFTapeLookAndFeel::setTickLabels(outputKnob, { "-24", "-12", "0", "+12", "+24" });
    NFTapeLookAndFeel::setTickLabels(lpfKnob, { "2k", "20k" });
    NFTapeLookAndFeel::setTickLabels(tapeAgeKnob, { "0", "100" });
    NFTapeLookAndFeel::setTickLabels(dropoutKnob, { "0", "10" });
    NFTapeLookAndFeel::setTickLabels(mixKnob, { "0", "100" });

    // Units/precision for the floating value bubble each knob pops up
    // while being dragged (see setupKnob's setPopupDisplayEnabled).
    inputKnob.setTextValueSuffix(" dB");     inputKnob.setNumDecimalPlacesToDisplay(1);
    outputKnob.setTextValueSuffix(" dB");    outputKnob.setNumDecimalPlacesToDisplay(1);
    eqLfKnob.setTextValueSuffix(" dB");      eqLfKnob.setNumDecimalPlacesToDisplay(1);
    eqHfKnob.setTextValueSuffix(" dB");      eqHfKnob.setNumDecimalPlacesToDisplay(1);
    hpfKnob.setTextValueSuffix(" Hz");       hpfKnob.setNumDecimalPlacesToDisplay(0);
    lpfKnob.setTextValueSuffix(" Hz");       lpfKnob.setNumDecimalPlacesToDisplay(0);
    wowRateKnob.setTextValueSuffix(" Hz");   wowRateKnob.setNumDecimalPlacesToDisplay(2);
    driveKnob.setNumDecimalPlacesToDisplay(1);
    biasKnob.setNumDecimalPlacesToDisplay(1);
    noiseKnob.setNumDecimalPlacesToDisplay(1);
    dropoutKnob.setNumDecimalPlacesToDisplay(1);
    wowDepthKnob.setNumDecimalPlacesToDisplay(0);
    tapeAgeKnob.setTextValueSuffix(" %");    tapeAgeKnob.setNumDecimalPlacesToDisplay(0);
    mixKnob.setTextValueSuffix(" %");        mixKnob.setNumDecimalPlacesToDisplay(0);

    panel.addAndMakeVisible(inputKnob);
    panel.addAndMakeVisible(hpfKnob);
    panel.addAndMakeVisible(driveKnob);
    panel.addAndMakeVisible(biasKnob);
    panel.addAndMakeVisible(wowRateKnob);
    panel.addAndMakeVisible(wowDepthKnob);
    panel.addAndMakeVisible(noiseKnob);
    panel.addAndMakeVisible(eqLfKnob);
    panel.addAndMakeVisible(eqHfKnob);
    panel.addAndMakeVisible(outputKnob);
    panel.addAndMakeVisible(lpfKnob);
    panel.addAndMakeVisible(tapeAgeKnob);
    panel.addAndMakeVisible(dropoutKnob);
    panel.addAndMakeVisible(mixKnob);

    inputAttachment = std::make_unique<SliderAttachment>(apvts, "input", inputKnob);
    hpfAttachment = std::make_unique<SliderAttachment>(apvts, "hpf", hpfKnob);
    driveAttachment = std::make_unique<SliderAttachment>(apvts, "drive", driveKnob);
    biasAttachment = std::make_unique<SliderAttachment>(apvts, "bias", biasKnob);
    wowRateAttachment = std::make_unique<SliderAttachment>(apvts, "wowRate", wowRateKnob);
    wowDepthAttachment = std::make_unique<SliderAttachment>(apvts, "wowDepth", wowDepthKnob);
    noiseAttachment = std::make_unique<SliderAttachment>(apvts, "noise", noiseKnob);
    eqLfAttachment = std::make_unique<SliderAttachment>(apvts, "eqLf", eqLfKnob);
    eqHfAttachment = std::make_unique<SliderAttachment>(apvts, "eqHf", eqHfKnob);
    outputAttachment = std::make_unique<SliderAttachment>(apvts, "output", outputKnob);
    lpfAttachment = std::make_unique<SliderAttachment>(apvts, "lpf", lpfKnob);
    tapeAgeAttachment = std::make_unique<SliderAttachment>(apvts, "tapeAge", tapeAgeKnob);
    dropoutAttachment = std::make_unique<SliderAttachment>(apvts, "dropout", dropoutKnob);
    mixAttachment = std::make_unique<SliderAttachment>(apvts, "mix", mixKnob);

    // A plain click on INPUT or OUTPUT resets both to 0 dB, but only while
    // they're linked — setting input first and output second guarantees
    // the final values, since the link's own compensation (triggered by
    // the input write) only ever gets overwritten by the output write that
    // follows it.
    gainLinkResetListener.onClickNoMove = [this]
    {
        if (audioProcessor.apvts.getRawParameterValue("gainLink")->load() <= 0.5f)
            return;

        if (auto* in = audioProcessor.apvts.getParameter("input"))
            in->setValueNotifyingHost(in->convertTo0to1(0.0f));
        if (auto* out = audioProcessor.apvts.getParameter("output"))
            out->setValueNotifyingHost(out->convertTo0to1(0.0f));
    };
    inputKnob.addMouseListener(&gainLinkResetListener, false);
    outputKnob.addMouseListener(&gainLinkResetListener, false);

    // ---- Section captions ----------------------------------------------
    addCaption("INPUT", true, 15.0f);
    addCaption("HPF", false, 10.0f);
    addCaption("TAPE TYPE", true, 13.0f);
    addCaption("DRIVE", true, 15.0f);
    addCaption("BIAS", true, 15.0f);
    addCaption("WOW & FLUTTER", true, 13.0f);
    addCaption("RATE", false, 10.0f);
    addCaption("DEPTH", false, 10.0f);
    addCaption("NOISE", true, 15.0f);
    addCaption("EQ", true, 15.0f);
    addCaption("LF", false, 11.0f);
    addCaption("HF", false, 11.0f);
    addCaption("OUTPUT", true, 15.0f);
    addCaption("LPF", false, 10.0f);
    addCaption("TAPE SPEED", true, 13.0f);
    addCaption("REPRO HEAD", true, 13.0f);
    addCaption("TAPE AGE", true, 13.0f);
    addCaption("NEW", false, 9.5f);
    addCaption("WORN", false, 9.5f);
    addCaption("DROPOUTS", true, 13.0f);
    addCaption("MIX", true, 15.0f);
    addCaption("DRY", false, 9.5f);
    addCaption("WET", false, 9.5f);
    addCaption("OUTPUT METER", true, 13.0f);
    addCaption("BYPASS", true, 14.0f);
    addCaption(juce::String(juce::CharPointer_UTF8("WARMTH  \xc2\xb7  BODY  \xc2\xb7  CHARACTER  \xc2\xb7  ANALOG MAGIC")), false, 13.0f)
        .setColour(juce::Label::textColourId, NFTapeColours::textDim);
    addCaption("v0.1", true, 13.0f, juce::Justification::centred)
        .setColour(juce::Label::textColourId, NFTapeColours::textDim.withAlpha(0.75f));

    // ---- LED-pill toggles -----------------------------------------------
    setupLedPill(satButton, "SAT");
    setupLedPill(calButton, "CAL");
    setupLedPill(wowInButton, "IN");
    setupLedPill(noiseInButton, "IN");
    setupLedPill(dropoutInButton, "IN");
    setupLedPill(gainLinkButton, "LINK");

    bypassButton.getProperties().set(NFTapeProps::powerSquare, true);
    bypassButton.setClickingTogglesState(true);
    panel.addAndMakeVisible(bypassButton);

    satAttachment = std::make_unique<ButtonAttachment>(apvts, "satEnabled", satButton);
    calAttachment = std::make_unique<ButtonAttachment>(apvts, "biasCal", calButton);
    wowInAttachment = std::make_unique<ButtonAttachment>(apvts, "wowFlutterEnabled", wowInButton);
    noiseInAttachment = std::make_unique<ButtonAttachment>(apvts, "noiseEnabled", noiseInButton);
    dropoutInAttachment = std::make_unique<ButtonAttachment>(apvts, "dropoutEnabled", dropoutInButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(apvts, "bypass", bypassButton);
    gainLinkAttachment = std::make_unique<ButtonAttachment>(apvts, "gainLink", gainLinkButton);

    // ---- Segmented choice groups -----------------------------------------
    buildChoiceGroup(tapeTypeGroup, { "GP9", "456", "499", "250" });
    buildChoiceGroup(tapeSpeedGroup, { "7.5 IPS", "15 IPS", "30 IPS" });
    buildChoiceGroup(reproHeadGroup, { "NAB", "IEC" });

    panel.addAndMakeVisible(outputMeterBar);

    setResizable(true, true);
    setResizeLimits(designWidth / 3, designHeight / 3, designWidth * 2, designHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double) designWidth / (double) designHeight);

    // Opens well under a typical 1080p screen (the full design canvas plus
    // title bar can otherwise push the resize handle off-screen) — the
    // aspect-locked resizer above still lets it grow back up to designWidth
    // x designHeight or beyond.
    setSize(designWidth * 3 / 4, designHeight * 3 / 4);

    startTimerHz(30);
}

NFTapeMachineAudioProcessorEditor::~NFTapeMachineAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

juce::Slider& NFTapeMachineAudioProcessorEditor::setupKnob(juce::Slider& knob)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                             juce::MathConstants<float>::pi * 2.8f, true);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    // A small floating value bubble follows the mouse while dragging, so
    // you can read the exact number instead of judging it by ear/eye.
    knob.setPopupDisplayEnabled(true, true, &panel);
    return knob;
}

juce::TextButton& NFTapeMachineAudioProcessorEditor::setupLedPill(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.getProperties().set(NFTapeProps::ledPill, true);
    button.setClickingTogglesState(true);
    panel.addAndMakeVisible(button);
    return button;
}

juce::Label& NFTapeMachineAudioProcessorEditor::addCaption(const juce::String& text, bool bold, float relativeSize,
                                                           juce::Justification justification)
{
    auto* label = captions.add(new juce::Label(juce::String(), text));
    label->setJustificationType(justification);
    label->setFont(juce::Font(juce::FontOptions(relativeSize, bold ? juce::Font::bold : juce::Font::plain)));
    label->setColour(juce::Label::textColourId, NFTapeColours::white);
    label->setInterceptsMouseClicks(false, false);
    panel.addAndMakeVisible(label);
    return *label;
}

void NFTapeMachineAudioProcessorEditor::buildChoiceGroup(ChoiceGroup& group, const juce::StringArray& names)
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto* button = group.buttons.add(new juce::TextButton(names[i]));
        button->getProperties().set(NFTapeProps::segmented, true);
        button->setClickingTogglesState(false);

        const juce::String paramId = group.paramId;
        button->onClick = [this, paramId, i]
        {
            if (auto* param = audioProcessor.apvts.getParameter(paramId))
                param->setValueNotifyingHost(param->convertTo0to1((float) i));
        };

        panel.addAndMakeVisible(button);
    }

    syncChoiceGroup(group);
}

void NFTapeMachineAudioProcessorEditor::syncChoiceGroup(ChoiceGroup& group)
{
    const int current = (int) audioProcessor.apvts.getRawParameterValue(group.paramId)->load();

    for (int i = 0; i < group.buttons.size(); ++i)
        group.buttons[i]->setToggleState(i == current, juce::dontSendNotification);
}

void NFTapeMachineAudioProcessorEditor::refreshPresetLabel()
{
    presetNameLabel.setText(audioProcessor.getProgramName(audioProcessor.getCurrentPresetIndex()),
                            juce::dontSendNotification);
}

void NFTapeMachineAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < NFTapeMachineAudioProcessor::getNumFactoryPresets(); ++i)
        menu.addItem(i + 1, NFTapeMachineAudioProcessor::getFactoryPresetName(i),
                    true, i == audioProcessor.getCurrentPresetIndex());

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        if (result > 0)
        {
            audioProcessor.loadPreset(result - 1);
            refreshPresetLabel();
        }
    });
}

void NFTapeMachineAudioProcessorEditor::timerCallback()
{
    vuMeter.setLevels(audioProcessor.getOutputLevelL(), audioProcessor.getOutputLevelR());
    outputMeterBar.setLevels(audioProcessor.getOutputLevelL(), audioProcessor.getOutputLevelR());

    syncChoiceGroup(tapeTypeGroup);
    syncChoiceGroup(tapeSpeedGroup);
    syncChoiceGroup(reproHeadGroup);

    const int tapeType = (int) audioProcessor.apvts.getRawParameterValue("tapeType")->load();
    leftReel.setTapeType(tapeType);
    rightReel.setTapeType(tapeType);

    // DRIVE is 0-10 — normalise to 0-1 for the tube glow.
    const float drive = audioProcessor.apvts.getRawParameterValue("drive")->load();
    panel.setDriveAmount(drive / 10.0f);
    panel.setBypassed(audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f);
    panel.setSatEnabled(audioProcessor.apvts.getRawParameterValue("satEnabled")->load() > 0.5f);
}

void NFTapeMachineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void NFTapeMachineAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    panel.setTransform(juce::AffineTransform::scale(scale));

    // Every position below is an absolute coordinate on the fixed 1536x1024
    // design canvas (see designWidth/designHeight) — panel's single
    // transform above scales the whole tree together, so these numbers
    // never need to be recomputed per host size.
    auto placeCaptionIndex = [this](int index, int x, int y, int w, int h)
    { captions.getUnchecked(index)->setBounds(x, y, w, h); };

    // ---- Top bar ------------------------------------------------------
    // Logo block and title block share the same vertical centre so the
    // brand mark reads as aligned with the plugin name next to it.
    logoLabel.setBounds(155, 24, 150, 38);
    captions.getUnchecked(0)->setBounds(155, 64, 150, 16); // "AUDIO TOOLS"

    titleLabel.setBounds(488, 20, 560, 42);
    subtitleLabel.setBounds(488, 64, 560, 20);

    prevPresetButton.setBounds(1052, 29, 48, 48);
    presetNameLabel.setBounds(1100, 29, 182, 48);
    nextPresetButton.setBounds(1284, 29, 60, 48);
    menuButton.setBounds(1344, 29, 58, 38);
    powerButton.setBounds(1430, 29, 58, 38);

    // ---- Reels / VU -----------------------------------------------------
    // Height (not width) sets the drawn circle's diameter here since the
    // box is wider than tall — growing only height makes the reels bigger
    // without pushing into the VU meter or the tube lamps on either side.
    leftReel.setBounds(113, 104, 410, 420);
    rightReel.setBounds(1015, 104, 410, 420);

    // Centred on the canvas (x) and on the reels' own vertical centre (y =
    // reel top 104 + reel height 420 / 2 = 314), not just eyeballed.
    const int vuX = (designWidth - 478) / 2;
    const int vuY = 104 + 420 / 2 - 200 / 2;
    vuMeter.setBounds(vuX, vuY, 478, 200);

    // Nameplate: a proper engraved-metal badge (drawn by the background
    // panel) rather than plain floating text, centred below the VU pair.
    const int plateWidth = 320;
    const int plateX = vuX + (478 - plateWidth) / 2;
    const int plateTop = vuY + 200;
    const int plateBottom = vuY + 284;
    panel.nameplateArea = juce::Rectangle<int>(plateX, plateTop, plateWidth, plateBottom - plateTop).toFloat();

    auto* studioLabel = captions.getUnchecked(1); // "NF TAPE MACHINE" small plate caption
    studioLabel->setBounds(plateX, vuY + 206, plateWidth, 22);
    captions.getUnchecked(2)->setBounds(plateX, vuY + 228, plateWidth, 16); // STUDIO SERIES
    captions.getUnchecked(3)->setBounds(plateX, vuY + 246, plateWidth, 16); // 15 IPS . NAB . 2 TRACK
    captions.getUnchecked(4)->setBounds(plateX, vuY + 264, plateWidth, 14); // MADE IN ANALOG

    // ---- Control row 1 --------------------------------------------------
    const int capY = 584;

    // The whole row is shifted by one constant offset so its true centre
    // (the WOW & FLUTTER pair, roughly the middle of the eight sections)
    // lands exactly under the nameplate/reels centre (x=768) instead of
    // wherever the sum of the original per-section widths happened to
    // put it. Every section moves by the same amount, so the spacing
    // pattern between them is unchanged — only the whole row's position is.
    constexpr int rowShift = -32;

    // INPUT — knob is shifted left off its blueprint x so it clears TAPE
    // TYPE below (the two boxes overlapped by ~44px in the original
    // coordinate sheet); caption and HPF sub-knob are centred on the
    // knob's actual centre rather than the old unshifted box, so the
    // label and sub-knob both sit right under the knob instead of
    // reading as offset from it.
    inputKnob.setBounds(100 + rowShift, 610, 130, 130);
    placeCaptionIndex(5, 70 + rowShift, capY, 190, 20);
    hpfKnob.setBounds(130 + rowShift, 758, 70, 70);
    placeCaptionIndex(6, 130 + rowShift, 829, 70, 12); // HPF

    // TAPE TYPE (4 stacked buttons) — sized to the widest box that still
    // clears INPUT's and DRIVE's tick-label rings on either side.
    placeCaptionIndex(7, 240 + rowShift, capY, 115, 24);
    {
        int by = 610;
        for (auto* b : tapeTypeGroup.buttons)
        {
            b->setBounds(240 + rowShift, by, 115, 28);
            by += 34;
        }
    }

    // DRIVE
    placeCaptionIndex(8, 365 + rowShift, capY, 135, 20);
    driveKnob.setBounds(365 + rowShift, 608, 135, 135);
    satButton.setBounds(401 + rowShift, 779, 56, 31);

    // BIAS
    placeCaptionIndex(9, 521 + rowShift, capY, 135, 20);
    biasKnob.setBounds(521 + rowShift, 608, 135, 135);
    calButton.setBounds(558 + rowShift, 779, 56, 31);

    // WOW & FLUTTER
    placeCaptionIndex(10, 690 + rowShift, capY, 220, 20);
    wowRateKnob.setBounds(690 + rowShift, 610, 100, 100);
    wowDepthKnob.setBounds(810 + rowShift, 610, 100, 100);
    placeCaptionIndex(11, 690 + rowShift, 714, 100, 14);  // RATE
    placeCaptionIndex(12, 810 + rowShift, 714, 100, 14);  // DEPTH
    wowInButton.setBounds(772 + rowShift, 779, 56, 31);

    // NOISE
    placeCaptionIndex(13, 944 + rowShift, capY, 116, 20);
    noiseKnob.setBounds(944 + rowShift, 610, 116, 110);
    noiseInButton.setBounds(979 + rowShift, 779, 56, 31);

    // EQ
    placeCaptionIndex(14, 1090 + rowShift, capY, 217, 20);
    eqLfKnob.setBounds(1090 + rowShift, 610, 100, 100);
    eqHfKnob.setBounds(1207 + rowShift, 610, 100, 100);
    placeCaptionIndex(15, 1090 + rowShift, 714, 100, 14); // LF
    placeCaptionIndex(16, 1207 + rowShift, 714, 100, 14); // HF

    // GAIN LINK — sits in EQ's own sub-button row (same slot pattern as
    // SAT/CAL/IN below their knobs), centred between the LF and HF dials.
    // Toggles whether INPUT and OUTPUT are coupled (see PluginProcessor's
    // parameterChanged for the actual dB-for-dB compensation logic).
    gainLinkButton.setBounds(1171 + rowShift, 779, 56, 31);

    // OUTPUT — mirrors INPUT exactly around the canvas centre (1536/2),
    // so both knobs sit the same distance from their side screw (46px in
    // from the chassis edge) instead of drifting apart after the row1
    // recentring above.
    placeCaptionIndex(17, 1308, capY, 190, 20);
    outputKnob.setBounds(1338, 610, 130, 130);
    lpfKnob.setBounds(1368, 758, 70, 70);
    placeCaptionIndex(18, 1368, 829, 70, 12); // LPF

    // ---- Control row 2 (bottomPanel 31,838,1472,139) ---------------------
    const int row2CapY = 845;

    // Same idea as row1's rowShift: move the whole row as one block. DROPOUTS
    // is the middle group of the seven here (like WOW & FLUTTER in row1),
    // so the shift is chosen to land ITS centre on the nameplate centre
    // (x=768) rather than on the edge-to-screw margins.
    constexpr int row2Shift = 33;

    // TAPE SPEED
    placeCaptionIndex(19, 61 + row2Shift, row2CapY, 244, 16);
    tapeSpeedGroup.buttons.getUnchecked(0)->setBounds(61 + row2Shift, 895, 80, 43);
    tapeSpeedGroup.buttons.getUnchecked(1)->setBounds(143 + row2Shift, 895, 80, 43);
    tapeSpeedGroup.buttons.getUnchecked(2)->setBounds(225 + row2Shift, 895, 80, 43);

    // REPRO HEAD
    placeCaptionIndex(20, 329 + row2Shift, row2CapY, 162, 16);
    reproHeadGroup.buttons.getUnchecked(0)->setBounds(329 + row2Shift, 895, 80, 43);
    reproHeadGroup.buttons.getUnchecked(1)->setBounds(411 + row2Shift, 895, 80, 43);

    // TAPE AGE
    placeCaptionIndex(21, 542 + row2Shift, row2CapY, 90, 16);
    tapeAgeKnob.setBounds(542 + row2Shift, 873, 90, 90);
    placeCaptionIndex(22, 542 + row2Shift, 965, 45, 12);      // NEW
    placeCaptionIndex(23, 587 + row2Shift, 965, 45, 12);      // WORN

    // DROPOUTS
    placeCaptionIndex(24, 690 + row2Shift, row2CapY, 149, 16);
    dropoutKnob.setBounds(690 + row2Shift, 873, 90, 90);
    dropoutInButton.setBounds(797 + row2Shift, 910, 42, 38);

    // MIX
    placeCaptionIndex(25, 882 + row2Shift, row2CapY, 90, 16);
    mixKnob.setBounds(882 + row2Shift, 873, 90, 90);
    placeCaptionIndex(26, 882 + row2Shift, 965, 45, 12);      // DRY
    placeCaptionIndex(27, 927 + row2Shift, 965, 45, 12);      // WET

    // OUTPUT METER
    placeCaptionIndex(28, 1033 + row2Shift, 863, 300, 16);
    outputMeterBar.setBounds(1033 + row2Shift, 882, 300, 74);

    // BYPASS
    placeCaptionIndex(29, 1366 + row2Shift, 863, 79, 16);
    bypassButton.setBounds(1366 + row2Shift, 884, 79, 79);

    // ---- Tagline ----------------------------------------------------------
    captions.getUnchecked(30)->setBounds(423, 985, 690, 25);

    // Version tag, centred under the BYPASS button rather than pinned to
    // the corner screw.
    placeCaptionIndex(31, 1401, 982, 75, 20);
}

//==============================================================================
void NFTapeMachineAudioProcessorEditor::OutputMeterBar::setLevels(float leftDb, float rightDb)
{
    leftDb = juce::jlimit(-60.0f, 12.0f, leftDb);
    rightDb = juce::jlimit(-60.0f, 12.0f, rightDb);

    smoothedLeft = leftDb > smoothedLeft ? leftDb : smoothedLeft + (leftDb - smoothedLeft) * 0.3f;
    smoothedRight = rightDb > smoothedRight ? rightDb : smoothedRight + (rightDb - smoothedRight) * 0.3f;

    repaint();
}

juce::Colour NFTapeMachineAudioProcessorEditor::OutputMeterBar::zoneColour(float segmentDb)
{
    if (segmentDb >= 6.0f) return NFTapeColours::ledRed;
    if (segmentDb >= -6.0f) return NFTapeColours::ledAmber;
    return NFTapeColours::ledGreen;
}

void NFTapeMachineAudioProcessorEditor::OutputMeterBar::drawColumn(juce::Graphics& g, juce::Rectangle<float> bar, float levelDb)
{
    constexpr int numSegments = 20;
    constexpr float gap = 1.6f;
    const float segHeight = (bar.getHeight() - gap * (float) (numSegments - 1)) / (float) numSegments;

    for (int i = 0; i < numSegments; ++i)
    {
        const float segDb = -60.0f + ((float) i + 0.5f) / (float) numSegments * 72.0f;
        const bool lit = levelDb >= segDb;

        const float y = bar.getBottom() - (float) i * (segHeight + gap) - segHeight;
        juce::Rectangle<float> segment(bar.getX(), y, bar.getWidth(), segHeight);

        g.setColour(lit ? zoneColour(segDb) : zoneColour(segDb).interpolatedWith(NFTapeColours::bezelBlack, 0.88f));
        g.fillRect(segment);
    }
}

void NFTapeMachineAudioProcessorEditor::OutputMeterBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(NFTapeColours::bezelBlack);
    g.fillRoundedRectangle(bounds, 4.0f);

    auto inner = bounds.reduced(3.0f);
    constexpr float gap = 4.0f;
    const float columnWidth = (inner.getWidth() - gap) * 0.5f;

    auto leftBar = inner.removeFromLeft(columnWidth);
    inner.removeFromLeft(gap);
    auto rightBar = inner;

    drawColumn(g, leftBar, smoothedLeft);
    drawColumn(g, rightBar, smoothedRight);

    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.4f);
}
