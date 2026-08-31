#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFLookAndFeel.h"
#include "UI/SpectrumComponent.h"
#include "UI/ControlCurveComponent.h"
class NFResonanceAudioProcessorEditor:public juce::AudioProcessorEditor{public:NFResonanceAudioProcessorEditor(NFResonanceAudioProcessor&);~NFResonanceAudioProcessorEditor()override;void paint(juce::Graphics&)override;void resized()override;private:NFResonanceAudioProcessor&p;NFLookAndFeel lf;SpectrumComponent spectrum;ControlCurveComponent curve;juce::Slider depth,sharp,select,attack,release,output,mix,low,high,transient;juce::ComboBox mode,quality;juce::ToggleButton delta{"DELTA"},bypass{"BYPASS"};std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sa;std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ma,qa;std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> da,ba;void setup(juce::Slider&,const char*,const char*);JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFResonanceAudioProcessorEditor)};