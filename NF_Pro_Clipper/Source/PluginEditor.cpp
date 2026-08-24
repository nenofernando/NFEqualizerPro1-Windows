#include "PluginEditor.h"
#include <algorithm>

namespace
{
    // Onde cada instalador realmente deposita o manual PDF bilíngue -
    // ver NF_Pro_Clipper/installer/mac/build_installer.sh (pacote docs)
    // e installer/windows-installer.nsi (pasta Manual ao lado do VST3).
    juce::File getManualFile (bool english)
    {
       #if JUCE_MAC
        juce::File dir (
            "/Users/Shared/NF Audio Tools/NF Pro Clipper/Manual");

        return dir.getChildFile (
            english
                ? "NF_Pro_Clipper_Manual_EN.pdf"
                : "NF_Pro_Clipper_Manual_PT.pdf");
       #elif JUCE_WINDOWS
        juce::File dir =
            juce::File::getSpecialLocation (
                juce::File::globalApplicationsDirectory)
                .getChildFile ("NF Audio Tools")
                .getChildFile ("NF Pro Clipper")
                .getChildFile ("Manual");

        return dir.getChildFile (
            english
                ? "NF Pro Clipper Manual (English).pdf"
                : "NF Pro Clipper Manual (Portugues).pdf");
       #else
        return {};
       #endif
    }
}

// ============================================================
// SKINS
//
// Skin 1 = roxa (nova). Skin 2 = preta/amarela (original).
// Só a paleta muda - todo o desenho (chassis, knobs, botões,
// gráfico, medidores) é o mesmo código para as duas.
// ============================================================

NFSkinPalette makeClassicSkin()
{
    NFSkinPalette s;

    s.name = "Skin 2";

    s.accent       = juce::Colour::fromRGB (255, 211, 0);
    s.accentBright = juce::Colour::fromRGB (255, 232, 45);

    s.chassisTop    = juce::Colour::fromRGB (40, 40, 43);
    s.chassisBottom = juce::Colour::fromRGB (7, 7, 8);

    s.knobBodyTop    = juce::Colour::fromRGB (65, 65, 68);
    s.knobBodyMid    = juce::Colour::fromRGB (37, 37, 40);
    s.knobBodyBottom = juce::Colour::fromRGB (7, 7, 8);

    s.buttonTopIdle    = juce::Colour::fromRGB (125, 101, 0);
    s.buttonTopDown     = juce::Colour::fromRGB (66, 53, 0);
    s.buttonBottomIdle = juce::Colour::fromRGB (25, 21, 4);
    s.buttonBottomDown  = juce::Colour::fromRGB (145, 114, 0);
    s.buttonMidStop     = juce::Colour::fromRGB (82, 67, 0);

    return s;
}

NFSkinPalette makePurpleSkin()
{
    NFSkinPalette s;

    s.name = "Skin 1";

    s.accent       = juce::Colour::fromRGB (190, 130, 250);
    s.accentBright = juce::Colours::white;

    s.chassisTop    = juce::Colour::fromRGB (100, 50, 160);
    s.chassisBottom = juce::Colour::fromRGB (28, 14, 45);

    s.knobBodyTop    = juce::Colour::fromRGB (145, 95, 195);
    s.knobBodyMid    = juce::Colour::fromRGB (90, 50, 135);
    s.knobBodyBottom = juce::Colour::fromRGB (28, 14, 45);

    s.buttonTopIdle    = juce::Colour::fromRGB (145, 85, 210);
    s.buttonTopDown     = juce::Colour::fromRGB (75, 35, 130);
    s.buttonBottomIdle = juce::Colour::fromRGB (50, 25, 90);
    s.buttonBottomDown  = juce::Colour::fromRGB (170, 115, 235);
    s.buttonMidStop     = juce::Colour::fromRGB (110, 65, 170);

    return s;
}

NFSkinPalette getSkinByIndex (int index)
{
    return index == 0 ? makePurpleSkin() : makeClassicSkin();
}

// ============================================================
// KNOB
// ============================================================

