#include "ControlCurveComponent.h"
#include "SpectrumComponent.h"
#include "../DSP/ResonanceDetector.h"
#include <algorithm>

ControlCurveComponent::ControlCurveComponent(juce::AudioProcessorValueTreeState& s) : state(s)
{
    setInterceptsMouseClicks(true, false);
    startTimerHz(30); // live-reflects host automation, matching SpectrumComponent's own rate
}

bool ControlCurveComponent::isActive(int slot) const { return state.getRawParameterValue(activeParamId(slot))->load() > 0.5f; }
float ControlCurveComponent::lowHzOf() const { return state.getRawParameterValue("lowHz")->load(); }
float ControlCurveComponent::highHzOf() const { return state.getRawParameterValue("highHz")->load(); }
bool ControlCurveComponent::lowEnabledOf() const { return state.getRawParameterValue("lowEnabled")->load() > 0.5f; }
bool ControlCurveComponent::highEnabledOf() const { return state.getRawParameterValue("highEnabled")->load() > 0.5f; }
float ControlCurveComponent::freqOf(int slot) const { return state.getRawParameterValue(freqParamId(slot))->load(); }
float ControlCurveComponent::sensOf(int slot) const { return state.getRawParameterValue(sensParamId(slot))->load(); }
float ControlCurveComponent::widthOf(int slot) const { return state.getRawParameterValue(widthParamId(slot))->load(); }
int ControlCurveComponent::shapeOf(int slot) const { return (int) state.getRawParameterValue(shapeParamId(slot))->load(); }
float ControlCurveComponent::focusOf(int slot) const { return state.getRawParameterValue(focusParamId(slot))->load(); }
const char* ControlCurveComponent::shapeName(int shape)
{
    switch (shape) { case 1: return "Wide Bell"; case 2: return "Low Shelf"; case 3: return "High Shelf"; case 4: return "Low Focus"; case 5: return "High Focus"; default: return "Bell"; }
}

// Small original glyphs (own geometry, not copied from any reference) --
// each drawn as a single stroked path inside `b`, one line per shape so the
// six read as a coherent family. All match ResonanceDetector::bandContribution's
// actual shapes: Bell/WideBell symmetric bumps (WideBell flatter/broader),
// Shelf a smooth step, Focus an asymmetric bump leaning toward the favoured side.
static void drawShapeIcon(juce::Graphics& g, juce::Rectangle<float> b, int shape, juce::Colour col)
{
    auto r = b.reduced(3.0f);
    float midY = r.getCentreY(), baseY = r.getBottom();
    juce::Path p;
    switch (shape)
    {
        case 1: // Wide Bell: broader, flatter bump
            p.startNewSubPath(r.getX(), baseY);
            p.quadraticTo(r.getX(), midY, r.getCentreX(), midY - r.getHeight() * 0.32f);
            p.quadraticTo(r.getRight(), midY, r.getRight(), baseY);
            break;
        case 2: // Low Shelf: high on the left, smooth step down to the right
            p.startNewSubPath(r.getX(), r.getY() + 2.0f);
            p.lineTo(r.getX() + r.getWidth() * 0.45f, r.getY() + 2.0f);
            p.quadraticTo(r.getCentreX(), r.getCentreY(), r.getX() + r.getWidth() * 0.75f, baseY - 2.0f);
            p.lineTo(r.getRight(), baseY - 2.0f);
            break;
        case 3: // High Shelf: mirror of Low Shelf
            p.startNewSubPath(r.getX(), baseY - 2.0f);
            p.lineTo(r.getX() + r.getWidth() * 0.25f, baseY - 2.0f);
            p.quadraticTo(r.getCentreX(), r.getCentreY(), r.getX() + r.getWidth() * 0.55f, r.getY() + 2.0f);
            p.lineTo(r.getRight(), r.getY() + 2.0f);
            break;
        case 4: // Low Focus: bump leaning left (favours the region below the band's frequency)
            p.startNewSubPath(r.getX(), baseY);
            p.quadraticTo(r.getX(), midY - r.getHeight() * 0.1f, r.getX() + r.getWidth() * 0.32f, r.getY() + 1.0f);
            p.quadraticTo(r.getX() + r.getWidth() * 0.6f, midY, r.getRight(), baseY);
            break;
        case 5: // High Focus: mirror of Low Focus
            p.startNewSubPath(r.getX(), baseY);
            p.quadraticTo(r.getX() + r.getWidth() * 0.4f, midY, r.getX() + r.getWidth() * 0.68f, r.getY() + 1.0f);
            p.quadraticTo(r.getRight(), midY - r.getHeight() * 0.1f, r.getRight(), baseY);
            break;
        default: // Bell: symmetric, narrower than Wide Bell
            p.startNewSubPath(r.getX(), baseY);
            p.quadraticTo(r.getX() + r.getWidth() * 0.15f, midY, r.getCentreX(), r.getY() + 1.0f);
            p.quadraticTo(r.getRight() - r.getWidth() * 0.15f, midY, r.getRight(), baseY);
            break;
    }
    g.setColour(col);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

std::vector<int> ControlCurveComponent::activeSlots() const
{
    std::vector<int> v;
    for (int i = 0; i < kMaxBands; ++i) if (isActive(i)) v.push_back(i);
    return v;
}
int ControlCurveComponent::firstInactiveSlot() const
{
    for (int i = 0; i < kMaxBands; ++i) if (! isActive(i)) return i;
    return -1;
}
int ControlCurveComponent::activeCount() const { int n = 0; for (int i = 0; i < kMaxBands; ++i) if (isActive(i)) ++n; return n; }

// This component's bounds are set (in PluginEditor::resized()) to exactly
// match SpectrumComponent's inner plot area in editor coordinates, so its
// own getLocalBounds() IS that plot rect -- xForHzIn/hzForXIn on that rect
// line up pixel-for-pixel with the analyzer's own frequency axis, at any
// window size.
juce::Point<float> ControlCurveComponent::pointFor(int slot) const
{
    auto r = getLocalBounds().toFloat();
    float x = SpectrumComponent::xForHzIn(r, freqOf(slot));
    float y = juce::jmap(sensOf(slot), -12.0f, 12.0f, r.getBottom(), r.getY());
    return { x, y };
}

int ControlCurveComponent::nearestActivePoint(juce::Point<float> p, float& outDist) const
{
    int best = -1; float bestD = 1.0e9f;
    for (int i = 0; i < kMaxBands; ++i)
    {
        if (! isActive(i)) continue;
        float d = p.getDistanceFrom(pointFor(i));
        if (d < bestD) { bestD = d; best = i; }
    }
    outDist = bestD;
    return best;
}

// Stable per-slot neon identity colour: a pure function of slot index (never
// random, never repaint-dependent), cycling through the palette if there are
// more bands than colours. The curve LINE stays the main Neon Blue -- only
// the control points themselves get individual colour identity.
static juce::Colour paletteColorForSlot(int slot)
{
    static const juce::Colour palette[7] = {
        juce::Colour(0xff27d8ff), // cyan
        juce::Colour(0xff4c6fff), // blue
        juce::Colour(0xff3dffb0), // green
        juce::Colour(0xffb06cff), // purple
        juce::Colour(0xffff6ec7), // pink
        juce::Colour(0xffffe066), // yellow
        juce::Colour(0xffff9f4a), // orange
    };
    return palette[(size_t) (slot % 7)];
}

juce::String ControlCurveComponent::formatHz(float hz)
{
    if (hz < 1000.0f) return juce::String((int) std::round(hz)) + " Hz";
    return juce::String(hz / 1000.0f, 2) + " kHz";
}

// Catmull-Rom -> cubic Bezier, clamped to each segment's own
// [min(y0,y1),max(y0,y1)] range -- passes exactly through every point,
// no overshoot beyond neighbouring values.
static juce::Path buildClampedSplinePath(const std::vector<juce::Point<float>>& pts)
{
    juce::Path path;
    if (pts.size() < 2) return path;
    path.startNewSubPath(pts.front());
    if (pts.size() == 2) { path.lineTo(pts[1]); return path; }
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        auto p0 = i > 0 ? pts[i - 1] : pts[i];
        auto p1 = pts[i];
        auto p2 = pts[i + 1];
        auto p3 = (i + 2 < pts.size()) ? pts[i + 2] : pts[i + 1];
        auto c1 = p1 + (p2 - p0) / 6.0f;
        auto c2 = p2 - (p3 - p1) / 6.0f;
        float loY = juce::jmin(p1.y, p2.y), hiY = juce::jmax(p1.y, p2.y);
        c1.y = juce::jlimit(loY, hiY, c1.y);
        c2.y = juce::jlimit(loY, hiY, c2.y);
        path.cubicTo(c1, c2, p2);
    }
    return path;
}

std::array<juce::Rectangle<float>, 6> ControlCurveComponent::shapePanelButtonRects() const
{
    // Panel top-left is fixed at (plot.x+6, plot.y+6); 14px label row, then
    // a single row of 6x22px icon buttons with 3px gaps.
    auto full = getLocalBounds().toFloat();
    const float btn = 22.0f, gap = 3.0f, pad = 6.0f;
    float x0 = full.getX() + pad + pad, y0 = full.getY() + pad + 14.0f + pad;
    std::array<juce::Rectangle<float>, 6> out;
    for (int i = 0; i < 6; ++i) out[(size_t) i] = juce::Rectangle<float>(x0 + (btn + gap) * (float) i, y0, btn, btn);
    return out;
}

