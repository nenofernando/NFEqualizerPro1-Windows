#include "GainMaskEngine.h"
#include <cmath>
#include <algorithm>

void GainMaskEngine::prepare(double sr, int fft, int hop)
{
    sampleRate = sr; fftSize = fft; hopSize = hop; bins = fft / 2 + 1;
    prom.prepare(bins, sampleRate, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope); // Method C default, UNCHANGED
    aux.prepare(sampleRate);
    conf.prepare(sampleRate, fftSize, hopSize);
    trans.prepare(sampleRate, fftSize, hopSize);

    promOut.assign((size_t) bins, 0.0f);
    rawTargetDb.assign((size_t) bins, 0.0f);
    regularizedDb.assign((size_t) bins, 0.0f);
    smoothedDb.assign((size_t) bins, 0.0f);
    prefixScratch.assign((size_t) bins + 1, 0.0f);

    setParams(depth, selectivity, 10.0f, 80.0f, lowHz, highHz); // provisional defaults, overwritten by the host's actual attack/release on the first real setParams() call
}

void GainMaskEngine::reset()
{
    conf.reset();
    aux.reset();
    trans.reset();
    std::fill(rawTargetDb.begin(), rawTargetDb.end(), 0.0f);
    std::fill(regularizedDb.begin(), regularizedDb.end(), 0.0f);
    std::fill(smoothedDb.begin(), smoothedDb.end(), 0.0f);
    maxReductionCapSmoothedDb = maxReductionCapTargetDb; // no stale ramp across a reset
    lastActiveContributing = 0;
    for (auto& buf : uiReductionBuffers) buf.fill(0.0f);
}

void GainMaskEngine::setParams(float depthIn, float selectivityIn, float attackMs, float releaseMs, float lowHzIn, float highHzIn)
{
    depth = depthIn; selectivity = selectivityIn; lowHz = lowHzIn; highHz = highHzIn;
    double hopMs = 1000.0 * (double) hopSize / sampleRate;
    attackCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) attackMs));
    releaseCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) releaseMs));
    // MAX REDUCTION cap smoothing: fixed ~30ms time constant, independent
    // of the user's own Attack/Release (those shape detection response,
    // not this control's own movement) -- just enough to make a live knob
    // move click-free without adding audible lag to the ceiling itself.
    const double capSmoothMs = 30.0;
    capSmoothCoeff = (float) std::exp(-hopMs / capSmoothMs);
}

void GainMaskEngine::setSensitivityCurve(const float bandFreq[ResonanceDetector::kMaxBands], const float bandSens[ResonanceDetector::kMaxBands], const float bandWidth[ResonanceDetector::kMaxBands], const int bandShape[ResonanceDetector::kMaxBands], const float bandFocus[ResonanceDetector::kMaxBands], const bool bandActive[ResonanceDetector::kMaxBands])
{
    std::copy(bandFreq, bandFreq + ResonanceDetector::kMaxBands, sensBandFreq.begin());
    std::copy(bandSens, bandSens + ResonanceDetector::kMaxBands, sensBandSens.begin());
    std::copy(bandWidth, bandWidth + ResonanceDetector::kMaxBands, sensBandWidth.begin());
    std::copy(bandShape, bandShape + ResonanceDetector::kMaxBands, sensBandShape.begin());
    std::copy(bandFocus, bandFocus + ResonanceDetector::kMaxBands, sensBandFocus.begin());
    std::copy(bandActive, bandActive + ResonanceDetector::kMaxBands, sensBandActive.begin());
}

// Smooth, continuous, bounded remapping of action authority by local
// sensitivity: shifts action in the LOGIT domain (never a hard threshold,
// never a jump) so action=0 stays exactly 0 (logit(0)=-inf, any bounded
// shift keeps it -inf -> sigmoid=0 -- the curve can never manufacture
// reduction where PHYSICAL C found no evidence) and action=1 stays exactly
// 1 by the same symmetry. kSensGainPerDb is deliberately modest: PHYSICAL
// C's own confidence remains the primary authority, the curve only nudges it.
static float applySensitivity(float baseAction, float sensitivityDb)
{
    float a = juce::jlimit(1.0e-4f, 1.0f - 1.0e-4f, baseAction);
    float logit = std::log(a / (1.0f - a));
    const float kSensGainPerDb = 0.18f;
    float shifted = logit + sensitivityDb * kSensGainPerDb;
    return juce::jlimit(0.0f, 1.0f, 1.0f / (1.0f + std::exp(-shifted)));
}

