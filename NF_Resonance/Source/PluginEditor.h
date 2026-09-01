#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "UI/NFLookAndFeel.h"
#include "UI/SpectrumComponent.h"
#include "UI/ControlCurveComponent.h"
class NFResonanceAudioProcessorEditor:public juce::AudioProcessorEditor,private juce::Timer{
public:
    NFResonanceAudioProcessorEditor(NFResonanceAudioProcessor&);
    ~NFResonanceAudioProcessorEditor()override;
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    void timerCallback()override;
    void showPresetMenu();
    void showMainMenu();
    void promptSaveAs();
    static juce::File manualFile();
    NFResonanceAudioProcessor&p;
    NFLookAndFeel lf;
    SpectrumComponent spectrum;
    ControlCurveComponent curve;
    juce::Slider depth,sharp,select,attack,release,output,mix,low,high,transient;
    juce::ComboBox mode,quality;
    juce::ToggleButton delta{"DELTA"},bypass{"BYPASS"},fft{"FFT"};
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ma,qa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> da,ba,fa;
    void setup(juce::Slider&,const char*,const char*);
    // Preset selector + hamburger menu -- own header widgets, independent of
    // the host's generic preset UI (see PresetManager).
    std::unique_ptr<PresetManager> presetManager;
    juce::TextButton presetButton;
    juce::ShapeButton menuButton{"menu",juce::Colour(0xffc3d3e2),juce::Colour(0xffeaf4ff),juce::Colour(0xff00afff)};
    juce::String currentPresetName{"Default"};
    bool currentIsUserPreset=false;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFResonanceAudioProcessorEditor)
};