// LOW/HIGH is a SEPARATE concept from any band's Bell/Shelf/Focus: it's the
// global processing-range boundary, drawn as the two lateral "roll-offs" of
// the base line itself. At LOW=20Hz/HIGH=20000Hz (fully open, the default)
// this returns exactly 0 everywhere -- so with every band at 0 sensitivity,
// the curve is a flat horizontal line, full stop. Moving LOW up dips the
// line down starting from the far left, smoothly rising back to 0 right
// around LOW (mirrored for HIGH on the right). This is purely a VISUAL
// convention showing "the detector isn't looking below/above here" (the
// real DSP gate is ResonanceDetector's own hz>=lo&&hz<=hi check, untouched)
// -- it is added to, never mixed into, each band's own localized shape.
// taperOct = how many octaves the roll-off takes to reach the (fixed,
// far-below-the-plot) floor -- smaller is steeper/narrower, larger is
// gentler/wider, wheel-adjustable per side (see mouseWheelMove).
static float globalRangeShapeAt(float hz, float lowHz, float highHz, float lowTaperOct, float highTaperOct)
{
    const double floorDb = -60.0;
    double hzLog = std::log2(juce::jmax(1.0f, hz));
    double distBelow = std::log2(juce::jmax(1.0f, lowHz)) - hzLog;   // >0 when hz < lowHz
    double distAbove = hzLog - std::log2(juce::jmax(1.0f, highHz));  // >0 when hz > highHz
    auto slope = [&](double distOct, double taperOct) -> double
    {
        if (distOct <= 0.0) return 0.0;
        // A generous, proportional knee (not a fixed tiny one) so the line
        // eases into the slope gradually instead of a sharp corner right at
        // the LOW/HIGH boundary.
        double kneeOct = juce::jlimit(0.15, 1.0, taperOct * 0.35);
        double eased = distOct < kneeOct ? (distOct * distOct) / (2.0 * kneeOct) : distOct - kneeOct * 0.5;
        double dbPerOct = -floorDb / juce::jmax(0.1, (double) taperOct);
        return juce::jmax(floorDb, -dbPerOct * eased);
    };
    return (float) juce::jmin(slope(distBelow, (double) lowTaperOct), slope(distAbove, (double) highTaperOct));
}

