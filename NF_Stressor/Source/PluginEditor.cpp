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
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(tray, 6.0f, 1.0f);
    }

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

    // Brand medallion, top-left of the title row.
    {
        const auto c = juce::Point<float>(22.0f, 25.0f);
        const float r = 11.0f;
        juce::ColourGradient badge(pointerTip.brighter(0.2f), c.x - r, c.y - r, pointerTip.darker(0.4f), c.x + r, c.y + r, false);
        g.setGradientFill(badge);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour(textLight);
        g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
        g.drawFittedText("NF", juce::Rectangle<float>(c.x - r, c.y - r, r * 2.0f, r * 2.0f).toNearestInt(),
                         juce::Justification::centred, 1);
    }
}

NFStressorAudioProcessorEditor::NFStressorAudioProcessorEditor(NFStressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(content);
    content.addAndMakeVisible(panel);

    setupCaption(titleLabel, "NF - STRESSOR", 15.0f, true);
    content.addAndMakeVisible(titleLabel);

    setupCaption(taglineLabel, "REDLINE  /  1% THD", 9.0f, false);
    taglineLabel.setColour(juce::Label::textColourId, textDim);
    content.addAndMakeVisible(taglineLabel);

    powerButton.setButtonText(juce::String::fromUTF8("\xe2\x8f\xbb"));
    powerButton.setClickingTogglesState(true);
    content.addAndMakeVisible(powerButton);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bypass", powerButton);

    auto setupMainKnob = [this](juce::Slider& knob, juce::Label& caption, const juce::String& text)
    {
        setupKnob(knob);
        content.addAndMakeVisible(knob);
        setupCaption(caption, text, 10.0f, true);
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

    content.addAndMakeVisible(grMeter);

    setupCaption(ratioCaption, "RATIO", 9.0f, true);
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
            for (int i = 0; i < ratioButtons.size(); ++i)
                ratioButtons[i]->setToggleState(i == currentRatioIndex, juce::dontSendNotification);
            taglineLabel.setColour(juce::Label::textColourId,
                                   currentRatioIndex >= 5 ? amber : textDim);
        });
    ratioAttachment->sendInitialUpdate();

    setupCaption(detectorCaption, "DETECTOR", 8.5f, true);
    content.addAndMakeVisible(detectorCaption);
    setupCaption(audioCaption, "AUDIO", 8.5f, true);
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
    setupCaption(mixCaption, "MIX", 9.0f, true);
    content.addAndMakeVisible(mixCaption);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "mix", mixKnob);

    setupCaption(footerLabel, "NF - Stressor", 10.0f, false);
    footerLabel.setColour(juce::Label::textColourId, textDim);
    content.addAndMakeVisible(footerLabel);

    content.setBounds(0, 0, designWidth, designHeight);
    layOutContent();

    setResizable(true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio((double) designWidth / (double) designHeight);
        constrainer->setSizeLimits(designWidth / 2, designHeight / 2,
                                   (int) (designWidth * 1.5f), (int) (designHeight * 1.5f));
    }

    // Open a little smaller than the full design size so the whole window —
    // corner resizer included — comfortably fits on screen at first launch.
    setSize((int) (designWidth * 0.8f), (int) (designHeight * 0.8f));

    startTimerHz(30);
}

NFStressorAudioProcessorEditor::~NFStressorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

juce::Slider& NFStressorAudioProcessorEditor::setupKnob(juce::Slider& knob)
{
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

void NFStressorAudioProcessorEditor::timerCallback()
{
    grMeter.setGainReductionDb(audioProcessor.getGainReductionDb());
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
    auto topRow = bounds.removeFromTop(30);
    powerButton.setBounds(topRow.removeFromRight(26));
    topRow.removeFromLeft(26); // clears the brand medallion painted on the panel
    titleLabel.setBounds(topRow);

    taglineLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(8);

    // --- Knobs + GR ladder -------------------------------------------
    auto mainArea = bounds.removeFromTop((int) (bounds.getHeight() * 0.62f));
    auto knobSectionInset = mainArea.expanded(2, 4);
    auto meterColumn = mainArea.removeFromRight((int) (mainArea.getWidth() * 0.30f));

    juce::Slider* knobs[] { &inputKnob, &attackKnob, &releaseKnob, &outputKnob };
    juce::Label* knobCaptions[] { &inputCaption, &attackCaption, &releaseCaption, &outputCaption };
    const int knobRowHeight = mainArea.getHeight() / 4;

    for (int i = 0; i < 4; ++i)
    {
        auto row = mainArea.removeFromTop(knobRowHeight);
        knobCaptions[i]->setBounds(row.removeFromTop(16));
        knobs[i]->setBounds(row.reduced(row.getWidth() / 5, 2));
    }

    grMeter.setBounds(meterColumn.reduced(4, 16));

    bounds.removeFromTop(10);

    // --- Ratio row -----------------------------------------------------
    const int ratioSectionTop = bounds.getY();
    ratioCaption.setBounds(bounds.removeFromTop(14));
    auto ratioRow = bounds.removeFromTop(30);
    const int ratioButtonWidth = ratioRow.getWidth() / ratioButtons.size();
    for (auto* button : ratioButtons)
        button->setBounds(ratioRow.removeFromLeft(ratioButtonWidth).reduced(2, 0));
    juce::Rectangle<int> ratioSectionInset(bounds.getX(), ratioSectionTop, bounds.getWidth(), bounds.getY() - ratioSectionTop);

    bounds.removeFromTop(12);

    // --- Character grid --------------------------------------------
    const int charSectionTop = bounds.getY();
    auto captionRow = bounds.removeFromTop(14);
    detectorCaption.setBounds(captionRow.removeFromLeft(captionRow.getWidth() / 2));
    audioCaption.setBounds(captionRow);

    auto charRow1 = bounds.removeFromTop(38);
    hpButton.setBounds(charRow1.removeFromLeft(charRow1.getWidth() / 2).reduced(4, 2));
    dist2Button.setBounds(charRow1.reduced(4, 2));

    bounds.removeFromTop(6);

    auto charRow2 = bounds.removeFromTop(38);
    linkButton.setBounds(charRow2.removeFromLeft(charRow2.getWidth() / 2).reduced(4, 2));
    dist3Button.setBounds(charRow2.reduced(4, 2));
    juce::Rectangle<int> charSectionInset(bounds.getX(), charSectionTop, bounds.getWidth(), bounds.getY() - charSectionTop);

    bounds.removeFromTop(14);

    // --- Mix -------------------------------------------------------
    const int mixSectionTop = bounds.getY();
    mixCaption.setBounds(bounds.removeFromTop(14));
    auto mixArea = bounds.removeFromTop(90);
    mixKnob.setBounds(mixArea.withSizeKeepingCentre(80, 80));
    juce::Rectangle<int> mixSectionInset(bounds.getX(), mixSectionTop, bounds.getWidth(), bounds.getY() - mixSectionTop);

    // --- Footer ------------------------------------------------------
    footerLabel.setBounds(bounds.removeFromBottom(24));

    panel.setInsetPanels({ knobSectionInset, ratioSectionInset.expanded(2, 3),
                          charSectionInset.expanded(2, 3), mixSectionInset.expanded(2, 3) });
}
