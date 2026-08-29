#include "PluginEditor.h"
#include <BinaryData.h>

using ParamIDs = NFWhiteDelayAudioProcessor::ParamIDs;
using LNF = NFWhiteLookAndFeel;

namespace
{
    juce::String formatMs(double v)      { return juce::String(juce::roundToInt(v)) + " ms"; }
    juce::String formatPercent(double v) { return juce::String(juce::roundToInt(v)) + " %"; }
    juce::String formatDbSigned(double v)
    {
        return (v >= 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
    }
    juce::String formatRateHz(double v)  { return juce::String(v, 2) + " Hz"; }
    juce::String formatFilterHz(double v)
    {
        if (v >= 1000.0)
            return juce::String(v / 1000.0, 1) + " kHz";
        return juce::String(juce::roundToInt(v)) + " Hz";
    }
}

void NFWhiteDelayAudioProcessorEditor::ValueChip::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f, 0.5f);
    g.setColour(LNF::kKnobLight);
    g.fillRoundedRectangle(b, 5.0f);
    g.setColour(LNF::kKnobOutline);
    g.drawRoundedRectangle(b, 5.0f, 1.0f);

    g.setColour(findColour(juce::Label::textColourId));
    g.setFont(getFont());
    g.drawFittedText(getText(), getLocalBounds().reduced(3, 0), juce::Justification::centred, 1);
}

void NFWhiteDelayAudioProcessorEditor::ThinLine::paint(juce::Graphics& g)
{
    g.setColour(colour);
    g.fillRect(getLocalBounds());
}

void NFWhiteDelayAudioProcessorEditor::NfLogoBadge::paint(juce::Graphics& g)
{
    // Selo/chip metálico -- FASE 7.3: identidade "NF" com peso e
    // profundidade próprios (sombra + gradiente + aro), em vez de duas
    // letras soltas sobre o chassis.
    auto bounds = getLocalBounds().toFloat();
    const float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto badge = bounds.withSizeKeepingCentre(side, side);
    const float corner = side * 0.24f;

    juce::Path badgePath;
    badgePath.addRoundedRectangle(badge, corner);
    juce::DropShadow badgeShadow(juce::Colours::black.withAlpha(0.20f), (int) (side * 0.10f), juce::Point<int>(0, (int) (side * 0.035f)));
    badgeShadow.drawForPath(g, badgePath);

    juce::ColourGradient badgeGradient(LNF::kKnobLight, badge.getCentreX(), badge.getY(),
                                        LNF::kKnobDark, badge.getCentreX(), badge.getBottom(), false);
    badgeGradient.addColour(0.5, LNF::kKnobLight.interpolatedWith(LNF::kKnobDark, 0.20f));
    g.setGradientFill(badgeGradient);
    g.fillRoundedRectangle(badge, corner);

    // Realce superior (bisel de luz).
    auto topHalf = badge.reduced(1.2f).withHeight(badge.getHeight() * 0.48f);
    juce::ColourGradient topHighlight(juce::Colours::white.withAlpha(0.55f), topHalf.getCentreX(), topHalf.getY(),
                                       juce::Colours::white.withAlpha(0.0f), topHalf.getCentreX(), topHalf.getBottom(), false);
    g.setGradientFill(topHighlight);
    g.fillRoundedRectangle(topHalf, corner);

    g.setColour(LNF::kKnobOutline.darker(0.15f));
    g.drawRoundedRectangle(badge, corner, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(badge.reduced(1.6f), juce::jmax(0.0f, corner - 1.2f), 0.8f);

    g.setColour(LNF::kText);
    g.setFont(juce::FontOptions(side * 0.44f, juce::Font::bold));
    g.drawFittedText("NF", badge.toNearestInt(), juce::Justification::centred, 1);
}

void NFWhiteDelayAudioProcessorEditor::SectionPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    constexpr float corner = 10.0f;

    // Sombra projetada por baixo do painel inteiro -- lê como uma
    // placa física separada, pousada sobre o chassis, não uma área
    // pintada no mesmo plano (FASE 7.3, item 8).
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(bounds, corner);
        juce::DropShadow panelShadow(juce::Colours::black.withAlpha(0.10f), 6, juce::Point<int>(0, 2));
        panelShadow.drawForPath(g, shadowPath);
    }

    g.setColour(LNF::kPanelBackground);
    g.fillRoundedRectangle(bounds, corner);

    // Sombra interna quase imperceptível no topo -- sensação de painel
    // levemente rebaixado no chassis, não colado por cima (FASE 7.2B).
    {
        auto topShadowArea = bounds.reduced(1.5f).withHeight(bounds.getHeight() * 0.22f);
        juce::ColourGradient innerShadow(juce::Colours::black.withAlpha(0.05f), topShadowArea.getCentreX(), topShadowArea.getY(),
                                          juce::Colours::black.withAlpha(0.0f), topShadowArea.getCentreX(), topShadowArea.getBottom(), false);
        g.setGradientFill(innerShadow);
        g.fillRoundedRectangle(bounds.reduced(1.5f), corner - 1.5f);
    }

    g.setColour(LNF::kKnobOutline.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(1.6f), corner - 1.0f, 0.7f);

    if (title.isNotEmpty())
    {
        juce::Font font(juce::FontOptions(12.0f, juce::Font::bold));
        g.setFont(font);
        const float textWidth = juce::GlyphArrangement::getStringWidth(font, title);
        const float pad = 10.0f;
        const float centreX = bounds.getCentreX();
        const float titleY = bounds.getY() + 2.0f;
        const float titleH = 18.0f;

        g.setColour(LNF::kTextMuted.darker(0.25f));
        g.drawText(title, juce::Rectangle<float>(centreX - textWidth * 0.5f - pad, titleY,
                                                   textWidth + pad * 2.0f, titleH),
                   juce::Justification::centred);

        // Pequeno ponto de acento à esquerda do título -- toque de
        // identidade, sem pesar a composição.
        g.setColour(LNF::kAccent.withAlpha(0.55f));
        g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f)
                           .withCentre({ centreX - textWidth * 0.5f - pad - 4.0f, titleY + titleH * 0.5f }));

        // Tracinhos finos ladeando o título -- "MODULATION" ladeado
        // por linhas curtas, não uma caixa pesada.
        const float lineY = titleY + titleH * 0.5f;
        const float lineMarginFromEdge = 16.0f;
        const float gapFromText = textWidth * 0.5f + pad + 8.0f;
        g.setColour(LNF::kKnobOutline);
        g.drawLine(bounds.getX() + lineMarginFromEdge, lineY, centreX - gapFromText, lineY, 1.0f);
        g.drawLine(centreX + gapFromText, lineY, bounds.getRight() - lineMarginFromEdge, lineY, 1.0f);
    }
}

