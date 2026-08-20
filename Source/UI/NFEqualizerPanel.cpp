#include "NFEqualizerPanel.h"

namespace
{
    using DM = DesignMetrics;

    enum class KnobFormat { Gain, Frequency, Percent, Ratio };

    void applyFormat(NFKnob& knob, KnobFormat format)
    {
        switch (format)
        {
            case KnobFormat::Gain:
                knob.textFromValueFunction = [](double v)
                {
                    return juce::String(v, 1) + " dB";
                };
                knob.valueFromTextFunction = [](const juce::String& t)
                {
                    return t.retainCharacters("0123456789.-").getDoubleValue();
                };
                break;

            case KnobFormat::Frequency:
                knob.textFromValueFunction = [](double v)
                {
                    if (v >= 1000.0)
                        return juce::String(v / 1000.0, 2) + " kHz";

                    return juce::String((int) std::round(v)) + " Hz";
                };
                knob.valueFromTextFunction = [](const juce::String& t)
                {
                    const bool isKilo = t.containsIgnoreCase("k");
                    const double numeric =
                        t.retainCharacters("0123456789.-").getDoubleValue();
                    return isKilo ? numeric * 1000.0 : numeric;
                };
                break;

            case KnobFormat::Percent:
                knob.textFromValueFunction = [](double v)
                {
                    return juce::String((int) std::round(v * 100.0)) + " %";
                };
                knob.valueFromTextFunction = [](const juce::String& t)
                {
                    return t.retainCharacters("0123456789.-").getDoubleValue() / 100.0;
                };
                break;

            case KnobFormat::Ratio:
                knob.textFromValueFunction = [](double v)
                {
                    return juce::String(v, 2);
                };
                knob.valueFromTextFunction = [](const juce::String& t)
                {
                    return t.retainCharacters("0123456789.-").getDoubleValue();
                };
                break;
        }

        knob.updateText();
    }

    // gapBeforeTextBox inserts breathing room between the dial and its
    // value box below (JUCE's TextBoxBelow otherwise butts them together).
    juce::Rectangle<int> knobBounds(float cx, float cy, float d, float textBoxH = 18.0f,
                                    float gapBeforeTextBox = 0.0f)
    {
        return { (int) (cx - d / 2.0f), (int) (cy - d / 2.0f),
                (int) d, (int) (d + gapBeforeTextBox + textBoxH) };
    }

