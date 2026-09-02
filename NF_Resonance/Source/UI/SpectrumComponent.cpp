#include "SpectrumComponent.h"
// Neon Blue palette (0.1c) + frequency/dB scale (0.1d) + resizable/integrated-
// curve pass (0.1f): plot-area geometry now exposed statically so
// ControlCurveComponent, overlaid directly on top of this component, can
// share the exact same log-frequency x-axis. Analyzer proportions now scale
// with the component's own size (no hardcoded pixel margins beyond small,
// size-relative insets), since the editor is resizable.

juce::Rectangle<float> SpectrumComponent::plotAreaFor(juce::Rectangle<float> full)
{
    const float topLabelH = juce::jmax(12.0f, full.getHeight() * 0.035f);
    const float rightLabelW = juce::jmax(26.0f, full.getWidth() * 0.035f);
    const float margin = juce::jmax(4.0f, full.getWidth() * 0.006f);
    return { full.getX() + margin, full.getY() + topLabelH,
             full.getWidth() - margin - rightLabelW, full.getHeight() - topLabelH - margin };
}
float SpectrumComponent::xForHzIn(juce::Rectangle<float> plot, float hz)
{
    float xn = (std::log10(juce::jmax(20.0f, hz)) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
    return plot.getX() + juce::jlimit(0.0f, 1.0f, xn) * plot.getWidth();
}
float SpectrumComponent::hzForXIn(juce::Rectangle<float> plot, float x)
{
    float xn = plot.getWidth() > 1.0e-6f ? (x - plot.getX()) / plot.getWidth() : 0.0f;
    xn = juce::jlimit(0.0f, 1.0f, xn);
    float logHz = std::log10(20.0f) + xn * (std::log10(20000.0f) - std::log10(20.0f));
    return juce::jlimit(20.0f, 20000.0f, std::pow(10.0f, logHz));
}

void SpectrumComponent::resampleReductionForDisplay(const std::vector<float>& binReductionDb, double sampleRate, int fftSize, int numPts, std::vector<float>& outRedAt)
{
    outRedAt.assign((size_t) juce::jmax(0, numPts), 0.0f);
    if (binReductionDb.size() < 2 || numPts < 2) return;
    const int bins = (int) binReductionDb.size();
    const float engineSr = (float) juce::jmax(1.0, sampleRate);
    const float fftSizeF = (float) juce::jmax(2, fftSize);
    auto binPosForHz = [&](float hz) { return juce::jlimit(0.0f, (float) (bins - 1), hz * fftSizeF / engineSr); };
    auto lerpAtBinPos = [&](float binPos) {
        size_t lo = (size_t) binPos, hi = juce::jmin((size_t) (bins - 1), lo + 1);
        float frac = binPos - (float) lo;
        return binReductionDb[lo] + (binReductionDb[hi] - binReductionDb[lo]) * frac;
    };
    const double lo20 = std::log10(20.0), hi20k = std::log10(20000.0), range = hi20k - lo20;
    auto hzForT = [&](float t) { return (float) std::pow(10.0, lo20 + (double) t * range); };
    const float halfCellT = 0.5f / (float) (numPts - 1);
    for (int k = 0; k < numPts; ++k)
    {
        float t = (float) k / (float) (numPts - 1);
        float tLo = juce::jmax(0.0f, t - halfCellT), tHi = juce::jmin(1.0f, t + halfCellT);
        float binPosCenter = binPosForHz(hzForT(t));
        float binPosLo = binPosForHz(hzForT(tLo)), binPosHi = binPosForHz(hzForT(tHi));
        if (binPosHi - binPosLo < 1.0f)
        {
            outRedAt[(size_t) k] = lerpAtBinPos(binPosCenter);
            continue;
        }
        int iLo = juce::jlimit(0, bins - 1, (int) std::ceil(binPosLo));
        int iHi = juce::jlimit(0, bins - 1, (int) std::floor(binPosHi));
        if (iLo > iHi) { outRedAt[(size_t) k] = lerpAtBinPos(binPosCenter); continue; }
        float worst = binReductionDb[(size_t) iLo];
        for (int b = iLo + 1; b <= iHi; ++b) worst = juce::jmin(worst, binReductionDb[(size_t) b]);
        outRedAt[(size_t) k] = worst;
    }
}

// REDUCTION visual amplification -- DISPLAY ONLY. Never touches DSP,
// GainMaskEngine, Depth, or appliedReductionSnapshot() itself: whatever
// calls this has already finished every real decision (gate threshold,
// valley grouping/splitting, taper) from the true dB values, so what gets
// GATED, GROUPED, and SPLIT is entirely unaffected -- only how deep the
// already-decided shape is DRAWN changes. mag=0 always maps to 0 exactly,
// and the mapping is strictly increasing in mag, so it can never invent a
// valley, flip an ordering, or make a small reduction look deeper than a
// genuinely larger one. visualGain (1.7-2.0 requested; 1.85 chosen as a
// middle value) is a flat multiplier; the mild tanh term adds a bit more
// emphasis specifically to medium/strong reductions (approaching +10%
// extra by ~6-8dB) while leaving small ones close to the flat gain.
static float warpMagnitude(float mag)
{
    const float visualGain = 1.85f;
    if (mag <= 1.0e-6f) return 0.0f;
    float emphasis = 1.0f + 0.10f * std::tanh(mag / 4.0f);
    return mag * visualGain * emphasis;
}
// Exact numerical inverse of warpMagnitude (the tanh term has no closed
// form) -- bisection, 40 iterations (~1e-10 relative precision on a value
// that never exceeds a few dozen dB), UI-thread only (Max Reduction line
// drag), never called from the audio thread.
static float unwarpMagnitude(float targetMag)
{
    if (targetMag <= 1.0e-6f) return 0.0f;
    float lo = 0.0f, hi = 60.0f;
    for (int i = 0; i < 40; ++i)
    {
        float mid = 0.5f * (lo + hi);
        if (warpMagnitude(mid) < targetMag) lo = mid; else hi = mid;
    }
    return 0.5f * (lo + hi);
}

float SpectrumComponent::mapRealReductionDbToDisplayY(juce::Rectangle<float> plot, float realDb)
{
    float displayDb = (realDb >= 0.0f ? 1.0f : -1.0f) * warpMagnitude(std::abs(realDb));
    float y = plot.getCentreY() - displayDb * dbPxPerDbFor(plot);
    return juce::jlimit(plot.getY(), plot.getBottom(), y);
}
float SpectrumComponent::mapDisplayYToRealReductionDb(juce::Rectangle<float> plot, float y)
{
    float displayDb = (plot.getCentreY() - y) / juce::jmax(1.0e-6f, dbPxPerDbFor(plot));
    float sign = displayDb >= 0.0f ? 1.0f : -1.0f;
    return sign * unwarpMagnitude(std::abs(displayDb));
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff101722));
    g.fillRoundedRectangle(full, 7);

    auto plot = plotAreaFor(full);
    auto xForHz = [&](float hz) { return xForHzIn(plot, hz); };

    // Frequency scale: log-spaced gridlines + labels along the top.
    static const float freqs[] = { 20, 50, 100, 250, 500, 1000, 2000, 4000, 8000, 16000, 20000 };
    static const char* freqLabels[] = { "20", "50", "100", "250", "500", "1k", "2k", "4k", "8k", "16k", "20k" };
    g.setColour(juce::Colour(0xff1c2733));
    for (float hz : freqs)
        g.drawVerticalLine((int) xForHz(hz), plot.getY(), plot.getBottom());
    g.setColour(juce::Colour(0xff7f9cb3)); // 0.1e: slightly brighter than 0.1d's 0x5b7086, still secondary
    g.setFont(juce::jlimit(9.0f, 12.0f, plot.getHeight() * 0.028f));
    for (int i = 0; i < 11; ++i)
    {
        float x = xForHz(freqs[i]);
        g.drawText(freqLabels[i], (int) (x - 14.0f), (int) full.getY() + 1, 28, (int) (plot.getY() - full.getY()) - 1, juce::Justification::centred);
    }

    // Reduction dB scale: gridlines + labels down the right edge. Y
    // positions now go through the SAME canonical mapping the cyan curve
    // and the Max Reduction line use (mapRealReductionDbToDisplayY) -- so
    // a "-3" label always sits exactly where a genuine -3dB point actually
    // renders, even though the curve itself is visually amplified. Only
    // the POSITION is warped; the printed number is always the real dB.
    const float centreY = plot.getCentreY(); // still used below as the fill gradient's own anchor (== mapRealReductionDbToDisplayY(plot,0) exactly)
    static const float dbs[] = { 0, -3, -6, -9, -12 };
    g.setColour(juce::Colour(0xff1c2733));
    for (float db : dbs)
    {
        float y = mapRealReductionDbToDisplayY(plot, db);
        if (y >= plot.getY() && y <= plot.getBottom())
            g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }
    g.setColour(juce::Colour(0xff7f9cb3));
    for (float db : dbs)
    {
        float y = mapRealReductionDbToDisplayY(plot, db);
        if (y >= plot.getY() - 5.0f && y <= plot.getBottom() + 5.0f)
            g.drawText(juce::String((int) db), (int) plot.getRight() + 2, (int) (y - 6.0f), (int) (full.getRight() - plot.getRight()) - 4, 12, juce::Justification::centredLeft);
    }

    auto m = engine.getLastSpectrum();
    // Sonic Alpha V2 visual connect: read the REAL, final applied mask from
    // GainMaskEngine's own realtime-safe double-buffer snapshot (post
    // Problem-Confidence -> Selectivity -> Transient Protection -> Depth ->
    // spatial regularization -> temporal smoothing) -- not
    // getLastReduction()'s unsynchronized vector copy, and never a V1/
    // ResonanceDetector buffer (V1's reduction path is no longer wired to
    // the audio at all as of the V2 gain mask swap).
    const auto& redSnap = engine.getAppliedReductionSnapshot();
    int redBinCount = juce::jmin((int) redSnap.size(), engine.getAppliedReductionSnapshotBinCount());
    std::vector<float> red((size_t) juce::jmax(0, redBinCount));
    for (size_t i = 0; i < red.size(); ++i) red[i] = redSnap[i];
    if (m.size() < 2 || red.size() < 2) return;

    // 0.1l: the "water surface" is driven by REDUCTION, not input level --
    // a bin sitting at ~0dB reduction must stay visually still no matter how
    // loud/busy the program material is; only bins the detector is actually
    // reducing should deform. Asymmetric display smoothing: fast attack
    // (deformation appears quickly) but slower release (settles back down
    // more gently) -- both purely visual, independent of the DSP's own
    // attack/release times.
    if (smoothedMagDb.size() != m.size()) { smoothedMagDb = m; smoothedRedDb = red; peakHoldUntilMs.assign(red.size(), 0.0); }
    if (peakHoldUntilMs.size() != red.size()) peakHoldUntilMs.assign(red.size(), 0.0);
    const float origEma = 0.30f;      // ORIGINAL: light symmetric smoothing, reference line only
    const float redAttackEma = 0.70f; // REDUCTION surface: fast/lively when deformation is growing
    const float redReleaseEma = 0.20f; // REDUCTION surface: a touch slower than attack, never frozen-looking
    const double redHoldMs = 40.0;    // short hold before release is allowed to start (25-50ms requested)
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    for (size_t i = 0; i < m.size(); ++i)
    {
        smoothedMagDb[i] += (m[i] - smoothedMagDb[i]) * origEma;
        float target = red[i];
        if (std::abs(target) > std::abs(smoothedRedDb[i]))
        {
            // Attack: fast, and (re)arm the hold window from right now --
            // per-bin, so a fresh, deeper attack always gets its own hold.
            smoothedRedDb[i] += (target - smoothedRedDb[i]) * redAttackEma;
            peakHoldUntilMs[i] = nowMs + redHoldMs;
        }
        else if (nowMs < peakHoldUntilMs[i])
        {
            // Still holding the last real peak at THIS bin -- release hasn't
            // started yet. Purely position-indexed: whatever detected group
            // this bin's reduction came from, and whatever index/
            // classification that group has this frame, never resets this.
        }
        else
        {
            smoothedRedDb[i] += (target - smoothedRedDb[i]) * redReleaseEma;
        }
    }

    // 0.1n: fractional-bin position (not round-to-nearest-bin) so the log-X
    // resample LINEARLY interpolates between the two real neighbouring bins
    // instead of snapping to one -- this is what was producing the blocky/
    // stepped look, especially in the bass where the log axis visually
    // stretches out very few real FFT bins (23-94Hz/bin depending on SR) --
    // a nearest-bin snap there meant many consecutive render points shared
    // one bin's value, then jumped hard to the next. Linear interpolation
    // between the true bin values never exceeds either bin's own value.
    // Bin position must scale with the ENGINE's actual sample rate -- a
    // hardcoded 48000 here silently mis-mapped every bin (and therefore
    // every reduction valley's on-screen frequency) at 44.1/96/192kHz.
    const float engineSr = (float) juce::jmax(1.0, engine.currentSampleRate());
    auto binPosForHz = [&](float hz) { return juce::jlimit(1.0f, (float) (m.size() - 1), hz * (float) (2 * (m.size() - 1)) / engineSr); };
    auto lerpAtBinPos = [](const std::vector<float>& v, float binPos) {
        size_t lo = (size_t) binPos, hi = juce::jmin(v.size() - 1, lo + 1);
        float frac = binPos - (float) lo;
        return v[lo] + (v[hi] - v[lo]) * frac;
    };
    auto yOriginalAt = [&](float binPos) { return plot.getBottom() - juce::jlimit(0.0f, 1.0f, (lerpAtBinPos(smoothedMagDb, binPos) + 90.0f) / 102.0f) * plot.getHeight(); };

    // gateThresholdDb: the reduction magnitude (dB) at which a point's
    // visual weight crosses 0.5 in the SOFT-KNEE curve below -- no longer a
    // hard on/off cutoff (see weightForMag). Recalibrated for the V2 gain
    // mask (Sonic Alpha): V2's own Depth curve is deliberately conservative
    // (Depth=1 typically tops out well under 1dB even in a genuinely
    // treated region -- see GainMaskEngine's own depthToMaxReductionDb), so
    // the OLD V1-era threshold of 1.2dB would hide essentially all of
    // Depth 1-3's real, intentional reduction from the analyzer even though
    // it IS being applied to the audio. 0.15dB is safe as the knee's centre
    // because untouched bins read EXACTLY 0.0dB (Depth=0 is a bit-exact
    // identity, and Depth>0 bins with no real action authority stay at
    // their smoothed target of 0 too) -- there is no ambient dither/noise
    // in the mask that this could mistakenly bring to life.
    const float gateThresholdDb = 0.15f;

    const int numPts = juce::jlimit(48, 220, (int) plot.getWidth() / 4);
    std::vector<float> xAt(numPts), rawRedAt;
    std::vector<juce::Point<float>> origPts;
    origPts.reserve((size_t) numPts);
    for (int k = 0; k < numPts; ++k)
    {
        float t = (float) k / (float) (numPts - 1);
        float logHz = std::log10(20.0f) + t * (std::log10(20000.0f) - std::log10(20.0f));
        float hz = std::pow(10.0f, logHz);
        float binPos = binPosForHz(hz);
        xAt[(size_t) k] = xForHz(hz);
        origPts.push_back({ xAt[(size_t) k], yOriginalAt(binPos) });
    }
    // REDUCTION audit fix: min-preserving (most-negative-real-bin-wins)
    // downsample instead of single-point linear interpolation -- see
    // resampleReductionForDisplay()'s own header doc. Fixes narrow real
    // notches (Detail=10 especially) silently falling between two sample
    // points and vanishing from the display, without inventing/scaling any
    // value -- every point is still a real bin value or a lerp of two.
    resampleReductionForDisplay(smoothedRedDb, engine.currentSampleRate(), 2 * ((int) smoothedRedDb.size() - 1), numPts, rawRedAt);

    // 0.1p: light spatial smoothing in log-frequency FIRST (small symmetric
    // kernel) -- removes single-point noise/teeth without erasing real
    // shape, and gives the valley-splitting below a clean profile to work
    // on. Still bounded: a moving average of real values can't exceed the
    // real local max.
    const float octavesPerPoint = (std::log10(20000.0f) - std::log10(20.0f)) / std::log10(2.0f) / (float) (numPts - 1);
    auto oct2pts = [&](float oct) { return juce::jmax(1, (int) std::round(oct / octavesPerPoint)); };
    const int smoothRadius = juce::jmax(1, oct2pts(0.01f));
    std::vector<float> smAt((size_t) numPts, 0.0f);
    for (int k = 0; k < numPts; ++k)
    {
        double sum = 0.0; int n = 0;
        for (int j = juce::jmax(0, k - smoothRadius); j <= juce::jmin(numPts - 1, k + smoothRadius); ++j) { sum += rawRedAt[(size_t) j]; ++n; }
        smAt[(size_t) k] = (float) (sum / n);
    }

    // SOFT-KNEE continuous envelope -- replaces the old hard active[]/gate +
    // gap-bridged coarse spans + peak-detect/non-max-suppression valley
    // splitting + per-region independent taper entirely. A point's own
    // reduction magnitude alone decides its visual weight, smoothly: no
    // boolean ever flips on/off at the threshold (the old source of frame-
    // to-frame flicker when a bin hovered right at gateThresholdDb), and
    // every point across the whole numPts range gets a real weighted value
    // -- never a hard 0 for an "inactive" point -- so there is no seam or
    // hole for a single continuous path to cross. weight(0)=0 exactly (an
    // untouched bin still reads exactly 0dB display), weight(gateThresholdDb)
    // = 0.5, weight -> 1 as |mag| grows well past the knee. Genuinely quiet
    // stretches naturally settle to ~0dB display (weight -> 0) by the data
    // itself, with no explicit span/region bookkeeping needed to keep them
    // there or to fade them out.
    auto weightForMag = [gateThresholdDb](float mag) {
        float m2 = mag * mag;
        return m2 / (m2 + gateThresholdDb * gateThresholdDb);
    };
    std::vector<float> envAt((size_t) numPts, 0.0f);
    for (int k = 0; k < numPts; ++k)
    {
        float mag = smAt[(size_t) k];
        envAt[(size_t) k] = mag * weightForMag(std::abs(mag));
    }

    // Monotonic/bounded cubic through the resampled points: Catmull-Rom
    // control points clamped to each segment's own [min(y0,y1),max(y0,y1)]
    // range, so the rendered curve NEVER overshoots past a real sampled
    // value -- rounded valleys instead of straight-line facets, but the
    // depth of a valley can't be rendered deeper than the real data says.
    auto buildBoundedPath = [](const std::vector<juce::Point<float>>& pts) -> juce::Path
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
    };

    // ORIGINAL: infrastructure preserved (still computed/smoothed above) but
    // NOT drawn by default -- it was reading as a continuously "dancing"
    // background regardless of what's actually being corrected, which is
    // exactly the clutter this pass removes. Flip this on for a future
    // user-facing toggle; nothing else needs to change to re-enable it.
    // 0.1r: user-toggleable via the FFT button (PluginEditor), OFF by
    // default -- infrastructure above (smoothedMagDb, origPts) is always
    // computed regardless, so toggling ON never causes a first-frame pop.
    const bool showOriginal = showOriginalFftParam != nullptr && showOriginalFftParam->load() > 0.5f;
    if (showOriginal)
    {
        auto origPath = buildBoundedPath(origPts);
        // Filled spectrum: dark purple, translucent, between the real FFT
        // curve and the bottom of the plot -- never above the curve, never
        // touching REDUCTION's cyan or the Sensitivity Curve's white. Own
        // hue/identity (not copied from any reference plugin), subtle
        // enough to read as texture, not compete with REDUCTION.
        {
            juce::Path fillPath(origPath);
            fillPath.lineTo(origPts.back().x, plot.getBottom());
            fillPath.lineTo(origPts.front().x, plot.getBottom());
            fillPath.closeSubPath();
            juce::ColourGradient purpleGrad(juce::Colour(0xff6a3fb0).withAlpha(0.16f), 0, plot.getY(),
                                             juce::Colour(0xff6a3fb0).withAlpha(0.03f), 0, plot.getBottom(), false);
            g.setGradientFill(purpleGrad);
            g.fillPath(fillPath);
        }
        g.setColour(juce::Colour(0xff8fb8d9).withAlpha(0.38f));
        g.strokePath(origPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // REDUCTION water surface: ONE continuous path/fill across the entire
    // [20Hz,20kHz] width, built from envAt[] (soft-knee weighted, see
    // above) for every one of the numPts points -- never split into
    // independent per-group closed paths. A genuinely quiet stretch reads
    // as a smooth, near-flat approach to 0dB (weight -> 0) rather than a
    // seam, hole, or an abrupt vertical edge where a region used to start/
    // stop; nothing is invented across distant real peaks, since envAt[]
    // between them is still each point's own real (weighted) value.
    {
        std::vector<juce::Point<float>> allPts;
        allPts.reserve((size_t) numPts);
        for (int k = 0; k < numPts; ++k)
        {
            // Canonical mapping applied HERE, after the soft-knee weighting
            // already ran on the real envAt[] values above -- display only,
            // and the SAME function the gridlines and the Max Reduction
            // line use, so the curve can never visually appear to cross a
            // limit it hasn't actually reached. Already clamped to the
            // plot's own inner bounds internally.
            float y = mapRealReductionDbToDisplayY(plot, envAt[(size_t) k]);
            allPts.push_back({ xAt[(size_t) k], y });
        }
        auto envPath = buildBoundedPath(allPts);
        juce::Path fillPath(envPath);
        float zeroY = mapRealReductionDbToDisplayY(plot, 0.0f);
        fillPath.lineTo(allPts.back().x, zeroY);
        fillPath.lineTo(allPts.front().x, zeroY);
        fillPath.closeSubPath();
        juce::ColourGradient grad(juce::Colour(0xff27d8ff).withAlpha(0.30f), 0, centreY,
                                   juce::Colour(0xff27d8ff).withAlpha(0.10f), 0, plot.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(fillPath);
        g.setColour(juce::Colour(0xff27d8ff));
        g.strokePath(envPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // RESONANCES: architecture prep only -- no data exists yet (that's
    // ResonanceMapSnapshot / V2-B/V2-C), so nothing is drawn here. When it
    // exists, region.confidence should drive this layer's opacity, smoothly,
    // never a hard on/off threshold.
}