void NFProClipperLookAndFeel::
drawRotarySlider (
    juce::Graphics& g,

    int x,
    int y,
    int width,
    int height,

    float sliderPos,

    float rotaryStartAngle,
    float rotaryEndAngle,

    juce::Slider&)
{
    auto bounds =
        juce::Rectangle<float>(
            (float)x,
            (float)y,
            (float)width,
            (float)height)

        .reduced (5.0f);

    float radius =
        juce::jmin (
            bounds.getWidth(),
            bounds.getHeight())

        * 0.33f;

    auto centre =
        bounds.getCentre();

    float angle =
        rotaryStartAngle

        +

        sliderPos
        *
        (rotaryEndAngle
         -
         rotaryStartAngle);

    // ========================================================
    // TICKS
    // ========================================================

    constexpr int tickCount = 31;

    for (int i = 0;
         i < tickCount;
         ++i)
    {
        float p =
            (float)i
            /
            (float)(tickCount - 1);

        float a =
            rotaryStartAngle
            +
            p
            *
            (rotaryEndAngle
             -
             rotaryStartAngle)

            -
            juce::MathConstants<float>::
                halfPi;

        float innerRadius =
            radius
            +
            ((i % 5 == 0)
                 ? 6.0f
                 : 9.0f);

        float outerRadius =
            radius + 14.0f;

        float x1 =
            centre.x
            +
            std::cos(a)
            *
            innerRadius;

        float y1 =
            centre.y
            +
            std::sin(a)
            *
            innerRadius;

        float x2 =
            centre.x
            +
            std::cos(a)
            *
            outerRadius;

        float y2 =
            centre.y
            +
            std::sin(a)
            *
            outerRadius;

        g.setColour (
            juce::Colours::white
                .withAlpha (
                    i % 5 == 0
                    ?
                    0.90f
                    :
                    0.55f));

        g.drawLine (
            x1,
            y1,
            x2,
            y2,

            i % 5 == 0
            ?
            1.7f
            :
            1.0f);
    }

    // ========================================================
    // SHADOW
    // ========================================================

    g.setColour (
        juce::Colours::black
            .withAlpha (0.75f));

    g.fillEllipse (
        centre.x
            -
            radius
            +
            4.0f,

        centre.y
            -
            radius
            +
            6.0f,

        radius * 2.0f,
        radius * 2.0f);

    // ========================================================
    // ACCENT RING
    // ========================================================

    g.setColour (
        skin.accent
            .darker (0.25f));

    g.fillEllipse (
        centre.x
            -
            radius
            -
            6.0f,

        centre.y
            -
            radius
            -
            6.0f,

        (radius + 6.0f)
            * 2.0f,

        (radius + 6.0f)
            * 2.0f);

    // black inner ring

    g.setColour (
        juce::Colours::black);

    g.fillEllipse (
        centre.x
            -
            radius
            -
            3.0f,

        centre.y
            -
            radius
            -
            3.0f,

        (radius + 3.0f)
            * 2.0f,

        (radius + 3.0f)
            * 2.0f);

    // ========================================================
    // KNOB BODY
    // ========================================================

    juce::ColourGradient gradient (

        skin.knobBodyTop,

        centre.x,
        centre.y - radius,

        skin.knobBodyBottom,

        centre.x,
        centre.y + radius,

        false);

    gradient.addColour (
        0.30,

        skin.knobBodyMid);

    g.setGradientFill (
        gradient);

    g.fillEllipse (
        centre.x - radius,
        centre.y - radius,
        radius * 2.0f,
        radius * 2.0f);

    // ========================================================
    // HIGHLIGHT
    // ========================================================

    g.setColour (
        juce::Colours::white
            .withAlpha (0.11f));

    g.fillEllipse (
        centre.x
            -
            radius * 0.58f,

        centre.y
            -
            radius * 0.58f,

        radius * 0.95f,

        radius * 0.37f);

    // ========================================================
    // INNER RING
    // ========================================================

    g.setColour (
        skin.accent
            .withAlpha (0.35f));

    g.drawEllipse (
        centre.x
            -
            radius
            +
            5.0f,

        centre.y
            -
            radius
            +
            5.0f,

        (radius - 5.0f)
            * 2.0f,

        (radius - 5.0f)
            * 2.0f,

        1.0f);

    // ========================================================
    // POINTER
    // ========================================================

    float pointerLength =
        radius * 0.78f;

    float pointerWidth =
        juce::jmax (
            3.0f,
            radius * 0.085f);

    juce::Path pointer;

    pointer.addRoundedRectangle (

        -pointerWidth * 0.5f,

        -pointerLength,

        pointerWidth,

        pointerLength,

        pointerWidth * 0.5f);

    pointer.applyTransform (

        juce::AffineTransform::
            rotation(angle)

            .translated(
                centre.x,
                centre.y));

    g.setColour (
        skin.accentBright);

    g.fillPath (
        pointer);
}

// ============================================================
// 3D BUTTON
// ============================================================

void NFProClipperLookAndFeel::
drawButtonBackground (
    juce::Graphics& g,

    juce::Button& button,

    const juce::Colour&,

    bool mouseOver,

    bool buttonDown)
{
    auto bounds =
        button
            .getLocalBounds()
            .toFloat()
            .reduced (1.0f);

    float corner =
        juce::jmin (
            7.0f,

            bounds.getHeight()
            *
            0.22f);

    // shadow

    g.setColour (
        juce::Colours::black
            .withAlpha (0.80f));

    g.fillRoundedRectangle (
        bounds.translated (
            0.0f,
            3.0f),

        corner);

    // outer body

    g.setColour (
        juce::Colours::black);

    g.fillRoundedRectangle (
        bounds,
        corner);

    auto inner =
        bounds.reduced (2.0f);

    juce::Colour top =
        buttonDown

        ?
        skin.buttonTopDown

        :
        skin.buttonTopIdle;

    juce::Colour bottom =
        buttonDown

        ?
        skin.buttonBottomDown

        :
        skin.buttonBottomIdle;

    if (mouseOver)
        top =
            top.brighter (
                0.15f);

    juce::ColourGradient gradient (

        top,

        inner.getCentreX(),
        inner.getY(),

        bottom,

        inner.getCentreX(),
        inner.getBottom(),

        false);

    gradient.addColour (

        0.43,

        skin.buttonMidStop);

    g.setGradientFill (
        gradient);

    g.fillRoundedRectangle (
        inner,
        corner - 1.0f);

    // outline

    g.setColour (
        skin.accent
            .withAlpha (
                mouseOver
                ?
                0.95f
                :
                0.70f));

    g.drawRoundedRectangle (
        inner,
        corner - 1.0f,
        1.25f);

    // top highlight

    g.setColour (
        juce::Colours::white
            .withAlpha (
                buttonDown
                ?
                0.07f
                :
                0.25f));

    g.drawLine (

        inner.getX()
            +
            5.0f,

        inner.getY()
            +
            2.0f,

        inner.getRight()
            -
            5.0f,

        inner.getY()
            +
            2.0f,

        1.0f);

    // active glow

    if (button.getToggleState())
    {
        g.setColour (
            skin.accent
                .withAlpha (
                    0.23f));

        g.drawRoundedRectangle (

            inner.reduced (
                1.0f),

            corner - 2.0f,

            2.0f);
    }
}

// ============================================================
// BUTTON TEXT
// ============================================================