    void drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text)
    {
        g.setColour(NFColours::black);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(NFColours::fluorescentGreen.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

        g.setColour(NFColours::fluorescentGreen);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        NFGraphics::drawBoldText(g, text, bounds.toNearestInt(), juce::Justification::centred);
    }

    void drawScrew(juce::Graphics& g, juce::Point<float> centre)
    {
        constexpr float r = 7.0f;

        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(centre.x - r, centre.y - r + 1.0f, r * 2.0f, r * 2.0f);

        juce::ColourGradient body(juce::Colour(0xff3A3A3A), centre.x - r * 0.4f, centre.y - r * 0.5f,
                                  juce::Colour(0xff0C0C0C), centre.x + r * 0.5f, centre.y + r * 0.6f, true);
        g.setGradientFill(body);
        g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

        g.setColour(juce::Colours::black);
        g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.2f);

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.fillEllipse(centre.x - r * 0.45f, centre.y - r * 0.55f, r * 0.5f, r * 0.35f);

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(0.5f, centre.x, centre.y));
        g.setColour(juce::Colour(0xff050505));
        g.drawLine(centre.x - r * 0.65f, centre.y, centre.x + r * 0.65f, centre.y, 1.4f);
        g.restoreState();
    }

    // Places the min/max numbers right where the first and last tick marks
    // are, matching the knob's actual rotary geometry (the ticks sweep from
    // lower-left to lower-right, not a flat left/right split at mid-height).
    void drawKnobRange(juce::Graphics& g, const NFKnob& knob,
                       const juce::String& lowText, const juce::String& highText,
                       juce::Colour colour = NFColours::black, float fontSize = 8.5f,
                       float labelOffset = 20.0f, int labelBoxW = 40,
                       juce::Colour haloColour = juce::Colours::transparentBlack, float haloWidth = 0.0f)
    {
        auto b = knob.getBounds();
        const int dialHeight = juce::jmax(1, b.getHeight() - 18);

        auto dialBounds = juce::Rectangle<float>((float) b.getX(), (float) b.getY(),
                                                  (float) b.getWidth(), (float) dialHeight)
                              .reduced(9.0f);
        const auto centre = dialBounds.getCentre();
        const float radius = juce::jmin(dialBounds.getWidth(), dialBounds.getHeight()) * 0.42f;

        constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
        constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;
        const float labelRadius = radius + labelOffset;

        auto placeAt = [&](float angle, const juce::String& text)
        {
            // The knob's own rotary-angle convention needs a quarter-turn
            // correction to map onto plain cos/sin (JUCE's rotary angles are
            // "0 = 12 o'clock, clockwise"; cos/sin expect "0 = 3 o'clock").
            // Without it, both labels drift toward the left/below instead of
            // splitting cleanly to lower-left / lower-right.
            const float screenAngle = angle - juce::MathConstants<float>::halfPi;
            const float tx = centre.x + std::cos(screenAngle) * labelRadius;
            const float ty = centre.y + std::sin(screenAngle) * labelRadius;
            const int w = labelBoxW, h = 18;
            juce::Rectangle<float> area(tx - w / 2.0f, ty - h / 2.0f, (float) w, (float) h);
            NFGraphics::drawThickText(g, text, area,
                                      juce::Font(juce::FontOptions(fontSize, juce::Font::bold)),
                                      juce::Justification::centred, colour, 0.5f,
                                      haloColour, haloWidth);
        };

        placeAt(startAngle, lowText);
        placeAt(endAngle, highText);
    }

    void drawLogo(juce::Graphics& g, juce::Colour textColour,
                 juce::Colour haloColour, float haloWidth)
    {
        // True centre of the green face, not the raw (slightly off-centre)
        // DesignMetrics logo box, so the lockup sits dead-centre on the
        // plugin regardless of the box the spec numbers implied.
        const float centreX = DM::faceX + DM::faceW / 2.0f;

        auto nfArea = juce::Rectangle<float>(centreX - 110.0f, 25.0f, 220.0f, 47.0f);
        auto subArea = juce::Rectangle<float>(centreX - 150.0f, 71.0f, 300.0f, 18.0f);

        // No horizontal stretch — a stretched glyph is what read as
        // "out of formatting". Real weight comes from filling AND stroking
        // the glyph outlines (drawThickText), not from a bigger font alone.
        NFGraphics::drawThickText(g, "NF", nfArea,
                                  juce::Font(juce::FontOptions(40.0f, juce::Font::bold)),
                                  juce::Justification::centred, textColour, 1.3f,
                                  haloColour, haloWidth);

        NFGraphics::drawThickText(g, "EQUALIZER PRO 1", subArea,
                                  juce::Font(juce::FontOptions(14.0f, juce::Font::bold)),
                                  juce::Justification::centredTop, textColour, 0.65f,
                                  haloColour, haloWidth * 0.75f);
    }

    // Ticks + dB numbers always sit to the LEFT of the meter, stacked
    // neatly one under the other, right-aligned against the tick.
    void drawMeterScale(juce::Graphics& g, juce::Rectangle<float> meterBounds)
    {
        constexpr float minDb = -60.0f, maxDb = 12.0f;
        const std::vector<int> ticks { 12, 6, 0, -6, -12, -18, -24, -36, -48, -60 };
        constexpr float tickLen = 5.0f;
        constexpr float labelW = 26.0f;

        const auto font = juce::Font(juce::FontOptions(9.0f, juce::Font::bold));

        for (auto db : ticks)
        {
            const float y = meterBounds.getBottom() -
                ((float) db - minDb) / (maxDb - minDb) * meterBounds.getHeight();

            // Both the tick strokes and the numbers sit on the near-black
            // INPUT/OUTPUT panel, so they need to be white to read at all.
            g.setColour(juce::Colours::white);
            const float tickX = meterBounds.getX() - tickLen;
            g.drawLine(tickX, y, tickX + tickLen, y, 1.2f);

            auto label = (db > 0 ? "+" : "") + juce::String(db);
            juce::Rectangle<float> area(meterBounds.getX() - tickLen - labelW - 2.0f, y - 6.0f,
                                        labelW, 12.0f);

            NFGraphics::drawThickText(g, label, area, font,
                                     juce::Justification::centredRight,
                                     juce::Colours::white, 0.4f);
        }
    }
}

