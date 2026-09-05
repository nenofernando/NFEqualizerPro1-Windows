#include "PluginEditor.h"
#include "DSP/HostSync.h"
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
    // Reference crop (TIME): dark pill + cyan value text.
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 5.0f;

    {
        juce::Path chipPath;
        chipPath.addRoundedRectangle(b, corner);
        juce::DropShadow(juce::Colours::black.withAlpha(0.16f), 3, { 0, 1 }).drawForPath(g, chipPath);
    }

    juce::ColourGradient fill(juce::Colour(0xff1a2230), b.getCentreX(), b.getY(),
                               juce::Colour(0xff0b1018), b.getCentreX(), b.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(b, corner);
    g.setColour(LNF::kDisplayAccent.withAlpha(0.28f));
    g.drawRoundedRectangle(b.reduced(0.6f), juce::jmax(0.0f, corner - 0.6f), 0.9f);
    g.setColour(juce::Colour(0xff3a4558).withAlpha(0.95f));
    g.drawRoundedRectangle(b, corner, 1.0f);

    g.setColour(findColour(juce::Label::textColourId));
    g.setFont(getFont());
    g.drawFittedText(getText(), getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
}

void NFWhiteDelayAudioProcessorEditor::ThinLine::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (neonGlow)
    {
        const auto bar = juce::ImageCache::getFromMemory(BinaryData::neon_bar_png, BinaryData::neon_bar_pngSize);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour(LNF::kNeonGlow.withAlpha(0.18f / (float) i));
            g.fillRoundedRectangle(b.expanded(0.0f, (float) i * 2.0f), 2.0f);
        }
        if (bar.isValid())
        {
            g.setOpacity(1.0f);
            g.drawImage(bar, b, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            g.setColour(colour.brighter(0.35f));
            g.fillRoundedRectangle(b.withSizeKeepingCentre(b.getWidth(), juce::jmax(1.5f, b.getHeight() * 0.45f)), 1.0f);
        }
        return;
    }

    g.setColour(colour);
    g.fillRect(getLocalBounds());
}

void NFWhiteDelayAudioProcessorEditor::NfLogoBadge::paint(juce::Graphics& g)
{
    // Official metal NF badge — full opacity, crisp on the light chassis.
    auto bounds = getLocalBounds().toFloat();
    const auto badge = juce::ImageCache::getFromMemory(BinaryData::logo_nf_png, BinaryData::logo_nf_pngSize);
    if (badge.isValid())
    {
        {
            juce::Path shadowPath;
            shadowPath.addRoundedRectangle(bounds.reduced(1.5f), bounds.getWidth() * 0.22f);
            juce::DropShadow(juce::Colours::black.withAlpha(0.22f), 5, { 0, 1 }).drawForPath(g, shadowPath);
        }
        g.setOpacity(1.0f);
        g.drawImage(badge, bounds.reduced(0.5f),
                     juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);

        // Contorno reforçado -- traço fino escuro + filete claro por
        // dentro, mesma linguagem de moldura do resto do gabinete (pedido:
        // "melhorando o contorno" do selo NF).
        const float corner = bounds.getWidth() * 0.22f;
        g.setColour(juce::Colour(0xff2a2c31).withAlpha(0.55f));
        g.drawRoundedRectangle(bounds.reduced(0.6f), corner, 1.1f);
        g.setColour(juce::Colours::white.withAlpha(0.38f));
        g.drawRoundedRectangle(bounds.reduced(1.8f), corner - 1.0f, 0.8f);
        return;
    }

    g.setColour(juce::Colour(0xff121318));
    g.setFont(juce::FontOptions(juce::jmin(bounds.getHeight() * 0.72f, 34.0f), juce::Font::bold));
    g.drawFittedText("NF", bounds.toNearestInt(), juce::Justification::centred, 1);
}

namespace
{
    // Machined chassis bay — thick rounded lip + recessed floor (fit modules into).
    void paintChassisBayFrame(juce::Graphics& g, juce::Rectangle<float> bay, float corner)
    {
        const float lip = juce::jmin(8.5f, bay.getWidth() * 0.040f, bay.getHeight() * 0.09f);
        auto floor = bay.reduced(lip);
        const float floorCorner = juce::jmax(4.0f, corner - lip * 0.65f);

        // Raised lip body + recessed floor -- pedido: mesma cor do chassi,
        // chapada, sem gradiente/sombra; só o traço da moldura (abaixo)
        // marca o encaixe.
        g.setColour(LNF::kBackground);
        g.fillRoundedRectangle(bay, corner);
        g.fillRoundedRectangle(floor, floorCorner);

        // Lip outer edge (Etapa 2 -- traço fino e escuro, mesma família
        // visual do chassi/display/painéis, no lugar do bisel grosso).
        g.setColour(juce::Colour(0xff2a2c31).withAlpha(0.65f));
        g.drawRoundedRectangle(bay.reduced(0.35f), corner, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawLine(bay.getX() + corner * 0.55f, bay.getY() + 1.0f,
                   bay.getRight() - corner * 0.55f, bay.getY() + 1.0f, 1.0f);

        // Inner lip edge (where the module seats).
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.drawRoundedRectangle(floor.expanded(0.3f), floorCorner + 0.3f, 1.15f);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawRoundedRectangle(floor.reduced(0.8f), juce::jmax(2.0f, floorCorner - 0.8f), 0.85f);
    }
}

void NFWhiteDelayAudioProcessorEditor::ContentRoot::paint(juce::Graphics& g)
{
    for (const auto& bay : chassisBays)
        paintChassisBayFrame(g, bay, bay.getHeight() < 90.0f ? 11.0f : 14.0f);
}

void NFWhiteDelayAudioProcessorEditor::SectionPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const bool isButtonWell = title.isEmpty();
    const float corner = isButtonWell ? 9.0f : 10.0f;

    // Insert plate seated in the chassis bay — pedido: "escudo" com a
    // MESMA cor do chassi, chapada, sem gradiente -- só a moldura abaixo
    // marca onde a peça se encaixa.
    g.setColour(LNF::kBackground);
    g.fillRoundedRectangle(bounds, corner);

    // Etapa 2 -- moldura fina e elegante (mesma família visual do chassi/
    // display), no lugar do bisel cromado grosso de 4 camadas.
    g.setColour(juce::Colour(0xff2a2c31).withAlpha(0.80f));
    g.drawRoundedRectangle(bounds.reduced(0.35f), corner, 1.3f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds.reduced(1.7f), juce::jmax(0.0f, corner - 1.2f), 0.9f);

