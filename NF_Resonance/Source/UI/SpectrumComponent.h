#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectralEngine.h"
#include "../DSP/ResonanceMapSnapshot.h"
class SpectrumComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumComponent(SpectralEngine& e) : engine(e) { startTimerHz(30); }
    void paint(juce::Graphics&) override;

    // Architecture prep only (see ResonanceMapSnapshot.h): optionally attach
    // a read-only pointer to a resonance map snapshot. Left unset (nullptr)
    // by PluginEditor today -- there is no real confidence/persistence data
    // yet (that's V2-B/V2-C), so the RESONANCES overlay stays fully inert
    // until this is actually wired up. paint() below never fabricates data:
    // with no snapshot attached (or an empty one), the RESONANCES pass
    // simply draws nothing, and ORIGINAL/REDUCTION are unaffected either way.
    void setResonanceMapSnapshot(const ResonanceMapSnapshot* snap) { resonanceMap = snap; }
private:
    void timerCallback() override { repaint(); }
    SpectralEngine& engine;
    const ResonanceMapSnapshot* resonanceMap = nullptr;
};
