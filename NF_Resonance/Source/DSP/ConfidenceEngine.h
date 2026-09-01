#pragma once
#include <JuceHeader.h>
#include <array>
#include "LowFrequencyHarmonicAnalyzer.h"

// PHYSICAL C: prominence -> evidence features -> confidence. DIAGNOSTIC
// ONLY -- nothing here touches gain/reduction. Answers, per tracked
// region: "is this a persistent, stable, non-harmonic anomaly, or is it
// legitimate musical content (a sustained note, a harmonic partial, a
// transient)?"
//
// Zero heap allocation, zero locks, fixed-size preallocated region pool --
// safe to call once per frame on the audio thread (still diagnostic-only
// for now; the same real-time discipline is required regardless, since
// PHYSICAL D/E will eventually read this same state).
//
// Region tracking: each frame's prominence array is scanned for local
// peaks above a generous (not final-decision) floor, grouped into
// contiguous regions, then matched by log-frequency proximity against
// regions already being tracked from previous frames. A match updates that
// region's persistence (rises) and stability (log-freq history); an
// unmatched tracked region's persistence decays; a region whose persistence
// decays near zero is freed back to the pool. New peaks with no match take
// a free slot if one exists (capacity is fixed -- see kMaxRegions).
class ConfidenceEngine
{
public:
    static constexpr int kMaxRegions = 32;
    static constexpr int kStabilityHistoryLen = 8;

    // C2.3e diagnostic-only tag (never read by decision logic): where a
    // candidate's most recent detection came from. MAIN = main's own
    // unmodified topology; AUX_RESCUE = a candidate main's own topology had
    // nothing at all for, admitted from the aux analyzer's own local
    // maxima; a MERGE (aux contributing evidence to an existing MAIN
    // candidate) keeps source=Main but is visible via lastAuxRescueAuthority>0.
    enum class CandidateSource { Main = 0, AuxRescue = 1 };