    if (title.isNotEmpty())
    {
        const float dividerY = bounds.getY() + 22.0f;
        juce::ColourGradient dividerShade(juce::Colours::black.withAlpha(0.05f), bounds.getCentreX(), dividerY,
                                           juce::Colours::transparentBlack, bounds.getCentreX(), dividerY + 8.0f, false);
        g.setGradientFill(dividerShade);
        g.fillRect(juce::Rectangle<float>(bounds.getX() + 10.0f, dividerY, bounds.getWidth() - 20.0f, 8.0f));

        juce::Font font(juce::FontOptions(11.0f, juce::Font::bold));
        g.setFont(font);
        const float textWidth = juce::GlyphArrangement::getStringWidth(font, title);
        const float pad = 8.0f;
        const float centreX = bounds.getCentreX();
        const float titleY = bounds.getY() + 5.0f;
        const float titleH = 16.0f;

        g.setColour(LNF::kTextMuted.darker(0.40f));
        g.drawText(title, juce::Rectangle<float>(centreX - textWidth * 0.5f - pad, titleY,
                                                   textWidth + pad * 2.0f, titleH),
                   juce::Justification::centred);

        g.setColour(LNF::kAccent.withAlpha(0.45f));
        g.fillEllipse(juce::Rectangle<float>(2.5f, 2.5f)
                           .withCentre({ centreX - textWidth * 0.5f - pad - 5.0f, titleY + titleH * 0.5f }));

        const float lineY = titleY + titleH * 0.5f;
        const float lineMarginFromEdge = 14.0f;
        const float gapFromText = textWidth * 0.5f + pad + 10.0f;
        g.setColour(LNF::kKnobOutline.withAlpha(0.55f));
        g.drawLine(bounds.getX() + lineMarginFromEdge, lineY, centreX - gapFromText, lineY, 0.85f);
        g.drawLine(centreX + gapFromText, lineY, bounds.getRight() - lineMarginFromEdge, lineY, 0.85f);
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
    constexpr float corner = 15.0f;

    // Etapa 1 -- moldura fina e elegante (bate com o novo frame do chassi),
    // no lugar do bisel cromado grosso anterior.
    {
        juce::ColourGradient bezel(juce::Colour(0xfff0f1f4), bounds.getCentreX(), bounds.getY(),
                                   juce::Colour(0xff8a8c94), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(bezel);
        g.fillRoundedRectangle(bounds, corner);
    }
    g.setColour(juce::Colour(0xff2a2c31).withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner - 0.3f, 1.4f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds.reduced(1.8f), corner - 1.4f, 1.0f);

    // Glass inset behind a slim chrome lip.
    auto glass = bounds.reduced(6.5f);
    const float glassCorner = corner - 4.0f;
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

    // Optional frame asset (glass only) — chrome rim is re-asserted below.
    {
        const auto frame = juce::ImageCache::getFromMemory (BinaryData::display_frame_png,
                                                            BinaryData::display_frame_pngSize);
        if (frame.isValid())
            g.drawImage (frame, glass, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
    }

    // Re-assert a moldura fina por cima (Etapa 1 -- traço único e nítido,
    // no lugar do bisel cromado grosso de 4 camadas).
    g.setColour(juce::Colour(0xff2a2c31).withAlpha(0.90f));
    g.drawRoundedRectangle(bounds.reduced(0.4f), corner, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), corner - 1.4f, 1.0f);

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
    constexpr float corner = 15.0f;
    auto glass = bounds.reduced(6.5f);
    const float glassCorner = corner - 4.0f;

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

    // Stereo Delay Activity Visualizer — real wet L/R history (FIFO).
    // Drawn behind child text labels; side meters stay independent.
    paintActivityVisualizer(g, glass);

    // Side L/R meters — embedded in the glass, outside the central text.
    paintSideMeter(g, glass, 0.08f, meterL, "L");
    paintSideMeter(g, glass, 0.92f, meterR, "R");
}

void NFWhiteDelayAudioProcessorEditor::DisplayPanel::consumeWetActivity(NFWhiteDelayAudioProcessor& processor)
{
    float tmpL[48];
    float tmpR[48];
    const int n = processor.pullWetActivitySamples(tmpL, tmpR, 48);

    for (int i = 0; i < n; ++i)
    {
        // Soft curve: quiet wet repeats stay readable; peaks stay controlled.
        const float inL = juce::jlimit(0.0f, 1.4f, std::sqrt(juce::jmax(0.0f, tmpL[i])) * 1.55f);
        const float inR = juce::jlimit(0.0f, 1.4f, std::sqrt(juce::jmax(0.0f, tmpR[i])) * 1.55f);
        vizHistL[vizWrite] = juce::jmax(vizHistL[vizWrite] * 0.32f, inL);
        vizHistR[vizWrite] = juce::jmax(vizHistR[vizWrite] * 0.32f, inR);
        vizWrite = (vizWrite + 1) % kVizHistory;
        vizFilled = juce::jmin(kVizHistory, vizFilled + 1);
    }

    // Temporal fade — older echoes linger then die (feedback feel).
    for (int i = 0; i < kVizHistory; ++i)
    {
        vizHistL[i] *= 0.982f;
        vizHistR[i] *= 0.982f;
    }

    // Side L/R meters track the same wet L/R tap as the visualizer (UI-only).
    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i < vizFilled; ++i)
    {
        peakL = juce::jmax(peakL, vizHistL[i]);
        peakR = juce::jmax(peakR, vizHistR[i]);
    }
    const float targetL = juce::jlimit(0.0f, 1.0f, peakL * 0.95f);
    const float targetR = juce::jlimit(0.0f, 1.0f, peakR * 0.95f);
    meterL += (targetL - meterL) * 0.42f;
    meterR += (targetR - meterR) * 0.42f;
}

void NFWhiteDelayAudioProcessorEditor::DisplayPanel::paintActivityVisualizer(juce::Graphics& g,
                                                                              juce::Rectangle<float> glass) const
{
    if (vizFilled < 2)
        return;

    const float gw = glass.getWidth();
    const float gh = glass.getHeight();
    const float zeroY = glass.getY() + gh * 0.32f;
    const float amp = gh * 0.16f;

    auto paintChannel = [&](float x0Norm, float x1Norm, const float* hist)
    {
        const float x0 = glass.getX() + gw * x0Norm;
        const float x1 = glass.getX() + gw * x1Norm;

        g.setColour(LNF::kDisplayAccent.withAlpha(0.18f));
        g.drawLine(x0, zeroY, x1, zeroY, 0.85f);

        juce::Path upper, lower;
        bool started = false;
        for (int i = 0; i < vizFilled; ++i)
        {
            const int idx = (vizWrite - vizFilled + i + kVizHistory) % kVizHistory;
            const float t = (float) i / (float) (vizFilled - 1); // oldest→newest L→R
            const float fade = 0.35f + 0.65f * t;
            const float x = x0 + t * (x1 - x0);
            const float v = hist[idx] * fade;
            const float yUp = zeroY - v * amp;
            const float yDn = zeroY + v * amp * 0.45f;

            if (! started)
            {
                upper.startNewSubPath(x, yUp);
                lower.startNewSubPath(x, yDn);
                started = true;
            }
            else
            {
                upper.lineTo(x, yUp);
                lower.lineTo(x, yDn);
            }
        }

        g.setColour(LNF::kNeonGlow.withAlpha(0.22f + 0.14f * activity));
        g.strokePath(upper, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(LNF::kDisplayAccent.withAlpha(0.42f + 0.28f * activity));
        g.strokePath(upper, juce::PathStrokeType(1.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(LNF::kKnobValueArcCore.withAlpha(0.55f));
        g.strokePath(upper, juce::PathStrokeType(0.70f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(LNF::kDisplayAccent.withAlpha(0.20f + 0.12f * activity));
        g.strokePath(lower, juce::PathStrokeType(1.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    // Keep clear of side meters (~8% / ~92%) and central text (38–62%).
    paintChannel(0.10f, 0.38f, vizHistL);
    paintChannel(0.62f, 0.90f, vizHistR);
}

void NFWhiteDelayAudioProcessorEditor::DisplayPanel::paintSideMeter(juce::Graphics& g,
                                                                     juce::Rectangle<float> glass,
                                                                     float centreXNorm,
                                                                     float level,
                                                                     const char* label) const
{
    const float w = glass.getWidth();
    const float h = glass.getHeight();
    const float centreX = glass.getX() + w * centreXNorm;
    const float topY = glass.getY() + h * 0.25f;
    const float bottomY = glass.getY() + h * 0.72f;
    const float meterH = bottomY - topY;
    const float meterW = juce::jlimit(5.0f, 14.0f, w * 0.036f);
    auto meterBounds = juce::Rectangle<float>(meterW, meterH).withCentre({ centreX, (topY + bottomY) * 0.5f });

    constexpr int numSegments = 16;
    const float gap = juce::jmax(1.0f, meterH * 0.035f);
    const float segH = (meterH - gap * (float) (numSegments - 1)) / (float) numSegments;
    const float lit = juce::jlimit(0.0f, 1.0f, level);
    const int litCount = juce::jlimit(0, numSegments, (int) std::ceil(lit * (float) numSegments));

    // Soft outer glow of the column (controlled, not neon blowout).
    g.setColour(LNF::kNeonGlow.withAlpha(0.16f + 0.14f * lit));
    g.fillRoundedRectangle(meterBounds.expanded(2.6f), 2.0f);

    for (int i = 0; i < numSegments; ++i)
    {
        // Bottom segment = index 0 (fills upward with level).
        const int fromBottom = i;
        const float y = bottomY - (float) (fromBottom + 1) * segH - (float) fromBottom * gap;
        auto seg = juce::Rectangle<float>(meterW, segH).withX(meterBounds.getX()).withY(y);
        const bool on = fromBottom < litCount;

        if (on)
        {
            juce::ColourGradient segGrad(LNF::kKnobValueArcCore.withAlpha(1.0f), seg.getCentreX(), seg.getCentreY(),
                                          LNF::kDisplayAccent.withAlpha(0.90f), seg.getX(), seg.getCentreY(), true);
            g.setGradientFill(segGrad);
            g.fillRoundedRectangle(seg, 1.0f);
            g.setColour(LNF::kNeonGlow.withAlpha(0.35f));
            g.drawRoundedRectangle(seg.expanded(0.7f), 1.2f, 0.9f);
        }
        else
        {
            g.setColour(LNF::kDisplayAccent.withAlpha(0.22f));
            g.fillRoundedRectangle(seg, 1.0f);
            g.setColour(LNF::kDisplayAccent.withAlpha(0.12f));
            g.drawRoundedRectangle(seg, 1.0f, 0.6f);
        }
    }

    g.setColour(LNF::kDisplayAccent.withAlpha(0.92f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    auto labelArea = juce::Rectangle<float>(18.0f, 12.0f).withCentre({ centreX, bottomY + 11.0f });
    g.drawFittedText(label, labelArea.toNearestInt(), juce::Justification::centred, 1);
}

namespace
{
    // Shared header-icon finish — same shadow / press / cyan rim language as BYPASS.
    void paintPremiumHeaderIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 const juce::Image& asset, bool hover, bool down)
    {
        if (! asset.isValid())
            return;

        auto draw = down ? bounds.reduced(0.6f).translated(0.0f, 0.6f) : bounds;
        if (hover && ! down)
            draw = draw.expanded(0.5f);

        const float corner = draw.getWidth() * 0.28f;

        {
            juce::Path shadowPath;
            shadowPath.addRoundedRectangle(draw.reduced(0.5f), corner);
            juce::DropShadow(juce::Colours::black.withAlpha(down ? 0.08f : 0.14f),
                             down ? 2 : 3, { 0, down ? 0 : 1 }).drawForPath(g, shadowPath);
        }

        // Soft cyan rim — ties SAVE/MENU to the BYPASS neon edge without competing.
        if (! down)
        {
            g.setColour(LNF::kNeonGlow.withAlpha(hover ? 0.20f : 0.12f));
            g.drawRoundedRectangle(draw.expanded(0.9f), corner + 0.6f, 1.1f);
            g.setColour(LNF::kDisplayAccent.withAlpha(hover ? 0.16f : 0.09f));
            g.drawRoundedRectangle(draw.expanded(0.15f), corner, 0.8f);
        }

        g.setOpacity(1.0f);
        g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);

        // Tiny top specular so icons sit in the same metal plane as BYPASS.
        auto top = draw.reduced(2.2f).withHeight(draw.getHeight() * 0.36f);
        juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.09f), top.getCentreX(), top.getY(),
                                   juce::Colours::transparentWhite, top.getCentreX(), top.getBottom(), false);
        g.setGradientFill(gloss);
        g.fillRoundedRectangle(top, juce::jmax(1.0f, corner - 1.2f));
    }
}

void NFWhiteDelayAudioProcessorEditor::SaveIconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto asset = juce::ImageCache::getFromMemory(BinaryData::icon_save_png, BinaryData::icon_save_pngSize);
    paintPremiumHeaderIcon(g, getLocalBounds().toFloat(), asset, isMouseOverButton, isButtonDown);
}

void NFWhiteDelayAudioProcessorEditor::MenuIconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto asset = juce::ImageCache::getFromMemory(BinaryData::icon_menu_png, BinaryData::icon_menu_pngSize);
    paintPremiumHeaderIcon(g, getLocalBounds().toFloat(), asset, isMouseOverButton, isButtonDown);
}

void NFWhiteDelayAudioProcessorEditor::BypassButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    // Official assets: bypass_on (neon+glyph) when processing; silver shell when bypassed.
    // Margem interna pro glow caber dentro dos limites do componente (ver
    // nota em PowerButton::paint -- mesmo motivo, mesma técnica).
    constexpr float glowMargin = 12.0f;
    auto bounds = getLocalBounds().toFloat().reduced(glowMargin);
    const bool bypassed = getToggleState();
    // bypass_off.png tem "PING PONG" gravado por engano (asset errado) --
    // usa o shell prateado em branco (button_off.png, mesmo da família
    // PING PONG/LO-FI/ANALOG/TAPE) e desenha o rótulo "BYPASS" por cima,
    // sem duplicar/sobrepor texto.
    const auto* data = bypassed ? BinaryData::button_off_png : BinaryData::bypass_on_png;
    const int dataSize = bypassed ? BinaryData::button_off_pngSize : BinaryData::bypass_on_pngSize;
    const auto asset = juce::ImageCache::getFromMemory(data, dataSize);
    if (! asset.isValid())
        return;

    auto draw = isButtonDown ? bounds.reduced(0.6f).translated(0.0f, 0.6f) : bounds;
    if (isMouseOverButton && ! isButtonDown)
        draw = draw.expanded(0.5f);

    const float corner = draw.getHeight() * 0.5f;

    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(draw.reduced(1.0f), corner);
        juce::DropShadow(juce::Colours::black.withAlpha(isButtonDown ? 0.08f : 0.16f),
                         isButtonDown ? 2 : 4, { 0, isButtonDown ? 0 : 1 }).drawForPath(g, shadowPath);
    }

    if (! bypassed)
    {
        for (int i = 5; i >= 1; --i)
        {
            const float expand = (float) i * 2.0f;
            g.setColour(LNF::kNeonGlow.withAlpha(0.22f / (float) i));
            g.fillRoundedRectangle(draw.expanded(expand), corner + expand * 0.4f);
        }
    }

    g.setOpacity(1.0f);
    g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);

    // OFF shell is textless — draw muted BYPASS label.
    if (bypassed)
    {
        g.setColour(LNF::kText.withAlpha(0.78f));
        g.setFont(juce::FontOptions(juce::jmin(13.0f, draw.getHeight() * 0.42f), juce::Font::bold));
        g.drawFittedText("BYPASS", draw.toNearestInt(), juce::Justification::centred, 1);
    }
}

void NFWhiteDelayAudioProcessorEditor::PowerButton::paint(juce::Graphics& g)
{
    // Componente maior que o ícone visível (margem = glowMargin) pra o
    // glow caber DENTRO dos próprios limites do componente -- evita tanto
    // o corte quadrado (JUCE recorta pintura na borda do componente) quanto
    // os artefatos de usar setPaintingIsUnclipped (pintar fora dos limites
    // pode deixar pixels de vizinhos sem invalidar corretamente).
    constexpr float glowMargin = 13.0f;
    auto bounds = getLocalBounds().toFloat().reduced(glowMargin);
    const auto asset = juce::ImageCache::getFromMemory(BinaryData::power_on_png, BinaryData::power_on_pngSize);

    auto draw = pressed ? bounds.reduced(0.8f).translated(0.0f, 0.5f) : bounds;
    if (isMouseOver(false) && ! pressed)
        draw = draw.expanded(0.5f);

    {
        juce::Path shadowPath;
        shadowPath.addEllipse(draw.reduced(1.0f));
        juce::DropShadow(juce::Colours::black.withAlpha(pressed ? 0.08f : 0.18f),
                         pressed ? 2 : 4, { 0, pressed ? 0 : 1 }).drawForPath(g, shadowPath);
    }

    if (lit)
    {
        for (int i = 5; i >= 1; --i)
        {
            const float expand = (float) i * 2.2f;
            g.setColour(LNF::kNeonGlow.withAlpha(0.26f / (float) i));
            g.fillEllipse(draw.expanded(expand));
        }
    }

    if (asset.isValid())
    {
        g.setOpacity(lit ? 1.0f : 0.40f);
        g.drawImage(asset, draw, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }
    else
    {
        g.setColour(lit ? LNF::kNeonGlow : LNF::kKnobDark);
        g.fillEllipse(draw.reduced(2.0f));
    }
}

void NFWhiteDelayAudioProcessorEditor::PowerButton::mouseDown(const juce::MouseEvent&)
{
    pressed = true;
    repaint();
}

void NFWhiteDelayAudioProcessorEditor::PowerButton::mouseUp(const juce::MouseEvent& e)
{
    pressed = false;
    repaint();
    if (e.mouseWasClicked() && onClick)
        onClick();
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

    // Neon underline — editor child (not under content AffineTransform).
    headerDivider.colour = LNF::kDisplayAccent;
    headerDivider.neonGlow = true;
    addAndMakeVisible(headerDivider);

    // Header hierarchy (reference): Audio Tools / WHITE DELAY / Professional Delay.
    audioToolsLabel.setText("Audio Tools", juce::dontSendNotification);
    audioToolsLabel.setFont(juce::FontOptions(15.5f, juce::Font::bold));
    audioToolsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3a3c44));
    audioToolsLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(audioToolsLabel);

    whiteDelayLabel.setText("WHITE DELAY", juce::dontSendNotification);
    whiteDelayLabel.setFont(juce::FontOptions(37.0f, juce::Font::bold));
    whiteDelayLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1a1b20));
    whiteDelayLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(whiteDelayLabel);

    professionalDelayLabel.setText("Professional Delay", juce::dontSendNotification);
    professionalDelayLabel.setFont(juce::FontOptions(13.5f));
    professionalDelayLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5a5c64));
    professionalDelayLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(professionalDelayLabel);

    saveButton.onClick = [this] { showPresetPlaceholder(); };
    content.addAndMakeVisible(saveButton);

    hamburgerButton.onClick = [this] { showHamburgerMenu(); };
    content.addAndMakeVisible(hamburgerButton);

    // POWER — editor child (not under content AffineTransform).
    addAndMakeVisible(powerButton);

    bypassButton.setClickingTogglesState(true);
    content.addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, ParamIDs::bypass, bypassButton);
    powerButton.onClick = [this]
    {
        if (auto* bypassParam = audioProcessor.apvts.getParameter(ParamIDs::bypass))
        {
            const bool next = ! (bypassParam->getValue() > 0.5f);
            bypassParam->beginChangeGesture();
            bypassParam->setValueNotifyingHost(next ? 1.0f : 0.0f);
            bypassParam->endChangeGesture();
        }
    };

    // ---- Display central ----
    content.addAndMakeVisible(displayPanel);

    displayLine1.setJustificationType(juce::Justification::centred);
    displayLine1.setFont(juce::FontOptions(88.0f, juce::Font::bold));
    displayLine1.setColour(juce::Label::textColourId, LNF::kDisplayText);
    displayPanel.addAndMakeVisible(displayLine1);

    displayLine2.setJustificationType(juce::Justification::centred);
    displayLine2.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    displayLine2.setColour(juce::Label::textColourId, LNF::kDisplayAccent.brighter(0.05f));
    displayPanel.addAndMakeVisible(displayLine2);

    displayLine3.setJustificationType(juce::Justification::centred);
    displayLine3.setFont(juce::FontOptions(14.0f));
    displayLine3.setColour(juce::Label::textColourId, LNF::kDisplayAccent.withAlpha(0.90f));
    displayPanel.addAndMakeVisible(displayLine3);

    static const char* const statusTitles[4] { "DELAY", "PING PONG", "LO-FI", "MODE" };
    for (int i = 0; i < 4; ++i)
    {
        displayStatusTitle[i].setText(statusTitles[i], juce::dontSendNotification);
        displayStatusTitle[i].setJustificationType(juce::Justification::centred);
        displayStatusTitle[i].setFont(juce::FontOptions(10.0f));
        displayStatusTitle[i].setColour(juce::Label::textColourId, LNF::kDisplayText.withAlpha(0.52f));
        displayPanel.addAndMakeVisible(displayStatusTitle[i]);

        displayStatusValue[i].setJustificationType(juce::Justification::centred);
        displayStatusValue[i].setFont(juce::FontOptions(13.0f, juce::Font::bold));
        displayStatusValue[i].setColour(juce::Label::textColourId, LNF::kDisplayAccent.brighter(0.04f));
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
    setupChoice(divisionControl, ParamIDs::syncDivision, "DIVISION",
                juce::StringArray { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" });
    setupChoice(modifierControl, ParamIDs::syncModifier, "MODIFIER",
                juce::StringArray { "Straight", "Dotted", "Triplet" });
    setupToggle(pingPongControl, ParamIDs::pingPong, "PING PONG");
    setupToggle(loFiControl, ParamIDs::loFiEnabled, "LO-FI");
    setupSegmented(modeControl, ParamIDs::delayMode, juce::StringArray { "DIGITAL", "ANALOG", "TAPE" });

    // FASE 2 -- official button_on / button_off assets (same as SYNC).
    // Attachments / setValueNotifyingHost / DSP untouched.
    for (auto* button : { &syncControl.button, &pingPongControl.button, &loFiControl.button })
        button->setLookAndFeel(&premiumLookAndFeel);
    for (auto* btn : modeControl.buttons)
        btn->setLookAndFeel(&premiumLookAndFeel);

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

    // Abre menor que o canvas de referência — scale uniforme em
    // resized() (proporção defaultWidth:defaultHeight), sem tocar
    // posições internas de layoutContent().
    setSize(openWidth, openHeight);

    updateDisplay();
    updateTimeControlForSync();
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
    c.titleLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    c.titleLabel.setColour(juce::Label::textColourId, LNF::kTextMuted.darker(0.35f));

    c.valueLabel.setLookAndFeel(&nfLookAndFeel);
    c.valueLabel.setJustificationType(juce::Justification::centred);
    c.valueLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    c.valueLabel.setColour(juce::Label::textColourId, LNF::kDisplayAccent.brighter(0.15f));
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

void NFWhiteDelayAudioProcessorEditor::positionRotary(RotaryControl& c, juce::Rectangle<int> area, int maxKnobSize)
{
    // O bloco (knob + nome + chip) usa sempre a altura "pretendida"
    // (maxKnobSize + footer), não a altura real do knob depois de
    // limitado pela largura da coluna -- assim, colunas com a mesma
    // intenção de tamanho (ex.: as 3 do módulo, ou as 4 do hero row)
    // ficam com nome/chip exatamente no mesmo eixo horizontal mesmo
    // que a largura varie um pouco entre colunas (divisão inteira de
    // pixels). O knob em si fica centralizado dentro desse bloco.
    constexpr int titleH = 18;
    constexpr int valueH = 22;
    constexpr int footerH = titleH + valueH;
    const int intendedStackH = maxKnobSize + footerH;
    auto stack = area.withSizeKeepingCentre(area.getWidth(), juce::jmin(area.getHeight(), intendedStackH));

    auto footer = stack.removeFromBottom(footerH);
    c.titleLabel.setBounds(footer.removeFromTop(titleH));
    c.valueLabel.setBounds(footer.withSizeKeepingCentre(72, 18));

    const int maxKnob = juce::jmax(48, juce::jmin(area.getWidth() - 4, stack.getHeight()));
    const int knobSize = juce::jmin(maxKnobSize, maxKnob);
    c.slider.setBounds(stack.withSizeKeepingCentre(knobSize, knobSize));
}

void NFWhiteDelayAudioProcessorEditor::positionToggle(ToggleControl& c, juce::Rectangle<int> area)
{
    c.button.setBounds(area);
}

void NFWhiteDelayAudioProcessorEditor::positionChoice(ChoiceControl& c, juce::Rectangle<int> area)
{
    constexpr int titleHeight = 16;
    constexpr int boxHeight = 60;
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

    // Keep logo / chrome / modules / neon inside the rounded chassis lips.
    // Neon X inset matches content inset so ends align with brand & chrome.
    constexpr int chassisInsetX = 64;
    constexpr int chassisInsetY = 20;
    constexpr int neonInsetX = 64;

    // ---- Header — brand + chrome vertically centred; neon stays bottom divider ----
    auto headerBand = bounds.removeFromTop(128);
    auto header = headerBand.reduced(chassisInsetX, chassisInsetY);

    // Right chrome — slightly larger, vertically centred in header.
    auto bypassArea = header.removeFromRight(128);
    bypassButton.setBounds(bypassArea.withSizeKeepingCentre(146, 64)); // 122x40 visível + margem pro glow

    header.removeFromRight(12);
    auto hamburgerArea = header.removeFromRight(46);
    hamburgerButton.setBounds(hamburgerArea.withSizeKeepingCentre(40, 40));

    header.removeFromRight(8);
    auto saveArea = header.removeFromRight(46);
    saveButton.setBounds(saveArea.withSizeKeepingCentre(40, 40));

    header.removeFromRight(12);
    auto powerArea = header.removeFromRight(46);
    powerButton.setBounds(powerArea.withSizeKeepingCentre(70, 70)); // 44 visível + margem pro glow

    // Brand — NF + text stack as one optically centred block in the header.
    // Etapa 1: tipografia maior (WHITE DELAY em destaque real), bloco
    // ligeiramente mais alto pra acomodar sem apertar as três linhas.
    auto brand = header.removeFromLeft(430);
    constexpr int logoSize = 62;
    constexpr int textStackH = 80;
    constexpr int brandBlockH = 84;
    constexpr int brandGap = 16;
    constexpr int brandContentW = logoSize + brandGap + 300;
    auto brandInner = brand.withSizeKeepingCentre(juce::jmin(brand.getWidth(), brandContentW), brandBlockH);
    auto logoCol = brandInner.removeFromLeft(logoSize);
    nfLogoLabel.setBounds(logoCol.withSizeKeepingCentre(logoSize, logoSize));
    brandInner.removeFromLeft(brandGap);
    auto textCol = brandInner.withSizeKeepingCentre(brandInner.getWidth(), textStackH);
    audioToolsLabel.setBounds(textCol.removeFromTop(18));
    textCol.removeFromTop(1);
    whiteDelayLabel.setBounds(textCol.removeFromTop(40));
    textCol.removeFromTop(1);
    professionalDelayLabel.setBounds(textCol.removeFromTop(17));

    // Neon under header (unchanged as lower divider).
    neonBarLogical = { headerBand.getX() + neonInsetX, headerBand.getBottom() - 7,
                       headerBand.getWidth() - neonInsetX * 2, 6 };
    powerLogical = powerArea.withSizeKeepingCentre(70, 70);

    // ---- Avançado (rodapé) — raised inside chassis, clear of bottom lip ----
    content.chassisBays.clearQuick();
    constexpr int chassisBottomPad = 58; // lifts modules above the rounded bevel
    constexpr int bottomModuleH = 228; // Etapa 2: painel só do tamanho do conteúdo (título + knobs), sem sobra vazia embaixo
    auto bottomStrip = bounds.removeFromBottom(chassisBottomPad + bottomModuleH);
    bottomStrip.removeFromBottom(chassisBottomPad);
    auto bottomRow = bottomStrip.reduced(chassisInsetX, 0);

    const int bottomGap = 14;
    const int bottomInnerW = bottomRow.getWidth() - bottomGap * 2;
    const int modW = (bottomInnerW * 380) / (380 + 360 + 320);
    const int filtW = (bottomInnerW * 360) / (380 + 360 + 320);
    constexpr int bayLip = 7;

    auto modulationBay = bottomRow.removeFromLeft(modW);
    bottomRow.removeFromLeft(bottomGap);
    auto filtersBay = bottomRow.removeFromLeft(filtW);
    bottomRow.removeFromLeft(bottomGap);
    auto characterBay = bottomRow;

    content.chassisBays.add(modulationBay.toFloat());
    content.chassisBays.add(filtersBay.toFloat());
    content.chassisBays.add(characterBay.toFloat());

    auto modulationColumn = modulationBay.reduced(bayLip);
    auto filtersColumn = filtersBay.reduced(bayLip);
    auto characterColumn = characterBay.reduced(bayLip);

    modulationPanel.setBounds(modulationColumn);
    filtersPanel.setBounds(filtersColumn);
    characterPanel.setBounds(characterColumn);

    modulationColumn.removeFromTop(28);
    filtersColumn.removeFromTop(28);
    characterColumn.removeFromTop(28);

    // Etapa 2: knobs dos módulos inferiores replicam a mesma proporção
    // maior do TIME/FEEDBACK/DRY-WET/OUTPUT (mesmo material, só maiores).
    constexpr int bottomKnobAreaHeight = 176;
    constexpr int moduleKnobMax = 140;
    auto modulationKnobs = modulationColumn.removeFromTop(bottomKnobAreaHeight);
    auto filtersKnobs = filtersColumn.removeFromTop(bottomKnobAreaHeight);
    auto characterKnobs = characterColumn.removeFromTop(bottomKnobAreaHeight);

    {
        const int colWidth = modulationKnobs.getWidth() / 4;
        positionRotary(rateControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0), moduleKnobMax);
        positionRotary(depthControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0), moduleKnobMax);
        positionChoice(shapeControl, modulationKnobs.removeFromLeft(colWidth).reduced(8, 0));
        positionRotary(spreadControl, modulationKnobs.reduced(8, 0), moduleKnobMax);
    }
    {
        const int colWidth = filtersKnobs.getWidth() / 3;
        positionRotary(highPassControl, filtersKnobs.removeFromLeft(colWidth).reduced(10, 0), moduleKnobMax);
        positionRotary(lowPassControl, filtersKnobs.removeFromLeft(colWidth).reduced(10, 0), moduleKnobMax);
        positionRotary(resonanceControl, filtersKnobs.reduced(10, 0), moduleKnobMax);
    }
    {
        const int colWidth = characterKnobs.getWidth() / 2;
        positionRotary(characterControl, characterKnobs.removeFromLeft(colWidth).reduced(14, 0), moduleKnobMax);
        positionRotary(duckingControl, characterKnobs.reduced(14, 0), moduleKnobMax);
    }

    // ---- Controles rápidos (linha logo abaixo do centro principal) ----
    // Botões maiores e todos na MESMA altura (SYNC, DIVISION/MODIFIER,
    // PING PONG/LO-FI, DIGITAL/ANALOG/TAPE) -- igual à referência: pills
    // grandes com relevo 3D e contorno neon visível, não pastilhas finas.
    auto quickRow = bounds.removeFromBottom(114).reduced(chassisInsetX, 4);

    constexpr int syncW = 187, comboW = 160, pingLofiGroupW = 291, modeGroupW = 300, gapW = 14;
    const int quickTotalWidth = syncW + comboW + comboW + pingLofiGroupW + modeGroupW + gapW * 4;
    auto quickBlock = quickRow.withSizeKeepingCentre(quickTotalWidth, quickRow.getHeight());

    // Mesmo recuo vertical (8px) pra SYNC e pros grupos (bay + botão
    // interno), assim todos os pills ficam com a MESMA altura e alinhados
    // no mesmo eixo horizontal (pedido: "alinhar os botões debaixo").
    positionToggle(syncControl, quickBlock.removeFromLeft(syncW).reduced(0, 8));
    quickBlock.removeFromLeft(gapW);
    positionChoice(divisionControl, quickBlock.removeFromLeft(comboW));
    quickBlock.removeFromLeft(gapW);
    positionChoice(modifierControl, quickBlock.removeFromLeft(comboW));
    quickBlock.removeFromLeft(gapW);

    auto pingLofiBay = quickBlock.removeFromLeft(pingLofiGroupW);
    content.chassisBays.add(pingLofiBay.toFloat());
    auto pingLofiArea = pingLofiBay.reduced(4);
    pingPongLoFiGroup.setBounds(pingLofiArea);
    auto pingLofiInner = pingLofiArea.reduced(4, 4);
    const int halfW = (pingLofiInner.getWidth() - 8) / 2;
    positionToggle(pingPongControl, pingLofiInner.removeFromLeft(halfW));
    pingLofiInner.removeFromLeft(8);
    positionToggle(loFiControl, pingLofiInner);

    quickBlock.removeFromLeft(gapW);
    auto modeBay = quickBlock.removeFromLeft(modeGroupW);
    content.chassisBays.add(modeBay.toFloat());
    auto modeArea = modeBay.reduced(4);
    modeGroup.setBounds(modeArea);
    positionSegmented(modeControl, modeArea.reduced(4, 4));

    // ---- Centro principal: TIME | DISPLAY | FEEDBACK | DRY/WET | OUTPUT ----
    // Redesign visual (Design_Reference/01_REFERENCIA_VISUAL_APROVADA.png):
    // TIME é o "hero" (coluna e knob maiores); FEEDBACK/DRY-WET/OUTPUT
    // replicam a mesma proporção maior (Etapa 2), um degrau abaixo do TIME,
    // igual à referência -- mesmo material/skin dos knobs, só o tamanho.
    auto mainRow = bounds.reduced(chassisInsetX, 4);

    constexpr int timeColumnW = 210;
    constexpr int heroKnobW = 148; // encolhido pra dar mais largura ao visor (pedido: "visor maior")
    constexpr int heroGap = 12;
    auto timeColumn = mainRow.removeFromLeft(timeColumnW);
    mainRow.removeFromLeft(heroGap);
    auto outputColumn = mainRow.removeFromRight(heroKnobW);
    mainRow.removeFromRight(heroGap);
    auto dryWetColumn = mainRow.removeFromRight(heroKnobW);
    mainRow.removeFromRight(heroGap);
    auto feedbackColumn = mainRow.removeFromRight(heroKnobW);
    mainRow.removeFromRight(heroGap);
    auto displayColumn = mainRow;

    positionRotary(timeControl, timeColumn, 196);
    positionRotary(feedbackControl, feedbackColumn, 150);
    positionRotary(dryWetControl, dryWetColumn, 150);
    positionRotary(outputControl, outputColumn, 150);

    {
        // Display valorizado (Etapa 1) -- ocupa muito mais da faixa central,
        // fechando o vazio vertical entre o header e a linha de controles
        // rápidos, igual à referência aprovada.
        constexpr int displayH = 262;
        const int displayW = juce::jmin(displayColumn.getWidth(), 460);
        auto displayArea = juce::Rectangle<int>(displayW, displayH)
                               .withCentre(displayColumn.getCentre());
        displayPanel.setBounds(displayArea);

        // Inset clears the thinner chrome bezel before text/status.
        auto displayLocal = displayPanel.getLocalBounds().reduced(20, 18);
        auto statusBand = displayLocal.removeFromBottom(34);
        displayLocal.removeFromBottom(3);

        displayLine1.setBounds(displayLocal.removeFromTop((int) (displayLocal.getHeight() * 0.46f)));
        displayLine2.setBounds(displayLocal.removeFromTop((int) (displayLocal.getHeight() * 0.28f)));
        displayLine3.setBounds(displayLocal);

        const int colW = statusBand.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto col = statusBand.removeFromLeft(colW);
            displayStatusTitle[i].setBounds(col.removeFromTop(12));
            displayStatusValue[i].setBounds(col);
        }
    }
}

void NFWhiteDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto base = LNF::kBackground; // exact brand colour; depth via luminosity only

    // Pedido explícito: plugin todo branco, sem diferença de cor entre
    // chassi/escudos -- chapa lisa de cor única, profundidade só pelas
    // bordas/contornos (frame abaixo), não por gradiente/vinheta.
    g.setColour(base);
    g.fillRect(bounds);

    // Chassis frame — Etapa 1 do redesign visual (Design_Reference/
    // 01_REFERENCIA_VISUAL_APROVADA.png): moldura fina e elegante com
    // profundidade real, no lugar do bisel cromado grosso anterior (que
    // lia como plástico). Um traço externo escuro e nítido + um filete
    // interno claro, cantos mais arredondados -- linguagem de gabinete
    // premium, não de brinquedo.
    constexpr float inset = 3.0f;
    constexpr float chassisCorner = 18.0f;
    auto chassisRect = bounds.reduced(inset);

    // (Removida a sombra que cobria a chapa inteira de um escurecimento
    // uniforme ~12% -- framePath preenche quase todo o componente, então
    // o "drop shadow" virava uma tinta plana, deixando o chassi mais
    // escuro que os escudos/painéis pintados por cima sem essa camada.
    // Definição de profundidade agora só pelo traço da moldura abaixo.)

    // Traço externo escuro, fino e nítido (a "moldura metálica" da
    // referência -- carbono/grafite, não cromo espesso).
    g.setColour(juce::Colour(0xff26282d).withAlpha(0.95f));
    g.drawRoundedRectangle(chassisRect, chassisCorner, 2.4f);
    // Filete interno claro -- aresta pegando luz, bem discreta.
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(chassisRect.reduced(2.6f), chassisCorner - 1.8f, 1.0f);
    // Segunda linha, quase invisível -- profundidade sem ficar espessa.
    g.setColour(juce::Colours::black.withAlpha(0.05f));
    g.drawRoundedRectangle(chassisRect.reduced(4.4f), chassisCorner - 3.2f, 0.8f);
}

