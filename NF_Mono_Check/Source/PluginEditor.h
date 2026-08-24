#pragma once

#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"

class NFMonoCheckAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit NFMonoCheckAudioProcessorEditor (NFMonoCheckAudioProcessor&);
    ~NFMonoCheckAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:

    static constexpr int defaultWidth  = 720;
    static constexpr int defaultHeight = 480;

    NFMonoCheckAudioProcessor& processor;

    juce::TextButton leftButton;
    juce::TextButton monoButton;
    juce::TextButton rightButton;

    juce::TextButton aboutButton;

    juce::ComponentBoundsConstrainer resizeConstrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    void timerCallback() override;

    void selectMode (int mode);

    void drawModeButton (juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         const juce::String& largeText,
                         const juce::String& smallText,
                         bool active);

    void drawLevelMeter (juce::Graphics& g,
                        juce::Rectangle<float> bounds,
                        float level,
                        bool leftSide);

    void drawMeterLabels (juce::Graphics& g,
                         juce::Rectangle<float> meterBounds,
                         bool labelsOnLeft);

    void drawCorrelationMeter (juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              float correlationValue);

    void drawVectorscope (juce::Graphics& g,
                         juce::Rectangle<float> bounds);

    static constexpr int scopePointCount = 2048;

    std::array<float, scopePointCount> scopeSnapshotL {};
    std::array<float, scopePointCount> scopeSnapshotR {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NFMonoCheckAudioProcessorEditor)
};