void NFProClipperLookAndFeel::
drawButtonText (
    juce::Graphics& g,

    juce::TextButton& button,

    bool,

    bool buttonDown)
{
    auto bounds =
        button
            .getLocalBounds();

    if (buttonDown)
        bounds.translate (
            0,
            1);

    g.setFont (
        juce::FontOptions (
            juce::jmax (
                11.0f,

                button.getHeight()
                *
                0.42f),

            juce::Font::bold));

    // text shadow

    g.setColour (
        juce::Colours::black
            .withAlpha (0.75f));

    g.drawFittedText (

        button.getButtonText(),

        bounds.translated (
            0,
            1),

        juce::Justification::centred,

        1);

    // text

    g.setColour (
        juce::Colours::white);

    g.drawFittedText (

        button.getButtonText(),

        bounds,

        juce::Justification::centred,

        1);
}

// ============================================================
// CONSTRUCTOR
// ============================================================

NFProClipperAudioProcessorEditor::
NFProClipperAudioProcessorEditor (
    NFProClipperAudioProcessor& p)

    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    // alcinha de redimensionar no canto inferior direito
    // (própria, em vez da automática do JUCE, para poder
    // reaproveitar a mesma lógica se precisarmos reposicioná-la
    // de novo no futuro).
    //
    // PRECISA ser criada antes de setResizable/setResizeLimits:
    // essas chamadas já disparam resized() internamente, e
    // resized() usa cornerResizer - criar depois causava um
    // ponteiro nulo e derrubava o host ao abrir o plugin.
    cornerResizer =
        std::make_unique<
            juce::ResizableCornerComponent>(

                this,
                getConstrainer());

    addAndMakeVisible (
        *cornerResizer);

    setResizable (
        true,
        false);

    setResizeLimits (
        420,
        687,
        900,
        1480);

    if (auto* constrainer =
        getConstrainer())
    {
        constrainer
            ->setFixedAspectRatio (
                660.0 / 1080.0);
    }

    setSize (
        defaultEditorWidth,
        defaultEditorHeight);

    // ========================================================
    // KNOBS
    // ========================================================

    for (auto* slider :
         {
             &input,
             &drive,
             &output,

             &ceiling,
             &knee,
             &mix,
             &tone
         })
    {
        addAndMakeVisible (
            *slider);

        slider->setSliderStyle (
            juce::Slider::
                RotaryHorizontalVerticalDrag);

        slider->setTextBoxStyle (
            juce::Slider::
                TextBoxBelow,

            false,

            90,

            24);

        slider->setRotaryParameters (

            juce::MathConstants<float>::pi
            *
            1.25f,

            juce::MathConstants<float>::pi
            *
            2.75f,

            true);

        slider->setLookAndFeel (
            &lookAndFeel);

        // textBoxTextColourId é ajustado por applySkin()

        slider->setColour (
            juce::Slider::
                textBoxBackgroundColourId,

            juce::Colours::black);

        slider->setColour (
            juce::Slider::
                textBoxOutlineColourId,

            juce::Colours::
                transparentBlack);
    }

    configureKnob (
        input,
        " dB");

    configureKnob (
        drive,
        " dB");

    configureKnob (
        output,
        " dB");

    configureKnob (
        ceiling,
        " dB");

    configureKnob (
        knee,
        " dB");

    configureKnob (
        mix,
        " %");

    configureKnob (
        tone,
        " %");

    // ========================================================
    // COMBOBOX
    // ========================================================

    for (auto* combo :
         {
             &clipMode,
             &oversampling,
             &monitor
         })
    {
        addAndMakeVisible (
            *combo);

        combo->setColour (
            juce::ComboBox::
                backgroundColourId,

            juce::Colours::black);

        // textColourId e outlineColourId são ajustados por
        // applySkin()

        combo->setColour (
            juce::ComboBox::
                arrowColourId,

            juce::Colours::white);
    }

    clipMode.addItemList (
        {
            "SOFT",
            "MEDIUM",
            "HARD"
        },
        1);

    oversampling.addItemList (
        {
            "1x",
            "2x",
            "4x",
            "8x",
            "16x"
        },
        1);

    monitor.addItemList (
        {
            "IN",
            "OUT",
            "CLIP"
        },
        1);

    // ========================================================
    // BUTTONS
    // ========================================================

    for (auto* button :
         {
             &bypass,
             &previousPreset,
             &nextPreset,
             &savePreset,
             &loadPreset,
             &skinButton
         })
    {
        addAndMakeVisible (
            *button);

        button->setLookAndFeel (
            &lookAndFeel);
    }

    bypass.setClickingTogglesState (
        true);

    previousPreset.onClick =
        [this] { goToPreviousPreset(); };

    nextPreset.onClick =
        [this] { goToNextPreset(); };

    savePreset.onClick =
        [this] { promptSavePreset(); };

    loadPreset.onClick =
        [this] { promptLoadPreset(); };

    skinButton.onClick =
        [this]
        {
            applySkin (
                1 - currentSkinIndex);
        };

    // ========================================================
    // PRESET DISPLAY
    // ========================================================

    presetDisplay.setText (
        "Default",
        juce::dontSendNotification);

    presetDisplay
        .setJustificationType (
            juce::Justification::
                centred);

    presetDisplay.setColour (
        juce::Label::
            textColourId,

        juce::Colours::white);

    presetDisplay.setColour (
        juce::Label::
            backgroundColourId,

        juce::Colours::black);

    // outlineColourId é ajustado por applySkin()

    addAndMakeVisible (
        presetDisplay);

    // ========================================================
    // ATTACHMENTS
    // ========================================================

    inputAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "input",
                input);

    driveAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "drive",
                drive);

    outputAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "output",
                output);

    ceilingAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "ceiling",
                ceiling);

    kneeAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "knee",
                knee);

    mixAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "mix",
                mix);

    toneAttachment =
        std::make_unique<
            SliderAttachment>(

                audioProcessor.apvts,
                "tone",
                tone);

    clipModeAttachment =
        std::make_unique<
            ComboAttachment>(

                audioProcessor.apvts,
                "clipMode",
                clipMode);

    oversamplingAttachment =
        std::make_unique<
            ComboAttachment>(

                audioProcessor.apvts,
                "oversampling",
                oversampling);

    monitorAttachment =
        std::make_unique<
            ComboAttachment>(

                audioProcessor.apvts,
                "monitor",
                monitor);

    bypassAttachment =
        std::make_unique<
            ButtonAttachment>(

                audioProcessor.apvts,
                "bypass",
                bypass);

    // ========================================================
    // PRESETS: garante que exista ao menos o preset "Default"
    // em disco, para que PREVIOUS/NEXT sempre tenham algo para
    // navegar, sem forçar o carregamento dele no estado atual.
    // ========================================================

    refreshPresetList();

    if (presetFiles.isEmpty())
    {
        auto defaultFile =
            NFProClipperAudioProcessor::
                getPresetsDirectory()

                .getChildFile (
                    "Default.xml");

        auto state =
            audioProcessor.apvts.copyState();

        if (auto xml =
            state.createXml())
        {
            xml->writeTo (
                defaultFile);
        }

        refreshPresetList();
    }

    currentPresetIndex = -1;

    // ========================================================
    // SKIN: carrega a preferência salva (ou Skin 2/clássica por
    // padrão) e aplica as cores em todos os componentes.
    // ========================================================

    applySkin (
        audioProcessor.getSkinIndex());

    // meter 30 fps

    startTimerHz (
        30);
}