void NFWhiteDelayAudioProcessorEditor::DisplayPanel::renderStaticLayerToCache()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    constexpr float scaleFactor = 2.0f; // qualidade "retina" no cache
    cachedStaticLayer = juce::Image(juce::Image::ARGB, (int) ((float) w * scaleFactor), (int) ((float) h * scaleFactor), true);
    juce::Graphics g(cachedStaticLayer);
    g.addTransform(juce::AffineTransform::scale(scaleFactor));

    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h);
    constexpr float corner = 12.0f;

    // Moldura metálica escura ao redor do vidro -- sensação de display
    // de hardware encaixado no chassis, não um retângulo preto boiando.
    g.setColour(juce::Colour(0xff3a3d45));
    g.fillRoundedRectangle(bounds, corner);
    juce::ColourGradient bezelGradient(juce::Colour(0xff55585f), bounds.getCentreX(), bounds.getY(),
                                        juce::Colour(0xff222327), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(bezelGradient);
    g.drawRoundedRectangle(bounds.reduced(1.0f), corner - 1.0f, 2.0f);

    // Vidro -- preto/navy bem profundo, mais navy ainda nas bordas
    // (item 4: "black/very dark navy background").
    auto glass = bounds.reduced(3.0f);
    const float glassCorner = corner - 3.0f;
    juce::ColourGradient bgGradient(LNF::kDisplayBackground, glass.getCentreX(), glass.getCentreY(),
                                     LNF::kDisplayBackgroundEdge, glass.getX(), glass.getY(), true);
    g.setGradientFill(bgGradient);
    g.fillRoundedRectangle(glass, glassCorner);

    // Vinheta -- escurece sutilmente os cantos do vidro, reforça
    // profundidade "por trás do vidro" (item 4: "depth behind the
    // glass").
    {
        juce::ColourGradient vignette(juce::Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                                       juce::Colours::black.withAlpha(0.35f), glass.getX(), glass.getY(), true);
        g.setGradientFill(vignette);
        g.fillRoundedRectangle(glass, glassCorner);
    }

    // Linha neon fina separando o valor principal do resto -- posição
    // fixa, não anima, então fica no cache.
    const float lineY = glass.getY() + glass.getHeight() * 0.56f;
    {
        juce::ColourGradient lineGradient(LNF::kNeonGlow.withAlpha(0.0f), glass.getX(), lineY,
                                           LNF::kDisplayAccent.withAlpha(0.55f), glass.getCentreX(), lineY, false);
        lineGradient.addColour(0.5, LNF::kDisplayAccent.withAlpha(0.55f));
        lineGradient.addColour(1.0, LNF::kNeonGlow.withAlpha(0.0f));
        g.setGradientFill(lineGradient);
        g.fillRect(juce::Rectangle<float>(glass.getX() + glass.getWidth() * 0.1f, lineY,
                                           glass.getWidth() * 0.8f, 1.2f));
    }

    // Segunda linha, mais discreta, acima da fileira de status.
    const float lowerLineY = glass.getY() + glass.getHeight() * 0.82f;
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(juce::Rectangle<float>(glass.getX() + glass.getWidth() * 0.06f, lowerLineY,
                                       glass.getWidth() * 0.88f, 1.0f));

    // Borda do vidro, fina e nítida, com leve glow externo (item 4:
    // "inner shadow" -- a borda funciona como o limite escurecido do
    // vidro, complementando a vinheta acima).
    g.setColour(LNF::kNeonGlow.withAlpha(0.16f));
    g.drawRoundedRectangle(glass.reduced(0.5f), glassCorner, 3.0f);
    g.setColour(LNF::kDisplayAccent.withAlpha(0.5f));
    g.drawRoundedRectangle(glass.reduced(1.6f), glassCorner - 1.0f, 1.0f);

    // Reflexo sutil no topo do vidro (item 4: "subtle reflection").
    juce::Path reflection;
    reflection.startNewSubPath(glass.getX() + 6.0f, glass.getY() + 3.0f);
    reflection.lineTo(glass.getRight() * 0.55f, glass.getY() + 3.0f);
    reflection.lineTo(glass.getRight() * 0.35f, glass.getY() + glass.getHeight() * 0.16f);
    reflection.lineTo(glass.getX() + 6.0f, glass.getY() + glass.getHeight() * 0.16f);
    reflection.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.035f));
    g.fillPath(reflection);

    // FASE 2: official display_frame.png over the procedural glass
    // (transparent centre of the asset keeps the neon glass visible).
    {
        const auto frame = juce::ImageCache::getFromMemory (BinaryData::display_frame_png,
                                                            BinaryData::display_frame_pngSize);
        if (frame.isValid())
            g.drawImage (frame, bounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
    }

    cachedStaticW = w;
    cachedStaticH = h;
}

