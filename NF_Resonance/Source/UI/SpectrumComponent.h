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
    // Shared REDUCTION dB<->Y mapping (0dB at plot centre, dbPxPerDb scales
    // with plot height) -- so an overlay (e.g. the Max Reduction line/drag
    // in ControlCurveComponent) stays pixel-exact with the same vertical
    // scale this component's own REDUCTION curve/gridlines use, without
    // duplicating the formula.
    static float dbPxPerDbFor(juce::Rectangle<float> plot) { return plot.getHeight() * 0.045f; }
    static float yForReductionDbIn(juce::Rectangle<float> plot, float db) { return plot.getCentreY() - db * dbPxPerDbFor(plot); }
    static float reductionDbForYIn(juce::Rectangle<float> plot, float y) { return (plot.getCentreY() - y) / juce::jmax(1.0e-6f, dbPxPerDbFor(plot)); }

    // CANONICAL REDUCTION display mapping -- the ONE function every visual
    // element referencing a real reduction dB value must use: the cyan
    // curve/fill, the dB gridlines' own Y position, the Max Reduction
    // dashed line, and its drag interaction. Includes the display-only
    // visual amplification (see SpectrumComponent.cpp's own warpMagnitude
    // doc) on top of yForReductionDbIn's plain linear scale, plus a clamp
    // to the plot's own inner bounds. realDb=0 always maps to exactly 0;
    // strictly monotonic in |realDb|, so ordering/valleys are preserved
    // and nothing can be invented. mapDisplayYToRealReductionDb is its
    // exact numerical inverse (bisection -- the warp has no closed form),
    // used by the Max Reduction line's own drag so the line tracks the
    // mouse exactly, never a different curve than what's drawn. Neither
    // touches DSP, GainMaskEngine, or appliedReductionSnapshot() -- purely
    // a shared rendering transform.
    static float mapRealReductionDbToDisplayY(juce::Rectangle<float> plot, float realDb);
    static float mapDisplayYToRealReductionDb(juce::Rectangle<float> plot, float y);
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
    // Per-bin (position/frequency-indexed, NEVER by a detected group's
    // temporary identity) short hold before release is allowed to begin --
    // reset only on a genuine new/deeper attack at that same bin. A bin
    // whose underlying detected group changes index/classification frame
    // to frame is still the SAME bin here, so its hold/release state is
    // never disturbed by that alone.
    std::vector<double> peakHoldUntilMs;
};
