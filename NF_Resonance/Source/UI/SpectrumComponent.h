#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectralEngine.h"
class SpectrumComponent:public juce::Component,private juce::Timer{public:SpectrumComponent(SpectralEngine& e):engine(e){startTimerHz(30);}void paint(juce::Graphics&) override;private:void timerCallback() override{repaint();}SpectralEngine& engine;};