void NFWhiteDelayAudioProcessorEditor::resized()
{
    content.setBounds(0, 0, defaultWidth, defaultHeight);

    const float scale = juce::jmin((float) getWidth() / (float) defaultWidth,
                                    (float) getHeight() / (float) defaultHeight);
    content.setTransform(juce::AffineTransform::scale(scale));

    // Scale chrome into editor pixels (siblings of content, no transform).
    auto mapRect = [scale] (juce::Rectangle<int> r)
    {
        return juce::Rectangle<int> (juce::roundToInt ((float) r.getX() * scale),
                                     juce::roundToInt ((float) r.getY() * scale),
                                     juce::jmax (1, juce::roundToInt ((float) r.getWidth() * scale)),
                                     juce::jmax (1, juce::roundToInt ((float) r.getHeight() * scale)));
    };
    headerDivider.setBounds (mapRect (neonBarLogical));
    powerButton.setBounds (mapRect (powerLogical));
    headerDivider.toFront (false);
    powerButton.toFront (false);
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
    // After generic refreshers: SYNC owns delay time — dim TIME + show effective ms.
    updateTimeControlForSync();
}

void NFWhiteDelayAudioProcessorEditor::updateTimeControlForSync()
{
    auto& apvts = audioProcessor.apvts;
    const bool syncOn = apvts.getRawParameterValue(ParamIDs::syncEnabled)->load() > 0.5f;

    timeControl.slider.setEnabled(! syncOn);
    timeControl.slider.setAlpha(syncOn ? 0.42f : 1.0f);
    timeControl.titleLabel.setAlpha(syncOn ? 0.50f : 1.0f);
    timeControl.valueLabel.setAlpha(syncOn ? 0.80f : 1.0f);

    if (! syncOn)
        return;

    // Readout only — does not write delayTimeMs (preserved for when SYNC turns off).
    const double bpm = audioProcessor.lastKnownHostBpm.load(std::memory_order_relaxed);
    const int divisionIndex = juce::jlimit(0, 7, (int) apvts.getRawParameterValue(ParamIDs::syncDivision)->load());
    const int modifierIndex = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ParamIDs::syncModifier)->load());
    const double ms = NF::syncDivisionMs(bpm,
                                         static_cast<NF::SyncDivision>(divisionIndex),
                                         static_cast<NF::SyncModifier>(modifierIndex));
    timeControl.valueLabel.setText(formatMs(ms), juce::dontSendNotification);
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

    // Stereo Delay Activity Visualizer + wet L/R meters (same observe-only FIFO).
    displayPanel.consumeWetActivity(audioProcessor);

    // Keep POWER LED in sync with bypass (shared param, no 2nd Attachment).
    powerButton.lit = ! bypassed;
    powerButton.repaint();

    // A fase avança sempre (nunca para de vez -- "quase parada", não
    // "parada"), mais rápido quanto mais ativo o delay estiver.
    displayPanel.animPhase += 0.006f + 0.018f * displayPanel.activity;
    if (displayPanel.animPhase > 10000.0f)
        displayPanel.animPhase -= 10000.0f; // evita crescer sem limite numa sessão longa

    displayPanel.repaint();
}

