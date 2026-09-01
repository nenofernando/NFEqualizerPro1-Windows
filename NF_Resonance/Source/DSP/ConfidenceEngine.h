#pragma once
#include <JuceHeader.h>
#include <array>

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
    void setPeakFloorDb(float db) { peakFloorDb = db; } // generous region-detection floor, NOT the final decision threshold

    void prepare(double sampleRate, int fftSize, int hopSize);
    void reset();

    // One call per frame. `prominence` is V2-A5C's own output (same array
    // computeProminence() fills) -- this class never recomputes prominence
    // itself, only reasons about it.
    void process(const std::vector<float>& prominence);

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

    std::array<Region, kMaxRegions> regionPool;
    std::array<F0CandidateInfo, kMaxRegions> f0CandidateDebug;
    int f0WinnerIndex = -1;

    struct DetectedPeak { int centerBin; float centerHz; float rawBinHz; float peakDb; int widthBins; float widthHz; float integratedDb; };
    std::array<DetectedPeak, kMaxRegions> detectedScratch; // fixed-size, no allocation
    int detectPeaks(const std::vector<float>& prominence, int maxPeaks);

    float binToHz(int bin) const { return (float) (bin * sampleRate / fftSize); }
    // C1.1: 3-point parabolic interpolation on the (log/dB-domain) prominence
    // array around bin k, returns the sub-bin offset delta in [-0.5, 0.5].
    // Degenerate/flat-top cases (denominator ~0) return delta=0, i.e. fall
    // back to the raw bin center rather than producing a wild extrapolation.
    static float parabolicDelta(float leftDb, float centerDb, float rightDb);
    static float harmonicClosenessFor(float regionHz, float f0Hz, float sigmaCents, float sampleRateHz, int fftSizeSamples, float& outExpectedHz, float& outDistanceCents);
    void updateHarmonicLikelihoods();
    void computeConfidence();
};
