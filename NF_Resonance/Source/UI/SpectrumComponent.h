#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectralEngine.h"
class SpectrumComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumComponent(SpectralEngine& e) : engine(e) { startTimerHz(30); }
    void paint(juce::Graphics&) override;
    // Shared geometry so ControlCurveComponent (now an overlay on top of the
    // analyzer) can align its log-frequency x-axis pixel-exactly to this
    // component's own plot area, without duplicating the margin constants.
    static juce::Rectangle<float> plotAreaFor(juce::Rectangle<float> full);
    static float xForHzIn(juce::Rectangle<float> plot, float hz);
    static float hzForXIn(juce::Rectangle<float> plot, float x); // inverse of xForHzIn -- used by the curve's X-drag
    // REDUCTION visual audit: downsamples `binReductionDb` (one real value
    // per FFT bin, <=0dB) to `numPts` log-frequency render points. Pure/
    // testable (no Graphics dependency) -- exercised directly by
    // Tests/ReductionRenderCheck.cpp. Where a render point's own visual
    // cell spans >=1 real bin (true at most frequencies above the bass,
    // where the log-X axis compresses many real bins per pixel), the MOST
    // NEGATIVE real bin value in that cell is kept -- never averaged --
    // so a narrow real notch a few bins wide can never fall silently
    // between two sample points and disappear. Where a cell spans <1 real
    // bin (the log-X axis oversamples relative to real bin density, as in
    // the bass), falls back to linear interpolation between the two real
    // neighbouring bins, unchanged from before. Every output value is a
    // real bin value (or a linear blend of two adjacent real bin values)
    // -- never invented, never scaled/multiplied.
    static void resampleReductionForDisplay(const std::vector<float>& binReductionDb, double sampleRate, int fftSize, int numPts, std::vector<float>& outRedAt);
    // 0.1r: FFT/SPECTRUM toggle -- optional pointer to the "showOriginalFft"
    // parameter's raw value, set once by PluginEditor after construction.
    // nullptr (or param==false) means ORIGINAL stays hidden, the existing
    // default behaviour.
    void setShowOriginalFftParam(const std::atomic<float>* p) { showOriginalFftParam = p; }
private:
    void timerCallback() override { repaint(); }
    SpectralEngine& engine;
    const std::atomic<float>* showOriginalFftParam = nullptr;
    // 0.1l: the "fluid" surface is driven by REDUCTION (reductionDb per bin),
    // not by input magnitude -- a bin with ~0dB reduction stays visually
    // still regardless of how loud/busy the music is. smoothedRedDb uses an
    // asymmetric attack/release one-pole (fast attack, slower release, both
    // DISPLAY-only, never the DSP's own attack/release). smoothedMagDb is
    // still used for ORIGINAL, now a thin, low-opacity reference line only
    // (light symmetric EMA, no fill). Sized lazily to match the engine's own
    // spectrum size on first paint(). Never fed back into the DSP.
    std::vector<float> smoothedMagDb, smoothedRedDb;
};
