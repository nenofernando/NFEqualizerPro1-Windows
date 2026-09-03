#include "PluginEditor.h"

NFEqualizerAudioProcessorEditor::NFEqualizerAudioProcessorEditor(
    NFEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      panel(p),
      licenseOverlay(p.licenseManager)
{
    panel.setBounds(0, 0, NFEqualizerPanel::designWidth, NFEqualizerPanel::designHeight);
    addAndMakeVisible(panel);

    setResizable(true, true);
    setResizeLimits(NFEqualizerPanel::designWidth / 2, NFEqualizerPanel::designHeight / 2,
                    NFEqualizerPanel::designWidth * 2, NFEqualizerPanel::designHeight * 2);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(
            (double) NFEqualizerPanel::designWidth / (double) NFEqualizerPanel::designHeight);

    setSize(NFEqualizerPanel::designWidth, NFEqualizerPanel::designHeight);

    // NF License System: popup centralizado até ativar, fundo escurecido
    // semi-transparente (o plugin continua visível por baixo - o bloqueio
    // de verdade é no áudio, no processor). Some sozinho quando ativar.
    addChildComponent(licenseOverlay);
    licenseOverlay.setVisible(! p.licenseManager.isActivated());
    licenseOverlay.onActivated = [this] { licenseOverlay.setVisible(false); };
}

void NFEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void NFEqualizerAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) NFEqualizerPanel::designWidth;
    panel.setTransform(juce::AffineTransform::scale(scale));

    licenseOverlay.setBounds(getLocalBounds());
}
