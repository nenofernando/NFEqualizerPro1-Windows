#pragma once
#include <JuceHeader.h>
#include <vector>

// PHYSICAL D -- TRANSIENT PROTECTION. Diagnostic-only (no gain touched
// here). Answers, per frequency: "is energy RISING here right now?" --
// deliberately a DIFFERENT question from PHYSICAL C's Existence/Problem
// Confidence (which answer "does this exist/persist" and "should this be
// treated"). PHYSICAL C stays completely frozen: Method C, prominence,
// soft admission, top-K, Region Continuation, dual-source rescue, harmonic
// reasoning, Existence/Problem Confidence, Selectivity, F0/classification
// reliability are none of them touched or re-derived here. This class
// consumes the host's own raw magnitude spectrum independently and hands
// back a per-frequency transientProtection value; nothing here writes back
// into ConfidenceEngine or reads its internals.
//
// Why per-bin, not per-region: a transient's spectral footprint doesn't
// need a tracked region's persistence-smoothed identity to be measured --
// it's a raw, local, purely spectral question ("is this bin's energy
// rising faster than its own recent past"). Keeping this fully independent
// of ConfidenceEngine's region pool means PHYSICAL D can never accidentally
// perturb PHYSICAL C's own tracking, matching, or admission behaviour.
//
// TRANSIENT != RESONANCE (the whole point of this checkpoint): a genuine
// attack shows a brief, strong POSITIVE derivative (spectral flux) and/or
// fast-vs-slow energy ratio spike; a sustained resonance/ringing tail does
// NOT keep rising once the attack has passed, so transientEvidence (and,
// with a short release, transientProtection) naturally decays back toward
// 0 within tens of ms even while the SAME frequency's Problem Confidence
// (a completely separate, ConfidenceEngine-owned quantity) keeps reporting
// the ringing as persistent. No persistence term is mixed in here on
// purpose -- the divergence between "energy stopped rising" (this class)
// and "region still persists" (ConfidenceEngine) IS the mechanism that
// lets an attack's tail be told apart from its onset.
//
// Zero heap allocation once prepare() has run, zero locks, no new thread,
// fixed-size buffers reused every frame -- safe to call once per host hop
// on the audio thread, same discipline as every other PHYSICAL C/D class.
class TransientProtectionEngine
{
public:
    void prepare(double sampleRate, int fftSize, int hopSize);
    void reset();

    // Attack/release time constants, in MILLISECONDS (not frames) -- so
    // behaviour means the same physical duration regardless of hop size at
    // any sample rate (converted internally once prepare() knows the real
    // hop/sampleRate). attackMs: how fast transientProtection can rise to
    // meet a genuine onset (short, so a real attack is never smeared/missed).
    // releaseMs: how fast it decays back down once energy stops rising (the
    // parameter item 9 asks to measure T63/T90 against) -- short enough that
    // a ringing tail becomes treatable again quickly, long enough that a
    // single-frame flux blip mid-attack doesn't cause protection to chatter.
    void setTransientTimeConstants(float attackMs, float releaseMs);

    // One call per host hop. magDb is the SAME host-rate raw magnitude
    // array the caller already computed for SpectralProminenceEngineV5 /
    // ConfidenceEngine -- this class never recomputes it, only reasons
    // about its frame-to-frame change. Independent of prominence entirely.
    void process(const std::vector<float>& magDb);

    // transientEvidence: 0..1, the RAW (attack-smoothed only, not yet
    // release-held) "is this rising right now" reading at queryHz --
    // diagnostic, shows the instantaneous signal before the release-time
    // decay is applied.
    float transientEvidenceFor(float queryHz) const;
    // transientProtection: 0..1, transientEvidence run through the
    // asymmetric attack/release envelope -- THIS is what a future gain
    // stage would use: effectiveActionAuthority = problemConfidence * (1 -
    // transientProtection). NOT connected to gain in this checkpoint.
    float transientProtectionFor(float queryHz) const;

    double binHz() const { return sampleRate / (double) fftSize; }

private:
    double sampleRate = 48000.0;
    int fftSize = 2048, hopSize = 512, bins = 1025;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f; // precomputed EMA coefficients from ms + hop/sampleRate

    std::vector<float> bandMagDb;           // 3-bin box-car energy sum (dB) of the current frame's magDb -- see process()
    std::vector<float> prevMagDb;          // previous frame's BAND-smoothed magnitude (dB), for spectral flux
    std::vector<float> fastEnvLin, slowEnvLin; // short-term vs long-term linear-magnitude envelopes, per bin
    std::vector<float> transientEvidence;  // per-bin, 0..1, attack-only (diagnostic)
    std::vector<float> protection;         // per-bin, 0..1, attack+release envelope (the actual output)
    float fastEnvCoeff = 0.0f, slowEnvCoeff = 0.0f; // fixed ~15ms/~200ms EMA coefficients for the rise-ratio feature
    bool primed = false; // guards the first frame (no previous magDb yet -- flux undefined)

    static float dbFluxToEvidence(float fluxDb);
    static float riseRatioToEvidence(float ratio);
    int nearestBin(float hz) const;
};