void NFWhiteDelayAudioProcessorEditor::DisplayPanel::paint(juce::Graphics& g)
{
    // Camadas estáticas (bezel/vidro/vinheta/linhas/borda/reflexo) --
    // pré-renderizadas uma vez por tamanho e só reblitadas aqui (item 6
    // do pedido: nada caro é refeito a cada frame). Só recalcula se o
    // tamanho do painel mudou (ex.: resize da janela).
    if (getWidth() != cachedStaticW || getHeight() != cachedStaticH || ! cachedStaticLayer.isValid())
        renderStaticLayerToCache();
    g.drawImage(cachedStaticLayer, 0, 0, getWidth(), getHeight(), 0, 0, cachedStaticLayer.getWidth(), cachedStaticLayer.getHeight());

    auto bounds = getLocalBounds().toFloat();
    constexpr float corner = 12.0f;
    auto glass = bounds.reduced(3.0f);
    const float glassCorner = corner - 3.0f;

    // Tudo daqui pra baixo É dinâmico (muda com animPhase/activity a
    // cada frame) -- glow/halo respirando, vazamento de luz, waveform,
    // anéis do "eco". animPhase/activity são atualizados em
    // updateDisplay() a partir de audioProcessor.wetActivity (envelope
    // REAL do sinal wet, calculado no audio thread -- ver
    // PluginProcessor.h/.cpp), não mais um proxy de parâmetros.

    // Glow neon atrás da linha grande de texto -- preenchendo um
    // RETÂNGULO, não uma elipse (um ColourGradient radial do JUCE é
    // sempre circular; fillEllipse numa área alongada corta o
    // gradiente antes dele desvanecer, criando uma borda dura em vez
    // de um brilho suave -- o retângulo deixa o próprio GRADIENTE
    // desenhar a forma orgânica).
    // Respiração muito sutil (FASE 7.3, item 5: "display vivo") --
    // module a INTENSIDADE do glow/halo com uma senoide lenta de
    // animPhase, misturada com activity: quase estática quando o delay
    // está bypassado/100% dry, com uma leve pulsação quando ativo.
    // Nunca chega a "piscar" -- variação de +-12% no máximo.
    const float breathe = std::sin(animPhase * 0.55f);
    const float breatheAmount = 0.03f + 0.12f * activity;

    auto glowArea = glass.withSizeKeepingCentre(glass.getWidth() * 0.9f, glass.getHeight() * 0.9f)
                         .withY(glass.getY())
                         .withHeight(glass.getHeight() * 0.5f);
    const float glowRadius = juce::jmax(glowArea.getWidth(), glowArea.getHeight()) * 0.5f;
    const float glowAlpha = 0.34f * (1.0f + breathe * breatheAmount);
    juce::ColourGradient glow(LNF::kNeonGlow.withAlpha(glowAlpha), glowArea.getCentreX(), glowArea.getCentreY(),
                               LNF::kNeonGlow.withAlpha(0.0f), glowArea.getCentreX() + glowRadius, glowArea.getCentreY(), true);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(glass, glassCorner);

    // Halo mais apertado e mais brilhante, bem atrás do valor grande --
    // "brilho interno" concentrado, por cima do glow amplo acima (que
    // fica mais como iluminação ambiente do vidro inteiro).
    {
        auto innerHaloArea = glowArea.withSizeKeepingCentre(glowArea.getWidth() * 0.55f, glowArea.getHeight() * 0.62f);
        const float innerRadius = juce::jmax(innerHaloArea.getWidth(), innerHaloArea.getHeight()) * 0.5f;
        const float innerAlpha = 0.32f * (1.0f + breathe * breatheAmount * 1.4f);
        juce::ColourGradient innerHalo(LNF::kDisplayAccent.withAlpha(innerAlpha), innerHaloArea.getCentreX(), innerHaloArea.getCentreY(),
                                        LNF::kDisplayAccent.withAlpha(0.0f), innerHaloArea.getCentreX() + innerRadius, innerHaloArea.getCentreY(), true);
        g.setGradientFill(innerHalo);
        g.fillRoundedRectangle(glass, glassCorner);
    }

    // Luz vazando discretamente do vidro pra moldura metálica ao redor
    // -- reforça a leitura de "tela premium acesa dentro do chassis"
    // (item 5: "mais glow azul, mais profundidade").
    {
        auto bleedArea = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.7f, bounds.getHeight() * 0.55f)
                                .withY(bounds.getY());
        const float bleedRadius = juce::jmax(bleedArea.getWidth(), bleedArea.getHeight()) * 0.5f;
        juce::ColourGradient bleed(LNF::kNeonGlow.withAlpha(0.10f * (1.0f + breathe * breatheAmount)), bleedArea.getCentreX(), bleedArea.getY(),
                                    LNF::kNeonGlow.withAlpha(0.0f), bleedArea.getCentreX(), bleedArea.getY() + bleedRadius, true);
        g.setGradientFill(bleed);
        g.fillRoundedRectangle(bounds, corner);
    }

    // Representação gráfica decorativa de "echo estéreo/waveform" no
    // fundo do display -- uma curva contínua e suave (não barras soltas
    // como na FASE 7.2), estática, NÃO é um medidor de nível real (o
    // motor não expõe metering de sinal pro editor ainda -- seção
    // "informações inexistentes podem ser omitidas"). Puramente
    // ornamental, calculada a partir de uma soma fixa de senoides, não
    // de dado de áudio algum -- desenhada BEM discreta, por trás do
    // valor grande (glow + texto são pintados por cima depois).
    {
        const float waveY = glass.getY() + glass.getHeight() * 0.30f;
        const float waveWidth = glass.getWidth() * 0.86f;
        const float waveX0 = glass.getCentreX() - waveWidth * 0.5f;
        constexpr int numPoints = 64;

        // Amplitude e velocidade ligadas à activity real (bypass +
        // Dry/Wet, ver updateDisplay()) -- quase parada quando o delay
        // não está presente, mais viva quando está.
        const float amp = 3.0f + 7.0f * activity;
        const float phase = animPhase;

        // t em 0..1, soma de 3 senoides com deriva lenta e independente
        // por termo (phase*multiplicador diferente) -- textura de "eco"
        // que evolui organicamente com o tempo, não um scroll uniforme.
        auto sampleWave = [phase](float t)
        {
            return 0.5f * std::sin(t * juce::MathConstants<float>::twoPi * 2.3f + phase * 1.7f)
                 + 0.3f * std::sin(t * juce::MathConstants<float>::twoPi * 5.1f + 1.3f + phase * 2.3f)
                 + 0.2f * std::sin(t * juce::MathConstants<float>::twoPi * 9.7f + 0.6f + phase * 3.1f);
        };

        juce::Path wave;
        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (float) i / (float) (numPoints - 1);
            const float x = waveX0 + t * waveWidth;
            // Envelope suave (sobe e desce nas pontas) pra a curva
            // nascer/morrer discretamente dentro do vidro, sem cortar
            // seca nas bordas.
            const float envelope = std::sin(t * juce::MathConstants<float>::pi);
            const float y = waveY + sampleWave(t) * amp * envelope;
            if (i == 0) wave.startNewSubPath(x, y);
            else        wave.lineTo(x, y);
        }

        g.setColour(LNF::kDisplayAccent.withAlpha(0.14f + 0.10f * activity));
        g.strokePath(wave, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Espelho vertical, mais fraco ainda -- sugere "eco estéreo"
        // (duas passagens levemente diferentes), sem parecer um
        // segundo canal de dado real.
        juce::Path waveMirror;
        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (float) i / (float) (numPoints - 1);
            const float x = waveX0 + t * waveWidth;
            const float envelope = std::sin(t * juce::MathConstants<float>::pi);
            const float y = waveY - sampleWave(t) * amp * 0.66f * envelope;
            if (i == 0) waveMirror.startNewSubPath(x, y);
            else        waveMirror.lineTo(x, y);
        }
        g.setColour(LNF::kDisplayAccent.withAlpha(0.08f));
        g.strokePath(waveMirror, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A linha neon divisória, a segunda linha, a borda do vidro e o
    // reflexo já foram desenhados no cache estático (ver
    // renderStaticLayerToCache()) -- só recalculamos lineY aqui (não
    // desenhamos de novo) porque o ícone de "eco" dinâmico abaixo
    // precisa da mesma posição de referência.
    const float lineY = glass.getY() + glass.getHeight() * 0.56f;

    // Pequeno elemento decorativo circular ("eco") -- reposicionado pra
    // junto da linha neon superior (não mais no canto inferior direito,
    // onde disputava espaço com o valor "MODE" da fileira de status --
    // problema identificado na FASE 7.2, corrigido na FASE 7.2B).
    auto decoArea = juce::Rectangle<float>(26.0f, 26.0f)
                         .withCentre({ glass.getRight() - 22.0f, lineY });
    const auto centre = decoArea.getCentre();
    // Anéis do "eco" respiram levemente em fase com o resto do display
    // -- só quando o delay está de fato ativo (activity), sugerindo
    // repetições se propagando, sem virar um medidor de nível.
    const float ringPulse = 1.0f + 0.10f * activity * breathe;
    g.setColour(LNF::kDisplayAccent.withAlpha(0.28f));
    for (int i = 0; i < 3; ++i)
    {
        const float r = (3.5f + (float) i * 3.6f) * ringPulse;
        g.drawEllipse(juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(centre), 1.0f);
    }
    g.setColour(LNF::kDisplayAccent);
    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(centre));
}