// ============================================================
// DESTRUCTOR
// ============================================================

NFProClipperAudioProcessorEditor::
~NFProClipperAudioProcessorEditor()
{
    for (auto* slider :
         {
             &input,
             &drive,
             &output,
             &ceiling,
             &knee,
             &mix,
             &tone
         })
    {
        slider->setLookAndFeel (
            nullptr);
    }

    for (auto* button :
         {
             &bypass,
             &previousPreset,
             &nextPreset,
             &savePreset,
             &loadPreset,
             &skinButton
         })
    {
        button->setLookAndFeel (
            nullptr);
    }
}

// ============================================================
// SKIN
// ============================================================

void NFProClipperAudioProcessorEditor::
applySkin (
    int index)
{
    currentSkinIndex =
        juce::jlimit (
            0,
            1,
            index);

    skin =
        getSkinByIndex (
            currentSkinIndex);

    lookAndFeel.setSkin (
        skin);

    for (auto* slider :
         {
             &input,
             &drive,
             &output,
             &ceiling,
             &knee,
             &mix,
             &tone
         })
    {
        slider->setColour (
            juce::Slider::
                textBoxTextColourId,

            skin.accent);
    }

    for (auto* combo :
         {
             &clipMode,
             &oversampling,
             &monitor
         })
    {
        combo->setColour (
            juce::ComboBox::
                textColourId,

            skin.accent);

        combo->setColour (
            juce::ComboBox::
                outlineColourId,

            skin.accent
                .darker (0.25f));
    }

    presetDisplay.setColour (
        juce::Label::
            outlineColourId,

        skin.accent
            .darker (0.30f));

    audioProcessor.setSkinIndex (
        currentSkinIndex);

    repaint();
}

// ============================================================

void NFProClipperAudioProcessorEditor::
buildGlowImage()
{
    constexpr int designWidth = 660;
    constexpr int designHeight = 1080;

    glowImage =
        juce::Image (
            juce::Image::ARGB,
            designWidth,
            designHeight,
            true);

    juce::Graphics g (
        glowImage);

    juce::Path chassisOutline;

    chassisOutline.addRoundedRectangle (

        juce::Rectangle<float> (
            0.0f,
            0.0f,
            (float) designWidth,
            (float) designHeight)

            .reduced (8.0f),

        20.0f);

    // branco fixo (não segue a skin) - efeito de luz por trás
    // do chassis, igual nas duas skins. Só é calculado uma vez
    // e reaproveitado em todo repaint().
    juce::DropShadow glow (
        juce::Colours::white
            .withAlpha (0.55f),

        10,

        { 0, 0 });

    glow.drawForPath (
        g,
        chassisOutline);
}

// ============================================================

void NFProClipperAudioProcessorEditor::
configureKnob (
    juce::Slider& slider,
    const juce::String& suffix)
{
    slider.setTextValueSuffix (
        suffix);
}

// ============================================================
// PRESETS
// ============================================================

void NFProClipperAudioProcessorEditor::
refreshPresetList()
{
    presetFiles.clear();

    auto dir =
        NFProClipperAudioProcessor::
            getPresetsDirectory();

    for (auto entry :
         juce::RangedDirectoryIterator (
             dir,
             false,
             "*.xml",
             juce::File::findFiles))
    {
        presetFiles.add (
            entry.getFile());
    }

    std::sort (
        presetFiles.begin(),
        presetFiles.end(),

        [] (const juce::File& a,
            const juce::File& b)
        {
            return
                a.getFileNameWithoutExtension()
                    .compareIgnoreCase (
                        b.getFileNameWithoutExtension())
                < 0;
        });
}

// ============================================================

void NFProClipperAudioProcessorEditor::
loadPresetFile (
    const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    auto xml =
        juce::XmlDocument::parse (
            file);

    if (xml == nullptr)
        return;

    if (! xml->hasTagName (
            audioProcessor.apvts.state.getType()))
    {
        return;
    }

    audioProcessor.apvts.replaceState (
        juce::ValueTree::fromXml (
            *xml));

    currentPresetIndex =
        presetFiles.indexOf (
            file);

    updatePresetDisplay (
        file.getFileNameWithoutExtension());
}

// ============================================================

void NFProClipperAudioProcessorEditor::
loadPresetByIndex (
    int index)
{
    if (presetFiles.isEmpty())
        return;

    int size =
        presetFiles.size();

    index =
        ((index % size)
         +
         size)
        % size;

    loadPresetFile (
        presetFiles[index]);
}

// ============================================================

