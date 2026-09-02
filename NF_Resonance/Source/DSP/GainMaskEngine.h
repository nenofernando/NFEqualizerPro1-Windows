#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include "SpectralProminenceEngineV5.h"
#include "LowFrequencyHarmonicAnalyzer.h"
#include "ConfidenceEngine.h"
#include "TransientProtectionEngine.h"
#include "ResonanceDetector.h"

// FIRST V2 SONIC GAIN MASK -- Sonic Alpha. Wires PHYSICAL C (ConfidenceEngine's
// problemConfidence / Selectivity actionWeight) and PHYSICAL D
// (TransientProtectionEngine's transientProtection) into an actual per-bin
// gain-reduction curve for the FIRST time -- neither ConfidenceEngine nor
// TransientProtectionEngine is modified anywhere in this class, only
// consumed. Method C / prominence / soft admission / top-K / Region
// Continuation / dual-source rescue / harmonic reasoning / Existence-
// Problem Confidence / Selectivity / F0-classification reliability /
// transientEvidence-transientProtection are ALL exactly what PHYSICAL C/D
// already validated -- this class adds nothing to that decision layer,
// it only converts the decision into a gain curve.
//
// action = SelectivityActionWeight * (1 - transientProtection)   [per region]
// regionPeakReductionDb = -depthToMaxReductionDb(depth) * action  [<=0]
// -- a smooth, log-frequency-local envelope around EACH active region's own
// measured centerHz/widthHz (never a global/diffuse cut), regions combined
// per-bin via the most-negative value (not summed, so overlapping small
// resonances never stack into a broad EQ-like cut), then a light 3-bin
// spatial regularization pass, then a per-bin causal attack/release
// envelope in dB (so a discontinuous problemConfidence jump -- the
// documented PHYSICAL C corner case -- is absorbed here rather than
// reaching the gain directly).
//
// Depth == 0 is an EXACT, unconditional identity: every region's
// regionPeakReductionDb is 0 regardless of action, so the raw target mask
// is 0 at every bin, every frame -- a plugin that starts at Depth=0 and is
// never touched produces bit-exact 0dB output (verified by the Depth-0
// unity/null test).
//
// Zero heap allocation, zero locks, no new thread once prepare() has run --
// every buffer is a std::vector sized ONCE and reused every frame.
class GainMaskEngine
{
public:
    // std::atomic<int> below (the UI double-buffer index) has no move/copy
    // constructor, which would otherwise implicitly delete GainMaskEngine's
    // own move ops -- and SpectralEngine::prepare() move-constructs Chan
    // (which owns a GainMaskEngine) when resizing its channel vector. This
    // move happens only during prepare(), before any audio/UI activity, so
    // it's safe to just reset the atomic index to 0 rather than preserve its
    // value; everything else moves normally.
    GainMaskEngine() = default;
    GainMaskEngine(GainMaskEngine&& o) noexcept
        : sampleRate(o.sampleRate), fftSize(o.fftSize), hopSize(o.hopSize), bins(o.bins),
          depth(o.depth), selectivity(o.selectivity), actionShapeGamma(o.actionShapeGamma), attackCoeff(o.attackCoeff), releaseCoeff(o.releaseCoeff),
          lowHz(o.lowHz), highHz(o.highHz), detail(o.detail),
          maxReductionEnabled(o.maxReductionEnabled), maxReductionCapTargetDb(o.maxReductionCapTargetDb), maxReductionCapSmoothedDb(o.maxReductionCapSmoothedDb), capSmoothCoeff(o.capSmoothCoeff),
          prom(std::move(o.prom)), aux(std::move(o.aux)), conf(std::move(o.conf)), trans(std::move(o.trans)),
          promOut(std::move(o.promOut)), rawTargetDb(std::move(o.rawTargetDb)), regularizedDb(std::move(o.regularizedDb)), smoothedDb(std::move(o.smoothedDb)),
          prefixScratch(std::move(o.prefixScratch)),
          lastActiveContributing(o.lastActiveContributing), regionDebug(o.regionDebug), uiReductionBuffers(o.uiReductionBuffers),
          sensBandFreq(o.sensBandFreq), sensBandSens(o.sensBandSens), sensBandWidth(o.sensBandWidth), sensBandFocus(o.sensBandFocus), sensBandActive(o.sensBandActive), sensBandShape(o.sensBandShape) {}
    GainMaskEngine& operator=(GainMaskEngine&& o) noexcept
    {
        sampleRate=o.sampleRate; fftSize=o.fftSize; hopSize=o.hopSize; bins=o.bins;
        depth=o.depth; selectivity=o.selectivity; actionShapeGamma=o.actionShapeGamma; attackCoeff=o.attackCoeff; releaseCoeff=o.releaseCoeff;
        lowHz=o.lowHz; highHz=o.highHz; detail=o.detail;
        maxReductionEnabled=o.maxReductionEnabled; maxReductionCapTargetDb=o.maxReductionCapTargetDb; maxReductionCapSmoothedDb=o.maxReductionCapSmoothedDb; capSmoothCoeff=o.capSmoothCoeff;
        prom=std::move(o.prom); aux=std::move(o.aux); conf=std::move(o.conf); trans=std::move(o.trans);
        promOut=std::move(o.promOut); rawTargetDb=std::move(o.rawTargetDb); regularizedDb=std::move(o.regularizedDb); smoothedDb=std::move(o.smoothedDb);
        prefixScratch=std::move(o.prefixScratch);
        lastActiveContributing=o.lastActiveContributing; regionDebug=o.regionDebug; uiReductionBuffers=o.uiReductionBuffers;
        sensBandFreq=o.sensBandFreq; sensBandSens=o.sensBandSens; sensBandWidth=o.sensBandWidth; sensBandFocus=o.sensBandFocus; sensBandActive=o.sensBandActive; sensBandShape=o.sensBandShape;
        uiActiveBufferIndex.store(0, std::memory_order_relaxed);
        return *this;
    }
    GainMaskEngine(const GainMaskEngine&) = delete;
    GainMaskEngine& operator=(const GainMaskEngine&) = delete;