void NFWhiteDelayAudioProcessorEditor::SaveIconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    const auto asset = juce::ImageCache::getFromMemory(BinaryData::icon_save_png, BinaryData::icon_save_pngSize);
    if (! asset.isValid())
        return;

    auto draw = isButtonDown ? bounds.reduced(1.0f).translated(0.0f, 0.5f) : bounds;
    g.setOpacity(isMouseOverButton && ! isButtonDown ? 1.0f : (isButtonDown ? 0.90f : 0.96f));
    g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
}

void NFWhiteDelayAudioProcessorEditor::MenuIconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    const auto asset = juce::ImageCache::getFromMemory(BinaryData::icon_menu_png, BinaryData::icon_menu_pngSize);
    if (! asset.isValid())
        return;

    auto draw = isButtonDown ? bounds.reduced(1.0f).translated(0.0f, 0.5f) : bounds;
    g.setOpacity(isMouseOverButton && ! isButtonDown ? 1.0f : (isButtonDown ? 0.90f : 0.96f));
    g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
}

void NFWhiteDelayAudioProcessorEditor::BypassButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    // Reference: glowing cyan = processing (bypass OFF). Dull = bypassed.
    auto bounds = getLocalBounds().toFloat();
    const bool bypassed = getToggleState();
    const auto* data = bypassed ? BinaryData::bypass_off_png : BinaryData::bypass_on_png;
    const int dataSize = bypassed ? BinaryData::bypass_off_pngSize : BinaryData::bypass_on_pngSize;
    const auto asset = juce::ImageCache::getFromMemory(data, dataSize);
    if (! asset.isValid())
        return;

    auto draw = isButtonDown ? bounds.reduced(0.6f).translated(0.0f, 0.6f) : bounds;
    if (isMouseOverButton && ! isButtonDown)
        draw = draw.expanded(0.5f);
    g.setOpacity(1.0f);
    g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
}