void ControlCurveComponent::paint(juce::Graphics& g)
{
    auto slots = activeSlots();
    std::sort(slots.begin(), slots.end(), [this](int a, int b) { return freqOf(a) < freqOf(b); });

    auto full = getLocalBounds().toFloat();
    float lowHz = lowHzOf(), highHz = highHzOf();
    bool lowOn = lowEnabledOf(), highOn = highEnabledOf();
    // The HANDLE always sits at the real saved lowHz/highHz (so the user can
    // grab it and re-enable at the last value) -- but the darkening and the
    // white curve's taper use an EFFECTIVE bound that's fully open when that
    // side is OFF, matching the DSP exactly (see SpectralEngine::process()).
    float effLowHz = lowOn ? lowHz : 1.0f;
    float effHighHz = highOn ? highHz : 100000.0f;
    float lowX = SpectrumComponent::xForHzIn(full, lowHz);
    float highX = SpectrumComponent::xForHzIn(full, highHz);
    float zeroY = juce::jmap(0.0f, -12.0f, 12.0f, full.getBottom(), full.getY());

    // Darken outside the LOW/HIGH active range FIRST, so the curve/handles
    // drawn afterwards sit visibly on top of it. Uses the EFFECTIVE bound --
    // no darkening at all on a side that's OFF.
    {
        float effLowX = SpectrumComponent::xForHzIn(full, effLowHz);
        float effHighX = SpectrumComponent::xForHzIn(full, effHighHz);
        g.setColour(juce::Colour(0xff000000).withAlpha(0.30f));
        if (lowOn && effLowX > full.getX() + 0.5f) g.fillRect(juce::Rectangle<float>(full.getX(), full.getY(), effLowX - full.getX(), full.getHeight()));
        if (highOn && effHighX < full.getRight() - 0.5f) g.fillRect(juce::Rectangle<float>(effHighX, full.getY(), full.getRight() - effHighX, full.getHeight()));
    }

    // Curve rendering: globalRangeShapeAt(lowHz,highHz) -- the two lateral
    // roll-offs -- PLUS the ACTUAL mathematical sum of every active band's
    // own shaped contribution (ResonanceDetector::bandContribution/
    // combinedSensitivityAt), densely sampled across the log-frequency axis.
    // A single Bell only ever affects the region around its own frequency,
    // sized by its own Width -- it can never turn the whole 20Hz-20kHz line
    // into one giant arc, because nothing here mixes the two concepts.
    {
        float freqArr[ResonanceDetector::kMaxBands]{}, sensArr[ResonanceDetector::kMaxBands]{}, widthArr[ResonanceDetector::kMaxBands]{}, focusArr[ResonanceDetector::kMaxBands]{};
        int shapeArr[ResonanceDetector::kMaxBands]{}; bool activeArr[ResonanceDetector::kMaxBands]{};
        for (int i = 0; i < ResonanceDetector::kMaxBands; ++i)
        {
            activeArr[i] = isActive(i);
            if (activeArr[i]) { freqArr[i] = freqOf(i); sensArr[i] = sensOf(i); widthArr[i] = widthOf(i); shapeArr[i] = shapeOf(i); focusArr[i] = focusOf(i); }
        }
        const int numPts = juce::jlimit(48, 220, (int) full.getWidth() / 4);
        std::vector<juce::Point<float>> curvePts;
        curvePts.reserve((size_t) numPts);
        for (int k = 0; k < numPts; ++k)
        {
            float t = (float) k / (float) (numPts - 1);
            float logHz = std::log10(20.0f) + t * (std::log10(20000.0f) - std::log10(20.0f));
            float hz = std::pow(10.0f, logHz);
            float bandsSens = slots.empty() ? 0.0f : ResonanceDetector::combinedSensitivityAt(hz, freqArr, sensArr, widthArr, shapeArr, focusArr, activeArr);
            // No artificial +-12dB reclamp here: globalRangeShapeAt is a
            // purely visual convention (the real DSP gate is the detector's
            // own hz>=lo&&hz<=hi check), so outside LOW/HIGH the line is
            // free to run off the bottom of the plot like a real filter
            // slope instead of hitting a flat wall.
            float sens = globalRangeShapeAt(hz, effLowHz, effHighHz, lowTaperOct, highTaperOct) + bandsSens;
            float x = SpectrumComponent::xForHzIn(full, hz);
            float y = juce::jmap(sens, -12.0f, 12.0f, full.getBottom(), full.getY());
            curvePts.push_back({ x, y });
        }
        juce::Path path = buildClampedSplinePath(curvePts);

        // Neon white main line: cool, slightly blue-tinted white with a soft
        // glow, distinct from the band points' individual neon hues and from
        // the analyzer's blue/cyan REDUCTION surface below it.
        bool anySelected = selected >= 0, anyHover = (hovered >= 0 && ! anySelected) || dragging >= 0;
        float lineAlpha = anySelected ? 0.95f : (anyHover ? 0.9f : 0.62f);
        float glowAlpha = anySelected ? 0.20f : (anyHover ? 0.14f : 0.07f);
        const juce::Colour neonWhite(0xffeef7ff);
        if (glowAlpha > 0.0f) { g.setColour(neonWhite.withAlpha(glowAlpha)); g.strokePath(path, juce::PathStrokeType(6.0f)); }
        g.setColour(neonWhite.withAlpha(lineAlpha));
        g.strokePath(path, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // LOW/HIGH range handles -- small capsules sitting ON the 0dB line
    // itself (not a full-height vertical line), same log-Hz axis, drive the
    // real lowHz/highHz parameters (shared with the existing knobs, so both
    // stay in sync with no extra glue).
    auto drawHandle = [&](float x, juce::Colour col, bool active, bool on)
    {
        float w = active ? 9.0f : 7.0f, h = active ? 26.0f : 20.0f;
        juce::Rectangle<float> capsule(x - w * 0.5f, zeroY - h * 0.5f, w, h);
        // OFF: dark/desaturated, ~25-35% opacity, but the handle stays put
        // at its last frequency so it's still there to click and re-enable.
        juce::Colour body = on ? col : col.darker(0.75f).withAlpha(0.30f);
        g.setColour(body.withAlpha(on ? (active ? 0.28f : 0.14f) : 0.10f));
        g.fillRoundedRectangle(capsule.expanded(3.0f), (w + 6.0f) * 0.5f);
        g.setColour(on ? body.withAlpha(active ? 1.0f : 0.75f) : body);
        g.fillRoundedRectangle(capsule, w * 0.5f);
        if (! on)
        {
            // Small OFF slash through the capsule -- discreet, no extra chrome.
            g.setColour(juce::Colour(0xff090d13).withAlpha(0.6f));
            g.drawLine(x - w * 0.32f, zeroY - h * 0.28f, x + w * 0.32f, zeroY + h * 0.28f, 1.2f);
        }
        else
        {
            g.setColour(juce::Colour(0xff090d13).withAlpha(0.5f));
            g.drawLine(x, zeroY - h * 0.22f, x, zeroY + h * 0.22f, 1.2f); // small grip mark
        }
    };
    drawHandle(lowX, juce::Colour(0xff5ec8ff), draggingLow, lowOn);
    drawHandle(highX, juce::Colour(0xffffb15e), draggingHigh, highOn);
    if (draggingLow || draggingHigh || juce::Time::currentTimeMillis() < lowHighEditUntilMs)
    {
        g.setFont(11.0f);
        if (draggingLow || (! draggingHigh && juce::Time::currentTimeMillis() < lowHighEditUntilMs))
        {
            g.setColour(juce::Colour(0xff5ec8ff));
            juce::String txt = lowOn ? ("LOW " + formatHz(lowHzOf())) : ("LOW OFF \xC2\xB7 " + formatHz(lowHzOf()));
            g.drawText(txt, (int) (lowX - 55.0f), (int) (full.getY() + 20.0f), 110, 14, juce::Justification::centred);
        }
        if (draggingHigh)
        {
            g.setColour(juce::Colour(0xffffb15e));
            juce::String txt = highOn ? ("HIGH " + formatHz(highHzOf())) : ("HIGH OFF \xC2\xB7 " + formatHz(highHzOf()));
            g.drawText(txt, (int) (highX - 55.0f), (int) (full.getY() + 20.0f), 110, 14, juce::Justification::centred);
        }
    }

    int activeIdx = dragging >= 0 ? dragging : (selected >= 0 ? selected : hovered);

    for (int i : slots)
    {
        bool isSelected = (i == selected), isDragging = (i == dragging), isHover = (i == hovered && ! isSelected);
        auto pt = pointFor(i);
        auto col = paletteColorForSlot(i);
        float r = (isSelected || isDragging) ? 6.2f : (isHover ? 5.0f : 3.6f);
        if (isSelected || isDragging)
        {
            g.setColour(col.withAlpha(0.32f));
            g.fillEllipse(juce::Rectangle<float>(r * 3.4f, r * 3.4f).withCentre(pt));
            g.setColour(col); // full brightness -- selected/dragging
        }
        else g.setColour(col.withAlpha(isHover ? 0.85f : 0.55f)); // hover: extra highlight over the idle alpha; idle: same hue, more discreet
        g.fillEllipse(juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(pt));
    }

    // Label: ONLY the active point (hover/selected/dragging/width-editing)
    // gets a readout, and only while it stays active -- idle bands show NO
    // permanent label any more (the analyzer's own top frequency scale is
    // the reference; per-band frequency is transient, edit-time-only info).
    auto plotR = getLocalBounds().toFloat();
    if (activeIdx >= 0 && isActive(activeIdx))
    {
        auto pt = pointFor(activeIdx);
        float sens = sensOf(activeIdx);
        juce::String txt = formatHz(freqOf(activeIdx)) + "  |  " + (sens >= 0.0f ? "+" : "") + juce::String(sens, 1);
        // Width appended only briefly right after a wheel edit.
        bool showWidth = juce::Time::currentTimeMillis() < widthEditUntilMs;
        if (showWidth) txt += "  |  W " + juce::String(widthOf(activeIdx), 2) + " oct";
        float textW = showWidth ? 172.0f : 118.0f;
        float lx = juce::jlimit(plotR.getX() + 2.0f, plotR.getRight() - textW - 2.0f, pt.x - textW * 0.5f);
        g.setColour(juce::Colour(0xffeaf4ff));
        g.setFont(11.5f);
        g.drawText(txt, (int) lx, (int) (pt.y - 20.0f), (int) textW, 14, juce::Justification::centred);
    }

    // SHAPE panel: small, discreet, fixed in the top-left corner, visible
    // only while a band is selected. Own icons/geometry, not a copy of any
    // reference plugin's panel layout or iconography.
    if (selected >= 0 && isActive(selected))
    {
        auto buttons = shapePanelButtonRects();
        auto panel = buttons.front().getUnion(buttons.back()).expanded(5.0f);
        panel = panel.withY(panel.getY() - 17.0f).withHeight(panel.getHeight() + 17.0f);
        g.setColour(juce::Colour(0xff101722).withAlpha(0.92f));
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(juce::Colour(0xff283542));
        g.drawRoundedRectangle(panel, 6.0f, 1.0f);
        g.setColour(juce::Colour(0xff9fb3c8));
        g.setFont(9.0f);
        g.drawText("SHAPE", panel.getX() + 6.0f, panel.getY() + 2.0f, panel.getWidth() - 12.0f, 12.0f, juce::Justification::centredLeft);

        int curShape = shapeOf(selected);
        auto bandCol = paletteColorForSlot(selected);
        for (int s = 0; s < 6; ++s)
        {
            auto br = buttons[(size_t) s];
            bool active = (s == curShape);
            if (active) { g.setColour(bandCol.withAlpha(0.22f)); g.fillRoundedRectangle(br.expanded(1.5f), 4.0f); }
            drawShapeIcon(g, br, s, active ? juce::Colour(0xff27d8ff) : juce::Colour(0xff5b7086));
        }
    }

    // Discreet "band limit reached" feedback, fading out over ~1.2s.
    if (maxBandsFlashUntilMs > 0)
    {
        juce::int64 now = juce::Time::currentTimeMillis();
        float remain = (float) (maxBandsFlashUntilMs - now) / 1200.0f;
        if (remain > 0.0f)
        {
            g.setColour(juce::Colour(0xff27d8ff).withAlpha(juce::jlimit(0.0f, 0.85f, remain)));
            g.setFont(11.0f);
            g.drawText("32/32 BANDS -- delete one to add another", plotR.reduced(6.0f).removeFromBottom(16.0f), juce::Justification::centredRight);
        }
    }
}

void ControlCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    // SHAPE panel button hit-test takes priority when a band is selected --
    // clicking an icon must not also fall through to point-selection logic.
    if (selected >= 0 && isActive(selected) && ! e.mods.isPopupMenu())
    {
        auto buttons = shapePanelButtonRects();
        for (int s = 0; s < 6; ++s)
            if (buttons[(size_t) s].contains(e.position)) { setShape(selected, s); return; }
    }

    // LOW/HIGH handle hit-test: the capsules now sit right on the 0dB line,
    // so the hit region is bounded in both X (near the handle's own x) and Y
    // (near the 0dB line), not a full-height strip -- this keeps them from
    // stealing clicks meant for a band point that happens to pass nearby.
    if (! e.mods.isPopupMenu())
    {
        auto full = getLocalBounds().toFloat();
        float lowX = SpectrumComponent::xForHzIn(full, lowHzOf());
        float highX = SpectrumComponent::xForHzIn(full, highHzOf());
        float zeroY = juce::jmap(0.0f, -12.0f, 12.0f, full.getBottom(), full.getY());
        bool nearZeroY = std::abs(e.position.y - zeroY) <= 16.0f;
        // Don't begin the change gesture yet -- just arm dragging<Low/High>
        // and remember where the press started. mouseDrag only turns this
        // into an actual frequency edit (and reactivates an OFF side) once
        // the mouse moves past a small threshold; mouseUp with no real
        // movement toggles ON/OFF instead. This is what keeps a plain click
        // from accidentally nudging the frequency, and a drag from
        // accidentally toggling the filter off.
        if (nearZeroY && std::abs(e.position.x - lowX) <= 9.0f) { draggingLow = true; lowHighDidDrag = false; lowHighMouseDownPos = e.position; repaint(); return; }
        if (nearZeroY && std::abs(e.position.x - highX) <= 9.0f) { draggingHigh = true; lowHighDidDrag = false; lowHighMouseDownPos = e.position; repaint(); return; }
    }

    float d; int i = nearestActivePoint(e.position, d);
    bool hit = d <= 16.0f;

    if (e.mods.isPopupMenu())
    {
        if (hit)
        {
            juce::PopupMenu shapeMenu;
            int curShape = shapeOf(i);
            for (int s = 0; s < 6; ++s) shapeMenu.addItem(100 + s, shapeName(s), true, s == curShape);
            juce::PopupMenu m;
            m.addSubMenu("Shape", shapeMenu);
            m.addSeparator();
            m.addItem(1, "Delete Band");
            int slot = i;
            m.showMenuAsync(juce::PopupMenu::Options(), [this, slot](int result)
            {
                if (result == 1) setActive(slot, false);
                else if (result >= 100 && result < 106) setShape(slot, result - 100);
            });
        }
        return;
    }

    selected = hit ? i : -1;
    if (selected >= 0)
    {
        dragging = selected;
        if (auto* fp = state.getParameter(freqParamId(dragging))) fp->beginChangeGesture();
        if (auto* sp = state.getParameter(sensParamId(dragging))) sp->beginChangeGesture();
        auto r = getLocalBounds().toFloat();
        setFreq(dragging, SpectrumComponent::hzForXIn(r, e.position.x));
        setSens(dragging, juce::jmap(juce::jlimit(r.getY(), r.getBottom(), e.position.y), r.getBottom(), r.getY(), -12.0f, 12.0f));
    }
    repaint();
}
void ControlCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingLow || draggingHigh)
    {
        if (! lowHighDidDrag)
        {
            if (e.position.getDistanceFrom(lowHighMouseDownPos) < 4.0f) return; // still just a press, not a drag yet
            lowHighDidDrag = true;
            // Crossed the threshold: this is a real drag now -- begin the
            // change gesture. Dragging ONLY ever edits lowHz/highHz -- the
            // enabled flag is untouched either way, whether that side is
            // currently ON or OFF; only a double-click on the handle itself
            // may change lowEnabled/highEnabled (see mouseDoubleClick).
            const char* pid = draggingLow ? "lowHz" : "highHz";
            if (auto* p = state.getParameter(pid)) p->beginChangeGesture();
        }
        auto r = getLocalBounds().toFloat();
        float hz = SpectrumComponent::hzForXIn(r, e.position.x);
        const float minSeparationOct = 0.1f; // keep LOW strictly below HIGH by a small safe margin
        if (draggingLow)
        {
            float maxAllowed = highHzOf() * std::pow(2.0f, -minSeparationOct);
            hz = juce::jlimit(20.0f, juce::jmax(20.0f, maxAllowed), hz);
            if (auto* p = state.getParameter("lowHz")) p->setValueNotifyingHost(p->convertTo0to1(hz));
        }
        else
        {
            float minAllowed = lowHzOf() * std::pow(2.0f, minSeparationOct);
            hz = juce::jlimit(juce::jmin(20000.0f, minAllowed), 20000.0f, hz);
            if (auto* p = state.getParameter("highHz")) p->setValueNotifyingHost(p->convertTo0to1(hz));
        }
        lowHighEditUntilMs = juce::Time::currentTimeMillis() + 1500;
        repaint();
        return;
    }
    if (dragging < 0) return;
    auto r = getLocalBounds().toFloat();
    setFreq(dragging, SpectrumComponent::hzForXIn(r, e.position.x));
    setSens(dragging, juce::jmap(juce::jlimit(r.getY(), r.getBottom(), e.position.y), r.getBottom(), r.getY(), -12.0f, 12.0f));
}
void ControlCurveComponent::mouseUp(const juce::MouseEvent&)
{
    if (draggingLow)
    {
        // A plain click with no real movement is JUST selection/focus -- it
        // never touches lowHz or lowEnabled. Only a genuine drag (which
        // began its own change gesture in mouseDrag) needs ending here.
        if (lowHighDidDrag) { if (auto* p = state.getParameter("lowHz")) p->endChangeGesture(); }
        draggingLow = false;
    }
    if (draggingHigh)
    {
        if (lowHighDidDrag) { if (auto* p = state.getParameter("highHz")) p->endChangeGesture(); }
        draggingHigh = false;
    }
    lowHighDidDrag = false;
    if (dragging >= 0)
    {
        if (auto* fp = state.getParameter(freqParamId(dragging))) fp->endChangeGesture();
        if (auto* sp = state.getParameter(sensParamId(dragging))) sp->endChangeGesture();
    }
    dragging = -1;
    repaint();
}
void ControlCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    float d; int i = nearestActivePoint(e.position, d);
    int newHover = (d <= 16.0f) ? i : -1;
    setMouseCursor(newHover >= 0 ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    if (newHover != hovered) { hovered = newHover; repaint(); }
}
void ControlCurveComponent::mouseExit(const juce::MouseEvent&) { if (hovered != -1) { hovered = -1; repaint(); } }

void ControlCurveComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // LOW/HIGH taper steepness: mouse wheel while hovering the LOW or HIGH
    // capsule adjusts how many octaves the roll-off takes to reach the
    // floor -- same direction convention as band Width (scroll UP closes/
    // narrows the octaves -- steeper; scroll DOWN opens/widens them).
    {
        auto full = getLocalBounds().toFloat();
        float lowX = SpectrumComponent::xForHzIn(full, lowHzOf());
        float highX = SpectrumComponent::xForHzIn(full, highHzOf());
        float zeroY = juce::jmap(0.0f, -12.0f, 12.0f, full.getBottom(), full.getY());
        bool nearZeroY = std::abs(e.position.y - zeroY) <= 16.0f;
        if (nearZeroY && std::abs(e.position.x - lowX) <= 9.0f)
        {
            lowTaperOct = juce::jlimit(0.3f, 40.0f, lowTaperOct - wheel.deltaY * 0.6f);
            lowHighEditUntilMs = juce::Time::currentTimeMillis() + 1500;
            repaint();
            return;
        }
        if (nearZeroY && std::abs(e.position.x - highX) <= 9.0f)
        {
            highTaperOct = juce::jlimit(0.3f, 40.0f, highTaperOct - wheel.deltaY * 0.6f);
            lowHighEditUntilMs = juce::Time::currentTimeMillis() + 1500;
            repaint();
            return;
        }
    }

    // Width/Q editing: mouse wheel over the selected band (falls back to the
    // hovered one so a quick scroll-without-clicking still works). Direction
    // is explicit and mandatory: scroll UP (positive deltaY, JUCE's standard
    // "scrolled up" sign) narrows the band; scroll DOWN widens it -- so this
    // SUBTRACTS deltaY from width, the opposite of a naive "wheel increases
    // value" mapping.
    int slot = selected >= 0 ? selected : hovered;
    if (slot < 0 || ! isActive(slot)) return;
    float cur = widthOf(slot);
    float next = juce::jlimit(0.05f, 4.0f, cur - wheel.deltaY * 0.6f);
    setWidth(slot, next);
    widthEditUntilMs = juce::Time::currentTimeMillis() + 1500;
}