    struct Region
    {
        bool active = false;
        int centerBin = 0;
        float centerHz = 0.0f;      // sub-bin estimated (see C1.1) -- what everything else uses
        float rawBinHz = 0.0f;      // the un-interpolated bin center, kept only for diagnostics
        float peakProminenceDb = 0.0f;
        int widthBins = 0;
        float widthHz = 0.0f;
        float integratedEvidenceDb = 0.0f; // sum of prominence over the region's bins, one frame
        // PHYSICAL C2.2: continuous admission evidence (smoothstep between
        // lowFloorDb/strongFloorDb), used both as the "how strong is this
        // candidate" signal for top-K pool priority AND matched this frame
        // (used by the priority score too -- a candidate that just got
        // re-matched this frame counts for a small continuity bonus under
        // Policy P1, see priorityScore()).
        float candidateEvidence = 0.0f;
        bool matchedThisFrame = false;
        float persistence = 0.0f;          // 0..1, smoothed (see C2)
        float stability = 0.0f;            // 0..1, 1 = stays in the same log-frequency neighbourhood (see C3)
        float harmonicLikelihood = 0.0f;   // 0..1, continuous (see C1.2) -- never a binary in/out-of-tolerance decision
        float harmonicExpectedHz = 0.0f;   // diagnostic: the nearest-integer-multiple-of-f0 frequency this was compared against (0 if no f0 candidate existed)
        float harmonicDistanceCents = 0.0f;// diagnostic: signed distance from harmonicExpectedHz, in cents
        float confidence = 0.0f;           // 0..1, final combination (see C8)
        // Full decomposition of the last computeConfidence() call for this
        // region -- diagnostic only, lets tests/reporting show exactly why
        // a region scored what it scored (PHYSICAL C, item 3).
        float lastProminenceEvidence = 0.0f, lastPersistenceEvidence = 0.0f, lastStabilityEvidence = 0.0f, lastWidthEvidence = 0.0f, lastHarmonicPenalty = 1.0f;
        // PHYSICAL C2: baseProblemEvidence (the weighted combination of
        // prominence/persistence/stability/width, BEFORE any harmonic
        // reasoning is applied) kept explicit and separate from confidence,
        // per the closure requirement that reliability must not silently
        // collapse the base evidence toward a neutral guess.
        float lastBaseEvidence = 0.0f;
        // Single source of harmonic truth: below auxCrossoverLowHz this is
        // (almost) entirely LowFrequencyHarmonicAnalyzer's own likelihood/
        // reliability; above auxCrossoverHighHz it is entirely this class's
        // own host-rate multi-candidate F0 reasoning (harmonicLikelihood
        // above, implicitly reliability=1 since that reasoning was already
        // validated at frequencies with adequate main-STFT resolution); in
        // between, a smoothstep blend so there is no discontinuity exactly
        // at the crossover. auxHarmonicLikelihood/auxReliability are the
        // raw values read from the aux analyzer for this region's centerHz,
        // BEFORE blending -- kept for diagnostics.
        float auxHarmonicLikelihood = 0.0f, auxReliability = 0.0f;
        float effectiveLikelihood = 0.0f, effectiveReliability = 0.0f;
        // C2.3j -- non-diluting harmonic protection fusion. Per-source
        // protection evidence, combined via max() (never averaged): an
        // uncertain/unconvincing source can never REDUCE protection already
        // earned by a confident one (UNKNOWN != NON-HARMONIC). Diagnostic
        // fields, but structuralHarmonicProtection (not a blended average)
        // is what effectiveHarmonicProtection is now derived from.
        float mainProtectionEvidence = 0.0f, auxProtectionEvidence = 0.0f, structuralHarmonicProtection = 0.0f;
        // C2.3k -- EXISTENCE vs PROBLEM confidence. existenceConfidence
        // ("does this structure really exist and persist?") is the OLD
        // combinedEvidence formula, unchanged, kept explicit as its own
        // named field (was only reachable via lastBaseEvidence before).
        // problemConfidence ("is there evidence this should be treated?")
        // is what `confidence` now holds and what selectivity/passWeight
        // consumes -- persistence/stability alone can no longer drive this
        // near 1.0 for a harmonically-confident, non-excessive fundamental.
        // harmonicContextReliability: whichever source (main/aux) actually
        // determined the harmonic verdict this frame (winner-take-most, not
        // blended) -- diagnostic, shows which source "spoke" for this region.
        float existenceConfidence = 0.0f;
        float harmonicContextReliability = 0.0f;
        // C2.3k-R2: per-source non-harmonic evidence (non-diluting max, item
        // A), the two AFFIRMATIVE/attributable problem sources combined
        // (reliableProblemEvidence), and the renamed UNKNOWN-context
        // fallback (unknownAnomalySupport, was strongIndependentAnomalyEvidence)
        // whose AUTHORITY over ACTION (not evidence) is Selectivity-
        // dependent -- see actionWeight().
        // C2.3k-R3: root (n=1) membership evidence and the two sources'
        // structural membership (max(likelihood, rootMembership)), plus the
        // noise-robust F0 classification reliability used to gate rootMembership
        // and mainNonHarmonicEvidence (see ConfidenceEngine::harmonicSupportCoherence).
        float rootMembership = 0.0f, mainStructuralMembership = 0.0f, auxStructuralMembership = 0.0f;
        float mainRootMembership = 0.0f, auxRootMembership = 0.0f;
        float f0ClassificationReliability = 0.0f, auxClassificationReliability = 0.0f;
        float mainNonHarmonicEvidence = 0.0f, auxNonHarmonicEvidence = 0.0f;
        float nonHarmonicSupportEvidence = 0.0f, excessiveHarmonicEvidence = 0.0f;
        float reliableProblemEvidence = 0.0f, unknownAnomalySupport = 0.0f;
        float problemDecisionEvidence = 0.0f, problemConfidence = 0.0f;
        // excessFactor: progressively discounts protection for a peak whose
        // prominence is anomalously high for a normal harmonic partial
        // (item 4 -- "harmônico excessivo deve receber proteção parcial").
        // RELATIVE, not absolute: excess is measured against the mean
        // prominence of this region's harmonic siblings (other regions
        // matched as harmonics of the same f0 this frame), never against a
        // fixed dB constant -- a fixed absolute ceiling discounted normal,
        // loud-but-not-excessive harmonics too (measured directly: broke
        // the already-validated 120fund<170nonharm ordering during C2
        // integration testing). harmonicSiblingRefDb/Valid are the raw
        // reference used, kept for diagnostics.
        float harmonicSiblingRefDb = 0.0f;
        bool harmonicSiblingRefValid = false;
        float excessFactor = 1.0f;
        // effectiveHarmonicProtection = effectiveLikelihood * effectiveReliability
        // * excessFactor. This is what actually discounts confidence (via
        // harmonicPenalty) -- reliability scales how much the harmonic
        // protection TERM is trusted, it does NOT pull baseProblemEvidence
        // toward a neutral 0.5 (that lerp-to-0.5 shape was diagnostic-only,
        // from the Blocker 2 test harness, and is explicitly NOT used here).
        float effectiveHarmonicProtection = 0.0f;
        // C2.3e diagnostics-only (never read by any decision logic):
        // where this region's most recent detection came from, and (if
        // MERGED/AUX_RESCUE contributed this frame) the rescue authority
        // that was computed for it.
        CandidateSource lastCandidateSource = CandidateSource::Main;
        float lastAuxRescueAuthority = 0.0f;
        // C2.3h region continuation bridge (diagnostics + gating, see
        // continuationBridge() below): true if THIS frame kept the region
        // alive via physically-measured-but-not-a-fresh-local-max evidence,
        // rather than a normal detectPeaks() match.
        bool lastBridged = false;
        // Internal tracking state (not part of the public "result" but
        // kept in the same struct so the whole pool stays one fixed-size
        // array -- no separate parallel bookkeeping structure).
        int framesPresent = 0;
        int framesAbsent = 0;
        std::array<float, kStabilityHistoryLen> logHzHistory{};
        int historyCount = 0;
    };

