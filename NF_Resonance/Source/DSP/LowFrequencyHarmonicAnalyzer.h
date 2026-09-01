#pragma once
#include <JuceHeader.h>
#include <array>
#include "SpectralProminenceEngineV5.h"

// PHYSICAL C, Blocker 2: an AUXILIARY, audio-path-independent analyzer that
// keeps low-frequency harmonic context (f0, supporting-partial count,
// harmonic likelihood) usable at 96/192kHz, where the MAIN STFT (frozen,
// unchanged, still fftSize=2048 at the host's own sample rate) simply
// doesn't have the bin resolution to tell a bass fundamental from a
// nearby non-harmonic resonance (measured directly in the Blocker-1
// investigation: 93.75Hz/bin at 192kHz).
//
// This class does NOT touch gain, does NOT replace V2-A5C prominence (the
// real detector keeps using SpectralProminenceEngineV5 at the host rate,
// unchanged), does NOT change the plugin's reported latency, and does NOT
// alter the STFT/OLA/C2 path at all -- it runs a SEPARATE, internal,
// causal analysis on a DECIMATED copy of the same audio, purely to hand
// ConfidenceEngine a more reliable f0/harmonic-likelihood estimate for
// low-frequency regions.
//
// Sample-rate normalization: internally analyzes close to 44.1-48kHz
// regardless of the host rate --
//   44.1kHz host -> 1:1 (no decimation)
//   48kHz   host -> 1:1
//   96kHz   host -> 2:1 decimation
//   192kHz  host -> 4:1 decimation
// via a real 4th-order (2 cascaded biquads) anti-alias lowpass BEFORE
// downsampling -- never plain sample-dropping.
//
// Zero heap allocation, zero locks, fixed-size ring buffers, fully
// resettable in prepare(). Causal only: uses ONLY past decimated samples,
// never adds lookahead, never changes the plugin's reported latency.
//
// PHYSICAL C / C1 Blocker 2 -- CLOSED (harmonic-context checkpoint). This
// class is approved as the low-frequency auxiliary harmonic-context
// supplier. Gain reduction is NOT implemented here and must not be added
// without a separate, explicit checkpoint.
//
// FUTURE INTEGRATION CONTRACT (documented now, not implemented): callers
// (eventually ConfidenceEngine / a gain-decision stage) must keep THREE
// pieces of information separate and must not silently collapse them into
// one number:
//   baseProblemEvidence      -- how anomalous a region looks on its own
//                                (prominence/persistence/stability), with
//                                no harmonic reasoning applied yet.
//   harmonicLikelihood       -- purely geometric: harmonicLikelihoodFor()'s
//                                return value, "if f0Hz IS the fundamental,
//                                how close is queryHz to one of its
//                                harmonics." Independent of how much the f0
//                                estimate itself should be trusted.
//   harmonicContextReliability -- currentContext().f0Reliability, continuous
//                                in [0,1], NEVER hard-thresholded in
//                                production code (a report/test may count
//                                "reliable" cases at some cutoff for
//                                summary purposes, but the DSP contract
//                                itself must consume the continuous value).
// A future gain-decision stage should compose these roughly as
//   effectiveHarmonicProtection = lerp(noProtection, harmonicProtection(harmonicLikelihood), harmonicContextReliability)
//   finalDecision = combine(baseProblemEvidence, effectiveHarmonicProtection)
// i.e. reliability scales how much the harmonic-protection TERM is trusted,
// it must NOT be used to blindly pull the final decision toward a neutral
// midpoint the way this checkpoint's own offline test-harness proxy does
// for reporting purposes -- that proxy (problemConfidenceProxy() in
// LowFreqHarmonicAnalyzerCheck.cpp) is diagnostic-only and was never meant
// to be the production reduction formula.
//
// TRI-STATE HARMONIC CLASSIFICATION: low f0Reliability means UNKNOWN /
// insufficient evidence -- it is not equivalent to "confirmed harmonic"
// (reliability near 1 with high likelihood) nor to "confirmed non-harmonic"
// (reliability near 1 with low likelihood). A future gain decision must be
// able to represent and act on all three states distinctly; collapsing
// UNKNOWN into either extreme reintroduces the exact failure this
// checkpoint was built to close (a resolution collision at low SR
// confidently declaring the wrong answer).
class LowFrequencyHarmonicAnalyzer
{
public:
    // KNOWN, QUANTIFIED, NON-BLOCKING LIMITATION (documented at Blocker 2
    // closure): an 80Hz fundamental at 44.1kHz specifically (host rate,
    // decimation=1x, ~21.53Hz/bin) has its harmonics spaced only ~3.7 bins
    // apart, close enough for the Hann window's mainlobe to distort the
    // fundamental's own peak position even with no injected resonance.
    // Measured: estimated F0 error ~-7.7 cents (small, still usable),
    // f0Reliability ~0.492 (just under the report's 0.5 "reliable" cutoff,
    // itself not a production threshold -- see the class-level comment).
    // Not blocking because: the frequency estimate stays close, reliability
    // correctly expresses the reduced certainty, and no high-confidence
    // wrong decision is produced.
    static constexpr int kAnalysisFftSize = 2048;   // at the DECIMATED rate -- ~21.5-23.4Hz/bin regardless of host SR
    static constexpr int kRingCapacity = kAnalysisFftSize * 2; // headroom for the largest decimation factor's hop
    static constexpr int kMaxF0Candidates = 32;