NFWhiteDelayAudioProcessorEditor::NFWhiteDelayAudioProcessorEditor(NFWhiteDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&nfLookAndFeel);

    addAndMakeVisible(content);
    content.setBounds(0, 0, defaultWidth, defaultHeight);

    // ---- Header ----
    nfLogoLabel.addMouseListener(this, false); // double-click -> reset UI size
    content.addAndMakeVisible(nfLogoLabel);

    headerDivider.colour = LNF::kKnobOutline;
    content.addAndMakeVisible(headerDivider);

    // "AUDIO TOOLS" com tracking largo (espaço entre letras) + peso
    // maior + contraste mais forte -- identidade "NF Audio Tools" lida
    // como uma unidade só, não uma legenda apagada (FASE 7.3, item 1).
    audioToolsLabel.setText(juce::String::fromUTF8("A U D I O   T O O L S"), juce::dontSendNotification);
    audioToolsLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    audioToolsLabel.setColour(juce::Label::textColourId, LNF::kTextMuted.darker(0.55f));
    audioToolsLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(audioToolsLabel);

    whiteDelayLabel.setText("WHITE DELAY", juce::dontSendNotification);
    whiteDelayLabel.setFont(juce::FontOptions(23.0f, juce::Font::bold));
    whiteDelayLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(whiteDelayLabel);

    professionalDelayLabel.setText("Professional Delay", juce::dontSendNotification);
    professionalDelayLabel.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    professionalDelayLabel.setColour(juce::Label::textColourId, LNF::kTextMuted);
    professionalDelayLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(professionalDelayLabel);

    saveButton.onClick = [this] { showPresetPlaceholder(); };
    content.addAndMakeVisible(saveButton);

    hamburgerButton.onClick = [this] { showHamburgerMenu(); };
    content.addAndMakeVisible(hamburgerButton);

    bypassButton.setClickingTogglesState(true);
    content.addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, ParamIDs::bypass, bypassButton);

    // ---- Display central ----
    content.addAndMakeVisible(displayPanel);

    displayLine1.setJustificationType(juce::Justification::centred);
    displayLine1.setFont(juce::FontOptions(58.0f, juce::Font::bold));
    displayLine1.setColour(juce::Label::textColourId, LNF::kDisplayText);
    displayPanel.addAndMakeVisible(displayLine1);

    displayLine2.setJustificationType(juce::Justification::centred);
    displayLine2.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    displayLine2.setColour(juce::Label::textColourId, LNF::kDisplayAccent);
    displayPanel.addAndMakeVisible(displayLine2);

    displayLine3.setJustificationType(juce::Justification::centred);
    displayLine3.setFont(juce::FontOptions(13.0f));
    displayLine3.setColour(juce::Label::textColourId, LNF::kDisplayAccent.withAlpha(0.85f));
    displayPanel.addAndMakeVisible(displayLine3);

    static const char* const statusTitles[4] { "DELAY", "PING PONG", "LO-FI", "MODE" };
    for (int i = 0; i < 4; ++i)
    {
        displayStatusTitle[i].setText(statusTitles[i], juce::dontSendNotification);
        displayStatusTitle[i].setJustificationType(juce::Justification::centred);
        displayStatusTitle[i].setFont(juce::FontOptions(9.5f));
        displayStatusTitle[i].setColour(juce::Label::textColourId, LNF::kDisplayText.withAlpha(0.45f));
        displayPanel.addAndMakeVisible(displayStatusTitle[i]);

        displayStatusValue[i].setJustificationType(juce::Justification::centred);
        displayStatusValue[i].setFont(juce::FontOptions(12.5f, juce::Font::bold));
        displayStatusValue[i].setColour(juce::Label::textColourId, LNF::kDisplayAccent);
        displayPanel.addAndMakeVisible(displayStatusValue[i]);
    }

    // ---- Centro principal: os 4 controles protagonistas ----
    setupRotary(timeControl, ParamIDs::delayTimeMs, "TIME", formatMs);
    setupRotary(feedbackControl, ParamIDs::feedback, "FEEDBACK", formatPercent);
    setupRotary(dryWetControl, ParamIDs::dryWet, "DRY / WET", formatPercent);
    setupRotary(outputControl, ParamIDs::outputGain, "OUTPUT", formatDbSigned);

    // ---- Controles rápidos ----
    pingPongLoFiGroup.title = {};
    content.addAndMakeVisible(pingPongLoFiGroup);
    modeGroup.title = {};
    content.addAndMakeVisible(modeGroup);

    setupToggle(syncControl, ParamIDs::syncEnabled, "SYNC");
    syncControl.button.setLookAndFeel(&premiumLookAndFeel);
    setupChoice(divisionControl, ParamIDs::syncDivision, "DIVISION",
                juce::StringArray { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" });
    setupChoice(modifierControl, ParamIDs::syncModifier, "MODIFIER",
                juce::StringArray { "Straight", "Dotted", "Triplet" });
    setupToggle(pingPongControl, ParamIDs::pingPong, "PING PONG");
    setupToggle(loFiControl, ParamIDs::loFiEnabled, "LO-FI");
    setupSegmented(modeControl, ParamIDs::delayMode, juce::StringArray { "DIGITAL", "ANALOG", "TAPE" });

    // ---- Avançado: Modulation ----
    modulationPanel.title = "MODULATION";
    content.addAndMakeVisible(modulationPanel);
    setupRotary(rateControl, ParamIDs::modRate, "RATE", formatRateHz);
    setupRotary(depthControl, ParamIDs::modDepth, "DEPTH", formatPercent);
    setupChoice(shapeControl, ParamIDs::modShape, "SHAPE",
                juce::StringArray { "Sine", "Triangle", "Soft Random" });
    setupRotary(spreadControl, ParamIDs::modSpread, "SPREAD", formatPercent);

    // ---- Avançado: Filters ----
    filtersPanel.title = "FILTERS";
    content.addAndMakeVisible(filtersPanel);
    setupRotary(highPassControl, ParamIDs::highPass, "HIGH PASS", formatFilterHz);
    setupRotary(lowPassControl, ParamIDs::lowPass, "LOW PASS", formatFilterHz);
    setupRotary(resonanceControl, ParamIDs::resonance, "RESO", formatPercent);

    // ---- Avançado: Character ----
    characterPanel.title = "CHARACTER";
    content.addAndMakeVisible(characterPanel);
    setupRotary(characterControl, ParamIDs::characterAmount, "AMOUNT", formatPercent);
    setupRotary(duckingControl, ParamIDs::duckingAmount, "DUCKING", formatPercent);

    layoutContent();

    // Resize com alça no canto inferior direito -- setResizable(true,
    // true) já adiciona o ResizableCornerComponent automaticamente. O
    // layout interno é sempre recalculado em coordenadas FIXAS
    // (defaultWidth x defaultHeight) e só escalado via transform (ver
    // resized()), então o alinhamento nunca fica irregular no resize.
    setResizable(true, true);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double) defaultWidth / (double) defaultHeight);

    // Abre um pouco menor que o canvas de referência (pedido explícito
    // da FASE 7.3) -- mesma proporção 24:13, então isso só aplica um
    // scale levemente < 1.0 em resized(), sem tocar em nenhuma posição
    // interna calculada por layoutContent().
    setSize(openWidth, openHeight);

    updateDisplay();
    // 30Hz (antes 10Hz) -- necessário pra animação sutil do display
    // (FASE 7.3, item 5) ficar fluida; o resto do trabalho do timer
    // (updateDisplay/controlRefreshers) é leitura simples de floats
    // atômicos + formatação de texto, custo desprezível mesmo a 30Hz.
    startTimerHz(30);
}

NFWhiteDelayAudioProcessorEditor::~NFWhiteDelayAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void NFWhiteDelayAudioProcessorEditor::setupRotary(RotaryControl& c, const juce::String& paramID,
                                                    const juce::String& title, ValueFormatter formatValue)
{
    // FASE 2 -- every rotary uses official BinaryData knobs
    // (knob_large / knob_small chosen by diameter inside the L&F).
    c.slider.setLookAndFeel(&premiumLookAndFeel);
    c.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    c.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    c.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);

    c.titleLabel.setText(title, juce::dontSendNotification);
    c.titleLabel.setJustificationType(juce::Justification::centred);
    c.titleLabel.setFont(juce::FontOptions(12.0f));

    c.valueLabel.setLookAndFeel(&nfLookAndFeel);
    c.valueLabel.setJustificationType(juce::Justification::centred);
    c.valueLabel.setFont(juce::FontOptions(12.0f));
    c.valueLabel.setColour(juce::Label::textColourId, LNF::kText);
    c.valueLabel.setInterceptsMouseClicks(false, false);

    content.addAndMakeVisible(c.slider);
    content.addAndMakeVisible(c.titleLabel);
    content.addAndMakeVisible(c.valueLabel);

    c.attachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, c.slider);

    juce::Label* valueLabelPtr = &c.valueLabel;
    juce::Slider* sliderPtr = &c.slider;
    auto refreshValueLabel = [valueLabelPtr, sliderPtr, formatValue]
    {
        if (formatValue)
            valueLabelPtr->setText(formatValue(sliderPtr->getValue()), juce::dontSendNotification);
    };
    c.slider.onValueChange = refreshValueLabel;
    controlRefreshers.push_back(refreshValueLabel);
    refreshValueLabel();
}