    void prepare(double sampleRate, int fftSize, int hopSize);
    void reset();

    // depth: 0..10 (matches the existing "depth" APVTS parameter range
    // unchanged by this checkpoint). selectivity: 0..10, same convention
    // PHYSICAL C's own Selectivity already uses. attackMs/releaseMs: reuses
    // the EXISTING "attack"/"release" APVTS parameters (already in the UI,
    // not new) -- attack shapes how fast reduction can ENGAGE, release how
    // fast it recovers, exactly the semantics those knobs already carry.
    // lowHz/highHz: reuses the EXISTING low/high range gate (0/1e9 when
    // that side is disabled, matching how ResonanceDetector already
    // receives this pair) -- a region whose centerHz falls outside the
    // gated range gets no reduction, same behaviour users already expect
    // from those controls.
    void setParams(float depth, float selectivity, float attackMs, float releaseMs, float lowHz, float highHz);

    // SONIC ALPHA CALIBRATION 1 -- shapes ONLY the actionWeight -> gain
    // mapping (still the same PHYSICAL C/D actionWeight input, still the
    // same Depth-derived ceiling, still 0->0dB and 1->ceiling exactly).
    // gamma<1 gives a concave curve (shapedAction = action^gamma) that
    // raises intermediate action values toward the ceiling faster than
    // linear, without ever changing the two endpoints -- more musical
    // authority for "moderately confident" regions, no new hard knee, no
    // change to PHYSICAL C/D. gamma==1 reproduces the original linear
    // mapping exactly (the FIRST Alpha's behaviour).
    void setActionShapeGamma(float g) { actionShapeGamma = juce::jlimit(0.05f, 4.0f, g); }

    // WHITE SENSITIVITY CURVE integration (Sonic Alpha V2 -- Band Bias).
    // The white curve is NOT EQ -- it is LOCAL DETECTOR SENSITIVITY: 0dB =
    // normal V2 sensitivity, above 0 = more sensitive in that region, below
    // 0 = less sensitive. Reuses ResonanceDetector::combinedSensitivityAt()
    // verbatim -- the exact same math the UI curve renders, so the drawn
    // shape and what actually shifts action authority can never disagree.
    // Called once per frame (same cadence as setParams()); fixed-size
    // arrays, zero allocation.
    void setSensitivityCurve(const float bandFreq[ResonanceDetector::kMaxBands], const float bandSens[ResonanceDetector::kMaxBands], const float bandWidth[ResonanceDetector::kMaxBands], const int bandShape[ResonanceDetector::kMaxBands], const float bandFocus[ResonanceDetector::kMaxBands], const bool bandActive[ResonanceDetector::kMaxBands]);