void ControlCurveComponent::setWidth(int slot, float widthOct)
{
    if (auto* wp = state.getParameter(widthParamId(slot)))
    {
        wp->beginChangeGesture();
        wp->setValueNotifyingHost(wp->convertTo0to1(widthOct));
        wp->endChangeGesture();
    }
    repaint();
}
void ControlCurveComponent::setFocus(int slot, float focus)
{
    if (auto* fp = state.getParameter(focusParamId(slot)))
    {
        fp->beginChangeGesture();
        fp->setValueNotifyingHost(fp->convertTo0to1(focus));
        fp->endChangeGesture();
    }
    repaint();
}
void ControlCurveComponent::setShape(int slot, int shape)
{
    if (auto* sp = state.getParameter(shapeParamId(slot)))
    {
        sp->beginChangeGesture();
        sp->setValueNotifyingHost(sp->convertTo0to1((float) shape));
        sp->endChangeGesture();
    }
    repaint();
}

void ControlCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double LEFT-click on the LOW or HIGH capsule is the ONLY thing that
    // may toggle lowEnabled/highEnabled -- a single click just selects/
    // focuses the handle, and dragging only ever edits the frequency (see
    // mouseDown/mouseDrag/mouseUp). The frequency itself is never touched
    // here, so re-enabling always comes back at exactly the stored value.
    if (! e.mods.isPopupMenu())
    {
        auto full = getLocalBounds().toFloat();
        float lowX = SpectrumComponent::xForHzIn(full, lowHzOf());
        float highX = SpectrumComponent::xForHzIn(full, highHzOf());
        float zeroY = juce::jmap(0.0f, -12.0f, 12.0f, full.getBottom(), full.getY());
        bool nearZeroY = std::abs(e.position.y - zeroY) <= 16.0f;
        if (nearZeroY && std::abs(e.position.x - lowX) <= 9.0f)
        {
            if (auto* ep = state.getParameter("lowEnabled")) { ep->beginChangeGesture(); ep->setValueNotifyingHost(ep->getValue() < 0.5f ? 1.0f : 0.0f); ep->endChangeGesture(); }
            lowHighEditUntilMs = juce::Time::currentTimeMillis() + 1500;
            repaint();
            return;
        }
        if (nearZeroY && std::abs(e.position.x - highX) <= 9.0f)
        {
            if (auto* ep = state.getParameter("highEnabled")) { ep->beginChangeGesture(); ep->setValueNotifyingHost(ep->getValue() < 0.5f ? 1.0f : 0.0f); ep->endChangeGesture(); }
            lowHighEditUntilMs = juce::Time::currentTimeMillis() + 1500;
            repaint();
            return;
        }
    }

    float d; int i = nearestActivePoint(e.position, d);
    if (d <= 16.0f)
    {
        // Alt/Option + double-click on an EXISTING point resets ONLY its
        // sensitivity (depth) to 0 -- frequency/width/shape/focus and the
        // band's existence are untouched. A plain double-click (no
        // modifier) deletes the band instead (see below).
        if (e.mods.isAltDown())
        {
            if (auto* sp = state.getParameter(sensParamId(i)))
            {
                sp->beginChangeGesture();
                sp->setValueNotifyingHost(sp->convertTo0to1(0.0f));
                sp->endChangeGesture();
            }
            repaint();
            return;
        }
        // Double LEFT-click on an EXISTING point deletes that band (same
        // effect as the right-click "Delete Band" menu item -- active=false
        // only, the slot's own freq/sens/width/shape parameters stay alive).
        setActive(i, false);
        if (selected == i) selected = -1;
        if (hovered == i) hovered = -1;
        if (dragging == i) dragging = -1;
        repaint();
        return;
    }

    // Double-click on EMPTY area: create a new band in the first inactive
    // slot, X = frequency, Y = initial sensitivity, using the same
    // log-frequency mapping as the analyzer.
    int slot = firstInactiveSlot();
    if (slot < 0) { maxBandsFlashUntilMs = juce::Time::currentTimeMillis() + 1200; repaint(); return; }

    auto r = getLocalBounds().toFloat();
    float hz = SpectrumComponent::hzForXIn(r, e.position.x);
    float db = juce::jmap(juce::jlimit(r.getY(), r.getBottom(), e.position.y), r.getBottom(), r.getY(), -12.0f, 12.0f);
    if (auto* fp = state.getParameter(freqParamId(slot))) { fp->beginChangeGesture(); fp->setValueNotifyingHost(fp->convertTo0to1(hz)); fp->endChangeGesture(); }
    if (auto* sp = state.getParameter(sensParamId(slot))) { sp->beginChangeGesture(); sp->setValueNotifyingHost(sp->convertTo0to1(db)); sp->endChangeGesture(); }
    setActive(slot, true);
    selected = slot;
    repaint();
}

void ControlCurveComponent::setFreq(int slot, float hz)
{
    if (auto* fp = state.getParameter(freqParamId(slot))) fp->setValueNotifyingHost(fp->convertTo0to1(hz));
    repaint();
}
void ControlCurveComponent::setSens(int slot, float db)
{
    if (auto* sp = state.getParameter(sensParamId(slot))) sp->setValueNotifyingHost(sp->convertTo0to1(db));
    repaint();
}
void ControlCurveComponent::setActive(int slot, bool active)
{
    if (auto* ap = state.getParameter(activeParamId(slot)))
    {
        ap->beginChangeGesture();
        ap->setValueNotifyingHost(active ? 1.0f : 0.0f);
        ap->endChangeGesture();
    }
    repaint();
}