    // Rise/fall persistence time constants, in FRAMES (not ms) -- converted
    // internally once prepare() knows the real hop/sampleRate, so the
    // smoothing behaves consistently regardless of sample rate. Defaults
    // chosen so a genuinely sustained region reaches high persistence over
    // several tens of ms (see C2's own T63/T90/T95 report), while a single
    // 1-frame spike decays back down quickly once it stops recurring.
    void setPersistenceTimeConstants(float riseFrames, float fallFrames) { riseTau = juce::jmax(0.1f, riseFrames); fallTau = juce::jmax(0.1f, fallFrames); }
    void setStabilityToleranceOctaves(float oct) { stabilityToleranceOct = juce::jmax(0.01f, oct); }
    // C1.2: continuous harmonic likelihood = gaussian(distanceCents, sigma) *
    // resolutionConfidenceCap(binWidthCents). sigma is a FIXED estimate of
    // genuine sub-bin-estimator residual noise (not resolution-scaled --
    // that's the whole point: coarser resolution must lower the achievable
    // CEILING, never widen the acceptance window). resolutionConfidenceCap
    // shrinks as the relevant bin gets wider (in cents), so a bass region
    // at 192kHz (where one bin can be ~500+ cents) can never claim full
    // harmonic certainty even for a dead-on cents match.
    void setHarmonicSigmaCents(float cents) { harmonicSigmaCents = juce::jmax(1.0f, cents); }
    void setHarmonicToleranceCents(float cents) { harmonicToleranceCents = juce::jmax(1.0f, cents); } // kept for compatibility; superseded by the gaussian/cap model below
    void setHarmonicMaxPenalty(float p) { harmonicMaxPenalty = juce::jlimit(0.0f, 1.0f, p); } // never allow confidence to be forced to exactly 0 by harmonic reasoning alone
    void setPeakFloorDb(float db) { peakFloorDb = db; strongFloorDb = db; } // kept for compatibility: also drives the soft-admission "full evidence" ceiling below
    // PHYSICAL C2.2: soft candidate admission (replaces the old hard
    // `prominence > peakFloorDb` cliff). A local maximum between lowFloorDb
    // and strongFloorDb gets a CONTINUOUS candidateEvidence in [0,1]
    // (smoothstep), rather than a binary yes/no at one fixed dB value --
    // closes the "5.99dB=nothing, 6.00dB=region" discontinuity found during
    // the C2.1/C2.2 investigation. continuationFloorDb is the (lower)
    // hysteresis threshold: an ALREADY-tracked region only needs to clear
    // this to keep rising in persistence, avoiding creation/destruction
    // flicker for a region that dips slightly. All three provisional --
    // approved as a starting calibration, not final tuning.
    void setSoftAdmissionFloors(float lowFloorDbIn, float strongFloorDbIn, float continuationFloorDbIn)
    {
        lowFloorDb = lowFloorDbIn; strongFloorDb = juce::jmax(lowFloorDb + 0.1f, strongFloorDbIn); continuationFloorDb = juce::jlimit(lowFloorDb, strongFloorDb, continuationFloorDbIn);
        peakFloorDb = strongFloorDb;
    }