    struct Context
    {
        bool valid = false;            // some F0 candidate emerged at all (>=1 supporting partial) -- NOT a reliability judgment
        float f0Hz = 0.0f;
        int supportingPartials = 0;
        float f0Score = 0.0f;          // raw accumulated evidence for the winning f0 (diagnostic)
        float f0Confidence = 0.0f;     // 0..1, persistence-smoothed evidence-of-the-winner (unrelated to whether the winner itself should be trusted)
        // Structural reliability of the F0 estimate: requires >=2
        // independent supporting partials before this can rise above 0 at
        // all (conservative first implementation), then grows with
        // evidence density per match and temporal stability of the winner
        // across frames. UNKNOWN (low reliability) is NOT the same claim
        // as NON-HARMONIC -- callers must not treat a low f0Reliability as
        // "confirmed non-harmonic" (that would silently swing
        // problem-confidence up, which is exactly the failure mode this
        // was built to avoid for low-frequency fundamentals). Low
        // reliability means "insufficient evidence to say either way."
        float f0Reliability = 0.0f;
    };

    // hostSampleRate/hostBlockHint: hostBlockHint isn't required to be
    // exact (pushSamples() handles any block size), only used to size
    // internal scratch sensibly. F0 is searched in [f0SearchLowHz,
    // f0SearchHighHz]; harmonics are looked for up to harmonicEvidenceHighHz
    // (may need to look higher than the F0 range itself to confirm a bass
    // fundamental via its own upper partials -- C1.8 item 3).
    void prepare(double hostSampleRate, float f0SearchLowHz = 40.0f, float f0SearchHighHz = 800.0f, float harmonicEvidenceHighHz = 2500.0f);
    void reset();

    // Feed one host-rate audio block (mono-summed by the caller, or pass a
    // pre-summed pointer). Causal, no allocation. Internally filters +
    // decimates + accumulates into the analysis ring, and runs a new
    // analysis frame whenever a full hop's worth of new decimated samples
    // has arrived (same cadence as the host's own STFT hop, so this never
    // falls behind or adds extra delay).
    void pushSamples(const float* monoIn, int numHostSamples);

    const Context& currentContext() const { return smoothedContext; }
    // Continuous harmonic likelihood (see ConfidenceEngine::harmonicClosenessFor
    // -- same gaussian * resolutionConfidenceCap model, but evaluated with
    // THIS analyzer's own, much finer, decimated-rate bin resolution) for a
    // query frequency against the current f0 context. 0 if no F0 hypothesis
    // exists at all (f0Hz invalid). Deliberately NOT scaled by
    // f0Confidence/f0Reliability -- this is a purely geometric "if f0Hz IS
    // the fundamental, how close is queryHz to a harmonic of it" answer.
    // How much to trust that the f0 hypothesis itself is real is a SEPARATE
    // axis (currentContext().f0Reliability); combining the two is left to
    // the caller so "unreliable f0" and "confirmed non-harmonic" are never
    // conflated into the same low number.
    float harmonicLikelihoodFor(float queryHz) const;

    int decimationFactor() const { return decimation; }
    double analysisRate() const { return decimatedRate; }
    double analysisBinHz() const { return decimatedRate / kAnalysisFftSize; }