NFEqualizerPanel::NFEqualizerPanel(NFEqualizerAudioProcessor& p)
    : processor(p)
{
    setLookAndFeel(&nfLookAndFeel);
    setSize(DM::width, DM::height);

    configureKnob(input, "input", "", DM::inputKnobCX, DM::inputKnobCY, DM::inputKnobD);
    applyFormat(input, KnobFormat::Gain);
    input.getProperties().set("lightTicks", true);
    // Narrower value box so the -24/+24 range labels beside the dial have
    // room to clear it horizontally.
    input.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);

    configureKnob(lowFreq, "lowFreq", "FREQUENCY", DM::lowFreqCX, DM::lowFreqCY, DM::lowFreqD);
    applyFormat(lowFreq, KnobFormat::Frequency);
    configureKnob(lowGain, "lowGain", "GAIN", DM::lowGainCX, DM::lowGainCY, DM::lowGainD);
    applyFormat(lowGain, KnobFormat::Gain);

    configureKnob(midFreq, "midFreq", "FREQUENCY", DM::midFreqCX, DM::midFreqCY, DM::midFreqD);
    applyFormat(midFreq, KnobFormat::Frequency);
    configureKnob(midGain, "midGain", "GAIN", DM::midGainCX, DM::midGainCY, DM::midGainD);
    applyFormat(midGain, KnobFormat::Gain);
    configureKnob(midQ, "midQ", "Q", DM::midQCX, DM::midQCY, DM::midQD);
    applyFormat(midQ, KnobFormat::Ratio);

    configureKnob(highFreq, "highFreq", "FREQUENCY", DM::highFreqCX, DM::highFreqCY, DM::highFreqD);
    applyFormat(highFreq, KnobFormat::Frequency);
    configureKnob(highGain, "highGain", "GAIN", DM::highGainCX, DM::highGainCY, DM::highGainD);
    applyFormat(highGain, KnobFormat::Gain);

    configureKnob(drive, "drive", "DRIVE", DM::driveCX, DM::driveCY, DM::driveD);
    applyFormat(drive, KnobFormat::Percent);
    configureKnob(character, "character", "CHARACTER", DM::characterKnobCX, DM::characterKnobCY, DM::characterKnobD);
    applyFormat(character, KnobFormat::Percent);
    configureKnob(mix, "mix", "MIX", DM::mixCX, DM::mixCY, DM::mixD, 18.0f, 16.0f);
    applyFormat(mix, KnobFormat::Percent);

    configureKnob(output, "output", "", DM::outputKnobCX, DM::outputKnobCY, DM::outputKnobD);
    applyFormat(output, KnobFormat::Gain);
    output.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    output.getProperties().set("lightTicks", true);

    for (auto* toggle : { &lowShelf, &highShelf, &bypass })
    {
        toggle->setLookAndFeel(&nfLookAndFeel);
        toggle->setColour(juce::ToggleButton::textColourId, NFColours::white);
        addAndMakeVisible(*toggle);
    }

    bypass.getProperties().set("led", true);
    lowShelf.setBounds((int) DM::lowShelfX, (int) DM::lowShelfY, (int) DM::lowShelfW, (int) DM::lowShelfH);
    highShelf.setBounds((int) DM::highShelfX, (int) DM::highShelfY, (int) DM::highShelfW, (int) DM::highShelfH);
    bypass.setBounds((int) DM::bypassX, (int) DM::bypassY, (int) DM::bypassW, (int) DM::bypassH);

    for (auto* led : { &lowEnableButton, &midEnableButton, &highEnableButton, &characterEnableButton })
        addAndMakeVisible(*led);

    lowEnableButton.setBounds(knobBounds(DM::led1X, DM::ledY, DM::ledD, 0.0f));
    midEnableButton.setBounds(knobBounds(DM::led2X, DM::ledY, DM::ledD, 0.0f));
    highEnableButton.setBounds(knobBounds(DM::led3X, DM::ledY, DM::ledD, 0.0f));
    characterEnableButton.setBounds(knobBounds(DM::led4X, DM::ledY, DM::ledD, 0.0f));

    lowEnableAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "lowEnabled", lowEnableButton);
    midEnableAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "midEnabled", midEnableButton);
    highEnableAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "highEnabled", highEnableButton);
    characterEnableAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "characterEnabled", characterEnableButton);

    inputMeter.setBounds((int) DM::inputMeterX, (int) DM::inputMeterY, (int) DM::inputMeterW, (int) DM::inputMeterH);
    outputMeter.setBounds((int) DM::outputMeterX, (int) DM::outputMeterY, (int) DM::outputMeterW, (int) DM::outputMeterH);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    for (auto* button : { &prevPreset, &nextPreset, &saveButton, &loadButton })
    {
        button->setLookAndFeel(&nfLookAndFeel);
        button->setColour(juce::TextButton::textColourOffId, NFColours::white);
        button->setColour(juce::TextButton::buttonColourId, NFColours::black);
        button->setColour(juce::TextButton::buttonOnColourId, NFColours::black);
        addAndMakeVisible(*button);
    }

    prevPreset.setBounds((int) DM::prevX, (int) DM::prevY, (int) DM::prevW, (int) DM::prevH);
    nextPreset.setBounds((int) DM::nextX, (int) DM::nextY, (int) DM::nextW, (int) DM::nextH);
    saveButton.setBounds((int) DM::saveX, (int) DM::saveY, (int) DM::saveW, (int) DM::saveH);
    loadButton.setBounds((int) DM::loadX, (int) DM::loadY, (int) DM::loadW, (int) DM::loadH);

    prevPreset.onClick = [this]
    {
        processor.presetManager.loadPrevious();
        refreshPresetLabel();
    };

    nextPreset.onClick = [this]
    {
        processor.presetManager.loadNext();
        refreshPresetLabel();
    };

    saveButton.onClick = [this] { showSaveDialog(); };
    loadButton.onClick = [this] { showLoadDialog(); };

    presetName.setJustificationType(juce::Justification::centred);
    presetName.setColour(juce::Label::backgroundColourId, NFColours::black);
    presetName.setColour(juce::Label::textColourId, NFColours::white);
    presetName.setColour(juce::Label::outlineColourId, NFColours::purple);
    presetName.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    presetName.setBounds((int) DM::presetX, (int) DM::presetY, (int) DM::presetW, (int) DM::presetH);
    addAndMakeVisible(presetName);
    refreshPresetLabel();

    // One shared pill (painted in paint()) holds both the caption and the
    // value stepper side by side, rather than stacking two boxes.
    oversamplingCaption.setText("OVERSAMPLING", juce::dontSendNotification);
    oversamplingCaption.setJustificationType(juce::Justification::centredLeft);
    oversamplingCaption.setColour(juce::Label::textColourId, NFColours::white);
    oversamplingCaption.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    oversamplingCaption.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    oversamplingCaption.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    oversamplingCaption.setBounds((int) DM::oversamplingX + 10, (int) DM::oversamplingY,
                                  82, (int) DM::oversamplingH);
    addAndMakeVisible(oversamplingCaption);

    oversamplingBox.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    oversamplingBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    oversamplingBox.setColour(juce::ComboBox::textColourId, NFColours::white);
    oversamplingBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    oversamplingBox.setJustificationType(juce::Justification::centred);
    oversamplingBox.setBounds((int) DM::oversamplingX + 92, (int) DM::oversamplingY,
                              (int) DM::oversamplingW - 92 - 6, (int) DM::oversamplingH);
    addAndMakeVisible(oversamplingBox);

    lowShelfAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "lowShelf", lowShelf);
    highShelfAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "highShelf", highShelf);
    bypassAttachment = std::make_unique<ButtonAttachment>(
        processor.apvts, "bypass", bypass);
    oversamplingAttachment = std::make_unique<ComboBoxAttachment>(
        processor.apvts, "oversampling", oversamplingBox);

    // Skin picker: two small swatch buttons, each always shows its own
    // skin's face colour (not the currently active one), so it reads as
    // a colour picker rather than a mystery toggle.
    skinCaption.setText("SKIN", juce::dontSendNotification);
    skinCaption.setJustificationType(juce::Justification::centredLeft);
    skinCaption.setColour(juce::Label::textColourId, NFColours::white);
    skinCaption.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    skinCaption.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    skinCaption.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    skinCaption.setBounds((int) DM::skinCaptionX, (int) DM::skinCaptionY,
                          (int) DM::skinCaptionW, (int) DM::skinCaptionH);
    addAndMakeVisible(skinCaption);

    auto styleSkinButton = [this](juce::TextButton& button, const NFTheme& previewTheme, int index)
    {
        button.setLookAndFeel(&nfLookAndFeel);
        button.setColour(juce::TextButton::buttonColourId, previewTheme.faceBase);
        button.setColour(juce::TextButton::buttonOnColourId, previewTheme.faceBase);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        button.setBounds((int) (index == 0 ? DM::skinButton1X : DM::skinButton2X), (int) DM::skinButtonY,
                         (int) DM::skinButtonW, (int) DM::skinButtonH);
        addAndMakeVisible(button);
    };

    styleSkinButton(skinButton1, NFTheme::classicGreen(), 0);
    styleSkinButton(skinButton2, NFTheme::purpleNight(), 1);

    skinButton1.onClick = [this] { processor.skinIndex = 0; applyTheme(NFTheme::classicGreen()); };
    skinButton2.onClick = [this] { processor.skinIndex = 1; applyTheme(NFTheme::purpleNight()); };

    applyTheme(NFTheme::byIndex(processor.skinIndex.load()));

    startTimerHz(30);
}