    // PHYSICAL C2: crossover between the LowFrequencyHarmonicAnalyzer's
    // auxiliary context (below lowHz) and this class's own host-rate
    // multi-candidate F0 reasoning (above highHz), smoothstep-blended in
    // between so there is no confidence/likelihood discontinuity at any
    // single frequency. Provisional defaults (~600/900Hz) -- swept and
    // validated at Blocker/C2 closure, not final production tuning.
    void setAuxCrossover(float lowHz, float highHz) { auxCrossoverLowHz = juce::jmax(1.0f, lowHz); auxCrossoverHighHz = juce::jmax(auxCrossoverLowHz + 1.0f, highHz); }
    // Excess-above-siblings range (in dB, RELATIVE to the mean prominence
    // of this region's harmonic siblings, never an absolute dB value --
    // item 4): a harmonic within ceilingDb of its siblings' average keeps
    // full protection eligibility; ceilingDb+rangeDb above that average
    // discounts eligibility to 0. Provisional constants.
    void setExcessProminenceParams(float ceilingDb, float rangeDb) { excessProminenceCeilingDb = ceilingDb; excessProminenceRangeDb = juce::jmax(1.0f, rangeDb); }

    void prepare(double sampleRate, int fftSize, int hopSize);
    void reset();

    // PHYSICAL C2.3e -- DUAL-SOURCE CANDIDATE RESCUE (replaces the C2.3
    // bin-by-bin prominence blend, which was PROVEN -- via a direct
    // before/after local-maxima dump -- to alter main's own peak topology:
    // creating local maxima that didn't exist in main's raw prominence and
    // erasing ones that did, which then cascaded into different F0
    // selection/harmonic-likelihood results even at frequencies main was
    // already getting right. That architecture is retired; do not
    // reintroduce a pre-detectPeaks() bin-by-bin blend.
    //
    // New architecture: main's own detectPeaks() runs COMPLETELY UNCHANGED
    // on main's own unmodified prominence array -- main's peak topology,
    // region tracking, F0 selection and Policy A are byte-identical to
    // before C2.3 whenever this isn't called. Separately, this scans the
    // aux analyzer's OWN local maxima (in ITS OWN decimated domain,
    // location from ITS OWN raw magnitude) and either:
    //   MERGE   -- an aux candidate close (log-frequency) to an
    //              already-detected main candidate contributes evidence to
    //              that SAME region (never a second, competing region for
    //              the same physical peak), and may steer that region's
    //              location toward aux's own estimate ONLY when aux's own
    //              physical/frequency consistency is confirmed AND main's
    //              own is not (item 6) -- main's own detected peak value is
    //              never retroactively altered.
    //   AUX_RESCUE -- a frequency where main's topology found NOTHING (not
    //              "found something with low prominence" -- genuinely no
    //              local max there) but aux found a real, physically
    //              consistent candidate, in the range where aux has actual
    //              resolution advantage. Admitted into the SAME
    //              detectedScratch buffer main's own candidates already
    //              occupy, so it goes through the identical soft-admission/
    //              top-K/tracking/Policy-A pipeline as everything else --
    //              no separate code path downstream.
    // auxRescueAuthority (continuous, no hardcoded SR/frequency branch):
    //   frequencyWeight x resolutionAdvantageWeight x auxPhysicalConsistency
    //   x auxProminenceReliability x (1 - mainPhysicalConsistencyAtThatSpot)
    // -- the last term is what makes this a RESCUE, not a replacement: aux
    // only gains real authority where main's own topology is structurally
    // inconsistent there (or has nothing at all), never where main is
    // already confidently correct.
    // Call AFTER aux.pushSamples() for this frame, BEFORE process(). magDb
    // is the same host-rate raw magnitude array the caller already
    // computed to derive `prominence`.
    void setProminenceAssistCrossover(float lowHz, float highHz) { promCrossoverLowHz = juce::jmax(1.0f, lowHz); promCrossoverHighHz = juce::jmax(promCrossoverLowHz + 1.0f, highHz); }