    // DETAIL -- mask granularity/regularization, applied AFTER the raw
    // per-region gain mask and BEFORE temporal attack/release smoothing
    // (never touches ConfidenceEngine/TransientProtectionEngine/gamma/
    // Depth). 0..10, matching Depth/Selectivity's own convention. Controls
    // an octave-CONSISTENT (not fixed-bin) spectral smoothing half-width
    // applied to the raw mask -- low Detail widens it (nearby resonances
    // aggregate into one broader, shallower dip: mean-smoothing can only
    // ever make a dip SHALLOWER, never deeper, so this can't increase
    // overall reduction energy), high Detail narrows it toward the raw,
    // fully localized per-region mask (small independent valleys preserved).
    // Detail=5 is calibrated to reproduce the pre-Detail 3-bin regularization
    // baseline (Sonic Alpha Calibration 1) closely. Same physical octave
    // width at any sample rate (44.1/48/96/192kHz), unlike a fixed bin count.
    void setDetail(float d) { detail = juce::jlimit(0.0f, 10.0f, d); }

    // MAX REDUCTION -- a real ceiling on how much any bin can be reduced,
    // independent of Depth/Selectivity/Detail/resonance strength. NOT EQ,
    // NOT makeup gain, NOT another Depth: a hard floor on the reduction
    // value itself (finalReductionDb >= -maxReductionDb), applied AFTER
    // Detail's spectral regularization and BEFORE temporal attack/release
    // smoothing, with a second identical clamp on the final post-smoothing
    // value as a defensive guarantee (attack/release EMA already can't
    // overshoot past its own target, so this second pass is normally a
    // no-op -- it exists so the ceiling is a structural guarantee, not an
    // assumption about the smoother's own behaviour). When disabled, this
    // stage is skipped entirely (not merely set to a very large ceiling),
    // so OFF is bit-exactly today's DSP, not "practically identical".
    // capDb is itself smoothed toward its target (NOT the reduction
    // values) so moving the Max Reduction control live never clicks --
    // same per-hop EMA cadence as attack/release, own fixed ~30ms time
    // constant (independent of the user's own Attack/Release knobs, which
    // shape DETECTION response time, not this control's own movement).
    void setMaxReduction(bool enabled, float capDbValue) { maxReductionEnabled = enabled; maxReductionCapTargetDb = juce::jlimit(0.5f, 12.0f, capDbValue); }

    // One call per host hop (same cadence SpectralEngine::frame() already
    // runs at). magDb: the SAME host-rate raw magnitude array
    // SpectralEngine already computed for this frame -- reused, not
    // recomputed. hopSamples/hopCount: this CHANNEL's own raw time-domain
    // samples for the hop that just completed (needed by
    // LowFrequencyHarmonicAnalyzer, which is causal and wants real audio,
    // not just the spectral frame). Writes reductionDb (<=0 always,
    // Depth==0 => all exactly 0), same [-x,0] convention
    // SpectralEngine::frame() already expects from ResonanceDetector.
    void process(const std::vector<float>& magDb, const float* hopSamples, int hopCount, std::vector<float>& reductionDbOut);

    // Diagnostic-only: how many ConfidenceEngine regions were active and
    // contributing non-zero action this frame (for reporting, not used by
    // any decision).
    int lastActiveContributingRegions() const { return lastActiveContributing; }

    // ---- Realtime-safe UI snapshot (Sonic Alpha V2 visual connect) ----
    // The REDUCTION analyzer must show the exact final mask actually applied
    // to the audio (post Problem-Confidence -> Selectivity -> Transient
    // Protection -> Depth -> spatial regularization -> temporal smoothing),
    // not an earlier estimate and not a V1 buffer. process() already
    // computes exactly that in `smoothedDb` -- this just publishes it to the
    // UI thread safely: written into whichever of two fixed buffers is NOT
    // currently exposed, then an atomic index flip makes it visible in one
    // step. No allocation, no lock, no change to what's returned via
    // reductionDbOut (the actual applied gain) -- this is purely an
    // additional, read-only copy for display.
    static constexpr int kUIBins = 1025; // fftSize/2+1 for fftSize=2048 -- fixed regardless of sample rate in this codebase
    const std::array<float, kUIBins>& appliedReductionSnapshot() const { return uiReductionBuffers[(size_t) uiActiveBufferIndex.load(std::memory_order_acquire)]; }
    int appliedReductionBinCount() const { return bins; }

