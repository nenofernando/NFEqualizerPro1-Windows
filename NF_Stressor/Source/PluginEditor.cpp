#include "PluginEditor.h"

using namespace NFStressorColours;

void NFStressorAudioProcessorEditor::BackgroundPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg(panelTop, bounds.getX(), bounds.getY(),
                            panelBottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);

    // Faint horizontal brushed-metal striations.
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (float py = 0.0f; py < bounds.getHeight(); py += 2.0f)
        g.drawLine(0.0f, py, bounds.getWidth(), py, 1.0f);

    // Recessed trays behind each control group: a dropped shadow, a darker
    // fill than the surrounding chassis, and a bright top edge / dark
    // bottom edge to read as sunk into the panel rather than floating on it.
    for (const auto& area : insetPanels)
    {
        auto tray = area.toFloat();

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(tray.translated(0.0f, 1.5f).expanded(1.5f), 7.0f);

        juce::ColourGradient trayFill(panelBottom.darker(0.35f), tray.getX(), tray.getY(),
                                      panelBottom.darker(0.1f), tray.getX(), tray.getBottom(), false);
        g.setGradientFill(trayFill);
        g.fillRoundedRectangle(tray, 6.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawLine(tray.getX() + 4, tray.getY() + 0.6f, tray.getRight() - 4, tray.getY() + 0.6f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawLine(tray.getX() + 4, tray.getBottom() - 0.6f, tray.getRight() - 4, tray.getBottom() - 0.6f, 1.0f);

        // White contour on the darker recessed tray itself.
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawRoundedRectangle(tray, 6.0f, 2.6f);
    }

    // Ambient glow behind each knob, as if light from the circuit board is
    // leaking out around its base. Drawn here, on the panel itself, so it
    // isn't clipped by the knob component sitting on top of it — only the
    // ring peeking out past the knob's edge ends up visible. Intensity rises
    // with the knob's own value, so it reads as the circuit lighting up as
    // you turn it, not a fixed decoration.
    for (const auto& glow : knobGlows)
    {
        const auto knobCentre = glow.centre;
        const float knobRadius = glow.radius;
        const float glowRadius = knobRadius * 1.3f;
        const float intensity = 0.02f + juce::jlimit(0.0f, 1.0f, glow.value) * 0.14f;

        juce::ColourGradient knobGlow(glowWhite.withAlpha(intensity), knobCentre.x, knobCentre.y,
                                      glowWhite.withAlpha(0.0f), knobCentre.x, knobCentre.y + glowRadius, true);
        g.setGradientFill(knobGlow);
        g.fillEllipse(knobCentre.x - glowRadius, knobCentre.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
    }

    // SLOW/SLOW-FAST tick marks beside ATTACK/RELEASE (see setIndicatorTicks).
    g.setColour(textDim);
    for (const auto& tick : ticks)
        g.drawLine(tick, 1.6f);

    // Vignette
    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                  juce::Colours::black.withAlpha(0.35f), bounds.getX(), bounds.getY(), true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);

    auto drawScrew = [&g](juce::Point<float> c)
    {
        const float r = 3.2f;
        juce::ColourGradient grad(screwBody.brighter(0.3f), c.x - r, c.y - r, screwShadow, c.x + r, c.y + r, false);
        g.setGradientFill(grad);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(screwShadow);
        g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
        g.drawLine(c.x - r * 0.6f, c.y, c.x + r * 0.6f, c.y, 1.1f);
    };

    const float m = 7.5f;
    drawScrew({ m, m });
    drawScrew({ bounds.getWidth() - m, m });
    drawScrew({ m, bounds.getHeight() - m });
    drawScrew({ bounds.getWidth() - m, bounds.getHeight() - m });

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRect(bounds, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRect(bounds.reduced(2.0f), 1.0f);

    // Brand mark, set sideways (read bottom-to-top) in the space beside
    // MIX, mirroring NUKE on the other side — replaces the old horizontal
    // footer strip along the bottom edge.
    if (!brandLabelArea.isEmpty())
    {
        const auto area = brandLabelArea.toFloat();
        g.setColour(textLight.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(13.5f).withStyle("Bold")));
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, area.getCentreX(), area.getCentreY()));
        // Unrotated, text reads left-to-right along what becomes, after the
        // -90 degree turn, the vertical axis — so swap width/height here to
        // give drawText the long axis it needs.
        auto textBounds = juce::Rectangle<float>(area.getHeight(), area.getWidth()).withCentre(area.getCentre());
        g.drawText("NF AUDIO TOOLS", textBounds, juce::Justification::centred, false);
        g.restoreState();
    }
}