void NFEqualizerPanel::applyTheme(const NFTheme& theme)
{
    currentTheme = theme;
    nfLookAndFeel.setTheme(theme);

    for (auto* led : { &lowEnableButton, &midEnableButton, &highEnableButton, &characterEnableButton })
        led->setAccentColours(theme.accentBorderBright, theme.accentFill, theme.accentBorder);

    for (auto& label : labels)
        label->setColour(juce::Label::textColourId, theme.onFaceText);

    repaint();
}

NFEqualizerPanel::~NFEqualizerPanel()
{
    for (auto* toggle : { &lowShelf, &highShelf, &bypass })
        toggle->setLookAndFeel(nullptr);

    for (auto* button : { &prevPreset, &nextPreset, &saveButton, &loadButton, &skinButton1, &skinButton2 })
        button->setLookAndFeel(nullptr);

    setLookAndFeel(nullptr);
}

void NFEqualizerPanel::configureKnob(
    NFKnob& knob,
    const juce::String& parameterID,
    const juce::String& labelText,
    float cx, float cy, float diameter, float textBoxH, float gapBeforeTextBox)
{
    knob.setBounds(knobBounds(cx, cy, diameter, textBoxH, gapBeforeTextBox));
    addAndMakeVisible(knob);

    if (labelText.isNotEmpty())
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(labelText, juce::dontSendNotification);
        label->setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, NFColours::black);
        label->setJustificationType(juce::Justification::centred);

        const int labelW = (int) diameter + 40;
        const int labelH = 11;
        label->setBounds((int) (cx - labelW / 2.0f), (int) (cy - diameter / 2.0f) - labelH - 2,
                         labelW, labelH);
        addAndMakeVisible(*label);
        labels.push_back(std::move(label));
    }

    attachments.push_back(
        std::make_unique<SliderAttachment>(
            processor.apvts, parameterID, knob));
}