// DETAIL: octave-consistent box-mean smoothing, O(bins) via a cumulative
// sum (prefixScratch) so the cost is independent of the window's own
// width. Each bin's own window is derived from ITS OWN log-frequency
// position (not a fixed bin count), so the same halfWidthOct produces the
// same physical octave span at 44.1/48/96/192kHz alike -- only the NUMBER
// of bins spanning that window changes with sample rate/bin resolution,
// never the musical width itself.
void GainMaskEngine::octaveSmooth(const std::vector<float>& src, std::vector<float>& dst, float halfWidthOct)
{
    prefixScratch[0] = 0.0f;
    for (int i = 0; i < bins; ++i) prefixScratch[(size_t) i + 1] = prefixScratch[(size_t) i] + src[(size_t) i];
    const float mult = std::pow(2.0f, juce::jmax(0.0f, halfWidthOct));
    const double hzPerBin = sampleRate / (double) fftSize;
    for (int b = 0; b < bins; ++b)
    {
        float hz = binToHz(b);
        float loHz = juce::jmax(1.0f, hz / mult);
        float hiHz = hz * mult;
        int loBin = juce::jlimit(0, bins - 1, (int) std::floor((double) loHz / hzPerBin));
        int hiBin = juce::jlimit(0, bins - 1, (int) std::ceil((double) hiHz / hzPerBin));
        if (hiBin < loBin) hiBin = loBin;
        int count = hiBin - loBin + 1;
        dst[(size_t) b] = (prefixScratch[(size_t) hiBin + 1] - prefixScratch[(size_t) loBin]) / (float) count;
    }
}

float GainMaskEngine::detailToOctHalfWidth(float detailValue)
{
    // Anchors: Detail=0 -> wide/aggregated (nearby resonances merge into
    // one broader, shallower dip). Detail=5 -> the Sonic Alpha Calibration
    // 1 baseline (matches the old fixed 3-bin box average closely -- see
    // Tests/DetailCheck.cpp's Detail=5-vs-pre-Detail comparison). Detail=10
    // -> minimal/near-raw, just enough of a floor to avoid audible ringing
    // on inverse-FFT from a fully unsmoothed per-bin mask. Piecewise-
    // geometric interpolation between adjacent anchors is smooth,
    // continuous and monotonic -- no hard switches at any Detail value.
    const float kWideOct = 0.40f, kBaselineOct = 0.033f, kNarrowOct = 0.006f;
    float d = juce::jlimit(0.0f, 10.0f, detailValue);
    if (d <= 5.0f) { float t = d / 5.0f; return kWideOct * std::pow(kBaselineOct / kWideOct, t); }
    float t = (d - 5.0f) / 5.0f; return kBaselineOct * std::pow(kNarrowOct / kBaselineOct, t);
}

