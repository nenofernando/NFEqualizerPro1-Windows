#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFWhiteLookAndFeel.h"
#include "UI/NFPremiumLookAndFeel.h"

// ============================================================
// FASE 7 / 7.2 -- REDESIGN PROFISSIONAL DA UI (layout na FASE 7,
// acabamento visual pra bater com a referência oficial na FASE 7.2).
// Não toca em NADA do motor DSP (Source/DSP/*, PluginProcessor.cpp)
// -- só layout e aparência (Source/PluginEditor.*, Source/UI/
// NFWhiteLookAndFeel.*). DSP aprovado e testado no host real
// permanece byte-a-byte idêntico.
//
// Hierarquia (FASE 7):
//   HEADER            -- logo, disquete, menu, bypass
//   CENTRO PRINCIPAL  -- TIME | DISPLAY | FEEDBACK | DRY/WET | OUTPUT
//   CONTROLES RÁPIDOS -- SYNC, DIVISION, MODIFIER, PING PONG+LO-FI
//                        (agrupados), MODE (Digital/Analog/Tape,
//                        agrupado)
//   AVANÇADO          -- MODULATION | FILTERS | CHARACTER (em painéis
//                        próprios)
//
// Todo controle continua usando Attachment (SliderAttachment/
// ButtonAttachment/ComboBoxAttachment) -- o único controle sem um
// Attachment pronto da JUCE é o seletor Digital/Analog/Tape (choice
// de 3 valores exibido como 3 botões, não um combobox); esse usa o
// padrão correto pra parâmetros discretos fora de Slider/Button/
// ComboBox: RangedAudioParameter::setValueNotifyingHost() dentro de
// um begin/endChangeGesture(), sincronizado de volta pelo mesmo timer
// que já atualiza os value labels (leitura, não é o "polling de
// escrita" que se quer evitar).
// ============================================================

class NFWhiteDelayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit NFWhiteDelayAudioProcessorEditor(NFWhiteDelayAudioProcessor&);
    ~NFWhiteDelayAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateDisplay();
    void updateTimeControlForSync(); // SYNC on ⇒ TIME dimmed (delay follows division)
    void showHamburgerMenu();
    void showPresetPlaceholder();
    void layoutContent(); // layout fixo (defaultWidth x defaultHeight), calculado uma vez

    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;
    using ButtonAttachment = APVTS::ButtonAttachment;
    using ComboBoxAttachment = APVTS::ComboBoxAttachment;
    using ValueFormatter = std::function<juce::String(double)>;

    // Label com fundo em "chip" (fundo claro + borda) por trás do
    // texto -- FASE 7.2, valores dos knobs mostrados como chip, não
    // texto solto flutuando.
    struct ValueChip : public juce::Label
    {
        void paint(juce::Graphics&) override;
    };

    // Um rotary + rótulo de título + chip de VALOR + attachment.
    struct RotaryControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::Label titleLabel;
        ValueChip valueLabel;
        std::unique_ptr<SliderAttachment> attachment;
    };

    // Um TextButton com toggle + attachment.
    struct ToggleControl
    {
        juce::TextButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    // Um ComboBox + rótulo + attachment.
    struct ChoiceControl
    {
        juce::Label titleLabel;
        juce::ComboBox box;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    // N botões mutuamente exclusivos ligados a um AudioParameterChoice
    // -- ver setupSegmented(). Usado só pro seletor Digital/Analog/
    // Tape. Sem Attachment pronto da JUCE pra esse caso -- ver nota no
    // topo do arquivo.
    struct SegmentedControl
    {
        juce::OwnedArray<juce::TextButton> buttons;
        juce::RangedAudioParameter* parameter = nullptr;
    };

    // Painel escuro do display central (preto profundo + glow neon
    // azul) -- cuida do fundo/glow/moldura/linha de status; os labels
    // de conteúdo são filhos dele, adicionados pelo editor. FASE 7.3:
    // animPhase/activity alimentam a animação sutil ("display vivo")
    // -- atualizados pelo editor em updateDisplay() a partir de dados
    // REAIS da APVTS (bypass + Dry/Wet), nunca de metering fictício.
    struct DisplayPanel : public juce::Component
    {
        float animPhase = 0.0f;
        float activity = 0.0f;
        float meterL = 0.0f; // 0..1 smoothed UI readout of wet L (viz tap)
        float meterR = 0.0f; // 0..1 smoothed UI readout of wet R (viz tap)
        void paint(juce::Graphics&) override;
        void consumeWetActivity(NFWhiteDelayAudioProcessor&);

    private:
        static constexpr int kVizHistory = 96;
        float vizHistL[kVizHistory] {};
        float vizHistR[kVizHistory] {};
        int vizWrite = 0;
        int vizFilled = 0;

        juce::Image cachedStaticLayer;
        int cachedStaticW = -1, cachedStaticH = -1;
        void renderStaticLayerToCache();
        void paintSideMeter(juce::Graphics&, juce::Rectangle<float> glass,
                             float centreXNorm, float level, const char* label) const;
        void paintActivityVisualizer(juce::Graphics&, juce::Rectangle<float> glass) const;
    };

    // Selo/logo "NF" -- chip metálico com sombra própria por trás do
    // texto, em vez de um Label puro flutuando (FASE 7.3, item 1:
    // identidade mais forte). Continua recebendo o mouseListener pra
    // double-click = reset de tamanho, igual a um Label normal.
    struct NfLogoBadge : public juce::Component
    {
        void paint(juce::Graphics&) override;
    };

    // Painel decorativo com fundo levemente diferenciado do chassis +
    // borda fina -- usado pras 3 seções avançadas (título centralizado
    // no topo, ladeado por tracinhos finos) e pros containers
    // agrupados de botões (PING PONG+LO-FI, DIGITAL/ANALOG/TAPE --
    // título vazio nesse caso, só o agrupamento visual).
    struct SectionPanel : public juce::Component
    {
        juce::String title; // empty ⇒ recessed button well; non-empty ⇒ embedded module
        void paint(juce::Graphics&) override;
    };

    // FASE 2 -- header icons use official BinaryData bitmaps
    // (icon_save / icon_menu / bypass_on|off). Behaviour unchanged:
    // SAVE = preset placeholder; MENU = help/about; BYPASS = APVTS.
    struct SaveIconButton : public juce::Button
    {
        SaveIconButton() : juce::Button("save") {}
        void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    };

    struct MenuIconButton : public juce::Button
    {
        MenuIconButton() : juce::Button("menu") {}
        void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    };

    struct BypassButton : public juce::Button
    {
        BypassButton() : juce::Button("bypass") {}
        void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    };

    // Official circular POWER — same APVTS bypass param (lit = processing).
    // Plain Component (not Button) so paint is reliable under content transforms.
    struct PowerButton : public juce::Component
    {
        bool lit = true;
        bool pressed = false;
        std::function<void()> onClick;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
    };

    // Linha fina (divisor / underline neon do header).
    struct ThinLine : public juce::Component
    {
        juce::Colour colour { 0x00000000 };
        bool neonGlow = false;
        void paint(juce::Graphics&) override;
    };

    void setupRotary(RotaryControl&, const juce::String& paramID, const juce::String& title,
                      ValueFormatter formatValue);
    void setupToggle(ToggleControl&, const juce::String& paramID, const juce::String& text);
    void setupChoice(ChoiceControl&, const juce::String& paramID, const juce::String& title,
                      const juce::StringArray& choices);
    void setupSegmented(SegmentedControl&, const juce::String& paramID, const juce::StringArray& choices);

    static void positionRotary(RotaryControl&, juce::Rectangle<int> area);
    static void positionToggle(ToggleControl&, juce::Rectangle<int> area);
    static void positionChoice(ChoiceControl&, juce::Rectangle<int> area);
    static void positionSegmented(SegmentedControl&, juce::Rectangle<int> area);

    // Uma função de refresh por controle (RotaryControl e
    // SegmentedControl), reaplicada a 10Hz pelo timer (ver
    // timerCallback()) -- garante que a UI acompanhe TANTO a interação
    // do usuário quanto automação externa do host. Leitura pura, nunca
    // escreve nada.
    std::vector<std::function<void()>> controlRefreshers;

    NFWhiteDelayAudioProcessor& audioProcessor;
    NFWhiteLookAndFeel nfLookAndFeel;

    // FASE 2 -- official BinaryData knobs (large/small) + SYNC button
    // assets via NFPremiumLookAndFeel. Applied to all rotary controls
    // listed in the reference (hero + advanced panels).
    NFPremiumLookAndFeel premiumLookAndFeel;

    // Container de tamanho FIXO (defaultWidth x defaultHeight) --
    // recebe uma AffineTransform::scale() em resized() pra escalar TODO
    // o conteúdo proporcionalmente quando a janela é redimensionada --
    // todo o layout é feito em coordenadas fixas dessa escala de
    // referência (ver layoutContent()), então o alinhamento interno
    // nunca fica "torto" no resize -- só escala uniformemente.
    // chassisBays = recessed fitting frames machined into the plate.
    struct ContentRoot : public juce::Component
    {
        juce::Array<juce::Rectangle<float>> chassisBays;
        void paint(juce::Graphics&) override;
    };
    ContentRoot content;

    // Chrome fora do content transform — AffineTransform + OpaqueLayer
    // engole paint de alguns filhos; POWER/neon ficam no editor.
    juce::Rectangle<int> neonBarLogical;
    juce::Rectangle<int> powerLogical;

    // ---- Header ----
    NfLogoBadge nfLogoLabel;
    juce::Label audioToolsLabel, whiteDelayLabel, professionalDelayLabel;
    ThinLine headerDivider;
    SaveIconButton saveButton;
    MenuIconButton hamburgerButton;
    PowerButton powerButton;
    BypassButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    // ---- Display central ----
    DisplayPanel displayPanel;
    juce::Label displayLine1, displayLine2, displayLine3;
    juce::Label displayStatusTitle[4], displayStatusValue[4]; // DELAY / PING PONG / LO-FI / MODE

    // ---- Centro principal (os 4 protagonistas) ----
    RotaryControl timeControl, feedbackControl, dryWetControl, outputControl;

    // ---- Controles rápidos ----
    ToggleControl syncControl, pingPongControl, loFiControl;
    ChoiceControl divisionControl, modifierControl;
    SegmentedControl modeControl;
    SectionPanel pingPongLoFiGroup, modeGroup; // fundo compartilhado, puramente visual

    // ---- Avançado: Modulation ----
    SectionPanel modulationPanel;
    RotaryControl rateControl, depthControl, spreadControl;
    ChoiceControl shapeControl;

    // ---- Avançado: Filters ----
    SectionPanel filtersPanel;
    RotaryControl highPassControl, lowPassControl, resonanceControl;

    // ---- Avançado: Character ----
    SectionPanel characterPanel;
    RotaryControl characterControl, duckingControl;

    // Canvas de referência FIXO usado por layoutContent() -- TODO o
    // posicionamento interno é calculado nesses pixels; nunca mude
    // estes dois valores sozinhos, pois eles são a régua de todo o
    // layout (mudar exigiria reconferir cada offset fixo em
    // layoutContent()).
    static constexpr int defaultWidth = 1280;
    static constexpr int defaultHeight = 860; // taller — bay lips + bottom modules clear bevel

    // Tamanho de ABERTURA/reset — mesma proporção do canvas de referência
    // (scale uniforme em resized(); layout interno não muda).
    static constexpr int openWidth = 1000;
    static constexpr int openHeight = 672; // 1000 * 860/1280

    static constexpr int minWidth = 900;
    static constexpr int minHeight = 600;
    static constexpr int maxWidth = 1920;
    static constexpr int maxHeight = 1290;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFWhiteDelayAudioProcessorEditor)
};
