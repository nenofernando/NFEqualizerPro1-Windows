#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
// 0.1q: multiband Sensitivity Curve. Up to 32 pre-allocated band slots
// (band_active_N/band_freq_N/band_sens_N, N=0..31 -- see PluginProcessor's
// makeLayout()). A slot's IDENTITY is its own fixed parameter triplet, never
// its position in any array -- double-click-empty creates a band in the
// first inactive slot; right-click deletes (sets active=false, keeps the
// slot's freq/sens values and the parameter itself alive); the drawn curve
// sorts a TEMPORARY list of active-slot indices by frequency purely for
// rendering, which never touches which slot owns which parameter.
class ControlCurveComponent : public juce::Component, private juce::Timer
{
public:
    static constexpr int kMaxBands = 32;
    ControlCurveComponent(juce::AudioProcessorValueTreeState& s);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    juce::AudioProcessorValueTreeState& state;
    int selected = -1, hovered = -1, dragging = -1; // slot indices (0..31), -1 = none
    juce::int64 maxBandsFlashUntilMs = 0;
    juce::int64 widthEditUntilMs = 0; // ghost label shows "freq | sens | W x.xx oct" briefly after a wheel edit
    // LOW/HIGH range handles -- drag the SAME "lowHz"/"highHz" parameters the
    // existing knobs use, so both stay trivially in sync (one shared source
    // of truth in the APVTS; no extra listener needed).
    bool draggingLow = false, draggingHigh = false;
    juce::int64 lowHighEditUntilMs = 0;
    float lowHzOf() const;
    float highHzOf() const;
    bool lowEnabledOf() const;
    bool highEnabledOf() const;
    // Click-vs-drag distinction on the LOW/HIGH handles: mouseDown only
    // arms dragging<Low/High> and remembers where the press started; the
    // change gesture begins (and, if that side was OFF, reactivates it)
    // only once the mouse has actually moved past a small threshold in
    // mouseDrag. If mouseUp arrives with no real movement, it's a click ->
    // toggle that side's enabled flag instead of touching its frequency.
    juce::Point<float> lowHighMouseDownPos;
    bool lowHighDidDrag = false;
    // Taper steepness of the LOW/HIGH roll-off, in octaves-to-floor (smaller
    // = steeper/narrower, larger = gentler/wider) -- purely a local visual
    // preference, mouse-wheel-adjustable while hovering the LOW/HIGH
    // capsule, mirroring the per-band Width wheel gesture. Not an APVTS
    // parameter (no DSP effect exists for this, so it can't be a real
    // automatable host parameter) and not persisted across reloads.
    float lowTaperOct = 2.5f, highTaperOct = 2.5f;

    void timerCallback() override { repaint(); }

    bool isActive(int slot) const;
    float freqOf(int slot) const;
    float sensOf(int slot) const;
    float widthOf(int slot) const;
    int shapeOf(int slot) const;
    float focusOf(int slot) const;
    juce::Point<float> pointFor(int slot) const;
    void setFreq(int slot, float hz);
    void setSens(int slot, float db);
    void setWidth(int slot, float widthOct);
    void setShape(int slot, int shape);
    void setFocus(int slot, float focus);
    void setActive(int slot, bool active);
    std::vector<int> activeSlots() const; // unsorted
    int nearestActivePoint(juce::Point<float>, float& outDist) const;
    int firstInactiveSlot() const; // -1 if all 32 are in use
    int activeCount() const;

    static juce::String freqParamId(int slot) { return "band_freq_" + juce::String(slot); }
    static juce::String sensParamId(int slot) { return "band_sens_" + juce::String(slot); }
    static juce::String activeParamId(int slot) { return "band_active_" + juce::String(slot); }
    static juce::String widthParamId(int slot) { return "band_width_" + juce::String(slot); }
    static juce::String shapeParamId(int slot) { return "band_shape_" + juce::String(slot); }
    static juce::String focusParamId(int slot) { return "band_focus_" + juce::String(slot); }
    static juce::String formatHz(float hz);
    static const char* shapeName(int shape);
    // Small discreet SHAPE panel shown near the analyzer corner while a band
    // is selected -- 6 fixed-position icon buttons (Bell/Wide Bell/Low
    // Shelf/High Shelf/Low Focus/High Focus). Deliberately anchored to a
    // fixed corner (not tracking the point) so it never wanders around the
    // analyzer or covers the curve near wherever the band happens to be.
    std::array<juce::Rectangle<float>, 6> shapePanelButtonRects() const;
};