void NFStressorAudioProcessorEditor::OptoLed::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    // The glow layers below need headroom around the core dot, so the core
    // itself is only part of the component's box — sized so every glow
    // layer stays within bounds and never gets clipped into a square.
    const float box = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float diameter = box * 0.56f;
    const float margin = (box - diameter) * 0.5f;
    auto led = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(led.expanded(margin * 0.3f));

    if (isOn)
    {
        // Layered glow — bigger and brighter so the lit state reads as
        // vivid, not just a dim colour change.
        g.setColour(amber.withAlpha(0.22f));
        g.fillEllipse(led.expanded(margin));
        g.setColour(amber.withAlpha(0.4f));
        g.fillEllipse(led.expanded(margin * 0.55f));
    }

    juce::ColourGradient ledGradient(isOn ? amber.brighter(0.6f) : ledOff.brighter(0.1f), led.getX(), led.getY(),
                                     isOn ? amber.darker(0.1f) : ledOff.darker(0.2f), led.getX(), led.getBottom(), false);
    g.setGradientFill(ledGradient);
    g.fillEllipse(led);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(led, 0.7f);

    if (isOn)
    {
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.fillEllipse(led.reduced(diameter * 0.58f).translated(-diameter * 0.08f, -diameter * 0.1f));
    }
}

