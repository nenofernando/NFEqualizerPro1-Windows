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
    lastActiveContributing = 0;
    for (auto& buf : uiReductionBuffers) buf.fill(0.0f);
}

void GainMaskEngine::setParams(float depthIn, float selectivityIn, float attackMs, float releaseMs, float lowHzIn, float highHzIn)
{
    depth = depthIn; selectivity = selectivityIn; lowHz = lowHzIn; highHz = highHzIn;
    double hopMs = 1000.0 * (double) hopSize / sampleRate;
    attackCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) attackMs));
    releaseCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) releaseMs));
}

void GainMaskEngine::process(const std::vector<float>& magDb, const float* hopSamples, int hopCount, std::vector<float>& reductionDbOut)
{
    // ---- PHYSICAL C / D, byte-identical call pattern to their own validated harnesses ----
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
            d.transientProt = trans.transientProtectionFor(r.centerHz);
            d.action = juce::jlimit(0.0f, 1.0f, d.actionWeight * (1.0f - d.transientProt));
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

            // SONIC ALPHA CALIBRATION 1 -- the ONLY thing being calibrated:
            // shapedAction = action^gamma (gamma=1 reproduces the original
            // linear FIRST-Alpha mapping exactly). Concave (gamma<1) raises
            // intermediate action values toward the Depth ceiling faster
            // than linear -- endpoints action=0->0 and action=1->ceiling
            // are UNCHANGED by construction (0^g=0, 1^g=1 for any g>0), so
            // this is a smooth re-shaping, not a new knee/threshold and not
            // a blanket multiplier.
            float shapedAction = std::pow(action, actionShapeGamma);
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

    // Light spatial regularization (3-bin box average in dB) -- smooths
    // bin-to-bin discontinuities that would otherwise ring on inverse-FFT,
    // WITHOUT touching the region-local envelope shape already applied
    // above (this is not Detail -- just enough to avoid a jagged mask).
    for (int b = 0; b < bins; ++b)
    {
        float sum = rawTargetDb[(size_t) b]; int cnt = 1;
        if (b > 0) { sum += rawTargetDb[(size_t) (b - 1)]; ++cnt; }
        if (b < bins - 1) { sum += rawTargetDb[(size_t) (b + 1)]; ++cnt; }
        regularizedDb[(size_t) b] = sum / (float) cnt;
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
