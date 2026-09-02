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

    // Reduction dB scale: gridlines + labels down the right edge.
    const float dbPxPerDb = plot.getHeight() * 0.045f; // scales with analyzer height, was a fixed 6px
    const float centreY = plot.getCentreY();
    static const float dbs[] = { 0, -3, -6, -9, -12 };
    g.setColour(juce::Colour(0xff1c2733));
    for (float db : dbs)
    {
        float y = centreY - db * dbPxPerDb;
        if (y >= plot.getY() && y <= plot.getBottom())
            g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }
    g.setColour(juce::Colour(0xff7f9cb3));
    for (float db : dbs)
    {
        float y = centreY - db * dbPxPerDb;
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
    if (smoothedMagDb.size() != m.size()) { smoothedMagDb = m; smoothedRedDb = red; }
    const float origEma = 0.30f;      // ORIGINAL: light symmetric smoothing, reference line only
    const float redAttackEma = 0.55f; // REDUCTION surface: fast when deformation is growing
    const float redReleaseEma = 0.14f; // REDUCTION surface: slower when settling back toward 0
    for (size_t i = 0; i < m.size(); ++i)
    {
        smoothedMagDb[i] += (m[i] - smoothedMagDb[i]) * origEma;
        float target = red[i];
        float coeff = std::abs(target) > std::abs(smoothedRedDb[i]) ? redAttackEma : redReleaseEma;
        smoothedRedDb[i] += (target - smoothedRedDb[i]) * coeff;
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

    // 0.1m/0.1o: SELECTIVE visual activity gate. gateThresholdDb is the
    // MINIMUM real reduction magnitude (dB) a point must have to count as
    // "active" for the region grouping below -- uses ONLY the real
    // reductionDb already computed by the mask, no invented data.
    // Recalibrated for the V2 gain mask (Sonic Alpha): V2's own Depth curve
    // is deliberately conservative (Depth=1 typically tops out well under
    // 1dB even in a genuinely treated region -- see GainMaskEngine's own
    // depthToMaxReductionDb), so the OLD V1-era threshold of 1.2dB would
    // hide essentially all of Depth 1-3's real, intentional reduction from
    // the analyzer even though it IS being applied to the audio. 0.15dB
    // is safe as a noise floor because untouched bins read EXACTLY 0.0dB
    // (Depth=0 is a bit-exact identity, and Depth>0 bins with no real
    // action authority stay at their smoothed target of 0 too) -- there is
    // no ambient dither/noise in the mask to gate away.
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

    // Gate on the SMOOTHED profile (stabler than raw): a point counts as
    // "active" only above gateThresholdDb.
    const int maxGapPoints = oct2pts(0.06f);   // bridge only genuinely tiny gaps (~0.06 octave)
    const int taperPoints = oct2pts(0.02f);    // smooth corner width at each region's TRUE outer edge
    const int minPeakSepPoints = oct2pts(0.08f); // two local maxima closer than this are treated as one hump, not split
    const float splitRecoverFrac = 0.55f;      // a saddle recovering to <=55% of the smaller flanking peak's depth is a real gap between resonances
    std::vector<bool> active((size_t) numPts);
    for (int k = 0; k < numPts; ++k) active[(size_t) k] = std::abs(smAt[(size_t) k]) > gateThresholdDb;

    struct Span { int start, end; }; // inclusive point-index range, gap-bridged
    std::vector<Span> coarse;
    for (int k = 0; k < numPts; )
    {
        if (! active[(size_t) k]) { ++k; continue; }
        int start = k, end = k;
        while (true)
        {
            int next = end + 1;
            while (next < numPts && ! active[(size_t) next] && (next - end) <= maxGapPoints) ++next;
            if (next < numPts && active[(size_t) next] && (next - end) <= maxGapPoints) end = next;
            else break;
        }
        coarse.push_back({ start, end });
        k = end + 1;
    }

    // VALLEY SPLITTING: a coarse span can still contain several genuinely
    // distinct resonances (e.g. 90/180/320Hz) that never individually drop
    // below the gate -- find local peaks, and split the span at any saddle
    // between two well-separated peaks that recovers far enough back toward
    // 0dB. This is what stops a whole 50-500Hz stretch from becoming one
    // rectangular block: three real dips now become three regions.
    std::vector<Span> regions;
    for (auto& c : coarse)
    {
        std::vector<int> peaks;
        for (int k = c.start; k <= c.end; ++k)
        {
            float v = std::abs(smAt[(size_t) k]);
            if (v <= gateThresholdDb) continue;
            bool isPeak = (k == c.start || v >= std::abs(smAt[(size_t) (k - 1)])) && (k == c.end || v >= std::abs(smAt[(size_t) (k + 1)]));
            if (isPeak) peaks.push_back(k);
        }
        // Non-max suppression: keep only peaks separated by >= minPeakSepPoints.
        std::vector<int> kept;
        for (int p : peaks)
        {
            if (kept.empty() || (p - kept.back()) >= minPeakSepPoints) kept.push_back(p);
            else if (std::abs(smAt[(size_t) p]) > std::abs(smAt[(size_t) kept.back()])) kept.back() = p;
        }
        if (kept.size() <= 1) { regions.push_back(c); continue; }
        std::vector<int> splitPoints;
        for (size_t i = 0; i + 1 < kept.size(); ++i)
        {
            int a = kept[i], b = kept[i + 1];
            int valleyIdx = a; float valleyMag = std::abs(smAt[(size_t) a]);
            for (int k = a; k <= b; ++k) { float v = std::abs(smAt[(size_t) k]); if (v < valleyMag) { valleyMag = v; valleyIdx = k; } }
            float smallerPeak = juce::jmin(std::abs(smAt[(size_t) a]), std::abs(smAt[(size_t) b]));
            if (valleyMag <= gateThresholdDb || valleyMag <= splitRecoverFrac * smallerPeak) splitPoints.push_back(valleyIdx);
        }
        int segStart = c.start;
        for (int sp : splitPoints) { regions.push_back({ segStart, sp }); segStart = sp + 1; }
        regions.push_back({ segStart, c.end });
    }

    // Render each final region from its REAL smoothed profile -- never a
    // single constant mean/max across the whole width -- with a taper only
    // at the region's own true outer edges (a split point is already a real
    // near-baseline value from the data itself, so it needs no forced taper).
    std::vector<float> envAt((size_t) numPts, 0.0f);
    for (auto& r : regions)
    {
        int len = r.end - r.start;
        int taper = juce::jmin(taperPoints, juce::jmax(1, len / 2));
        for (int k = r.start; k <= r.end; ++k)
        {
            float w = 1.0f;
            int distStart = k - r.start, distEnd = r.end - k;
            if (distStart < taper) { float u = (float) distStart / (float) taper; w = juce::jmin(w, u * u * (3.0f - 2.0f * u)); }
            if (distEnd < taper) { float u = (float) distEnd / (float) taper; w = juce::jmin(w, u * u * (3.0f - 2.0f * u)); }
            envAt[(size_t) k] = smAt[(size_t) k] * w;
        }
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

    // REDUCTION water surface: drawn ONLY inside the gated `regions` --
    // never as one continuous path across the whole width. Previously the
    // cyan stroke ran edge-to-edge at every point, including the ~0dB-
    // reduction stretches, where it sat exactly on the 0dB gridline as a
    // permanent bright cyan bar competing with real valleys. Now cyan
    // exists ONLY where GainMaskEngine is actually reducing something; the
    // already-present discreet 0dB gridline (drawn earlier, same dark grey
    // as -3/-6/-9/-12) is the only thing marking 0dB elsewhere.
    for (auto& r : regions)
    {
        if (r.end <= r.start) continue;
        std::vector<juce::Point<float>> regPts;
        regPts.reserve((size_t) (r.end - r.start + 1));
        for (int k = r.start; k <= r.end; ++k)
            regPts.push_back({ xAt[(size_t) k], centreY - envAt[(size_t) k] * dbPxPerDb });
        auto regPath = buildBoundedPath(regPts);
        juce::Path fillPath(regPath);
        fillPath.lineTo(regPts.back().x, centreY);
        fillPath.lineTo(regPts.front().x, centreY);
        fillPath.closeSubPath();
        juce::ColourGradient grad(juce::Colour(0xff27d8ff).withAlpha(0.30f), 0, centreY,
                                   juce::Colour(0xff27d8ff).withAlpha(0.10f), 0, plot.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(fillPath);
        g.setColour(juce::Colour(0xff27d8ff));
        g.strokePath(regPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // RESONANCES: architecture prep only -- no data exists yet (that's
    // ResonanceMapSnapshot / V2-B/V2-C), so nothing is drawn here. When it
    // exists, region.confidence should drive this layer's opacity, smoothly,
    // never a hard on/off threshold.
}
