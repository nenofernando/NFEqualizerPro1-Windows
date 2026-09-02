#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "UI/NFLookAndFeel.h"
#include "UI/SpectrumComponent.h"
#include "UI/ControlCurveComponent.h"
// RANGE light -- a small, isolated usability aid for when the LOW/HIGH
// analyzer handles are hidden at the extreme edges (disabled state) and
// hard to click again. Purely a thin view over the EXISTING "lowEnabled"/
// "highEnabled" APVTS parameters -- reads them for its own tri-state
// paint(), writes them (both together) on click. No new parameter, no
// new state of its own, no DSP/detector/curve code touched. See
// NFResonanceAudioProcessorEditor::timerCallback() for how it's kept in
// sync (repainted on the SAME existing 15Hz poll that already dims the
// LOW/HIGH knobs -- no separate Timer).
class RangeLightButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RangeLightButton(juce::AudioProcessorValueTreeState& s) : state(s)
    {
        setTooltip("Frequency Range On/Off");
    }
    // Visible mark stays exactly 16x5px (the requested size) regardless of
    // this component's own bounds -- see paint(). The component itself
    // (set via setBounds() in PluginEditor::resized()) is deliberately
    // LARGER than that (a comfortable ~24x16px hit-area), because a 16x5px
    // clickable region is impractically small, doubly so on a Retina
    // screen. The extra transparent margin around the visible mark still
    // belongs to this same Component, so it already accepts clicks and
    // shows the same tooltip -- no separate invisible overlay needed.
    static constexpr float kVisibleW = 16.0f, kVisibleH = 5.0f;
    void paint(juce::Graphics& g) override
    {
        bool lowOn = state.getRawParameterValue("lowEnabled")->load() > 0.5f;
        bool highOn = state.getRawParameterValue("highEnabled")->load() > 0.5f;
        auto full = getLocalBounds().toFloat();
        juce::Rectangle<float> b(kVisibleW, kVisibleH);
        b.setCentre(full.getCentre());
        juce::Colour lit(0xff00afff); // same cyan/blue used across the plugin's own controls
        juce::Colour off(0xff2a323c); // dark grey, still clearly visible/clickable
        juce::Colour fill = (lowOn && highOn) ? lit
                           : (! lowOn && ! highOn) ? off
                           : lit.withAlpha(0.5f); // exactly one side on -- intermediate state
        if (lowOn && highOn)
        {
            // Very subtle glow when fully active -- a soft, low-alpha halo
            // behind the main shape, never a bright/large effect.
            g.setColour(lit.withAlpha(0.25f));
            g.fillRoundedRectangle(b.expanded(2.0f, 1.5f), b.getHeight() * 0.5f + 1.5f);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(b, b.getHeight() * 0.5f);
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        bool lowOn = state.getRawParameterValue("lowEnabled")->load() > 0.5f;
        bool highOn = state.getRawParameterValue("highEnabled")->load() > 0.5f;
        // Fully on -> turn both off. Fully off OR mixed (only one side on)
        // -> (re)activate both. lowHz/highHz themselves are never touched
        // here (same as the analyzer's own double-click toggle), so
        // reactivating always comes back at whatever frequency was already
        // stored -- never forced back to 25Hz/16kHz.
        bool turnOn = ! (lowOn && highOn);
        if (auto* lp = state.getParameter("lowEnabled"))
        { lp->beginChangeGesture(); lp->setValueNotifyingHost(turnOn ? 1.0f : 0.0f); lp->endChangeGesture(); }
        if (auto* hp = state.getParameter("highEnabled"))
        { hp->beginChangeGesture(); hp->setValueNotifyingHost(turnOn ? 1.0f : 0.0f); hp->endChangeGesture(); }
        repaint();
    }
private:
    juce::AudioProcessorValueTreeState& state;
};
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
    juce::Slider depth,sharp,detail,select,attack,release,output,mix,low,high,transient,maxRed;
    RangeLightButton rangeLight{p.state()};
    // Needed for RangeLightButton's setTooltip() to actually show anything --
    // no other component in this editor used tooltips before, so no
    // TooltipWindow existed. Purely passive/visual, no interaction with any
    // other control.
    juce::TooltipWindow tooltipWindow{this, 500};
    juce::ComboBox mode,detect;
    juce::ToggleButton delta{"DELTA"},bypass{"BYPASS"},fft{"FFT"},maxRedEnabled{"MAX RED"};
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ma,deta;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> da,ba,fa,mrea;
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