NFStressorAudioProcessorEditor::NFStressorAudioProcessorEditor(NFStressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(content);
    content.addAndMakeVisible(panel);

    setupCaption(titleLabel, "NF - STRESSOR", 19.0f, true);
    content.addAndMakeVisible(titleLabel);

    setupCaption(taglineLabel, "REDLINE  /  1% THD", 12.5f, false);
    taglineLabel.setColour(juce::Label::textColourId, textDim);
    content.addAndMakeVisible(taglineLabel);

    powerButton.setButtonText("BYPASS");
    powerButton.setClickingTogglesState(true);
    content.addAndMakeVisible(powerButton);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bypass", powerButton);

    menuButton.setButtonText("MENU");
    menuButton.setClickingTogglesState(false);
    menuButton.onClick = [this] { showPresetMenu(); };
    content.addAndMakeVisible(menuButton);

    setupCaption(versionLabel, "V1.0", 13.5f, false, juce::Justification::centredLeft);
    versionLabel.setColour(juce::Label::textColourId, textDim);
    content.addAndMakeVisible(versionLabel);

    auto setupMainKnob = [this](juce::Slider& knob, juce::Label& caption, const juce::String& text)
    {
        setupKnob(knob);
        content.addAndMakeVisible(knob);
        setupCaption(caption, text, 15.5f, true);
        content.addAndMakeVisible(caption);
    };

    setupMainKnob(inputKnob, inputCaption, "INPUT");
    setupMainKnob(attackKnob, attackCaption, "ATTACK");
    setupMainKnob(releaseKnob, releaseCaption, "RELEASE");
    setupMainKnob(outputKnob, outputCaption, "OUTPUT");

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "input", inputKnob);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "attack", attackKnob);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "release", releaseKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "output", outputKnob);

    // Double-click any knob to snap it back to that parameter's own default
    // value — the same convention Slate Digital's FG-Stress (also a
    // Distressor-style plug-in) uses on its Input/Attack/Release knobs.
    auto setKnobDoubleClickDefault = [this](juce::Slider& knob, const char* paramId)
    {
        if (auto* param = audioProcessor.apvts.getParameter(paramId))
            knob.setDoubleClickReturnValue(true, param->getNormalisableRange().convertFrom0to1(param->getDefaultValue()));
    };
    setKnobDoubleClickDefault(inputKnob, "input");
    setKnobDoubleClickDefault(attackKnob, "attack");
    setKnobDoubleClickDefault(releaseKnob, "release");
    setKnobDoubleClickDefault(outputKnob, "output");

    content.addAndMakeVisible(optoLed);
    setupCaption(optoLabel, "OPTO", 12.5f, true, juce::Justification::centred);
    optoLabel.setColour(juce::Label::textColourId, textLight);
    content.addAndMakeVisible(optoLabel);

    // SLOW/FAST hints, beside the tick marks drawn on the panel at each
    // knob's two end-stops ("0" on the left, "10" on the right). ATTACK is
    // slowest at 0 / fastest at 10; RELEASE is the other way round (fastest
    // at 0 / slowest at 10) — see mapAttackMs/mapReleaseMs in
    // StressorEngine.cpp.
    setupCaption(attackSlowLabel, "SLOW", 13.0f, true, juce::Justification::centredRight);
    attackSlowLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(attackSlowLabel);
    setupCaption(attackFastLabel, "FAST", 13.0f, true, juce::Justification::centredLeft);
    attackFastLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(attackFastLabel);
    setupCaption(releaseFastLabel, "FAST", 13.0f, true, juce::Justification::centredRight);
    releaseFastLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(releaseFastLabel);
    setupCaption(releaseSlowLabel, "SLOW", 13.0f, true, juce::Justification::centredLeft);
    releaseSlowLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(releaseSlowLabel);

    setupCaption(grCaption, "GR", 17.0f, true);
    content.addAndMakeVisible(grCaption);
    content.addAndMakeVisible(grMeter);

    setupCaption(ratioCaption, "RATIO", 15.5f, true);
    content.addAndMakeVisible(ratioCaption);

    static const juce::StringArray ratioNames { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1" };
    for (int i = 0; i < ratioNames.size(); ++i)
    {
        auto* button = ratioButtons.add(new juce::TextButton());
        setupSegmentButton(*button, ratioNames[i]);
        content.addAndMakeVisible(button);
        button->onClick = [this, i] { ratioAttachment->setValueAsCompleteGesture((float) i); };
    }

    ratioAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.apvts.getParameter("ratio"),
        [this](float newValue)
        {
            currentRatioIndex = (int) newValue;
            if (currentRatioIndex != NF::kOptoRatioIndex)
                lastNonOptoRatioIndex = currentRatioIndex;
            for (int i = 0; i < ratioButtons.size(); ++i)
                ratioButtons[i]->setToggleState(i == currentRatioIndex, juce::dontSendNotification);
            taglineLabel.setColour(juce::Label::textColourId,
                                   currentRatioIndex >= 5 ? amber : textDim);

            // Selecting 10:1 snaps ATTACK/RELEASE to their panel "OPTO"
            // positions (10 / 0) as a starting point, matching the
            // hardware's end-of-travel detents — but the knobs stay fully
            // interactive. Dragging either one away from that position
            // (checked live in timerCallback, since that's a continuous
            // knob-position condition, not something that only changes
            // when RATIO itself changes) is what actually turns the OPTO
            // light back off; see timerCallback and StressorEngine's own
            // optoEngaged check for the matching DSP-side condition.
            if (currentRatioIndex == NF::kOptoRatioIndex)
            {
                attackKnob.setValue(10.0, juce::sendNotificationSync);
                releaseKnob.setValue(0.0, juce::sendNotificationSync);
            }
        });
    ratioAttachment->sendInitialUpdate();

    // Clicking the OPTO light is a second way to engage/disengage it,
    // alongside clicking the 10:1 RATIO button directly.
    optoLed.onClick = [this]
    {
        if (isOptoActive())
        {
            // Fully exits — back to whatever ratio was last selected.
            ratioAttachment->setValueAsCompleteGesture((float) lastNonOptoRatioIndex);
        }
        else if (currentRatioIndex == NF::kOptoRatioIndex)
        {
            // RATIO's already at 10:1 but the knobs have drifted off the
            // OPTO detent (10 / 0) — re-snap them directly, since setting
            // the ratio parameter to the value it already has wouldn't
            // fire the parameter-change callback that normally does this.
            attackKnob.setValue(10.0, juce::sendNotificationSync);
            releaseKnob.setValue(0.0, juce::sendNotificationSync);
        }
        else
        {
            ratioAttachment->setValueAsCompleteGesture((float) NF::kOptoRatioIndex);
        }
    };

    setupCaption(detectorCaption, "DETECTOR", 14.0f, true);
    content.addAndMakeVisible(detectorCaption);
    setupCaption(audioCaption, "AUDIO", 14.0f, true);
    content.addAndMakeVisible(audioCaption);

    setupToggleButton(hpButton, "HP");
    setupToggleButton(linkButton, "LINK");
    setupToggleButton(dist2Button, "DIST 2");
    setupToggleButton(dist3Button, "DIST 3");

    hpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "hp", hpButton);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "link", linkButton);
    dist2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "dist2", dist2Button);
    dist3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "dist3", dist3Button);

    setupKnob(mixKnob);
    content.addAndMakeVisible(mixKnob);
    setupCaption(mixCaption, "MIX", 15.5f, true);
    content.addAndMakeVisible(mixCaption);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "mix", mixKnob);
    setKnobDoubleClickDefault(mixKnob, "mix");

    nukeButton.setButtonText("NUKE");
    nukeButton.setClickingTogglesState(true);
    content.addAndMakeVisible(nukeButton);
    nukeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "nuke", nukeButton);

    content.setBounds(0, 0, designWidth, designHeight);
    layOutContent();

    setResizable(true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio((double) designWidth / (double) designHeight);
        constrainer->setSizeLimits(designWidth / 2, designHeight / 2,
                                   (int) (designWidth * 1.5f), (int) (designHeight * 1.5f));
    }

    // Double-clicking the bare top-left corner of the chassis snaps the
    // window straight back to this same default size — a quick way out
    // after dragging the resize corner around.
    panel.onCornerDoubleClicked = [this] { applyDefaultSize(); };

    applyDefaultSize();

    startTimerHz(30);
}