    // PHYSICAL C2.3h -- REGION CONTINUATION BRIDGE. Ground-truth-validated
    // (C2.3g): at 44.1/48kHz, a genuinely persistent narrow-band resonance
    // (measured directly, both for the original beating-burst signal AND a
    // physically stable noise-driven bandpass resonator control) can fail
    // detectPeaks()'s strict local-maximum test on individual frames even
    // though the underlying physical structure -- and the prominence array's
    // OWN value at that location -- is still clearly present (measured
    // physical local/context ratio was statistically IDENTICAL between
    // detector-active and detector-absent frames: 14.02dB vs 13.97dB). That
    // flicker resets persistence/candidateEvidence and can let a legitimate,
    // still-present resonance lose to a merely-well-tracked harmonic
    // fundamental. This does NOT touch detectPeaks() or the prominence
    // topology at all -- it runs strictly in the tracker, AFTER normal
    // matching, and ONLY for an already-active region that received no
    // detection this frame. Time budget in ms (not a fixed frame count, so
    // it means the same physical duration at every sample rate) --
    // 100ms default chosen from C2.3g's own measured gap distribution
    // (max observed gap across both the beating-burst and stable-resonator
    // signals was 96ms; 100ms is the smallest round number at or above
    // that measured maximum).
    void setContinuationBridgeTimeMs(float ms) { bridgeMaxTimeMs = juce::jmax(0.0f, ms); }

    // One call per frame. `prominence` is V2-A5C's own output (same array
    // computeProminence() fills) -- this class never recomputes prominence
    // itself, only reasons about it. `aux`, if supplied, is queried for
    // low-frequency harmonic context (see setAuxCrossover); if null, this
    // class falls back to its own host-rate-only harmonic reasoning at
    // every frequency (unchanged from before C2 -- preserves the C1
    // regression baseline exactly when no aux analyzer is wired up).
    // magDb: optional, same host-rate raw magnitude array the caller
    // derived `prominence` from. NOTE (C2.3e): this does NOT change how
    // main's own detected peaks are located or blended -- main's topology
    // (detectPeaks() on the raw, unmodified prominence array) is always
    // byte-identical regardless of magDb/aux. When magDb AND aux are BOTH
    // supplied, it is used ONLY to run the dual-source rescue pass (see the
    // class comment above detectPeaks/rescue helpers): aux's own local
    // maxima are merged into or added alongside main's candidates, in a
    // SEPARATE pass, after main's own topology is already fixed. Supplying
    // aux without magDb still enables Policy A harmonic-context blending
    // exactly as in C2.2, just without the rescue pass.
    void process(const std::vector<float>& prominence, const LowFrequencyHarmonicAnalyzer* aux = nullptr, const std::vector<float>* magDb = nullptr);

    const std::array<Region, kMaxRegions>& regions() const { return regionPool; }
    int activeRegionCount() const;