    // ---- Diagnostic-only accessors (Blocker 2 F0-bias root-cause work) ----
    // Read-only windows into the last runAnalysisFrame()'s intermediate
    // state, so an external harness can compare peak position at each
    // pipeline stage (raw decimated magnitude vs. prominence) without
    // duplicating the decimation/window/FFT code and risking a mismatch
    // against what production actually does. Never written to gain/UI.
    const std::array<float, kAnalysisFftSize / 2 + 1>& debugMagDb() const { return magDbScratch; }
    const std::array<float, kAnalysisFftSize / 2 + 1>& debugProminence() const { return promScratch; }
    const std::array<float, kRingCapacity>& debugRing() const { return ring; }
    int debugRingWrite() const { return ringWrite; }
    int debugRingFilled() const { return ringFilled; }
    struct F0CandidateInfo { bool active = false; float centerHz = 0; float evidence = 0; int matches = 0; };
    const std::array<F0CandidateInfo, kMaxF0Candidates>& debugF0Candidates() const { return f0CandidateDebug; }
    int debugNumPeaks() const { return lastNumPeaks; }

    void setPersistenceTimeConstants(float riseFrames, float fallFrames) { riseTau = juce::jmax(0.1f, riseFrames); fallTau = juce::jmax(0.1f, fallFrames); }

private:
    double hostRate = 48000.0, decimatedRate = 48000.0;
    int decimation = 1;
    float f0Lo = 40.0f, f0Hi = 800.0f, harmHi = 2500.0f;

    // 2 cascaded biquad lowpass stages (4th order) for anti-alias filtering
    // before decimation -- real filtering, not sample-dropping.
    struct Biquad { float b0=1,b1=0,b2=0,a1=0,a2=0; float z1=0,z2=0;
        float process(float x) { float y = b0*x + z1; z1 = b1*x - a1*y + z2; z2 = b2*x - a2*y; return y; }
        void reset() { z1=0; z2=0; }
    };
    std::array<Biquad, 2> aaFilter;
    void designAntiAlias(); // called from prepare(), sets cutoff from decimation factor

    // Decimated-rate ring buffer (fixed size, circular) -- holds the most
    // recent kAnalysisFftSize (or more) decimated samples for the FFT.
    std::array<float, kRingCapacity> ring{};
    int ringWrite = 0, ringFilled = 0;
    int decimationPhase = 0; // counts host samples 0..decimation-1 between kept samples
    int samplesSinceLastAnalysis = 0;
    int hopDecimatedSamples = 128; // recomputed in prepare() = host hop (assumed 512) / decimation

    SpectralProminenceEngineV5 prom;
    juce::dsp::FFT fft{ 11 }; // fixed: log2(2048)=11
    std::array<float, kAnalysisFftSize> window{};
    std::array<float, kAnalysisFftSize * 2> fftScratch{};
    std::array<float, kAnalysisFftSize / 2 + 1> magDbScratch{};
    std::array<float, kAnalysisFftSize / 2 + 1> promScratch{};
    // SpectralProminenceEngineV5's API takes std::vector<float>& -- these
    // are allocated ONCE (prepare()/reset()) and reused every analysis
    // frame via std::copy, never resized in the hot path, so no heap
    // allocation happens once the analyzer is running.
    std::vector<float> magDbVecReused, promVecReused;

    // Fixed-size F0 candidate scoring scratch (same match-count-first
    // algorithm validated in ConfidenceEngine's Blocker-1 fix).
    struct Peak { float hz; float db; };
    std::array<Peak, kMaxF0Candidates> peakScratch{};

    Context rawContext, smoothedContext;
    float riseTau = 3.0f, fallTau = 8.0f;

    // Diagnostic-only: every frame's F0 candidate scoring state, mirroring
    // ConfidenceEngine's own debug accessor pattern -- not used by the
    // selection logic itself, purely for root-cause reporting.
    std::array<F0CandidateInfo, kMaxF0Candidates> f0CandidateDebug{};
    int lastNumPeaks = 0;

    // Temporal-stability tracking for f0Reliability: counts consecutive
    // frames where the winning candidate stayed within 50 cents of the
    // previous frame's winner (reset to 0 whenever no candidate wins or the
    // winner jumps). Reliability requires min-support (>=2 matches) before
    // this contributes anything, per the conservative first implementation.
    float f0StabilityLastHz = 0.0f;
    int f0StableFrameCount = 0;

    void runAnalysisFrame();
};