NFStressorAudioProcessorEditor::~NFStressorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void NFStressorAudioProcessorEditor::applyDefaultSize()
{
    // Small enough that the whole window — corner resizer included —
    // comfortably fits within the user's actual screen. The panel is a tall
    // vertical strip, so a fixed 0.8 scale can still open taller than some
    // screens' usable height, pushing the resize handle off-screen and out
    // of reach; scale to the display instead.
    float openScale = 0.8f;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto workArea = display->userBounds;
        const float maxHeightScale = (float) (workArea.getHeight() - 80) / (float) designHeight;
        const float maxWidthScale = (float) (workArea.getWidth() - 80) / (float) designWidth;
        openScale = juce::jlimit(0.35f, 0.8f, juce::jmin(openScale, maxHeightScale, maxWidthScale));
    }
    setSize((int) (designWidth * openScale), (int) (designHeight * openScale));
}

juce::File NFStressorAudioProcessorEditor::getPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("NF Audio Tools")
                   .getChildFile("NF - Stressor")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void NFStressorAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Default");
    menu.addSeparator();
    menu.addItem(2, "Save Preset...");
    menu.addItem(3, "Load Preset...");

    // List whatever's already saved in the presets folder directly in the
    // menu, so picking one is a single click instead of a file dialog trip
    // every time.
    juce::Array<juce::File> presetFiles;
    getPresetsDirectory().findChildFiles(presetFiles, juce::File::findFiles, false, "*.nfstressorpreset");
    presetFiles.sort();

    if (!presetFiles.isEmpty())
    {
        menu.addSeparator();
        constexpr int firstPresetId = 100;
        for (int i = 0; i < presetFiles.size(); ++i)
            menu.addItem(firstPresetId + i, presetFiles.getReference(i).getFileNameWithoutExtension());

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menuButton),
            [this, presetFiles](int result)
            {
                if (result == 1)
                    resetToDefault();
                else if (result == 2)
                    savePresetAs();
                else if (result == 3)
                    loadPresetFrom();
                else if (result >= 100 && result - 100 < presetFiles.size())
                    loadPresetFile(presetFiles.getReference(result - 100));
            });
        return;
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menuButton),
        [this](int result)
        {
            if (result == 1)
                resetToDefault();
            else if (result == 2)
                savePresetAs();
            else if (result == 3)
                loadPresetFrom();
        });
}

void NFStressorAudioProcessorEditor::resetToDefault()
{
    for (auto* param : audioProcessor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
}

void NFStressorAudioProcessorEditor::loadPresetFile(const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse(file))
    {
        auto newState = juce::ValueTree::fromXml(*xml);
        if (newState.isValid())
            audioProcessor.apvts.replaceState(newState);
    }
}