void GainMaskEngine::process(const std::vector<float>& magDb, const float* hopSamples, int hopCount, std::vector<float>& reductionDbOut)
{
    // ---- PHYSICAL C / D, byte-identical call pattern to their own validated harnesses ----
    // Detection stays on the ORIGINAL fixed 4.0f -- Sharpness's effect on
    // width now lives entirely in the deterministic sigmaOct multiplier
    // below (a closed-form, provably monotonic function of `sharpness`
    // alone). An earlier version of this fix instead varied THIS argument
    // with `sharpness`, routing width control through PHYSICAL C's own
    // peak-width detection (ConfidenceEngine, frozen/validated) -- real,
    // and correctly directional end to end, but not GUARANTEED monotonic
    // point-to-point (measured: a small non-monotonic wiggle around
    // Sharpness 6-8-10, traced to detection-layer width estimation, not to
    // this class). The audit required strict, click-proof monotonicity
    // with zero reversals anywhere in 0..10, so PHYSICAL C's own detection
    // is now left completely untouched by Sharpness (same fixed 4.0f as
    // pre-fix), and only the already-downstream, already-per-region
    // Gaussian envelope width is scaled.
    prom.computeProminence(magDb, 4.0f, promOut);
    if (hopSamples != nullptr && hopCount > 0) aux.pushSamples(hopSamples, hopCount);
    conf.process(promOut, &aux, &magDb);
    trans.process(magDb);

    // Diagnostic audit (SONIC ALPHA CALIBRATION 1): record actionWeight/
    // transientProt/action/requestedPeakDb for EVERY region this frame
    // (active or not), regardless of Depth -- so the real distribution can
    // be audited even while probing Depth=0, and so a calibration harness
    // never needs to guess what the mapping's own INPUT looks like.
    {
        int idx = 0;
        for (auto& r : conf.regions())
        {
            auto& d = regionDebug[(size_t) idx++];
            d = RegionActionDebug{};
            if (! r.active) continue;
            d.active = true; d.centerHz = r.centerHz;
            d.actionWeight = ConfidenceEngine::actionWeight(r.existenceConfidence, r.reliableProblemEvidence, r.unknownAnomalySupport, selectivity);
            // TRANSIENT knob: rawTransientProtection (PHYSICAL D's own,
            // completely unmodified measurement) is kept separately for
            // diagnostics; only its AUTHORITY over action is rescaled by
            // transientAmount before the (1-x) action-dampening step.
            d.transientProt = trans.transientProtectionFor(r.centerHz);
            d.effectiveTransientProt = juce::jlimit(0.0f, 1.0f, d.transientProt * transientAmount);
            d.action = juce::jlimit(0.0f, 1.0f, d.actionWeight * (1.0f - d.effectiveTransientProt));
        }
    }

    // Depth==0 fast-exact-path: never even look at regions, guarantees a
    // bit-exact 0dB raw target regardless of any detector state.
    const float maxReductionDb = depthToMaxReductionDb(depth);
    if (maxReductionDb <= 0.0f)
    {
        std::fill(rawTargetDb.begin(), rawTargetDb.end(), 0.0f);
        lastActiveContributing = 0;
    }
    else
    {
        std::fill(rawTargetDb.begin(), rawTargetDb.end(), 0.0f);
        lastActiveContributing = 0;
        int regionIdx = -1;
        for (auto& r : conf.regions())
        {
            ++regionIdx;
            if (! r.active) continue;
            if (r.centerHz < lowHz || r.centerHz > highHz) continue;

            float action = regionDebug[(size_t) regionIdx].action; // already computed above, SAME raw actionWeight/transientProt PHYSICAL C/D produced
            if (action <= 1.0e-4f) continue;
            ++lastActiveContributing;

            // WHITE SENSITIVITY CURVE: local detector-sensitivity modulation
            // of action authority, evaluated at THIS region's own centerHz
            // via the exact same math the UI curve renders. Skipped (exact
            // identity) when the curve is flat/neutral at this frequency --
            // zero drift from Calibration 1's own validated numbers for
            // anyone who never touches the curve.
            float sensDb = ResonanceDetector::combinedSensitivityAt(r.centerHz, sensBandFreq.data(), sensBandSens.data(), sensBandWidth.data(), sensBandShape.data(), sensBandFocus.data(), sensBandActive.data());
            regionDebug[(size_t) regionIdx].sensitivityDb = sensDb;
            float effectiveAction = (std::abs(sensDb) < 1.0e-6f) ? action : applySensitivity(action, sensDb);

            // SONIC ALPHA CALIBRATION 1 -- shapedAction = effectiveAction^gamma
            // (gamma=1 reproduces the original linear FIRST-Alpha mapping
            // exactly). Concave (gamma<1) raises intermediate action values
            // toward the Depth ceiling faster than linear -- endpoints
            // effectiveAction=0->0 and effectiveAction=1->ceiling are
            // UNCHANGED by construction (0^g=0, 1^g=1 for any g>0), so this
            // is a smooth re-shaping, not a new knee/threshold and not a
            // blanket multiplier.
            float shapedAction = std::pow(effectiveAction, actionShapeGamma);
            regionDebug[(size_t) regionIdx].requestedPeakDb = -maxReductionDb * shapedAction;
            float regionPeakDb = -maxReductionDb * shapedAction; // <=0
            // Local, log-frequency-Gaussian envelope around THIS region's
            // own measured centerHz/widthHz -- never a global cut. sigma
            // derived directly from the region's own physically-measured
            // width (half the local-max span already found by detectPeaks),
            // not a new arbitrary parameter.
            float halfWidthHz = juce::jmax(1.0f, r.widthHz * 0.5f);
            float loEdgeHz = juce::jmax(1.0f, r.centerHz - halfWidthHz);
            float hiEdgeHz = r.centerHz + halfWidthHz;
            float sigmaOct = juce::jmax(0.08f, 0.5f * std::log2(hiEdgeHz / loEdgeHz));
            // SHARPNESS width scaling -- direct control of how far THIS
            // region's own already-computed peak reduction spreads around
            // its own already-computed centerHz, pivoted exactly at
            // Sharpness=4 (multiplier==1.0 there, so the official default
            // reproduces bit-identical envelope width to the pre-Sharpness-
            // fix DSP, per the audit's own requirement). Deliberately NOT
            // routed through regionPeakDb/maxReductionDb/centerBin -- those
            // stay completely untouched by this line, so peak reduction
            // magnitude and center frequency can never move with Sharpness,
            // only the falloff shape can. Complements (does not replace)
            // the existing SpectralProminenceEngineV5 narrow/medium/broad
            // blend (setSharpness() above): that reshapes what the detector
            // considers prominent in the first place; this reshapes the
            // resulting envelope directly, giving a musically clear,
            // guaranteed-monotonic width range end to end (0=widest,
            // 10=narrowest) instead of relying solely on the blend's own,
            // more diffuse effect on measured region width.
            const float sharpnessWidthMult = std::pow(2.0f, 1.3f * (4.0f - sharpness) / 10.0f);
            sigmaOct = juce::jmax(0.03f, sigmaOct * sharpnessWidthMult);

            int centerBin = juce::jlimit(0, bins - 1, (int) std::round((double) r.centerHz / (sampleRate / fftSize)));
            // Only walk bins within ~4 sigma -- negligible contribution beyond that, keeps this O(local) not O(bins) per region.
            float logCenter = std::log2(juce::jmax(1.0f, r.centerHz));
            int halfSpanBins = juce::jlimit(1, bins - 1, (int) std::ceil((double) (sigmaOct * 4.0f) / std::log2(1.0 + (sampleRate / fftSize) / juce::jmax(1.0, (double) r.centerHz))));
            for (int b = juce::jmax(0, centerBin - halfSpanBins); b <= juce::jmin(bins - 1, centerBin + halfSpanBins); ++b)
            {
                float f = binToHz(b); if (f < 1.0f) continue;
                float d = (std::log2(f) - logCenter) / sigmaOct;
                float falloff = std::exp(-0.5f * d * d);
                float contribution = regionPeakDb * falloff; // <=0
                // Most-negative wins -- never summed, so overlapping small
                // resonances can't stack into a broad EQ-like cut.
                rawTargetDb[(size_t) b] = juce::jmin(rawTargetDb[(size_t) b], contribution);
            }
        }
    }

    // DETAIL -- octave-consistent spatial regularization. rawTargetDb is
    // the fully-localized mask (per-region Gaussian envelopes, most-
    // negative combine, untouched above). Mean-smoothing it over a WIDER
    // octave window can only ever move a bin's value TOWARD zero (never
    // past the deepest actual value already present in that window), so
    // widening Detail's window can never increase overall reduction energy
    // -- only dilute/merge nearby valleys into one broader, shallower one.
    octaveSmooth(rawTargetDb, regularizedDb, detailToOctHalfWidth(detail));

    // MAX REDUCTION -- hard ceiling on the TARGET mask, applied after
    // Detail and before temporal smoothing (per the approved pipeline
    // order). Not EQ, not makeup gain, not another Depth: a floor on the
    // reduction value itself, independent of what produced it. capDb
    // itself ramps toward its target (never the reduction values) so a
    // live knob move is click-free. Skipped entirely when disabled -- OFF
    // is bit-exactly the pre-Max-Reduction DSP, not merely equivalent.
    if (maxReductionEnabled)
    {
        maxReductionCapSmoothedDb += (maxReductionCapTargetDb - maxReductionCapSmoothedDb) * capSmoothCoeff;
        const float floorDb = -maxReductionCapSmoothedDb;
        for (int b = 0; b < bins; ++b) regularizedDb[(size_t) b] = juce::jmax(regularizedDb[(size_t) b], floorDb);
    }

    // Causal per-bin attack/release envelope in dB (log-gain domain) --
    // absorbs any single-frame jump in the underlying decision (including
    // the documented PHYSICAL C problemConfidence discontinuity) into a
    // smooth transition. target MORE negative (more reduction) uses
    // attackCoeff (engage fast); target LESS negative (recovering toward
    // 0dB) uses releaseCoeff.
    for (int b = 0; b < bins; ++b)
    {
        float target = regularizedDb[(size_t) b];
        float coeff = (target < smoothedDb[(size_t) b]) ? attackCoeff : releaseCoeff;
        smoothedDb[(size_t) b] = target + (smoothedDb[(size_t) b] - target) * coeff;
    }

    // Defensive final clamp: the attack/release EMA above can't overshoot
    // past its own already-capped target, so this is normally a no-op --
    // it exists so the ceiling is a structural guarantee on the ACTUALLY
    // APPLIED value, not an assumption about the smoother's own behaviour.
    if (maxReductionEnabled)
    {
        const float floorDb = -maxReductionCapSmoothedDb;
        for (int b = 0; b < bins; ++b) smoothedDb[(size_t) b] = juce::jmax(smoothedDb[(size_t) b], floorDb);
    }

    if ((int) reductionDbOut.size() != bins) reductionDbOut.assign((size_t) bins, 0.0f);
    std::copy(smoothedDb.begin(), smoothedDb.end(), reductionDbOut.begin());

    // Realtime-safe UI snapshot: write the SAME final applied mask into the
    // buffer NOT currently exposed, then flip the atomic index. Never
    // touches reductionDbOut/the audio path above -- read-only publication.
    {
        int writeIdx = 1 - uiActiveBufferIndex.load(std::memory_order_relaxed);
        auto& buf = uiReductionBuffers[(size_t) writeIdx];
        int n = juce::jmin((int) buf.size(), bins);
        for (int i = 0; i < n; ++i) buf[(size_t) i] = smoothedDb[(size_t) i];
        for (int i = n; i < (int) buf.size(); ++i) buf[(size_t) i] = 0.0f;
        uiActiveBufferIndex.store(writeIdx, std::memory_order_release);
    }
}