// Onde cada instalador deposita o manual PDF bilíngue -- ver installer/mac/
// build_installer.sh (pacote docs) e installer/windows-installer.nsi
// (pasta Manual ao lado do VST3).
juce::File NFWhiteDelayAudioProcessorEditor::getManualFile(bool english)
{
   #if JUCE_MAC
    juce::File dir("/Users/Shared/NF Audio Tools/NF White Delay/Manual");
    return dir.getChildFile(english ? "NF_White_Delay_Manual_EN.pdf"
                                     : "NF_White_Delay_Manual_PT.pdf");
   #elif JUCE_WINDOWS
    juce::File dir = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
        .getChildFile("NF Audio Tools")
        .getChildFile("NF White Delay")
        .getChildFile("Manual");
    return dir.getChildFile(english ? "NF White Delay Manual (English).pdf"
                                     : "NF White Delay Manual (Portugues).pdf");
   #else
    return {};
   #endif
}

void NFWhiteDelayAudioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;
    // NÃO seta um LookAndFeel customizado aqui de propósito (o
    // PopupMenu é assíncrono e pode sobreviver ao editor por um
    // instante -- ver auditoria de crash da FASE 6.5).
    menu.addItem(1, "About");
    menu.addItem(2, "Reset UI Size");
    menu.addItem(3, "Manual (English)", getManualFile(true).existsAsFile());
    menu.addItem(4, "Manual (Portugues)", getManualFile(false).existsAsFile());

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
            else if (result == 3)
            {
                getManualFile(true).startAsProcess();
            }
            else if (result == 4)
            {
                getManualFile(false).startAsProcess();
            }
        });
}

void NFWhiteDelayAudioProcessorEditor::showPresetPlaceholder()
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "Preset Manager", "Preset Manager -- coming in next phase", "OK");
}