void NFStressorAudioProcessorEditor::savePresetAs()
{
    activeFileChooser = std::make_unique<juce::FileChooser>(
        "Save Preset", getPresetsDirectory(), "*.nfstressorpreset");

    const auto flags = juce::FileBrowserComponent::saveMode
                      | juce::FileBrowserComponent::canSelectFiles
                      | juce::FileBrowserComponent::warnAboutOverwriting;

    activeFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{})
            return;
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension("nfstressorpreset");

        if (auto xml = audioProcessor.apvts.copyState().createXml())
            xml->writeTo(file);
    });
}

void NFStressorAudioProcessorEditor::loadPresetFrom()
{
    activeFileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset", getPresetsDirectory(), "*.nfstressorpreset");

    activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
            loadPresetFile(file);
    });
}

juce::Slider& NFStressorAudioProcessorEditor::setupKnob(juce::Slider& knob)
{
    // Relative vertical/horizontal drag (the project's original feel) —
    // moving the mouse up/right always increases the value and down/left
    // decreases it, regardless of where on the knob you first clicked.
    // True circular (angle-around-the-knob) drag was tried at one point but
    // felt unstable/jumpy under the mouse, so this reverts back to it.
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                             juce::MathConstants<float>::pi * 2.75f, true);
    return knob;
}

juce::TextButton& NFStressorAudioProcessorEditor::setupSegmentButton(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setClickingTogglesState(false);
    return button;
}

juce::TextButton& NFStressorAudioProcessorEditor::setupToggleButton(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setClickingTogglesState(true);
    content.addAndMakeVisible(button);
    return button;
}

void NFStressorAudioProcessorEditor::setupCaption(juce::Label& label, const juce::String& text, float pointSize,
                                                  bool bold, juce::Justification justification)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(justification);
    label.setFont(juce::Font(juce::FontOptions(pointSize).withStyle(bold ? "Bold" : "Regular")));
    label.setColour(juce::Label::textColourId, textLight);
    label.setInterceptsMouseClicks(false, false);
}

bool NFStressorAudioProcessorEditor::isOptoActive() const
{
    return currentRatioIndex == NF::kOptoRatioIndex
        && attackKnob.getValue() >= 9.95
        && releaseKnob.getValue() <= 0.05;
}

void NFStressorAudioProcessorEditor::timerCallback()
{
    grMeter.setGainReductionDb(audioProcessor.getGainReductionDb());
    optoLed.setOn(isOptoActive());

    // BYPASS blinks while engaged, at 30Hz timer / 15 ticks ~ 1s full cycle,
    // so it's hard to miss that the plugin is currently doing nothing.
    const bool bypassOn = audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f;
    if (bypassOn)
    {
        blinkCounter = (blinkCounter + 1) % 15;
        powerButton.getProperties().set("blinkVisible", blinkCounter < 8);
    }
    else
    {
        blinkCounter = 0;
        powerButton.getProperties().set("blinkVisible", true);
    }
    powerButton.repaint();

    // Refresh the per-knob glow intensity so it tracks the live value —
    // turning a knob up visibly brightens the light leaking out around it.
    panel.setKnobGlowValues({
        (float) inputKnob.valueToProportionOfLength(inputKnob.getValue()),
        (float) attackKnob.valueToProportionOfLength(attackKnob.getValue()),
        (float) releaseKnob.valueToProportionOfLength(releaseKnob.getValue()),
        (float) outputKnob.valueToProportionOfLength(outputKnob.getValue()),
        (float) mixKnob.valueToProportionOfLength(mixKnob.getValue())
    });
}

void NFStressorAudioProcessorEditor::paint(juce::Graphics&) {}

void NFStressorAudioProcessorEditor::resized()
{
    // `content` is laid out once at the fixed design size; dragging the
    // corner resizer just rescales that single component as one block so
    // every knob/button/label keeps its relative position and proportions.
    const float scale = juce::jmin((float) getWidth() / (float) designWidth,
                                   (float) getHeight() / (float) designHeight);
    content.setTransform(juce::AffineTransform::scale(scale));
}

