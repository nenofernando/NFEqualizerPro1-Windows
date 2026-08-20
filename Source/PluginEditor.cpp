#include "PluginEditor.h"

NFEqualizerAudioProcessorEditor::NFEqualizerAudioProcessorEditor(
    NFEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      panel(p)
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
}

void NFEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void NFEqualizerAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) NFEqualizerPanel::designWidth;
    panel.setTransform(juce::AffineTransform::scale(scale));
}