void NFEqualizerPanel::refreshPresetLabel()
{
    presetName.setText(processor.presetManager.getCurrentPresetName(),
                       juce::dontSendNotification);
}

void NFEqualizerPanel::showSaveDialog()
{
    auto* window = new juce::AlertWindow(
        "Save Preset", "Enter a name for this preset:",
        juce::MessageBoxIconType::NoIcon);

    window->addTextEditor("name", processor.presetManager.getCurrentPresetName());
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [this, window](int result)
            {
                if (result == 1)
                {
                    processor.presetManager.saveCurrentAs(
                        window->getTextEditorContents("name"));
                    refreshPresetLabel();
                }

                delete window;
            }),
        false);
}

void NFEqualizerPanel::showLoadDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset", processor.presetManager.getPresetsFolder(),
        "*.nfpreset");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file.existsAsFile())
            {
                processor.presetManager.loadFromFile(file);
                presetName.setText(file.getFileNameWithoutExtension(),
                                   juce::dontSendNotification);
            }
        });
}

void NFEqualizerPanel::timerCallback()
{
    constexpr float floorDb = -60.0f;

    inputMeter.setLevels(
        juce::Decibels::gainToDecibels(processor.getInputLevelL(), floorDb),
        juce::Decibels::gainToDecibels(processor.getInputLevelR(), floorDb));

    outputMeter.setLevels(
        juce::Decibels::gainToDecibels(processor.getOutputLevelL(), floorDb),
        juce::Decibels::gainToDecibels(processor.getOutputLevelR(), floorDb));

    constexpr float disabledAlpha = 0.45f;

    const float lowAlpha = lowEnableButton.getToggleState() ? 1.0f : disabledAlpha;
    lowFreq.setAlpha(lowAlpha);
    lowGain.setAlpha(lowAlpha);
    lowShelf.setAlpha(lowAlpha);

    const float midAlpha = midEnableButton.getToggleState() ? 1.0f : disabledAlpha;
    midFreq.setAlpha(midAlpha);
    midGain.setAlpha(midAlpha);
    midQ.setAlpha(midAlpha);

    const float highAlpha = highEnableButton.getToggleState() ? 1.0f : disabledAlpha;
    highFreq.setAlpha(highAlpha);
    highGain.setAlpha(highAlpha);
    highShelf.setAlpha(highAlpha);

    const float characterAlpha = characterEnableButton.getToggleState() ? 1.0f : disabledAlpha;
    drive.setAlpha(characterAlpha);
    character.setAlpha(characterAlpha);
    mix.setAlpha(characterAlpha);
}

