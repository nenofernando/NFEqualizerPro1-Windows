#include "NFPremiumLookAndFeel.h"
#include <BinaryData.h>

void NFPremiumLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    NFWhiteLookAndFeel::drawRotarySlider(g, x, y, width, height,
                                          sliderPosProportional, rotaryStartAngle,
                                          rotaryEndAngle, slider);
}

void NFPremiumLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                 bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Official ON/OFF shells (SYNC neon / PING PONG silver) — labels drawn in text pass.
    // Margem interna pro glow caber DENTRO dos limites do componente (o
    // botão recebe uma área maior que o pill visível -- ver setup dos
    // controles rápidos) -- evita tanto o corte quadrado do glow quanto
    // os artefatos de pintar sem recorte (setPaintingIsUnclipped) num app
    // interativo com repaint parcial.
    constexpr float glowMargin = 8.0f;
    auto bounds = button.getLocalBounds().toFloat().reduced(glowMargin);
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool down = shouldDrawButtonAsDown && isEnabled;

    auto fullBounds = down ? bounds.reduced(0.4f).translated(0.0f, 0.7f) : bounds;
    if (shouldDrawButtonAsHighlighted && isEnabled && ! down)
        fullBounds = fullBounds.expanded(0.4f);

    // O pill do asset é bem mais largo que alto (~3.2:1). Se o botão do
    // layout for mais "quadrado" que isso, esticar (stretchToFit) espreme
    // a arte e transforma os friso finos de cromo em barras pretas
    // grossas -- feio. Em vez disso, ajustamos SÓ a altura pro pill manter
    // a proporção nativa (undistorted), centralizado no espaço disponível;
    // sombra/glow acompanham esse retângulo, não o botão inteiro.
    constexpr float assetAspect = 3.2f;
    auto drawBounds = fullBounds;
    if (drawBounds.getWidth() > 0.0f && drawBounds.getWidth() / drawBounds.getHeight() < assetAspect)
        drawBounds = drawBounds.withSizeKeepingCentre(drawBounds.getWidth(), drawBounds.getWidth() / assetAspect);

    const float corner = drawBounds.getHeight() * 0.42f;

    if (isEnabled)
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(drawBounds.reduced(0.5f), corner);
        juce::DropShadow(juce::Colours::black.withAlpha(down ? 0.10f : 0.18f),
                         down ? 2 : 4, { 0, down ? 0 : 1 }).drawForPath(g, shadowPath);
    }

    if (isOn && isEnabled)
    {
        // Glow azul forte/neon, calibrado pra caber dentro da margem
        // reservada no componente (glowMargin, acima) -- sem isso o JUCE
        // recorta na borda quadrada do componente antes do glow acabar
        // de esmaecer, cortando o halo de forma abrupta/quadrada.
        const float glowBoost = shouldDrawButtonAsHighlighted ? 1.25f : 1.0f;
        for (int i = 4; i >= 1; --i)
        {
            const float expand = (float) i * 1.8f;
            g.setColour(kNeonGlow.withAlpha((0.30f * glowBoost) / (float) i));
            g.fillRoundedRectangle(drawBounds.expanded(expand), corner + expand * 0.5f);
        }
        g.setColour(kDisplayAccent.withAlpha(0.65f * glowBoost));
        g.drawRoundedRectangle(drawBounds.expanded(1.2f), corner + 0.6f, 2.2f);
    }

    const auto* data = isOn ? BinaryData::button_on_png : BinaryData::button_off_png;
    const int dataSize = isOn ? BinaryData::button_on_pngSize : BinaryData::button_off_pngSize;
    const auto asset = juce::ImageCache::getFromMemory(data, dataSize);

    if (asset.isValid())
    {
        g.setOpacity(isEnabled ? 1.0f : 0.55f);
        g.drawImage(asset, drawBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
        return;
    }

    // Fallback
    juce::ColourGradient fill(isOn ? kAccent : kKnobLight, drawBounds.getCentreX(), drawBounds.getY(),
                               isOn ? kAccent.darker(0.2f) : kKnobDark, drawBounds.getCentreX(), drawBounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(drawBounds, corner);
}

void NFPremiumLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool shouldDrawButtonAsDown)
{
    // Labels only — shells are textless assets (no baked SYNC/PING PONG).
    // Mesma margem de glowMargin do shell (drawButtonBackground) -- o texto
    // tem que ficar centrado no PILL visível, não no componente inteiro
    // (que é maior, pra dar espaço ao glow).
    constexpr int glowMargin = 8;
    const bool isOn = button.getToggleState();
    auto bounds = button.getLocalBounds().reduced(glowMargin);
    if (shouldDrawButtonAsDown && button.isEnabled())
        bounds = bounds.translated(0, 1);

    g.setFont(getTextButtonFont(button, bounds.getHeight()));
    const auto text = button.getButtonText();
    const auto textBounds = bounds.reduced(2, 0);

    if (isOn && button.isEnabled())
    {
        // Soft neon halo (glow only — single final glyph).
        g.setColour(kNeonGlow.withAlpha(0.35f));
        g.drawFittedText(text, textBounds.expanded(1), juce::Justification::centred, 2);
        g.setColour(juce::Colours::white.withAlpha(0.96f));
        g.drawFittedText(text, textBounds, juce::Justification::centred, 2);
        return;
    }

    juce::Colour textColour = button.findColour(juce::TextButton::textColourOffId);
    if (! button.isEnabled())
        textColour = textColour.withAlpha(0.45f);
    else
        textColour = kText.withAlpha(0.92f); // solid dark on silver shell
    g.setColour(textColour);
    g.drawFittedText(text, textBounds, juce::Justification::centred, 2);
}