void NFWhiteDelayAudioProcessorEditor::setupToggle(ToggleControl& c, const juce::String& paramID,
                                                    const juce::String& text)
{
    c.button.setLookAndFeel(&nfLookAndFeel);
    c.button.setButtonText(text);
    c.button.setClickingTogglesState(true);
    content.addAndMakeVisible(c.button);

    c.attachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, paramID, c.button);
}

void NFWhiteDelayAudioProcessorEditor::setupChoice(ChoiceControl& c, const juce::String& paramID,
                                                    const juce::String& title, const juce::StringArray& choices)
{
    c.box.setLookAndFeel(&nfLookAndFeel);

    c.titleLabel.setText(title, juce::dontSendNotification);
    c.titleLabel.setJustificationType(juce::Justification::centred);
    c.titleLabel.setFont(juce::FontOptions(10.0f));

    c.box.addItemList(choices, 1);

    content.addAndMakeVisible(c.titleLabel);
    content.addAndMakeVisible(c.box);

    c.attachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, paramID, c.box);
}

void NFWhiteDelayAudioProcessorEditor::setupSegmented(SegmentedControl& c, const juce::String& paramID,
                                                       const juce::StringArray& choices)
{
    c.parameter = audioProcessor.apvts.getParameter(paramID);
    jassert(c.parameter != nullptr);

    const int numChoices = choices.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto* btn = c.buttons.add(new juce::TextButton());
        btn->setLookAndFeel(&nfLookAndFeel);
        btn->setButtonText(choices[i]);
        btn->setClickingTogglesState(false); // estado sincronizado pelo refresher, não pelo clique em si

        auto* param = c.parameter;
        const float normalized = numChoices > 1 ? (float) i / (float) (numChoices - 1) : 0.0f;
        btn->onClick = [param, normalized]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalized);
            param->endChangeGesture();
        };
        content.addAndMakeVisible(btn);
    }

    SegmentedControl* cPtr = &c;
    auto refresh = [cPtr]
    {
        if (cPtr->parameter == nullptr || cPtr->buttons.isEmpty())
            return;
        const int numB = cPtr->buttons.size();
        const int currentIndex = juce::jlimit(0, numB - 1,
            juce::roundToInt(cPtr->parameter->getValue() * (float) (numB - 1)));
        for (int i = 0; i < numB; ++i)
            cPtr->buttons.getUnchecked(i)->setToggleState(i == currentIndex, juce::dontSendNotification);
    };
    controlRefreshers.push_back(refresh);
    refresh();
}

void NFWhiteDelayAudioProcessorEditor::positionRotary(RotaryControl& c, juce::Rectangle<int> area)
{
    auto titleArea = area.removeFromTop(16);
    c.titleLabel.setBounds(titleArea);
    auto valueArea = area.removeFromBottom(20);
    c.slider.setBounds(area);
    c.valueLabel.setBounds(valueArea.withSizeKeepingCentre(juce::jmin(valueArea.getWidth(), 74), 18));
}

void NFWhiteDelayAudioProcessorEditor::positionToggle(ToggleControl& c, juce::Rectangle<int> area)
{
    c.button.setBounds(area);
}

void NFWhiteDelayAudioProcessorEditor::positionChoice(ChoiceControl& c, juce::Rectangle<int> area)
{
    constexpr int titleHeight = 14;
    constexpr int boxHeight = 30;
    auto block = area.withSizeKeepingCentre(area.getWidth(), titleHeight + boxHeight);
    c.titleLabel.setBounds(block.removeFromTop(titleHeight));
    c.box.setBounds(block);
}

void NFWhiteDelayAudioProcessorEditor::positionSegmented(SegmentedControl& c, juce::Rectangle<int> area)
{
    const int n = c.buttons.size();
    if (n == 0)
        return;

    constexpr int gap = 4;
    const int totalGap = gap * (n - 1);
    const int w = (area.getWidth() - totalGap) / n;

    for (int i = 0; i < n; ++i)
    {
        c.buttons.getUnchecked(i)->setBounds(area.removeFromLeft(w));
        if (i < n - 1)
            area.removeFromLeft(gap);
    }
}