void NFEqualizerPanel::resized()
{
    // All layout is fixed at construction time against DesignMetrics —
    // the panel itself never changes size, only the editor's transform
    // scales it as a whole, so there is nothing to recompute here.
}

void NFEqualizerPanel::paint(juce::Graphics& g)
{
    g.fillAll(NFColours::black);

    juce::Rectangle<float> chassis(DM::chassisX, DM::chassisY, DM::chassisW, DM::chassisH);

    // Chassis: graphite base, fluorescent green rim, dark inner shadow
    g.setColour(NFColours::graphite);
    g.fillRoundedRectangle(chassis, 20.0f);

    g.setColour(NFColours::black);
    g.drawRoundedRectangle(chassis, 20.0f, 3.0f);

    juce::Rectangle<float> face(DM::faceX, DM::faceY, DM::faceW, DM::faceH);

    // Face with a subtle vertical gradient + faint sheen; colours come
    // from the active skin (see applyTheme / NFTheme).
    juce::ColourGradient faceGradient(
        currentTheme.faceBright.withAlpha(0.9f), face.getX(), face.getY(),
        currentTheme.faceDark, face.getX(), face.getBottom(), false);
    faceGradient.addColour(0.4, currentTheme.faceBase);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(face, 16.0f);

    // Very subtle horizontal sheen bands for a brushed-metal feel
    g.saveState();
    juce::Path faceClip;
    faceClip.addRoundedRectangle(face, 16.0f);
    g.reduceClipRegion(faceClip);
    for (int i = 0; i < 6; ++i)
    {
        const float y = face.getY() + face.getHeight() * ((float) i / 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.02f + 0.01f * (i % 2)));
        g.drawLine(face.getX(), y, face.getRight(), y, 1.0f);
    }
    g.restoreState();

    g.setColour(currentTheme.faceOutline);
    g.drawRoundedRectangle(face, 16.0f, 2.0f);
    g.setColour(currentTheme.faceDark.withAlpha(0.6f));
    g.drawRoundedRectangle(face.reduced(3.0f), 14.0f, 1.0f);

    // Screws
    drawScrew(g, { DM::screwLeftX, DM::screwTopY });
    drawScrew(g, { DM::screwNearInputX, DM::screwTopY });
    drawScrew(g, { DM::screwNearOutputX, DM::screwTopY });
    drawScrew(g, { DM::screwRightX, DM::screwTopY });
    drawScrew(g, { DM::screwLeftX, DM::screwBottomY });
    drawScrew(g, { DM::screwRightX, DM::screwBottomY });

    // Logo — bold, direct on the face, no plate behind it
    drawLogo(g, currentTheme.onFaceText, currentTheme.haloColour, currentTheme.haloWidth);

    // INPUT / OUTPUT — near-black panels, always outlined in green
    // regardless of skin (they never take the main face colour).
    for (auto panelRect : { juce::Rectangle<float>(DM::inputX, DM::inputY, DM::inputW, DM::inputH),
                            juce::Rectangle<float>(DM::outputX, DM::outputY, DM::outputW, DM::outputH) })
    {
        g.setColour(NFColours::panelBlack);
        g.fillRoundedRectangle(panelRect, 14.0f);
        g.setColour(NFColours::fluorescentGreen.withAlpha(0.8f));
        g.drawRoundedRectangle(panelRect, 14.0f, 1.6f);
    }

    // LOW / MID / HIGH / NF CHARACTER — same green as the face, black outline only
    juce::Rectangle<float> lowPanel(DM::lowX, DM::lowY, DM::lowW, DM::lowH);
    juce::Rectangle<float> midPanel(DM::midX, DM::lowY, DM::midEndX - DM::midX, DM::lowH);
    juce::Rectangle<float> highPanel(DM::highX, DM::lowY, DM::characterX - DM::highX, DM::lowH);
    juce::Rectangle<float> characterPanel(DM::characterX, DM::lowY, DM::characterEndX - DM::characterX, DM::lowH);

    for (auto panelRect : { lowPanel, midPanel, highPanel, characterPanel })
    {
        g.setColour(NFColours::black);
        g.drawRoundedRectangle(panelRect, 10.0f, 2.0f);
    }

    // Headers
    drawBadge(g, { DM::inputHeaderX, DM::inputHeaderY, DM::inputHeaderW, DM::inputHeaderH }, "INPUT");
    drawBadge(g, { DM::lowHeaderX, DM::lowHeaderY, DM::lowHeaderW, DM::lowHeaderH }, "LOW");
    drawBadge(g, { DM::midHeaderCX - DM::midHeaderW / 2.0f, DM::midHeaderY, DM::midHeaderW, DM::midHeaderH }, "MID");
    drawBadge(g, { DM::highHeaderX, DM::highHeaderY, DM::highHeaderW, DM::highHeaderH }, "HIGH");
    drawBadge(g, { DM::characterCX - DM::characterHeaderW / 2.0f, DM::characterHeaderY,
                  DM::characterHeaderW, DM::characterHeaderH }, "NF CHARACTER");
    drawBadge(g, { DM::outputHeaderX, DM::outputHeaderY, DM::outputHeaderW, DM::outputHeaderH }, "OUTPUT");

    // Knob range labels
    drawKnobRange(g, input, "-24", "+24", NFColours::white, 13.0f, 40.0f, 28);
    drawKnobRange(g, output, "-24", "+24", NFColours::white, 13.0f, 40.0f, 28);
    drawKnobRange(g, lowFreq, "30 Hz", "500 Hz", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, lowGain, "-15", "+15", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, midFreq, "200 Hz", "8.00 kHz", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, midGain, "-15", "+15", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, midQ, "0.25", "4.00", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, highFreq, "2.00 kHz", "20.0 kHz", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, highGain, "-15", "+15", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, drive, "0%", "100%", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, character, "0%", "100%", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);
    drawKnobRange(g, mix, "0%", "100%", currentTheme.onFaceText, 8.5f, 20.0f, 40,
                 currentTheme.haloColour, currentTheme.haloWidth);

    // Meter scales + captions
    juce::Rectangle<float> inputMeterBounds((float) inputMeter.getX(), (float) inputMeter.getY(),
                                            (float) inputMeter.getWidth(), (float) inputMeter.getHeight());
    juce::Rectangle<float> outputMeterBounds((float) outputMeter.getX(), (float) outputMeter.getY(),
                                             (float) outputMeter.getWidth(), (float) outputMeter.getHeight());

    drawMeterScale(g, inputMeterBounds);
    drawMeterScale(g, outputMeterBounds);

    g.setColour(NFColours::black);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    NFGraphics::drawBoldText(g, "INPUT",
                             inputMeter.getBounds().translated(0, inputMeter.getHeight() + 3).withHeight(14),
                             juce::Justification::centred);
    NFGraphics::drawBoldText(g, "OUTPUT",
                             outputMeter.getBounds().translated(0, outputMeter.getHeight() + 3).withHeight(14),
                             juce::Justification::centred);

    // Bottom control bar backdrop
    auto strip = juce::Rectangle<float>(DM::bypassX - 20.0f, DM::bypassY - 6.0f,
                                        (DM::oversamplingX + DM::oversamplingW) - (DM::bypassX - 20.0f) + 20.0f,
                                        DM::bypassH + 12.0f);

    g.setColour(NFColours::black);
    g.fillRoundedRectangle(strip, 10.0f);
    g.setColour(currentTheme.accentBorder.withAlpha(0.6f));
    g.drawRoundedRectangle(strip.reduced(1.5f), 8.0f, 1.4f);

    // OVERSAMPLING pill — one shared box behind the caption + stepper so
    // they read as a single control rather than two stacked pieces.
    juce::Rectangle<float> oversamplingPill(DM::oversamplingX, DM::oversamplingY,
                                            DM::oversamplingW, DM::oversamplingH);
    g.setColour(NFColours::black);
    g.fillRoundedRectangle(oversamplingPill, 6.0f);
    g.setColour(currentTheme.accentBorder.withAlpha(0.85f));
    g.drawRoundedRectangle(oversamplingPill.reduced(0.75f), 6.0f, 1.4f);
}