void NFStressorAudioProcessorEditor::layOutContent()
{
    auto bounds = content.getLocalBounds();
    panel.setBounds(bounds);
    bounds.reduce(10, 10);

    // --- Top plate --------------------------------------------------
    const int topSectionTop = bounds.getY();
    auto topRow = bounds.removeFromTop(38);
    // BYPASS sits centred in its own slot on the right, sized to fit inside
    // the row's own height rather than overflowing it (a taller square here
    // used to poke past the panel's top inset and clip against its rounded
    // corner).
    auto powerButtonSlot = topRow.removeFromRight(60);
    constexpr int bypassWidth = 54, bypassHeight = 26;
    powerButton.setBounds(juce::Rectangle<int>(bypassWidth, bypassHeight).withCentre(powerButtonSlot.getCentre()));
    // Mirrors BYPASS's slot on the right, so the title still centres — the
    // preset menu button (hamburger icon) sits centred in this same space.
    auto menuButtonSlot = topRow.removeFromLeft(60);
    menuButton.setBounds(juce::Rectangle<int>(32, 24).withCentre(menuButtonSlot.getCentre()));
    titleLabel.setBounds(topRow);
    // Double-clicking the "NF - STRESSOR" logo itself also resets the
    // window to its default size (see BackgroundPanel::mouseDoubleClick).
    panel.setLogoHotZone(topRow);

    // Tagline gets the full row width, centred the same way the title is —
    // nothing else shares this row now.
    auto taglineRow = bounds.removeFromTop(20);
    taglineLabel.setBounds(taglineRow.translated(0, -4));

    // A touch shorter than before, so the knob tray below (stretched up a
    // little more) can meet it neatly rather than leaving a big gap.
    juce::Rectangle<int> topSectionInset(bounds.getX(), topSectionTop, bounds.getWidth(), bounds.getY() - topSectionTop + 6);

    bounds.removeFromTop(13); // raises the whole knob row block a touch higher still

    // --- Knobs + GR ladder -------------------------------------------
    // Compact vertical rhythm and large knobs relative to their row, matching
    // the reference's dense, hardware-strip proportions.
    auto mainArea = bounds.removeFromTop((int) (bounds.getHeight() * 0.62f));
    // Stretched up close to (but not touching) the tagline row above, for a
    // tight, tidy gap instead of a loose one. Stretched further down than
    // mainArea's own bottom edge too — OUTPUT's knob (like all four) bleeds
    // 7px past its row via the negative reduce below, so the tray needs at
    // least that much clearance or its bottom border cuts across the knob.
    juce::Rectangle<int> knobSectionInset(mainArea.getX() - 2, mainArea.getY() - 17,
                                          mainArea.getWidth() + 4, mainArea.getHeight() + 17 + 10);
    const int mainAreaCentreX = mainArea.getCentreX(); // same centre MIX uses below,
                                                       // so the top 4 knobs line up with it
    auto meterColumn = mainArea.removeFromRight((int) (mainArea.getWidth() * 0.26f));

    juce::Slider* knobs[] { &inputKnob, &attackKnob, &releaseKnob, &outputKnob };
    juce::Label* knobCaptions[] { &inputCaption, &attackCaption, &releaseCaption, &outputCaption };
    const int knobGap = 16;
    const int knobRowHeight = (mainArea.getHeight() - knobGap * 3) / 4;

    std::vector<BackgroundPanel::KnobGlow> knobGlowSpots;
    auto addKnobGlow = [&knobGlowSpots](juce::Slider& knob)
    {
        auto kb = knob.getBounds().toFloat();
        const float r = juce::jmin(kb.getWidth(), kb.getHeight()) * 0.5f - 4.0f;
        const float value = (float) knob.valueToProportionOfLength(knob.getValue());
        knobGlowSpots.push_back({ kb.getCentre(), r, value });
    };

    int optoLedX = 0;

    for (int i = 0; i < 4; ++i)
    {
        // Every row is exactly knobRowHeight tall with nothing else nudging
        // it — that even, identical spacing is what keeps all four knobs
        // (and their captions) perfectly aligned on the same grid.
        auto row = mainArea.removeFromTop(knobRowHeight);
        auto captionRow = row.removeFromTop(15);
        const int shiftX = mainAreaCentreX - row.getCentreX();
        knobCaptions[i]->setBounds(captionRow.translated(shiftX, -5));
        // Knobs are height-bound (row is much wider than tall), so a small
        // negative vertical reduce is what actually makes them bigger —
        // bleeding a few px up into the caption band and down into the gap
        // below, both of which have slack to spare.
        knobs[i]->setBounds(row.reduced(row.getWidth() / 22, -7).translated(shiftX, 0));
        addKnobGlow(*knobs[i]);

        if (i == 1)
            optoLedX = captionRow.getCentreX() + shiftX - captionRow.getWidth() * 2 / 5 - 14;

        if (i < 3)
            mainArea.removeFromTop(knobGap);
    }

    // OPTO indicator, beside the word ATTACK — lit (and clickable) when
    // RATIO is at 10:1 and ATTACK/RELEASE are still sitting at their OPTO
    // detent (see isOptoActive()).
    {
        const int ledCentreY = attackCaption.getBounds().getCentreY();
        optoLed.setBounds(juce::Rectangle<int>(36, 36).withCentre({ optoLedX, ledCentreY }));
        optoLabel.setBounds(juce::Rectangle<int>(54, 15).withCentre({ optoLedX, ledCentreY + 24 }));
    }

    // SLOW/FAST tick marks — each knob gets one at "0" (the rotary's start
    // angle) and one at "10" (its end angle), labelled according to which
    // end is actually slow/fast for that parameter (see mapAttackMs/
    // mapReleaseMs in StressorEngine.cpp): ATTACK is slow at 0 / fast at 10;
    // RELEASE is fast at 0 / slow at 10.
    {
        constexpr float zeroAngle = juce::MathConstants<float>::pi * 1.25f;
        constexpr float tenAngle = juce::MathConstants<float>::pi * 2.75f;
        auto pointAtAngle = [](juce::Rectangle<int> knobBounds, float angleRad, float distFromCentre)
        {
            const auto c = knobBounds.toFloat().getCentre();
            return juce::Point<float>(c.x + distFromCentre * std::sin(angleRad),
                                      c.y - distFromCentre * std::cos(angleRad));
        };

        // `onLeft` picks which edge of the label box sits at the tick (right
        // edge for the "0" side, which reads leftward off the knob; left
        // edge for the "10" side, which reads rightward off it).
        auto makeTickAndLabel = [&](juce::Slider& knob, juce::Label& label, float angleRad, bool onLeft)
        {
            const auto kb = knob.getBounds();
            const float radius = juce::jmin(kb.getWidth(), kb.getHeight()) * 0.5f;
            const auto tickStart = pointAtAngle(kb, angleRad, radius + 2.0f);
            const auto tickEnd = pointAtAngle(kb, angleRad, radius + 11.0f);
            const auto labelPos = pointAtAngle(kb, angleRad, radius + 15.0f);
            auto labelBounds = juce::Rectangle<int>(64, 19).withY((int) labelPos.y - 9);
            label.setBounds(onLeft ? labelBounds.withRightX((int) labelPos.x)
                                   : labelBounds.withX((int) labelPos.x));
            return juce::Line<float>(tickStart, tickEnd);
        };

        panel.setIndicatorTicks({
            makeTickAndLabel(attackKnob, attackSlowLabel, zeroAngle, true),
            makeTickAndLabel(attackKnob, attackFastLabel, tenAngle, false),
            makeTickAndLabel(releaseKnob, releaseFastLabel, zeroAngle, true),
            makeTickAndLabel(releaseKnob, releaseSlowLabel, tenAngle, false)
        });
    }

    // The GR caption sits in a fixed-height band directly above the meter;
    // the whole (caption + meter) block is then shifted down within the
    // available column so the 4th LED (GRLadderMeter's steps[3] == 4 dB)
    // lands on the same horizontal line as the OPTO indicator.
    auto meterPadded = meterColumn.reduced(3, 8);
    constexpr int grCaptionHeight = 20;
    constexpr int numGrSteps = 16; // must match GRLadderMeter::steps.size()
    const int meterHeight = meterPadded.getHeight() - grCaptionHeight;
    const float grRowHeight = (float) meterHeight / (float) numGrSteps;
    const int optoLedCentreY = optoLed.getBounds().getCentreY();
    int meterTopY = (int) std::lround(optoLedCentreY - grRowHeight * 3.5f);
    // Never push it above the column's own top (would overlap INPUT's row).
    meterTopY = juce::jmax(meterTopY, meterPadded.getY() + grCaptionHeight);

    // Centred directly over the first LED itself (GRLadderMeter's number
    // column is the left 55% of its width, the LED centred in the middle
    // 60% of the remaining 45% — see GRLadderMeter::paint).
    const int grCentreX = meterPadded.getX() + (int) (meterPadded.getWidth() * 0.775f);
    grCaption.setBounds(juce::Rectangle<int>(60, grCaptionHeight)
                             .withCentre({ grCentreX, meterTopY - grCaptionHeight / 2 }));
    grMeter.setBounds(meterPadded.getX(), meterTopY, meterPadded.getWidth(), meterHeight);

    bounds.removeFromTop(12); // a little more room, so the knob tray's extra bottom stretch (for OUTPUT's bleed) has space to sit in without touching the RATIO tray below

    // --- Ratio row -----------------------------------------------------
    const int ratioSectionTop = bounds.getY();
    ratioCaption.setBounds(bounds.removeFromTop(20));
    auto ratioRow = bounds.removeFromTop(32);
    const int ratioButtonWidth = ratioRow.getWidth() / ratioButtons.size();
    for (auto* button : ratioButtons)
        button->setBounds(ratioRow.removeFromLeft(ratioButtonWidth).reduced(2, 0));
    juce::Rectangle<int> ratioSectionInset(bounds.getX(), ratioSectionTop, bounds.getWidth(), bounds.getY() - ratioSectionTop);

    bounds.removeFromTop(12);

    // --- Character grid, full width --------------------------------------
    const int charSectionTop = bounds.getY();
    auto captionRow = bounds.removeFromTop(20);
    detectorCaption.setBounds(captionRow.removeFromLeft(captionRow.getWidth() / 2));
    audioCaption.setBounds(captionRow);

    auto charRow1 = bounds.removeFromTop(38);
    hpButton.setBounds(charRow1.removeFromLeft(charRow1.getWidth() / 2).reduced(3, 2));
    dist2Button.setBounds(charRow1.reduced(3, 2));

    bounds.removeFromTop(4);

    auto charRow2 = bounds.removeFromTop(38);
    linkButton.setBounds(charRow2.removeFromLeft(charRow2.getWidth() / 2).reduced(3, 2));
    dist3Button.setBounds(charRow2.reduced(3, 2));
    juce::Rectangle<int> charSectionInset(bounds.getX(), charSectionTop, bounds.getWidth(), bounds.getY() - charSectionTop);

    bounds.removeFromTop(14);

    // --- Mix, same size as the main knobs, at the bottom, with the square
    // NUKE brick-wall-limiter button beside it. NUKE's space is trimmed
    // symmetrically off both sides of the row (same trick as the BYPASS/
    // title row above) so the MIX knob still centres on mainAreaCentreX
    // instead of drifting toward the middle of a lopsidedly-narrowed area.
    const int mixSectionTop = bounds.getY();
    bounds.removeFromTop(10);
    mixCaption.setBounds(bounds.removeFromTop(12).translated(0, -5));
    auto mixArea = bounds.removeFromTop(knobRowHeight - 12);
    auto nukeArea = mixArea.removeFromRight(70);
    auto brandArea = mixArea.removeFromLeft(70);
    mixKnob.setBounds(mixArea.reduced(mixArea.getWidth() / 10, -7).translated(0, 4));
    addKnobGlow(mixKnob);
    // Same size as BYPASS (50, see the top-right corner above) — +4 to
    // match the same downward nudge applied to mixKnob, so NUKE stays
    // vertically aligned with it.
    constexpr int nukeSize = 50;
    nukeButton.setBounds(juce::Rectangle<int>(nukeSize, nukeSize).withCentre(nukeArea.getCentre().translated(0, 4)));
    panel.setBrandLabelArea(brandArea.translated(-4, 4));
    // Stretched a bit further down than the raw row height, since the MIX
    // knob itself bleeds a few px past mixArea's own bottom edge (see the
    // negative vertical reduce above) — without this the tray's bottom edge
    // cut across the knob instead of framing it.
    juce::Rectangle<int> mixSectionInset(bounds.getX(), mixSectionTop, bounds.getWidth(), bounds.getY() - mixSectionTop + 10);

    panel.setInsetPanels({ topSectionInset.expanded(2, 3), knobSectionInset, ratioSectionInset.expanded(2, 3),
                          charSectionInset.expanded(2, 3), mixSectionInset.expanded(2, 3) });
    panel.setKnobGlows(knobGlowSpots);

    constexpr int versionW = 54, versionH = 19;
    versionLabel.setBounds(10, content.getHeight() - versionH - 6, versionW, versionH);
}