void NFProClipperAudioProcessorEditor::
updatePresetDisplay (
    const juce::String& name)
{
    presetDisplay.setText (
        name,
        juce::dontSendNotification);
}

// ============================================================

void NFProClipperAudioProcessorEditor::
goToPreviousPreset()
{
    if (presetFiles.isEmpty())
        return;

    int index =
        currentPresetIndex <= 0

        ?
        presetFiles.size() - 1

        :
        currentPresetIndex - 1;

    loadPresetByIndex (
        index);
}

// ============================================================

void NFProClipperAudioProcessorEditor::
goToNextPreset()
{
    if (presetFiles.isEmpty())
        return;

    int index =
        currentPresetIndex < 0

        ?
        0

        :
        currentPresetIndex + 1;

    loadPresetByIndex (
        index);
}

// ============================================================

void NFProClipperAudioProcessorEditor::
promptSavePreset()
{
    auto dir =
        NFProClipperAudioProcessor::
            getPresetsDirectory();

    activeFileChooser =
        std::make_unique<
            juce::FileChooser>(

                "Save NF Pro Clipper preset",

                dir.getChildFile (
                    "New Preset.xml"),

                "*.xml");

    auto flags =
        juce::FileBrowserComponent::saveMode

        |
        juce::FileBrowserComponent::canSelectFiles

        |
        juce::FileBrowserComponent::warnAboutOverwriting;

    activeFileChooser->launchAsync (
        flags,

        [this] (const juce::FileChooser& fc)
        {
            auto file =
                fc.getResult();

            if (file
                ==
                juce::File{})
            {
                return;
            }

            if (file.getFileExtension()
                .isEmpty())
            {
                file =
                    file.withFileExtension (
                        "xml");
            }

            auto state =
                audioProcessor.apvts.copyState();

            if (auto xml =
                state.createXml())
            {
                xml->writeTo (
                    file);
            }

            refreshPresetList();

            currentPresetIndex =
                presetFiles.indexOf (
                    file);

            updatePresetDisplay (
                file.getFileNameWithoutExtension());
        });
}

// ============================================================

void NFProClipperAudioProcessorEditor::
promptLoadPreset()
{
    auto dir =
        NFProClipperAudioProcessor::
            getPresetsDirectory();

    activeFileChooser =
        std::make_unique<
            juce::FileChooser>(

                "Load NF Pro Clipper preset",

                dir,

                "*.xml");

    auto flags =
        juce::FileBrowserComponent::openMode

        |
        juce::FileBrowserComponent::canSelectFiles;

    activeFileChooser->launchAsync (
        flags,

        [this] (const juce::FileChooser& fc)
        {
            auto file =
                fc.getResult();

            if (file
                ==
                juce::File{})
            {
                return;
            }

            refreshPresetList();

            loadPresetFile (
                file);
        });
}

// ============================================================
// TIMER
// ============================================================

float NFProClipperAudioProcessorEditor::
smoothMeterValue (
    float current,
    float target,
    float releasePerTick)
{
    if (target >= current)
        return target;

    return
        juce::jmax (
            target,
            current - releasePerTick);
}

void NFProClipperAudioProcessorEditor::
timerCallback()
{
    inputL =
        audioProcessor.getInputPeakDb (0);

    inputR =
        audioProcessor.getInputPeakDb (1);

    outputL =
        audioProcessor.getOutputPeakDb (0);

    outputR =
        audioProcessor.getOutputPeakDb (1);

    reduction =
        audioProcessor.getReductionDb();

    clipping =
        audioProcessor.getClipAmount();

    // ataque instantâneo, release suave - comportamento de
    // medidor de hardware profissional em vez de um valor
    // instantâneo "nervoso" a cada frame.

    displayInputL =
        smoothMeterValue (
            displayInputL,
            inputL,
            1.6f);

    displayInputR =
        smoothMeterValue (
            displayInputR,
            inputR,
            1.6f);

    displayOutputL =
        smoothMeterValue (
            displayOutputL,
            outputL,
            1.6f);

    displayOutputR =
        smoothMeterValue (
            displayOutputR,
            outputR,
            1.6f);

    displayReduction =
        smoothMeterValue (
            displayReduction,
            reduction,
            0.6f);

    displayClipping =
        smoothMeterValue (
            displayClipping,
            clipping,
            0.05f);

    repaint();
}

// ============================================================
// SCREW
// ============================================================

void NFProClipperAudioProcessorEditor::
drawScrew (
    juce::Graphics& g,
    juce::Point<float> centre)
{
    g.setColour (
        juce::Colours::black);

    g.fillEllipse (
        centre.x - 8.0f,
        centre.y - 8.0f,
        16.0f,
        16.0f);

    juce::ColourGradient grad (

        juce::Colour::fromRGB (
            80,
            80,
            82),

        centre.x,
        centre.y - 7.0f,

        juce::Colour::fromRGB (
            12,
            12,
            13),

        centre.x,
        centre.y + 7.0f,

        false);

    g.setGradientFill (
        grad);

    g.fillEllipse (
        centre.x - 6.0f,
        centre.y - 6.0f,
        12.0f,
        12.0f);

    g.setColour (
        juce::Colours::black);

    g.drawLine (
        centre.x - 3.5f,
        centre.y - 3.5f,
        centre.x + 3.5f,
        centre.y + 3.5f,
        1.5f);
}

// ============================================================
// SECTION TITLE
// ============================================================

void NFProClipperAudioProcessorEditor::
drawSectionTitle (
    juce::Graphics& g,
    juce::Rectangle<float> r,
    const juce::String& text)
{
    g.setColour (
        juce::Colours::white
            .withAlpha (0.88f));

    g.setFont (
        juce::FontOptions (
            13.0f,
            juce::Font::bold));

    g.drawText (
        text,

        r.toNearestInt(),

        juce::Justification::
            centred);
}

// ============================================================
// METER
// ============================================================