void NFWhiteDelayAudioProcessorEditor::layoutContent()
{
    auto bounds = content.getLocalBounds(); // sempre defaultWidth x defaultHeight

    // ---- Header ----
    auto header = bounds.removeFromTop(74).reduced(20, 10);

    auto logoArea = header.removeFromLeft(50);
    nfLogoLabel.setBounds(logoArea);
    header.removeFromLeft(4);
    headerDivider.setBounds(header.removeFromLeft(1).reduced(0, 4));
    header.removeFromLeft(14);

    auto textArea = header.removeFromLeft(240);
    audioToolsLabel.setBounds(textArea.removeFromTop(16));
    whiteDelayLabel.setBounds(textArea.removeFromTop(24));
    professionalDelayLabel.setBounds(textArea);

    auto bypassArea = header.removeFromRight(120);
    bypassButton.setBounds(bypassArea.withSizeKeepingCentre(112, 36));

    header.removeFromRight(10);
    auto hamburgerArea = header.removeFromRight(38);
    hamburgerButton.setBounds(hamburgerArea.withSizeKeepingCentre(32, 32));

    header.removeFromRight(6);
    auto saveArea = header.removeFromRight(38);
    saveButton.setBounds(saveArea.withSizeKeepingCentre(32, 32));

    // ---- Avançado (rodapé): Modulation | Filters | Character ----
    auto bottomRow = bounds.removeFromBottom(258).reduced(20, 8);

    auto modulationColumn = bottomRow.removeFromLeft(380);
    bottomRow.removeFromLeft(16);
    auto filtersColumn = bottomRow.removeFromLeft(360);
    bottomRow.removeFromLeft(16);
    auto characterColumn = bottomRow;

    modulationPanel.setBounds(modulationColumn);
    filtersPanel.setBounds(filtersColumn);
    characterPanel.setBounds(characterColumn);

    modulationColumn.removeFromTop(26);
    filtersColumn.removeFromTop(26);
    characterColumn.removeFromTop(26);

    constexpr int bottomKnobAreaHeight = 168;
    auto modulationKnobs = modulationColumn.removeFromTop(bottomKnobAreaHeight);
    auto filtersKnobs = filtersColumn.removeFromTop(bottomKnobAreaHeight);
    auto characterKnobs = characterColumn.removeFromTop(bottomKnobAreaHeight);

    {
        const int colWidth = modulationKnobs.getWidth() / 4;
        positionRotary(rateControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0));
        positionRotary(depthControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0));
        positionChoice(shapeControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0));
        positionRotary(spreadControl, modulationKnobs.reduced(8, 0));
    }
    {
        const int colWidth = filtersKnobs.getWidth() / 3;
        positionRotary(highPassControl, filtersKnobs.removeFromLeft(colWidth).reduced(10, 0));
        positionRotary(lowPassControl, filtersKnobs.removeFromLeft(colWidth).reduced(10, 0));
        positionRotary(resonanceControl, filtersKnobs.reduced(10, 0));
    }
    {
        const int colWidth = characterKnobs.getWidth() / 2;
        positionRotary(characterControl, characterKnobs.removeFromLeft(colWidth).reduced(14, 0));
        positionRotary(duckingControl, characterKnobs.reduced(14, 0));
    }

    // ---- Controles rápidos (linha logo abaixo do centro principal) ----
    auto quickRow = bounds.removeFromBottom(68).reduced(20, 8);

    constexpr int syncW = 100, comboW = 118, pingLofiGroupW = 214, modeGroupW = 222, gapW = 16;
    const int quickTotalWidth = syncW + comboW + comboW + pingLofiGroupW + modeGroupW + gapW * 4;
    auto quickBlock = quickRow.withSizeKeepingCentre(quickTotalWidth, quickRow.getHeight());

    positionToggle(syncControl, quickBlock.removeFromLeft(syncW));
    quickBlock.removeFromLeft(gapW);
    positionChoice(divisionControl, quickBlock.removeFromLeft(comboW));
    quickBlock.removeFromLeft(gapW);
    positionChoice(modifierControl, quickBlock.removeFromLeft(comboW));
    quickBlock.removeFromLeft(gapW);

    auto pingLofiArea = quickBlock.removeFromLeft(pingLofiGroupW);
    pingPongLoFiGroup.setBounds(pingLofiArea);
    auto pingLofiInner = pingLofiArea.reduced(8, 8);
    const int halfW = (pingLofiInner.getWidth() - 4) / 2;
    positionToggle(pingPongControl, pingLofiInner.removeFromLeft(halfW));
    pingLofiInner.removeFromLeft(4);
    positionToggle(loFiControl, pingLofiInner);

    quickBlock.removeFromLeft(gapW);
    auto modeArea = quickBlock.removeFromLeft(modeGroupW);
    modeGroup.setBounds(modeArea);
    positionSegmented(modeControl, modeArea.reduced(8, 8));

    // ---- Centro principal: TIME | DISPLAY | FEEDBACK | DRY/WET | OUTPUT ----
    auto mainRow = bounds.reduced(20, 8);

    auto timeColumn = mainRow.removeFromLeft(205);
    mainRow.removeFromLeft(14);
    auto outputColumn = mainRow.removeFromRight(145);
    mainRow.removeFromRight(14);
    auto dryWetColumn = mainRow.removeFromRight(175);
    mainRow.removeFromRight(14);
    auto feedbackColumn = mainRow.removeFromRight(205);
    mainRow.removeFromRight(14);
    auto displayColumn = mainRow; // resto -- o maior bloco, centro visual

    positionRotary(timeControl, timeColumn);
    positionRotary(feedbackControl, feedbackColumn);
    positionRotary(dryWetControl, dryWetColumn);
    positionRotary(outputControl, outputColumn);

    {
        displayPanel.setBounds(displayColumn);

        auto displayLocal = displayPanel.getLocalBounds().reduced(20, 14);
        displayLine1.setBounds(displayLocal.removeFromTop((int) (displayLocal.getHeight() * 0.40f)));
        displayLine2.setBounds(displayLocal.removeFromTop((int) (displayLocal.getHeight() * 0.30f)));
        displayLine3.setBounds(displayLocal.removeFromTop((int) (displayLocal.getHeight() * 0.34f)));

        displayLocal.removeFromTop(8); // respiro sob a segunda linha decorativa

        const int colW = displayLocal.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto col = displayLocal.removeFromLeft(colW);
            displayStatusTitle[i].setBounds(col.removeFromTop(14));
            displayStatusValue[i].setBounds(col);
        }
    }
}

void NFWhiteDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Chassis metálico -- gradiente vertical sutil (off-white/prata
    // muito claro, ligeiramente mais escuro na base) em vez de uma cor
    // chapada, dando volume físico ao painel como um todo (FASE 7.2B).
    // Preenchimento SEMPRE retangular e opaco (cobre 100% da janela) --
    // cantos "arredondados" do chassis são sugeridos por uma moldura
    // decorativa por cima, não por um recorte real da janela (evita
    // deixar pixels de canto sem pintura).
    juce::ColourGradient chassisGradient(LNF::kBackground.brighter(0.02f), bounds.getCentreX(), bounds.getY(),
                                          LNF::kBackground.darker(0.025f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(chassisGradient);
    g.fillRect(bounds);

    // Moldura decorativa arredondada, ligeiramente encolhida da borda
    // real da janela -- sugere um chassis com cantos chanfrados sem
    // recortar a janela de verdade.
    constexpr float inset = 2.5f;
    constexpr float chassisCorner = 14.0f;
    auto chassisRect = bounds.reduced(inset);

    g.setColour(juce::Colour(0xffaeb0b8).withAlpha(0.75f));
    g.drawRoundedRectangle(chassisRect, chassisCorner, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(chassisRect.reduced(1.4f), chassisCorner - 1.0f, 0.8f);

    // Realce superior (luz batendo no topo do chassis) + sombra
    // inferior (base ligeiramente recuada) -- bevel discreto.
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawLine(bounds.getX() + inset, bounds.getY() + inset, bounds.getRight() - inset, bounds.getY() + inset, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.07f));
    g.drawLine(bounds.getX() + inset, bounds.getBottom() - inset, bounds.getRight() - inset, bounds.getBottom() - inset, 1.0f);

    // Contorno externo fino, na borda exata da janela.
    g.setColour(juce::Colour(0xffb7b9c0).withAlpha(0.7f));
    g.drawRect(bounds, 1.0f);
}

void NFWhiteDelayAudioProcessorEditor::resized()
{
    content.setBounds(0, 0, defaultWidth, defaultHeight);

    const float scale = juce::jmin((float) getWidth() / (float) defaultWidth,
                                    (float) getHeight() / (float) defaultHeight);
    content.setTransform(juce::AffineTransform::scale(scale));
}

void NFWhiteDelayAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.eventComponent == &nfLogoLabel)
        setSize(openWidth, openHeight);
}