    // Diagnostic-only: the f0-candidate scoring state from the most recent
    // process() call (C1.3/blocker-1 investigation + reporting). Not used
    // by the confidence computation itself -- purely for tests/UI-future.
    struct F0CandidateInfo { bool active = false; float centerHz = 0; float evidence = 0; int matches = 0; };
    const std::array<F0CandidateInfo, kMaxRegions>& lastF0Candidates() const { return f0CandidateDebug; }
    int lastF0WinnerIndex() const { return f0WinnerIndex; }

    // Selectivity -> confidence threshold, continuous (no hard switch).
    // 0 = permissive (low threshold), 10 = only strong evidence (high
    // threshold). Returns a smooth [0,1] "pass" weight, not a boolean, so
    // there is no audible hard edge once this eventually feeds a gain
    // decision (PHYSICAL D+).
    static float selectivityToThreshold(float selectivity01to10);
    static float passWeight(float confidence, float selectivity01to10, float softness = 0.12f);
    // C2.3k-R2 item C/D: turns EVIDENCE (reliableProblemEvidence,
    // unknownAnomalySupport, existenceConfidence -- all Selectivity-free,
    // see Region::problemConfidence) into an ACTION weight. Selectivity
    // smoothly discounts how much unknownAnomalySupport alone may
    // contribute as it rises (0 = fully permissive, unknown evidence counts
    // same as reliable; 10 = unknown evidence contributes ~nothing, only
    // reliableProblemEvidence can justify action) -- reliableProblemEvidence
    // itself is NEVER discounted by this, only how far UNKNOWN evidence can
    // reach on its own. No hard switch, no per-frequency special-casing.
    static float actionWeight(float existenceConfidence, float reliableProblemEvidence, float unknownAnomalySupport, float selectivity01to10, float softness = 0.12f)
    {
        float unknownAuthority = juce::jlimit(0.0f, 1.0f, 1.0f - selectivity01to10 / 10.0f);
        float effectiveProblemEvidence = juce::jmax(reliableProblemEvidence, unknownAnomalySupport * unknownAuthority);
        float actionConfidence = juce::jlimit(0.0f, 1.0f, existenceConfidence * effectiveProblemEvidence);
        return passWeight(actionConfidence, selectivity01to10, softness);
    }