void NFProClipperAudioProcessorEditor::
drawMeter (
    juce::Graphics& g,

    juce::Rectangle<float> area,

    float leftDb,
    float rightDb,

    const juce::String& name)
{
    g.setColour (
        juce::Colours::black
            .withAlpha (0.70f));

    g.fillRoundedRectangle (
        area,
        10.0f);

    g.setColour (
        juce::Colours::white);

    g.setFont (
        juce::FontOptions (
            15.0f,
            juce::Font::bold));

    auto header =
        area.removeFromTop (
            28.0f);

    g.drawText (
        name,

        header.toNearestInt(),

        juce::Justification::
            centred);

    auto meterArea =
        area.reduced (
            14.0f,
            8.0f);

    float gap = 7.0f;

    auto left =
        meterArea.removeFromLeft (

            (meterArea.getWidth()
             -
             gap)

            /
            2.0f);

    meterArea.removeFromLeft (
        gap);

    auto right =
        meterArea;

    auto drawOne =
        [&g] (
            juce::Rectangle<float> meter,
            float db)
    {
        g.setColour (
            juce::Colour::fromRGB (
                24,
                24,
                25));

        g.fillRect (
            meter);

        float normalized =
            juce::jlimit (
                0.0f,
                1.0f,

                juce::jmap (
                    db,

                    -60.0f,
                    0.0f,

                    0.0f,
                    1.0f));

        auto fill =
            meter.withTrimmedTop (

                meter.getHeight()
                *
                (1.0f
                 -
                 normalized));

        juce::ColourGradient gradient (

            juce::Colour::fromRGB (
                255,
                55,
                0),

            fill.getX(),
            fill.getY(),

            // amarelo fixo (não segue a skin): é o degrau de
            // aviso convencional de um medidor de nível, deve
            // continuar legível/universal em qualquer skin
            juce::Colour::fromRGB (255, 211, 0),

            fill.getX(),
            fill.getCentreY(),

            false);

        gradient.addColour (

            0.75,

            juce::Colour::fromRGB (
                60,
                255,
                45));

        g.setGradientFill (
            gradient);

        g.fillRect (
            fill);

        g.setColour (
            juce::Colours::black
                .withAlpha (0.45f));

        constexpr int segments = 18;

        for (int i = 1;
             i < segments;
             ++i)
        {
            float y =
                meter.getY()

                +

                meter.getHeight()

                *

                ((float)i
                 /
                 (float)segments);

            g.drawHorizontalLine (
                (int)y,

                meter.getX(),

                meter.getRight());
        }
    };

    drawOne (
        left,
        leftDb);

    drawOne (
        right,
        rightDb);
}

// ============================================================
// TRANSFER GRAPH
//
// Usa audioProcessor.computeTransferCurveSample(), que aplica
// exatamente a mesma função de clipping do processBlock
// (incluindo o knee), então o gráfico nunca fica
// dessincronizado do áudio.
// ============================================================

void NFProClipperAudioProcessorEditor::
drawTransferGraph (
    juce::Graphics& g,
    juce::Rectangle<float> area)
{
    g.setColour (
        juce::Colours::black
            .withAlpha (0.82f));

    g.fillRoundedRectangle (
        area,
        12.0f);

    auto graph =
        area.reduced (
            18.0f);

    // grid

    g.setColour (
        juce::Colour::fromRGB (
            55,
            55,
            58));

    for (int i = 0;
         i <= 6;
         ++i)
    {
        float x =
            juce::jmap (
                (float)i,

                0.0f,
                6.0f,

                graph.getX(),
                graph.getRight());

        float y =
            juce::jmap (
                (float)i,

                0.0f,
                6.0f,

                graph.getY(),
                graph.getBottom());

        g.drawVerticalLine (
            (int)x,

            graph.getY(),
            graph.getBottom());

        g.drawHorizontalLine (
            (int)y,

            graph.getX(),
            graph.getRight());
    }

    juce::Path curve;

    constexpr int points = 220;

    for (int i = 0;
         i <= points;
         ++i)
    {
        float x =
            juce::jmap (
                (float)i,

                0.0f,
                (float)points,

                -1.40f,
                1.40f);

        float y =
            audioProcessor
                .computeTransferCurveSample (
                    x);

        float px =
            juce::jmap (
                x,

                -1.40f,
                1.40f,

                graph.getX(),
                graph.getRight());

        float py =
            juce::jmap (
                y,

                1.40f,
                -1.40f,

                graph.getY(),
                graph.getBottom());

        if (i == 0)
            curve.startNewSubPath (
                px,
                py);

        else
            curve.lineTo (
                px,
                py);
    }

    // shadow

    g.setColour (
        skin.accent
            .withAlpha (0.20f));

    g.strokePath (
        curve,

        juce::PathStrokeType (
            6.0f));

    // main curve

    g.setColour (
        juce::Colours::white);

    g.strokePath (
        curve,

        juce::PathStrokeType (
            2.2f));

    // accent frame

    g.setColour (
        skin.accent
            .withAlpha (0.65f));

    g.drawRoundedRectangle (
        area,
        12.0f,
        1.0f);
}

// ============================================================
// PAINT
// ============================================================

