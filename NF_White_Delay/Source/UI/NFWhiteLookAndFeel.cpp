#include "NFWhiteLookAndFeel.h"
#include <BinaryData.h>

// Paleta FASE 7 / 7.2 -- base quase-monocromática (branco/off-white/
// cinza/prata/preto) com azul neon como acento -- display, glow de
// estado ligado, e (desde a FASE 7.2, pra bater com a referência
// visual oficial) também o arco de VALOR dos knobs.
const juce::Colour NFWhiteLookAndFeel::kBackground        { 0xfff7f8fa };
const juce::Colour NFWhiteLookAndFeel::kPanelBackground   { 0xffeceef1 };
const juce::Colour NFWhiteLookAndFeel::kDisplayBackground { 0xff05070f }; // preto/navy bem profundo (FASE 7.4)
const juce::Colour NFWhiteLookAndFeel::kDisplayBackgroundEdge { 0xff101c33 }; // navy mais presente nas bordas
const juce::Colour NFWhiteLookAndFeel::kDisplayText       { 0xffeaf6ff };
const juce::Colour NFWhiteLookAndFeel::kDisplayAccent     { 0xff5fd8ff }; // ciano neon
const juce::Colour NFWhiteLookAndFeel::kNeonGlow          { 0xff3fb8ef }; // só pra efeitos de glow
const juce::Colour NFWhiteLookAndFeel::kText              { 0xff2c2d32 };
const juce::Colour NFWhiteLookAndFeel::kTextMuted         { 0xff8a8c93 };
const juce::Colour NFWhiteLookAndFeel::kAccent            { 0xff3f8fd6 }; // azul do estado ligado (fill)
const juce::Colour NFWhiteLookAndFeel::kKnobLight         { 0xfffdfdfe };
const juce::Colour NFWhiteLookAndFeel::kKnobDark          { 0xffc3c5cb };
const juce::Colour NFWhiteLookAndFeel::kKnobOutline       { 0xffb2b4bb };
const juce::Colour NFWhiteLookAndFeel::kKnobValueArc      { 0xff2f8fe0 }; // azul vivo -- referencia oficial FASE 7.2
const juce::Colour NFWhiteLookAndFeel::kKnobValueArcCore  { 0xff9fe8ff }; // nucleo ciano-claro do arco (FASE 7.2B)
const juce::Colour NFWhiteLookAndFeel::kTrackBackground   { 0xffdcdee2 };
const juce::Colour NFWhiteLookAndFeel::kBypassActive      { 0xffd8483c };

NFWhiteLookAndFeel::NFWhiteLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, kBackground);

    setColour(juce::Slider::textBoxTextColourId, kText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::thumbColourId, kKnobValueArc);

    setColour(juce::Label::textColourId, kText);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, kKnobOutline);
    setColour(juce::TextEditor::textColourId, kText);
    setColour(juce::CaretComponent::caretColourId, kText);

    setColour(juce::TextButton::buttonColourId, kKnobLight);
    setColour(juce::TextButton::buttonOnColourId, kAccent);
    setColour(juce::TextButton::textColourOffId, kText);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    setColour(juce::ComboBox::backgroundColourId, kKnobLight);
    setColour(juce::ComboBox::textColourId, kText);
    setColour(juce::ComboBox::outlineColourId, kKnobOutline);
    setColour(juce::ComboBox::arrowColourId, kTextMuted);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kPanelBackground);
    setColour(juce::PopupMenu::highlightedTextColourId, kText);
}

void NFWhiteLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider&)
{
    // FASE 2 -- official White Delay knobs (reference-matched).
    // Body: BinaryData knob_large / knob_small (pointer at 12 o'clock).
    // Overlay: soft ground shadow, ticks, cyan value arc just outside the rim.
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(2.0f);
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    if (diameter < 8.0f)
        return;

    auto knobArea = bounds.withSizeKeepingCentre(diameter, diameter);
    const float radius = diameter * 0.5f;
    const auto centre = knobArea.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const bool useLarge = diameter >= 72.0f;
    const float knobDiameter = diameter * (useLarge ? 0.76f : 0.74f);
    const float knobRadius = knobDiameter * 0.5f;
    auto knobRect = juce::Rectangle<float>(knobDiameter, knobDiameter).withCentre(centre);

    {
        auto shadowEllipse = juce::Rectangle<float>(knobDiameter * 0.95f, knobDiameter * 0.22f)
                                 .withCentre({ centre.x, centre.y + knobRadius * 0.82f });
        juce::ColourGradient shadowGrad(juce::Colours::black.withAlpha(useLarge ? 0.20f : 0.16f),
                                         centre.x, shadowEllipse.getY(),
                                         juce::Colours::transparentBlack,
                                         centre.x, shadowEllipse.getBottom(), false);
        g.setGradientFill(shadowGrad);
        g.fillEllipse(shadowEllipse);
    }

    constexpr int numTicks = 11;
    const float tickOuter = radius * 0.985f;
    g.setColour(kKnobOutline.darker(0.18f));
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = (float) i / (float) (numTicks - 1);
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const bool major = (i % 5 == 0);
        const float tickInner = radius * (major ? 0.905f : 0.935f);
        g.drawLine({ centre.getPointOnCircumference(tickInner, tickAngle),
                     centre.getPointOnCircumference(tickOuter, tickAngle) },
                   major ? 1.5f : 1.0f);
    }

    const float trackRadius = knobRadius * 1.055f;
    const float trackThickness = juce::jmax(2.0f, knobRadius * (useLarge ? 0.078f : 0.088f));

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(kTrackBackground.withAlpha(0.85f));
    g.strokePath(track, juce::PathStrokeType(trackThickness * 0.85f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                rotaryStartAngle, angle, true);

        for (int i = 3; i >= 1; --i)
        {
            const float expand = (float) i * (useLarge ? 1.7f : 1.35f);
            g.setColour(kKnobValueArc.darker(0.15f).withAlpha(0.11f / (float) i));
            g.strokePath(valueArc, juce::PathStrokeType(trackThickness + expand,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }

        g.setColour(kKnobValueArc.brighter(0.08f));
        g.strokePath(valueArc, juce::PathStrokeType(trackThickness,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        g.setColour(kKnobValueArcCore.withAlpha(0.95f));
        g.strokePath(valueArc, juce::PathStrokeType(trackThickness * 0.32f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    const auto* data = useLarge ? BinaryData::knob_large_png : BinaryData::knob_small_png;
    const int dataSize = useLarge ? BinaryData::knob_large_pngSize : BinaryData::knob_small_pngSize;
    const auto knobAsset = juce::ImageCache::getFromMemory(data, dataSize);

    if (knobAsset.isValid())
    {
        juce::Graphics::ScopedSaveState state(g);
        // Clip to the circular body so the PNG's white square corners
        // never flash when the asset is rotated.
        juce::Path clip;
        clip.addEllipse(knobRect.expanded(0.5f));
        g.reduceClipRegion(clip);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.drawImage(knobAsset, knobRect, juce::RectanglePlacement::centred);
    }
}

void NFWhiteLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&, bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();

    const juce::Colour onColour = button.findColour(juce::TextButton::buttonOnColourId);
    juce::Colour fill = isOn ? onColour : button.findColour(juce::TextButton::buttonColourId);

    if (! isEnabled)                        fill = fill.withMultipliedSaturation(0.3f).withAlpha(0.55f);
    else if (shouldDrawButtonAsDown)        fill = fill.darker(0.15f);
    else if (shouldDrawButtonAsHighlighted) fill = fill.brighter(isOn ? 0.10f : 0.06f);

    const float corner = juce::jmin(7.0f, bounds.getHeight() * 0.32f);

    // Sombra projetada discreta por baixo do botão -- "relevo" físico,
    // não um retângulo plano colado no chassis (FASE 7.2B).
    if (isEnabled)
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(bounds, corner);
        juce::DropShadow buttonShadow(juce::Colours::black.withAlpha(0.14f), 3, juce::Point<int>(0, 1));
        buttonShadow.drawForPath(g, shadowPath);
    }

    // Glow suave por trás quando ligado -- camadas concêntricas com
    // alpha decrescente, ao invés de um blur de verdade (JUCE não tem
    // blur de path barato) -- dá a sensação de "iluminado" sem ficar
    // "gamer" (pedido explícito: nada exagerado). Um pouco mais forte
    // no hover, pra dar feedback de interatividade.
    if (isOn && isEnabled)
    {
        // Usa a própria onColour do botão como base do glow -- dá azul
        // neon nos botões normais (onColour=kAccent) e vermelho no
        // Bypass (onColour=kBypassActive, setado localmente), sem
        // precisar de um caso especial.
        const float glowBoost = shouldDrawButtonAsHighlighted ? 1.3f : 1.0f;
        for (int i = 3; i >= 1; --i)
        {
            const float expand = (float) i * 2.2f;
            g.setColour(onColour.withAlpha((0.10f * glowBoost) / (float) i));
            g.fillRoundedRectangle(bounds.expanded(expand), corner + expand);
        }
    }

    // Preenchimento com "relevo" -- gradiente vertical (mais claro no
    // topo, mais escuro na base) em vez de cor chapada, tanto no OFF
    // (prata/off-white) quanto no ON (azul neon).
    juce::ColourGradient fillGradient(fill.brighter(isOn ? 0.16f : 0.35f), bounds.getCentreX(), bounds.getY(),
                                       fill.darker(isOn ? 0.10f : 0.05f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillRoundedRectangle(bounds, corner);

    // Realce fino no topo -- linha clara logo abaixo da borda superior,
    // como luz pegando o canto de uma peça com relevo físico.
    if (isEnabled)
    {
        auto topArea = bounds.reduced(1.2f).withHeight(bounds.getHeight() * 0.45f);
        juce::Path topHighlight;
        topHighlight.addRoundedRectangle(topArea.getX(), topArea.getY(), topArea.getWidth(), topArea.getHeight(),
                                          corner, corner, true, true, false, false);
        g.setColour(juce::Colours::white.withAlpha(isOn ? 0.16f : 0.45f));
        g.strokePath(topHighlight, juce::PathStrokeType(1.0f));
    }

    // Borda dupla discreta -- linha externa escura (define o contorno)
    // + linha interna clara fina (bisel), em vez de um único traço.
    g.setColour(isOn ? fill.darker(0.35f) : kKnobOutline.withAlpha(isEnabled ? 1.0f : 0.5f));
    g.drawRoundedRectangle(bounds, corner, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(isEnabled ? (isOn ? 0.12f : 0.55f) : 0.15f));
    g.drawRoundedRectangle(bounds.reduced(1.4f), juce::jmax(0.0f, corner - 1.4f), 0.8f);
}

void NFWhiteLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    const bool isOn = button.getToggleState();
    juce::Colour textColour = isOn ? button.findColour(juce::TextButton::textColourOnId)
                                    : button.findColour(juce::TextButton::textColourOffId);
    if (! button.isEnabled())
        textColour = textColour.withAlpha(0.45f);

    g.setColour(textColour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(2, 0),
                      juce::Justification::centred, 2);
}

void NFWhiteLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                       int buttonX, int buttonY, int buttonW, int buttonH,
                                       juce::ComboBox& box)
{
    // ComboBox customizado (FASE 7.2, item 4) -- fundo branco
    // metálico, borda cinza fina, seta própria desenhada via Path
    // (não a seta padrão do JUCE), leve realce azul discreto no hover.
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const float corner = juce::jmin(6.0f, bounds.getHeight() * 0.28f);
    const bool hovered = box.isMouseOver();

    // Sombra leve por baixo -- mesmo tratamento de "bevel" dos botões,
    // pra a caixa parecer parte do mesmo hardware (FASE 7.2B).
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(bounds, corner);
        juce::DropShadow boxShadow(juce::Colours::black.withAlpha(0.12f), 2, juce::Point<int>(0, 1));
        boxShadow.drawForPath(g, shadowPath);
    }

    juce::ColourGradient bg(kKnobLight, bounds.getCentreX(), bounds.getY(),
                             kKnobDark.interpolatedWith(kKnobLight, 0.5f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, corner);

    // Realce fino no topo (bisel) -- mesma linguagem dos botões.
    auto topArea = bounds.reduced(1.2f).withHeight(bounds.getHeight() * 0.5f);
    juce::Path topHighlight;
    topHighlight.addRoundedRectangle(topArea.getX(), topArea.getY(), topArea.getWidth(), topArea.getHeight(),
                                      corner, corner, true, true, false, false);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.strokePath(topHighlight, juce::PathStrokeType(1.0f));

    // Sombra interna sutil na base -- profundidade extra (FASE 7.3,
    // item 7), como se a seta/valor estivessem ligeiramente
    // "afundados" no material, não impressos por cima.
    {
        auto bottomArea = bounds.reduced(1.4f).withHeight(bounds.getHeight() * 0.4f)
                                 .withY(bounds.getBottom() - bounds.getHeight() * 0.4f - 1.4f);
        juce::ColourGradient innerShadow(juce::Colours::black.withAlpha(0.0f), bottomArea.getCentreX(), bottomArea.getY(),
                                          juce::Colours::black.withAlpha(0.06f), bottomArea.getCentreX(), bottomArea.getBottom(), false);
        g.setGradientFill(innerShadow);
        g.fillRoundedRectangle(bottomArea, juce::jmax(0.0f, corner - 1.4f));
    }

    g.setColour(hovered ? kAccent.withAlpha(0.6f) : kKnobOutline);
    g.drawRoundedRectangle(bounds, corner, 1.1f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(1.4f), juce::jmax(0.0f, corner - 1.4f), 0.7f);

    // Seta customizada -- triângulo simples, sem depender do glifo
    // padrão do LookAndFeel base.
    auto arrowArea = juce::Rectangle<float>((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    auto arrowCentre = arrowArea.getCentre();
    const float arrowSize = juce::jmin(arrowArea.getWidth(), arrowArea.getHeight()) * 0.32f;

    juce::Path arrow;
    arrow.startNewSubPath(arrowCentre.x - arrowSize, arrowCentre.y - arrowSize * 0.4f);
    arrow.lineTo(arrowCentre.x + arrowSize, arrowCentre.y - arrowSize * 0.4f);
    arrow.lineTo(arrowCentre.x, arrowCentre.y + arrowSize * 0.5f);
    arrow.closeSubPath();

    g.setColour(hovered ? kAccent : kTextMuted);
    g.fillPath(arrow);
}

void NFWhiteLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    // Popup customizado (item 6 da referência) -- painel branco/prata
    // com cantos arredondados, borda fina e sombra suave, coerente com
    // o resto do hardware, em vez do popup retangular padrão do JUCE.
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(1.0f);
    constexpr float corner = 7.0f;

    juce::Path shadowPath;
    shadowPath.addRoundedRectangle(bounds, corner);
    juce::DropShadow popupShadow(juce::Colours::black.withAlpha(0.22f), 10, juce::Point<int>(0, 3));
    popupShadow.drawForPath(g, shadowPath);

    g.setColour(findColour(juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(kKnobOutline);
    g.drawRoundedRectangle(bounds, corner, 1.0f);
}

void NFWhiteLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                            bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                            const juce::String& text, const juce::String& shortcutKeyText,
                                            const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.toFloat().reduced(10.0f, 0.0f);
        g.setColour(kKnobOutline.withAlpha(0.6f));
        g.drawLine(r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);
        return;
    }

    auto bounds = area.toFloat().reduced(4.0f, 1.0f);

    if (isHighlighted && isActive)
    {
        g.setColour(kAccent.withAlpha(0.14f));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(kAccent.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    auto textColourToUse = textColour != nullptr ? *textColour : kText;
    if (! isActive)
        textColourToUse = textColourToUse.withAlpha(0.4f);

    g.setColour(textColourToUse);
    g.setFont(juce::FontOptions(13.0f));
    g.drawFittedText(text, bounds.reduced(10.0f, 0.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);

    juce::ignoreUnused(isTicked, hasSubMenu, icon, shortcutKeyText);
}

juce::Font NFWhiteLookAndFeel::getLabelFont(juce::Label& label)
{
    return juce::FontOptions(juce::jmin(14.0f, (float) label.getHeight() * 0.75f));
}

juce::Font NFWhiteLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::FontOptions(juce::jmin(12.5f, (float) box.getHeight() * 0.55f));
}

juce::Font NFWhiteLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions(juce::jmin(12.0f, (float) buttonHeight * 0.4f));
}