    // Diagnostic-only (PHYSICAL C2 dual-uncertainty investigation): this
    // frame's own structural reliability for the host-rate F0 selection --
    // see the member comment. Not used by anything outside this class
    // itself and the diagnostic harnesses.
    float currentMainF0Reliability() const { return mainF0Reliability; }
    // C2.3k-R3: distinguishes "F0 estimate reliability" (matches/evidence-
    // count based, unchanged) from "F0 classification reliability" (also
    // requires the supporting partials to have actually persisted/stayed
    // stable over time -- see harmonicSupportCoherence).
    float currentHarmonicSupportCoherence() const { return harmonicSupportCoherence; }
    float currentF0ClassificationReliability() const { return mainF0Reliability * harmonicSupportCoherence; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048, hopSize = 512, bins = 1025;
    float riseTau = 3.0f, fallTau = 8.0f; // frames
    float stabilityToleranceOct = 0.18f;
    float harmonicToleranceCents = 40.0f;
    // 70 cents (not a tighter value like 30): even with correct f0 selection
    // and sub-bin interpolation, compounded residual error from TWO
    // independent parabolic estimates (f0's own and the harmonic's own)
    // needs realistic headroom, or genuine harmonics of a clean series
    // fail to register as harmonic at all (measured directly: 30 cents
    // dropped a true H4/H10's harmLike enough that H1's own harmonics
    // scored HIGHER confidence than H3's genuinely non-harmonic case --
    // exactly backwards). resolutionConfidenceCap (not this sigma) is what
    // keeps coarse-resolution cases from reaching false certainty.
    // Chosen from a sweep (35/40/45/50/60 cents) against H1/H2/H3 + the
    // 80/113.14 and 120/170 hard-required cases: all five values satisfy
    // the two hard-required orderings, but only 60 also satisfies
    // H3(non-harmonic) > H2(boosted harmonic) > H1(normal harmonic).
    float harmonicSigmaCents = 60.0f;
    float harmonicMaxPenalty = 0.7f;
    float peakFloorDb = 2.0f;
    // PHYSICAL C2.2 provisional soft-admission calibration (approved as a
    // starting point, not final): lowFloorDb=0.5 (any local max above this
    // can seed a weak candidate), strongFloorDb=2.0 (matches the old hard
    // floor -- full candidateEvidence at/above this), continuationFloorDb=1.0
    // (hysteresis: an existing region only needs this, not strongFloorDb,
    // to keep rising in persistence).
    float lowFloorDb = 0.5f, strongFloorDb = 2.0f, continuationFloorDb = 1.0f;
    float auxCrossoverLowHz = 600.0f, auxCrossoverHighHz = 900.0f;
    // PHYSICAL C2.3 prominence-assist crossover -- deliberately SEPARATE
    // member from the harmonic-context crossover above (different
    // question: "how much does this stand out" vs "is this musically
    // expected"). Provisional, per the C2.3 investigation's own defaults.
    float promCrossoverLowHz = 300.0f, promCrossoverHighHz = 800.0f;
    // 6dB above the sibling-harmonics' mean prominence before any discount
    // starts, reaching 0 protection eligibility by +18dB above that mean --
    // matches the +8/+9/+12dB "excessive harmonic" boosts used in this
    // checkpoint's own test material. Provisional, not final tuning.
    float excessProminenceCeilingDb = 6.0f, excessProminenceRangeDb = 12.0f;
    const LowFrequencyHarmonicAnalyzer* currentAux = nullptr;
    // C2.3h: physical-time budget (ms) for the region continuation bridge --
    // see setContinuationBridgeTimeMs() above for provenance.
    float bridgeMaxTimeMs = 100.0f;

    std::array<Region, kMaxRegions> regionPool;
    std::array<F0CandidateInfo, kMaxRegions> f0CandidateDebug;
    int f0WinnerIndex = -1;
    // PHYSICAL C2: this class's OWN structural reliability for its host-rate
    // F0 selection this frame, mirroring LowFrequencyHarmonicAnalyzer's own
    // gate (>=2 supporting partials required before reliability can rise
    // above 0, then grows with evidence density). Previously host-rate
    // reasoning was implicitly treated as always reliable(=1) above the
    // crossover; that silently manufactured false certainty whenever the
    // aux analyzer ALSO had low reliability (both signals weak, blend
    // treated as trustworthy). One value per frame -- the F0 selection
    // itself is frame-global, not per-region.
    float mainF0Reliability = 0.0f;
    // C2.3k-R3 item C: temporal coherence of the CURRENT f0 winner's
    // supporting siblings (persistence x stability, averaged over siblings
    // with harmonicLikelihood>0.05) -- distinguishes a genuine, steadily-
    // tracked harmonic series from a frame where random broadband content
    // (e.g. noise peaks) happens to align near integer multiples of some
    // candidate just this once. Frame-global, like mainF0Reliability.
    float harmonicSupportCoherence = 0.0f;

    // candidateSource: diagnostic-only, not used by any decision logic --
    // MAIN = from main's own unmodified topology, AUX_RESCUE = aux found
    // something main's topology had nothing at all, MERGED is not a
    // distinct detectedScratch entry (a merge boosts an existing MAIN entry
    // in place, see appendAuxRescueCandidates()).
    struct DetectedPeak { int centerBin; float centerHz; float rawBinHz; float peakDb; int widthBins; float widthHz; float integratedDb; float candidateEvidence; CandidateSource source = CandidateSource::Main; float rescueAuthority = 0.0f; };
    // PHYSICAL C2.2: soft admission means a frame can have MORE local maxima
    // above lowFloorDb than fit in the 32-region pool (e.g. dense/noisy
    // material) -- detectedScratch is sized above kMaxRegions so ALL of
    // them are seen before the deterministic top-K/priority admission step
    // decides which ones actually get a pool slot, rather than silently
    // capping detection itself at 32 (which would just reintroduce an
    // arbitrary, order-dependent cutoff one step earlier). Still fixed-size,
    // no heap allocation.
    static constexpr int kMaxDetections = 64;
    std::array<DetectedPeak, kMaxDetections> detectedScratch; // fixed-size, no allocation
    int detectPeaks(const std::vector<float>& prominence, int maxPeaks, const std::vector<float>* magDb = nullptr);

    // PHYSICAL C2.3 prominence-assist helpers (validated in C2.3a/b/c/d).
    // `data`/`size` rather than std::vector/std::array so this works
    // identically on main's std::vector prominence/magDb and on the aux
    // analyzer's fixed std::array debug buffers with zero copying.
    static float resolutionAdvantageWeight(double mainBinHz, double auxBinHz);
    static float mainLowBinReliability(int bin, const float* magDb, const float* promOut, int size, double binHz);
    static int localPeakBin(const float* data, int size, int approxBin, int radius);
    static float physicalConsistencyFromBins(int promBin, int magBin);
    // C2.3e dual-source rescue: scans aux's own local maxima and either
    // boosts an already-detected MAIN candidate's evidence (merge) or
    // appends a brand-new AUX_RESCUE candidate into detectedScratch,
    // starting at index `numMainDetected`, up to kMaxDetections total.
    // Returns the new total candidate count. Zero heap allocation.
    int appendAuxRescueCandidates(const std::vector<float>& mainProminence, const std::vector<float>& mainMagDb, const LowFrequencyHarmonicAnalyzer& aux, int numMainDetected);
    void applyDetectionToRegion(Region& reg, const DetectedPeak& d);
    // C2.3h: measures the prominence array's own value in a small window
    // (radius 2 bins, same convention as localPeakBin's other callers)
    // around a region's own expected location -- NOT requiring a strict
    // local-maximum (that's what detectPeaks() already tried and failed to
    // find this frame). outFoundHz is the sub-bin location (prominence-
    // domain parabolic interpolation, same convention detectPeaks() itself
    // falls back to when magDb isn't supplied). Zero heap allocation.
    float continuationProminenceAt(const std::vector<float>& prominence, float expectedHz, float& outFoundHz) const;

    float binToHz(int bin) const { return (float) (bin * sampleRate / fftSize); }
    // C1.1: 3-point parabolic interpolation on the (log/dB-domain) prominence
    // array around bin k, returns the sub-bin offset delta in [-0.5, 0.5].
    // Degenerate/flat-top cases (denominator ~0) return delta=0, i.e. fall
    // back to the raw bin center rather than producing a wild extrapolation.
    static float parabolicDelta(float leftDb, float centerDb, float rightDb);
    static float harmonicClosenessFor(float regionHz, float f0Hz, float sigmaCents, float sampleRateHz, int fftSizeSamples, float& outExpectedHz, float& outDistanceCents);
    static float crossoverWeight(float hz, float lowHz, float highHz);
    float computeExcessFactor(float peakProminenceDb, float siblingRefDb, bool siblingRefValid) const;
    // PHYSICAL C2.2 soft admission helpers.
    static float admissionSmoothstep(float lo, float hi, float x) { float t = juce::jlimit(0.0f, 1.0f, (x - lo) / juce::jmax(1.0e-6f, hi - lo)); return t * t * (3.0f - 2.0f * t); }
    // Policy P1 (chosen after the C2.2 adversarial comparison -- P1 and P2
    // performed equivalently in every test run, so the simpler/more
    // auditable of the two was kept per the approved tie-break rule):
    // priority is mostly candidateEvidence, with only a SMALL continuity
    // bonus for a region that was re-matched this frame -- persistence
    // itself is deliberately NOT a multiplicative factor here, so an old,
    // weak, merely-persistent region can never accumulate a de facto
    // "right to the slot" that blocks a genuinely stronger new candidate.
    static float admissionPriorityScore(float candidateEvidence, bool matchedThisFrame) { return 0.85f * candidateEvidence + (matchedThisFrame ? 0.15f : 0.0f); }
    void updateHarmonicLikelihoods();
    void computeConfidence();
};