void NFProClipperAudioProcessorEditor::
paint (
    juce::Graphics& g)
{
    g.fillAll (
        juce::Colours::black);

    // ========================================================
    // ESCALA GLOBAL
    //
    // Todo o desenho abaixo é escrito em coordenadas de design
    // fixas (a tela original de 660x1080). Em vez de recalcular
    // cada posição/fonte manualmente para o tamanho atual da
    // janela, aplicamos uma única transformação de escala aqui
    // e desenhamos tudo como se a janela sempre tivesse
    // 660x1080 - o Graphics context escala tudo de uma vez,
    // então a interface encolhe/cresce como um bloco único,
    // sem desconfigurar a posição relativa de nada.
    // ========================================================

    constexpr float designWidth = 660.0f;
    constexpr float designHeight = 1080.0f;

    float scale =
        juce::jmin (
            (float) getWidth() / designWidth,
            (float) getHeight() / designHeight);

    g.addTransform (
        juce::AffineTransform::scale (
            scale));

    auto chassis =
        juce::Rectangle<float> (
            0.0f,
            0.0f,
            designWidth,
            designHeight)

            .reduced (8.0f);

    // ========================================================
    // GLOW EXTERNO (pré-renderizado, ver buildGlowImage())
    // ========================================================

    if (glowImage.isNull())
    {
        buildGlowImage();
    }

    g.drawImageAt (
        glowImage,
        0,
        0);

    juce::ColourGradient background (

        skin.chassisTop,

        0.0f,
        chassis.getY(),

        skin.chassisBottom,

        0.0f,
        chassis.getBottom(),

        false);

    g.setGradientFill (
        background);

    g.fillRoundedRectangle (
        chassis,
        20.0f);

    // external accent border

    g.setColour (
        skin.accent);

    g.drawRoundedRectangle (

        chassis.reduced (
            2.0f),

        18.0f,

        2.6f);

    // second internal border

    g.setColour (
        skin.accent
            .darker (0.50f)
            .withAlpha (0.55f));

    g.drawRoundedRectangle (

        chassis.reduced (
            6.0f),

        16.0f,

        1.0f);

    // screws

    drawScrew (
        g,
        {
            29.0f,
            29.0f
        });

    drawScrew (
        g,
        {
            designWidth
                -
                29.0f,

            29.0f
        });

    drawScrew (
        g,
        {
            29.0f,

            designHeight
                -
                29.0f
        });

    drawScrew (
        g,
        {
            designWidth
                -
                29.0f,

            designHeight
                -
                29.0f
        });

    // ========================================================
    // NF LOGO
    // ========================================================

    g.setColour (
        skin.accentBright);

    g.setFont (
        juce::FontOptions (
            78.0f,
            juce::Font::bold));

    g.drawText (

        "NF",

        0,
        26,

        (int) designWidth,
        85,

        juce::Justification::
            centred);

    // title

    g.setColour (
        juce::Colours::white);

    g.setFont (
        juce::FontOptions (
            29.0f,
            juce::Font::bold));

    g.drawText (

        "PRO CLIPPER",

        0,
        105,

        (int) designWidth,
        38,

        juce::Justification::
            centred);

    // ========================================================
    // GRAPH
    // ========================================================

    drawTransferGraph (

        g,

        {
            118.0f,
            190.0f,

            (float)designWidth
                -
                236.0f,

            290.0f
        });

    // ========================================================
    // METERS
    // ========================================================

    drawMeter (

        g,

        {
            27.0f,
            185.0f,
            75.0f,
            340.0f
        },

        displayInputL,
        displayInputR,

        "IN");

    drawMeter (

        g,

        {
            (float)designWidth
                -
                102.0f,

            185.0f,
            75.0f,
            340.0f
        },

        displayOutputL,
        displayOutputR,

        "OUT");

    // ========================================================
    // LABELS
    // ========================================================

    drawSectionTitle (
        g,
        { 112, 532, 130, 22 },
        "INPUT");

    drawSectionTitle (
        g,
        { 225, 520, 210, 22 },
        "DRIVE");

    drawSectionTitle (
        g,
        { 418, 532, 130, 22 },
        "OUTPUT");

    drawSectionTitle (
        g,
        { 84, 705, 120, 22 },
        "CEILING");

    drawSectionTitle (
        g,
        { 220, 705, 120, 22 },
        "KNEE");

    drawSectionTitle (
        g,
        { 356, 705, 120, 22 },
        "MIX");

    drawSectionTitle (
        g,
        { 492, 705, 120, 22 },
        "TONE");

    // ========================================================
    // REDUCTION / CLIPPING
    // ========================================================

    g.setColour (
        juce::Colours::white
            .withAlpha (0.82f));

    g.setFont (
        juce::FontOptions (
            12.0f,
            juce::Font::bold));

    g.drawText (

        "REDUCTION",

        38,
        (int) designHeight - 112,

        110,
        20,

        juce::Justification::
            centredLeft);

    g.drawText (

        "CLIPPING",

        (int) designWidth - 150,
        (int) designHeight - 112,

        110,
        20,

        juce::Justification::
            centredRight);

    for (int i = 0;
         i < 7;
         ++i)
    {
        float threshold =
            (float)i
            /
            7.0f;

        bool reductionOn =
            juce::jlimit (
                0.0f,
                1.0f,

                displayReduction
                /
                18.0f)

            >
            threshold;

        bool clippingOn =
            juce::jlimit (
                0.0f,
                1.0f,
                displayClipping)

            >
            threshold;

        // reduction lights

        g.setColour (
            reductionOn

            ?
            skin.accent

            :
            juce::Colour::fromRGB (
                48,
                48,
                50));

        g.fillEllipse (

            48.0f
            +
            i
            *
            18.0f,

            designHeight
            -
            82.0f,

            10.0f,

            10.0f);

        // clipping lights

        g.setColour (
            clippingOn

            ?
            skin.accent

            :
            juce::Colour::fromRGB (
                48,
                48,
                50));

        g.fillEllipse (

            (float)designWidth
            -
            165.0f
            +
            i
            *
            18.0f,

            designHeight
            -
            82.0f,

            10.0f,

            10.0f);
    }

    // footer

    g.setColour (
        juce::Colours::white
            .withAlpha (0.55f));

    g.setFont (
        juce::FontOptions (
            9.5f));

    g.drawText (

        "v1.0.0",

        25,
        (int) designHeight - 34,

        80,
        18,

        juce::Justification::
            centredLeft);

    g.setColour (
        skin.accent
            .withAlpha (0.90f));

    g.drawText (

        "NENO FERNANDO AUDIO TOOLS",

        150,
        (int) designHeight - 34,

        (int) designWidth - 300,
        18,

        juce::Justification::
            centred);

    g.setColour (
        juce::Colours::white
            .withAlpha (0.55f));

    g.drawText (

        "DIGITAL PRECISION",

        (int) designWidth - 145,
        (int) designHeight - 34,

        120,
        18,

        juce::Justification::
            centredRight);
}