    // ---- Calibration-only diagnostics (SONIC ALPHA CALIBRATION 1) ----
    // Per-region audit of the RAW inputs to the mapping being calibrated --
    // actionWeight (straight from PHYSICAL C's Selectivity), transientProt
    // (straight from PHYSICAL D), the combined action, and the REQUESTED
    // peak reduction at that region's own centre (before spatial/temporal
    // shaping) -- so the actionWeight distribution can be audited directly
    // against real material, never guessed. Diagnostic-only, not used by
    // process() itself beyond being filled for reporting.
    struct RegionActionDebug { bool active = false; float centerHz = 0, actionWeight = 0, transientProt = 0, action = 0, requestedPeakDb = 0, sensitivityDb = 0; };
    static constexpr int kMaxDebugRegions = ConfidenceEngine::kMaxRegions;
    const std::array<RegionActionDebug, kMaxDebugRegions>& lastRegionActionDebug() const { return regionDebug; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048, hopSize = 512, bins = 1025;
    float depth = 0.0f, selectivity = 3.5f;
    // SONIC ALPHA CALIBRATION 1: 0.4 (was 1.0, the FIRST Alpha's plain
    // linear mapping). Chosen from CalibrationAudit's own gamma sweep
    // (0.4/0.55/0.7/1.0) against real actionWeight distributions measured
    // on Vocal/Guitar/Dense-mix material at Selectivity=3.5 -- monotonic
    // improvement at every step tested, endpoints (action=0->0dB,
    // action=1->Depth ceiling) unchanged by construction. Bass's own P50
    // actionWeight (~0.001) stays negligible under this gamma too
    // (0.001^0.4=0.063), so the low-frequency-fundamental protection this
    // session's whole PHYSICAL C effort established is not eroded.
    float actionShapeGamma = 0.4f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float lowHz = 20.0f, highHz = 20000.0f;
    float detail = 5.0f;
    bool maxReductionEnabled = false;
    float maxReductionCapTargetDb = 3.0f;   // user-facing target, 0.5..12
    float maxReductionCapSmoothedDb = 3.0f; // ramps toward the target, click-free
    float capSmoothCoeff = 0.0f;            // computed in setParams() from sampleRate/hop

    SpectralProminenceEngineV5 prom;
    LowFrequencyHarmonicAnalyzer aux;
    ConfidenceEngine conf;
    TransientProtectionEngine trans;

    std::vector<float> promOut;      // reused prominence scratch
    std::vector<float> rawTargetDb;  // this frame's un-smoothed, un-regularized mask (the DETAIL "fine" anchor)
    std::vector<float> regularizedDb; // after DETAIL's octave-consistent spatial pass
    std::vector<float> smoothedDb;   // after the temporal attack/release envelope -- what's returned
    std::vector<float> prefixScratch; // DETAIL scratch: cumulative sum for O(bins) octave-window averaging

    int lastActiveContributing = 0;
    std::array<RegionActionDebug, kMaxDebugRegions> regionDebug{};

    mutable std::array<std::array<float, kUIBins>, 2> uiReductionBuffers{};
    mutable std::atomic<int> uiActiveBufferIndex{0};

    // White Sensitivity Curve band data (copied from Params each frame,
    // zero allocation). All-inactive by default -- combinedSensitivityAt()
    // then returns exactly 0 (neutral), matching pre-integration behaviour
    // for anyone who never touches the curve.
    std::array<float, ResonanceDetector::kMaxBands> sensBandFreq{}, sensBandSens{}, sensBandWidth{}, sensBandFocus{};
    std::array<bool, ResonanceDetector::kMaxBands> sensBandActive{};
    std::array<int, ResonanceDetector::kMaxBands> sensBandShape{};

    float binToHz(int bin) const { return (float) (bin * sampleRate / fftSize); }
    static float depthToMaxReductionDb(float depthValue) { return juce::jmax(0.0f, depthValue) * 0.9f; } // provisional, conservative -- see PHYSICAL/GainMask checkpoint report, NOT a final calibration

    // DETAIL: octave-consistent box-mean smoothing of `src` into `dst`
    // (same array, in place is NOT safe -- always a distinct destination),
    // half-width `halfWidthOct` octaves at every bin regardless of its
    // frequency or the sample rate. O(bins) via prefixScratch (a cumulative
    // sum), zero allocation once prepare() has sized it.
    void octaveSmooth(const std::vector<float>& src, std::vector<float>& dst, float halfWidthOct);
    // DETAIL 0->10 maps to an octave half-width via piecewise-geometric
    // interpolation through 3 anchors (wide/aggregated at 0, the
    // Calibration-1-matching baseline at 5, minimal/near-raw at 10) --
    // continuous and monotonic, no hard switches. See setDetail().
    static float detailToOctHalfWidth(float detailValue);
};
