#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================
// SKINS
//
// Cada skin é só uma paleta de cores - a estrutura de desenho
// (chassis, knobs, botões, gráfico, medidores) é sempre a
// mesma, só a paleta muda. Isso mantém as duas skins visualmente
// coerentes entre si e evita duplicar todo o código de desenho.
// ============================================================

struct NFSkinPalette
{
    juce::String name;

    juce::Colour accent;         // aro dos knobs, contornos, LEDs acesos
    juce::Colour accentBright;   // pointer dos knobs, logo "NF"

    juce::Colour chassisTop;
    juce::Colour chassisBottom;

    juce::Colour knobBodyTop;
    juce::Colour knobBodyMid;
    juce::Colour knobBodyBottom;

    juce::Colour buttonTopIdle;
    juce::Colour buttonTopDown;
    juce::Colour buttonBottomIdle;
    juce::Colour buttonBottomDown;
    juce::Colour buttonMidStop;
};

NFSkinPalette makeClassicSkin();
NFSkinPalette makePurpleSkin();

NFSkinPalette getSkinByIndex (int index);

// ============================================================
// NF LOOK AND FEEL
// ============================================================

class NFProClipperLookAndFeel
    : public juce::LookAndFeel_V4
{
public:

    void setSkin (const NFSkinPalette& newSkin)
    {
        skin = newSkin;
    }

    void drawRotarySlider (
        juce::Graphics&,
        int x,
        int y,
        int width,
        int height,
        float sliderPos,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;

    void drawButtonBackground (
        juce::Graphics&,
        juce::Button&,
        const juce::Colour&,
        bool mouseOver,
        bool buttonDown) override;

    void drawButtonText (
        juce::Graphics&,
        juce::TextButton&,
        bool mouseOver,
        bool buttonDown) override;

private:

    NFSkinPalette skin = makeClassicSkin();
};

// ============================================================
// EDITOR
// ============================================================

class NFProClipperAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:

    explicit
    NFProClipperAudioProcessorEditor (
        NFProClipperAudioProcessor&);

    ~NFProClipperAudioProcessorEditor()
        override;

    void paint (
        juce::Graphics&) override;

    void resized() override;

    void mouseUp (
        const juce::MouseEvent&) override;

private:

    // tamanho nativo do plugin (o mesmo passado a setSize() no
    // construtor) - clicar no logo "NF" com a janela maior que
    // isso devolve o plugin a este tamanho.
    static constexpr int defaultEditorWidth = 440;
    static constexpr int defaultEditorHeight = 720;

    NFProClipperAudioProcessor&
        audioProcessor;

    NFProClipperLookAndFeel
        lookAndFeel;

    // alcinha de redimensionar no canto inferior direito
    std::unique_ptr<juce::ResizableCornerComponent>
        cornerResizer;

    // ========================================================
    // SKIN
    // ========================================================

    int currentSkinIndex = 0;
    NFSkinPalette skin = makeClassicSkin();

    juce::TextButton skinButton {
        "SKIN"
    };

    void applySkin (int index);

    // glow branco em volta do chassis, pré-renderizado uma
    // única vez num Image (ao invés de recalculado a cada
    // repaint() - DropShadow::drawForPath é caro e o timer de
    // 30fps dos medidores repinta o editor inteiro o tempo
    // todo, então recalcular o blur em todo frame travava a UI)
    juce::Image glowImage;

    void buildGlowImage();

    // ========================================================
    // KNOBS
    // ========================================================

    juce::Slider input;
    juce::Slider drive;
    juce::Slider output;

    juce::Slider ceiling;
    juce::Slider knee;
    juce::Slider mix;
    juce::Slider tone;

    // ========================================================
    // SELECTORS
    // ========================================================

    juce::ComboBox clipMode;
    juce::ComboBox oversampling;
    juce::ComboBox monitor;

    // ========================================================
    // BUTTONS
    // ========================================================

    juce::TextButton bypass {
        "BYPASS"
    };

    juce::TextButton previousPreset {
        "<"
    };

    juce::TextButton nextPreset {
        ">"
    };

    juce::TextButton savePreset {
        "SAVE"
    };

    juce::TextButton loadPreset {
        "LOAD"
    };

    juce::Label presetDisplay;

    // ========================================================
    // ATTACHMENTS
    // ========================================================

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::
            SliderAttachment;

    using ComboAttachment =
        juce::AudioProcessorValueTreeState::
            ComboBoxAttachment;

    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::
            ButtonAttachment;

    std::unique_ptr<SliderAttachment>
        inputAttachment;

    std::unique_ptr<SliderAttachment>
        driveAttachment;

    std::unique_ptr<SliderAttachment>
        outputAttachment;

    std::unique_ptr<SliderAttachment>
        ceilingAttachment;

    std::unique_ptr<SliderAttachment>
        kneeAttachment;

    std::unique_ptr<SliderAttachment>
        mixAttachment;

    std::unique_ptr<SliderAttachment>
        toneAttachment;

    std::unique_ptr<ComboAttachment>
        clipModeAttachment;

    std::unique_ptr<ComboAttachment>
        oversamplingAttachment;

    std::unique_ptr<ComboAttachment>
        monitorAttachment;

    std::unique_ptr<ButtonAttachment>
        bypassAttachment;

    // ========================================================
    // PRESETS
    // ========================================================

    juce::Array<juce::File> presetFiles;
    int currentPresetIndex = -1;
    std::unique_ptr<juce::FileChooser> activeFileChooser;

    void refreshPresetList();
    void loadPresetFile (const juce::File&);
    void loadPresetByIndex (int index);
    void updatePresetDisplay (const juce::String& name);
    void goToPreviousPreset();
    void goToNextPreset();
    void promptSavePreset();
    void promptLoadPreset();

    // ========================================================
    // MANUAL
    // ========================================================

    void showManualMenu();

    // ========================================================
    // METER VALUES (raw, updated every timer tick)
    // ========================================================

    float inputL = -100.0f;
    float inputR = -100.0f;

    float outputL = -100.0f;
    float outputR = -100.0f;

    float reduction = 0.0f;
    float clipping = 0.0f;

    // ========================================================
    // METER VALUES (com ballistics visuais: ataque instantâneo,
    // release suave, como um medidor de hardware profissional)
    // ========================================================

    float displayInputL = -100.0f;
    float displayInputR = -100.0f;

    float displayOutputL = -100.0f;
    float displayOutputR = -100.0f;

    float displayReduction = 0.0f;
    float displayClipping = 0.0f;

    static float smoothMeterValue (
        float current,
        float target,
        float releasePerTick);

    // ========================================================
    // HELPERS
    // ========================================================

    void configureKnob (
        juce::Slider&,
        const juce::String& suffix);

    void timerCallback() override;

    void drawMeter (
        juce::Graphics&,
        juce::Rectangle<float>,
        float leftDb,
        float rightDb,
        const juce::String&);

    void drawTransferGraph (
        juce::Graphics&,
        juce::Rectangle<float>);

    void drawScrew (
        juce::Graphics&,
        juce::Point<float>);

    void drawSectionTitle (
        juce::Graphics&,
        juce::Rectangle<float>,
        const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        NFProClipperAudioProcessorEditor)
};