// ============================================================
// RESIZED
// ============================================================

void NFProClipperAudioProcessorEditor::
resized()
{
    constexpr float designWidth =
        660.0f;

    constexpr float designHeight =
        1080.0f;

    float scaleX =
        getWidth()
        /
        designWidth;

    float scaleY =
        getHeight()
        /
        designHeight;

    auto bounds =
        [scaleX, scaleY] (
            float x,
            float y,
            float w,
            float h)
    {
        return
            juce::Rectangle<int>(

                juce::roundToInt (
                    x * scaleX),

                juce::roundToInt (
                    y * scaleY),

                juce::roundToInt (
                    w * scaleX),

                juce::roundToInt (
                    h * scaleY));
    };

    // presets

    previousPreset.setBounds (
        bounds (
            112,
            150,
            42,
            32));

    presetDisplay.setBounds (
        bounds (
            160,
            150,
            300,
            32));

    nextPreset.setBounds (
        bounds (
            466,
            150,
            42,
            32));

    skinButton.setBounds (
        bounds (
            518,
            150,
            90,
            32));

    // main knobs

    input.setBounds (
        bounds (
            105,
            550,
            145,
            155));

    // altura reduzida de 220 para 180: o texto "x.x dB" do
    // Drive ocupa os últimos 24px do seu próprio bounds
    // (LookAndFeel_V2::getSliderLayout), então com 220 ele
    // descia até y=745 e cobria o título "KNEE" da fileira de
    // baixo (que começa em y=705). Com 180 ele termina
    // exatamente em y=705, sem sobrepor nada - e ainda continua
    // sendo o maior knob da interface (180 > 155/145 dos demais).
    drive.setBounds (
        bounds (
            240,
            525,
            180,
            180));

    output.setBounds (
        bounds (
            410,
            550,
            145,
            155));

    // secondary knobs

    ceiling.setBounds (
        bounds (
            75,
            720,
            130,
            145));

    knee.setBounds (
        bounds (
            210,
            720,
            130,
            145));

    mix.setBounds (
        bounds (
            345,
            720,
            130,
            145));

    tone.setBounds (
        bounds (
            480,
            720,
            130,
            145));

    // control row

    clipMode.setBounds (
        bounds (
            70,
            865,
            155,
            34));

    oversampling.setBounds (
        bounds (
            252,
            865,
            155,
            34));

    monitor.setBounds (
        bounds (
            435,
            865,
            155,
            34));

    // save/load

    savePreset.setBounds (
        bounds (
            175,
            915,
            100,
            36));

    loadPreset.setBounds (
        bounds (
            385,
            915,
            100,
            36));

    // bypass

    bypass.setBounds (
        bounds (
            225,
            963,
            210,
            52));

    // alcinha de redimensionar - canto inferior direito
    //
    // guard contra nullptr: o JUCE pode disparar resized()
    // internamente (via setResizable/setResizeLimits) antes do
    // corpo do construtor terminar de criar o cornerResizer.

    if (cornerResizer != nullptr)
    {
        constexpr int resizerSize = 18;

        cornerResizer->setBounds (
            getWidth() - resizerSize,
            getHeight() - resizerSize,
            resizerSize,
            resizerSize);
    }
}

// ============================================================
// MOUSE UP
//
// - Clicar no logo "NF" encolhe a janela de volta ao tamanho
//   padrão quando ela está maior que isso (o retângulo do logo,
//   em coordenadas de design, é o mesmo usado em paint() para
//   desenhar "NF": x 0-660, y 26-111).
// - Clicar no título "PRO CLIPPER" logo abaixo abre o menu do
//   manual (mesmo retângulo usado em paint() para esse texto:
//   x 0-660, y 105-143).
// ============================================================

void NFProClipperAudioProcessorEditor::
mouseUp (
    const juce::MouseEvent& event)
{
    constexpr float designWidth = 660.0f;
    constexpr float designHeight = 1080.0f;

    float scale =
        juce::jmin (
            (float) getWidth() / designWidth,
            (float) getHeight() / designHeight);

    if (scale <= 0.0f)
    {
        return;
    }

    auto designPos =
        event.position / scale;

    juce::Rectangle<float> logoBounds (
        0.0f,
        26.0f,
        designWidth,
        85.0f);

    if (logoBounds.contains (designPos))
    {
        if (getWidth() > defaultEditorWidth
            || getHeight() > defaultEditorHeight)
        {
            setSize (
                defaultEditorWidth,
                defaultEditorHeight);
        }

        return;
    }

    juce::Rectangle<float> titleBounds (
        0.0f,
        105.0f,
        designWidth,
        38.0f);

    if (titleBounds.contains (designPos))
    {
        showManualMenu();
    }
}

// ============================================================

void NFProClipperAudioProcessorEditor::
showManualMenu()
{
    juce::PopupMenu menu;

    constexpr int manualEnId = 1;
    constexpr int manualPtId = 2;

    menu.addItem (
        manualEnId,
        "Manual (English)",
        getManualFile (true).existsAsFile());

    menu.addItem (
        manualPtId,
        "Manual (Portugues)",
        getManualFile (false).existsAsFile());

    menu.showMenuAsync (
        juce::PopupMenu::Options(),

        [manualEnId, manualPtId] (int result)
        {
            if (result == manualEnId)
            {
                getManualFile (true)
                    .startAsProcess();
            }
            else if (result == manualPtId)
            {
                getManualFile (false)
                    .startAsProcess();
            }
        });
}