void NFWhiteDelayAudioProcessorEditor::timerCallback()
{
    updateDisplay();
    for (auto& refresh : controlRefreshers)
        refresh();
}

void NFWhiteDelayAudioProcessorEditor::updateDisplay()
{
    auto& apvts = audioProcessor.apvts;

    const bool syncOn = apvts.getRawParameterValue(ParamIDs::syncEnabled)->load() > 0.5f;

    // NUNCA chamar audioProcessor.getPlayHead() daqui -- ver
    // PluginProcessor.h, lastKnownHostBpm (nota sobre o crash real
    // corrigido na FASE 6.5).
    const double bpm = audioProcessor.lastKnownHostBpm.load(std::memory_order_relaxed);
    const bool bpmKnown = bpm > 0.0;

    if (syncOn)
    {
        static const char* const divisionNames[] { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" };
        static const char* const modifierNames[] { "STRAIGHT", "DOTTED", "TRIPLET" };

        const int divisionIndex = juce::jlimit(0, 7, (int) apvts.getRawParameterValue(ParamIDs::syncDivision)->load());
        const int modifierIndex = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ParamIDs::syncModifier)->load());

        displayLine1.setText(divisionNames[divisionIndex], juce::dontSendNotification);
        displayLine2.setText(modifierNames[modifierIndex], juce::dontSendNotification);
        displayLine3.setText(bpmKnown ? (juce::String(bpm, 1) + " BPM") : "BPM: HOST N/A",
                              juce::dontSendNotification);
    }
    else
    {
        const float ms = apvts.getRawParameterValue(ParamIDs::delayTimeMs)->load();
        displayLine1.setText(juce::String(juce::roundToInt(ms)) + " ms", juce::dontSendNotification);
        displayLine2.setText("FREE", juce::dontSendNotification);
        displayLine3.setText({}, juce::dontSendNotification);
    }

    // Linha de status -- dados REAIS da APVTS (nada fictício).
    const bool bypassed = apvts.getRawParameterValue(ParamIDs::bypass)->load() > 0.5f;
    displayStatusValue[0].setText(bypassed ? "BYPASSED" : "ACTIVE", juce::dontSendNotification);

    const bool pingPongOn = apvts.getRawParameterValue(ParamIDs::pingPong)->load() > 0.5f;
    displayStatusValue[1].setText(pingPongOn ? "ON" : "OFF", juce::dontSendNotification);

    const bool loFiOn = apvts.getRawParameterValue(ParamIDs::loFiEnabled)->load() > 0.5f;
    displayStatusValue[2].setText(loFiOn ? "ON" : "OFF", juce::dontSendNotification);

    static const char* const modeNames[] { "DIGITAL", "ANALOG", "TAPE" };
    const int modeIndex = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ParamIDs::delayMode)->load());
    displayStatusValue[3].setText(modeNames[modeIndex], juce::dontSendNotification);

    // "Display vivo" (FASE 7.3, refinado na FASE 7.4) -- animação que
    // reage à atividade REAL do wet, não mais a um proxy de parâmetros.
    // O audio thread já calcula um envelope do sinal wet (pico + attack/
    // release, ver PluginProcessor.h/.cpp, wetActivity) e publica num
    // atomic; aqui só LEMOS esse valor, nunca tocamos em nenhum objeto
    // do audio thread. Suavizado de novo do lado da UI (mais lento que
    // o smoothing do audio thread) só pra a transição visual ficar
    // ainda mais macia a 30fps.
    const float wetActivityNow = juce::jlimit(0.0f, 1.0f, audioProcessor.wetActivity.load(std::memory_order_relaxed));
    displayPanel.activity += (wetActivityNow - displayPanel.activity) * 0.12f;

    // A fase avança sempre (nunca para de vez -- "quase parada", não
    // "parada"), mais rápido quanto mais ativo o delay estiver.
    displayPanel.animPhase += 0.006f + 0.018f * displayPanel.activity;
    if (displayPanel.animPhase > 10000.0f)
        displayPanel.animPhase -= 10000.0f; // evita crescer sem limite numa sessão longa

    displayPanel.repaint();
}

void NFWhiteDelayAudioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;
    // NÃO seta um LookAndFeel customizado aqui de propósito (o
    // PopupMenu é assíncrono e pode sobreviver ao editor por um
    // instante -- ver auditoria de crash da FASE 6.5).
    menu.addItem(1, "About");
    menu.addItem(2, "Reset UI Size");

    juce::Component::SafePointer<NFWhiteDelayAudioProcessorEditor> safeThis(this);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(hamburgerButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result == 1)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "NF White Delay",
                    "NF Audio Tools -- White Delay\n"
                    "Professional UI -- Phase 7.3.\n\n"
                    "DSP engine: commit c549a5f + Phase 6.6 refinements.",
                    "OK");
            }
            else if (result == 2)
            {
                safeThis->setSize(NFWhiteDelayAudioProcessorEditor::openWidth,
                                   NFWhiteDelayAudioProcessorEditor::openHeight);
            }
        });
}

void NFWhiteDelayAudioProcessorEditor::showPresetPlaceholder()
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "Preset Manager", "Preset Manager -- coming in next phase", "OK");
}
