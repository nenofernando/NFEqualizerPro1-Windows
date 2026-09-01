#include "ConfidenceEngine.h"
#include <cmath>
#include <algorithm>

void ConfidenceEngine::prepare(double sr, int fft, int hop)
{
    sampleRate = sr; fftSize = fft; hopSize = hop; bins = fft / 2 + 1;
    detectedScratch.fill(DetectedPeak{});
    reset();
}
void ConfidenceEngine::reset() { regionPool.fill(Region{}); }

float ConfidenceEngine::parabolicDelta(float leftDb, float centerDb, float rightDb)
{
    float denom = leftDb - 2.0f * centerDb + rightDb;
    if (std::abs(denom) < 1.0e-6f) return 0.0f; // flat top / degenerate -- fall back to the raw bin center
    float delta = 0.5f * (leftDb - rightDb) / denom;
    return juce::jlimit(-0.5f, 0.5f, delta);
}

int ConfidenceEngine::activeRegionCount() const
{
    int n = 0; for (auto& r : regionPool) if (r.active) ++n; return n;
}

// Fixed-size, no heap allocation: scans for local maxima above peakFloorDb,
// grows each into a contiguous region while prominence stays above
// (peakDb - 6dB) or the array ends, capped at kMaxRegions detections/frame.
int ConfidenceEngine::detectPeaks(const std::vector<float>& prominence, int maxPeaks)
{
    int n = (int) prominence.size();
    int count = 0;
    int b = 1;
    while (b < n - 1 && count < maxPeaks)
    {
        bool isPeak = prominence[(size_t) b] > prominence[(size_t) (b - 1)]
                    && prominence[(size_t) b] >= prominence[(size_t) (b + 1)]
                    && prominence[(size_t) b] > peakFloorDb;
        if (! isPeak) { ++b; continue; }
        float peakDb = prominence[(size_t) b];
        float half = peakDb - 6.0f;
        int lo = b, hi = b;
        while (lo > 0 && prominence[(size_t) (lo - 1)] > half) --lo;
        while (hi < n - 1 && prominence[(size_t) (hi + 1)] > half) ++hi;
        float integrated = 0.0f; for (int i = lo; i <= hi; ++i) integrated += juce::jmax(0.0f, prominence[(size_t) i]);
        // C1.1: sub-bin refinement via 3-point parabolic interpolation on
        // the prominence array itself (log/dB-domain already, since
        // prominence is a dB-domain quantity) -- this is what fixed the
        // 80/135Hz false-harmonic collision: the peak center no longer
        // snaps to whichever bin happens to be nearest.
        float delta = 0.0f;
        if (b > 0 && b < n - 1)
            delta = parabolicDelta(prominence[(size_t) (b - 1)], prominence[(size_t) b], prominence[(size_t) (b + 1)]);
        auto& d = detectedScratch[(size_t) count];
        d.centerBin = b; d.rawBinHz = binToHz(b); d.centerHz = (float) ((b + delta) * sampleRate / fftSize); d.peakDb = peakDb;
        d.widthBins = hi - lo + 1; d.widthHz = d.widthBins * (float) (sampleRate / fftSize);
        d.integratedDb = integrated;
        ++count;
        b = hi + 1; // skip past this region, avoids re-detecting its own tail as a new peak
    }
    return count;
}

void ConfidenceEngine::process(const std::vector<float>& prominence)
{
    int numDetected = detectPeaks(prominence, kMaxRegions);

    // Match each detection to an existing active region by log-frequency
    // proximity (closest within tolerance wins); unmatched detections take
    // a free slot if available. matchedRegion[k] = -1 if unmatched.
    std::array<int, kMaxRegions> matchedRegion; matchedRegion.fill(-1);
    std::array<bool, kMaxRegions> regionWasMatched; regionWasMatched.fill(false);

    for (int k = 0; k < numDetected; ++k)
    {
        float logHz = std::log2(juce::jmax(1.0f, detectedScratch[(size_t) k].centerHz));
        int best = -1; float bestDist = 1.0e9f;
        for (int r = 0; r < kMaxRegions; ++r)
        {
            if (! regionPool[(size_t) r].active || regionWasMatched[(size_t) r]) continue;
            float dist = std::abs(std::log2(juce::jmax(1.0f, regionPool[(size_t) r].centerHz)) - logHz);
            if (dist < bestDist) { bestDist = dist; best = r; }
        }
        if (best >= 0 && bestDist <= stabilityToleranceOct * 2.0f) // generous match window, separate from the tighter stability SCORE below
        {
            matchedRegion[(size_t) k] = best;
            regionWasMatched[(size_t) best] = true;
        }
    }

    // Update matched regions; decay/free unmatched active regions.
    for (int r = 0; r < kMaxRegions; ++r)
    {
        auto& reg = regionPool[(size_t) r];
        if (! reg.active) continue;
        if (regionWasMatched[(size_t) r]) continue; // handled below when we walk detections
        // No detection matched this region this frame -- decay persistence.
        float fallCoeff = std::exp(-1.0f / fallTau);
        reg.persistence *= fallCoeff;
        reg.framesAbsent++;
        reg.framesPresent = 0;
        if (reg.persistence < 0.02f) reg = Region{}; // free back to the pool
    }
    for (int k = 0; k < numDetected; ++k)
    {
        int r = matchedRegion[(size_t) k];
        if (r < 0)
        {
            for (int f = 0; f < kMaxRegions; ++f) if (! regionPool[(size_t) f].active) { r = f; break; }
            if (r < 0) continue; // pool full -- this detection is dropped, deterministically (lowest-index slots win, no allocation)
            regionPool[(size_t) r] = Region{};
            regionPool[(size_t) r].active = true;
        }
        auto& reg = regionPool[(size_t) r];
        auto& d = detectedScratch[(size_t) k];
        reg.centerBin = d.centerBin; reg.centerHz = d.centerHz; reg.rawBinHz = d.rawBinHz; reg.peakProminenceDb = d.peakDb;
        reg.widthBins = d.widthBins; reg.widthHz = d.widthHz; reg.integratedEvidenceDb = d.integratedDb;
        float riseCoeff = std::exp(-1.0f / riseTau);
        reg.persistence = 1.0f - (1.0f - reg.persistence) * riseCoeff; // rises toward 1
        reg.framesPresent++; reg.framesAbsent = 0;
        // Stability: push this frame's log-Hz into the small fixed ring
        // buffer, score = 1 - (spread relative to tolerance), clamped.
        float logHz = std::log2(juce::jmax(1.0f, d.centerHz));
        int idx = reg.historyCount % kStabilityHistoryLen;
        reg.logHzHistory[(size_t) idx] = logHz;
        reg.historyCount++;
        int n = juce::jmin(reg.historyCount, kStabilityHistoryLen);
        if (n >= 2)
        {
            float mean = 0.0f; for (int i = 0; i < n; ++i) mean += reg.logHzHistory[(size_t) i]; mean /= n;
            float var = 0.0f; for (int i = 0; i < n; ++i) { float dd = reg.logHzHistory[(size_t) i] - mean; var += dd * dd; } var /= n;
            float sd = std::sqrt(var);
            reg.stability = juce::jlimit(0.0f, 1.0f, 1.0f - sd / stabilityToleranceOct);
        }
        else reg.stability = 0.0f; // not enough history yet to claim stability either way
    }

    updateHarmonicLikelihoods();
    computeConfidence();
}

// C1.2 continuous likelihood for a single (region, f0) comparison. Distance
// falls off smoothly (Gaussian) around the nearest integer multiple of f0
// -- no hard in/out-of-tolerance decision anywhere. Critically, the
// ACHIEVABLE CEILING is capped by resolutionConfidenceCap(f0), which
// SHRINKS as the bin at f0 gets coarser (in cents) -- so higher spectral
// uncertainty can only ever lower the maximum certainty, never substitute
// for a wider acceptance window pretending to be confidence.
float ConfidenceEngine::harmonicClosenessFor(float regionHz, float f0Hz, float sigmaCents, float sampleRateHz, int fftSizeSamples, float& outExpectedHz, float& outDistanceCents)
{
    float ratio = regionHz / f0Hz;
    int nearestN = juce::jmax(1, (int) std::round(ratio));
    float expectedHz = f0Hz * (float) nearestN;
    float centsOff = 1200.0f * std::log2(regionHz / expectedHz);
    outExpectedHz = expectedHz; outDistanceCents = centsOff;
    float gaussian = std::exp(-0.5f * (centsOff / sigmaCents) * (centsOff / sigmaCents));
    float binWidthCentsAtF0 = 1200.0f * std::log2(1.0f + (float) (sampleRateHz / fftSizeSamples) / juce::jmax(1.0f, f0Hz));
    float resolutionConfidenceCap = juce::jlimit(0.15f, 1.0f, 1.0f - binWidthCentsAtF0 / 1200.0f);
    return gaussian * resolutionConfidenceCap;
}

// C1.3: f0 is no longer just "the lowest sufficiently-prominent region".
// Every active region is scored as a CANDIDATE f0 by how much harmonic
// evidence (sum of continuous closeness scores from OTHER regions) it
// accumulates; the candidate with the strongest evidence wins. A single
// weak match does not automatically grant the winning f0 full credit as a
// genuine fundamental -- f0SeriesConfidence itself scales with the amount
// of accumulated evidence (see below), and is what the f0 region's own
// harmonicLikelihood is set to (never a flat 1.0 for "I found one match").
void ConfidenceEngine::updateHarmonicLikelihoods()
{
    for (auto& reg : regionPool) { reg.harmonicLikelihood = 0.0f; reg.harmonicExpectedHz = 0.0f; reg.harmonicDistanceCents = 0.0f; }

    // Fixed-size scratch, no heap: evidence accumulated per candidate slot.
    std::array<float, kMaxRegions> candidateEvidence{}; candidateEvidence.fill(0.0f);
    std::array<int, kMaxRegions> candidateMatches{}; candidateMatches.fill(0);

    for (int c = 0; c < kMaxRegions; ++c)
    {
        auto& cand = regionPool[(size_t) c];
        if (! cand.active || cand.peakProminenceDb < peakFloorDb) continue;
        for (int r = 0; r < kMaxRegions; ++r)
        {
            if (r == c) continue;
            auto& reg = regionPool[(size_t) r];
            if (! reg.active) continue;
            float ratio = reg.centerHz / cand.centerHz;
            if (ratio < 1.4f) continue; // below ~n=1.4, not meaningfully "another harmonic" of this candidate
            float expectedHz, distCents;
            float closeness = harmonicClosenessFor(reg.centerHz, cand.centerHz, harmonicSigmaCents, (float) sampleRate, fftSize, expectedHz, distCents);
            if (closeness > 0.05f) { candidateEvidence[(size_t) c] += closeness; candidateMatches[(size_t) c]++; }
        }
    }

    for (int c = 0; c < kMaxRegions; ++c)
    {
        f0CandidateDebug[(size_t) c].active = regionPool[(size_t) c].active;
        f0CandidateDebug[(size_t) c].centerHz = regionPool[(size_t) c].centerHz;
        f0CandidateDebug[(size_t) c].evidence = candidateEvidence[(size_t) c];
        f0CandidateDebug[(size_t) c].matches = candidateMatches[(size_t) c];
    }
    // BLOCKER 1 root cause: choosing the candidate with the highest RAW
    // EVIDENCE SUM let a genuine harmonic (e.g. 2f0) outscore the true
    // fundamental whenever its few matches each happened to land closer in
    // cents than the fundamental's own (more numerous, but individually
    // noisier) matches -- e.g. 2f0 with 3 tight matches (evidence 2.25)
    // beat the real f0 with 7 looser matches (evidence 2.03), even though
    // 7 supporting partials is far more physically plausible than 3.
    // Fixed: rank candidates primarily by MATCH COUNT (how many partials
    // support it -- "supported by the set of partials", not by how clean
    // any one of them happens to be), evidence sum only as a tiebreaker
    // between equally-supported candidates, and lowest frequency as the
    // final tiebreaker (the more plausible fundamental among near-ties).
    int f0Region = -1; float bestEvidence = 0.0f; int bestMatches = 0;
    for (int c = 0; c < kMaxRegions; ++c)
    {
        if (! regionPool[(size_t) c].active || candidateMatches[(size_t) c] <= 0) continue;
        int m = candidateMatches[(size_t) c]; float e = candidateEvidence[(size_t) c];
        bool better = (m > bestMatches)
                   || (m == bestMatches && e > bestEvidence + 1.0e-6f)
                   || (m == bestMatches && std::abs(e - bestEvidence) <= 1.0e-6f && f0Region >= 0 && regionPool[(size_t) c].centerHz < regionPool[(size_t) f0Region].centerHz);
        if (f0Region < 0 || better) { bestEvidence = e; bestMatches = m; f0Region = c; }
    }
    f0WinnerIndex = f0Region;
    if (f0Region < 0) return; // no region has ANY harmonic evidence from another region -- nothing to anchor a series on

    float f0Hz = regionPool[(size_t) f0Region].centerHz;
    for (int r = 0; r < kMaxRegions; ++r)
    {
        if (r == f0Region) continue;
        auto& reg = regionPool[(size_t) r];
        if (! reg.active) continue;
        float ratio = reg.centerHz / f0Hz;
        if (ratio < 1.4f) continue;
        float expectedHz, distCents;
        float closeness = harmonicClosenessFor(reg.centerHz, f0Hz, harmonicSigmaCents, (float) sampleRate, fftSize, expectedHz, distCents);
        reg.harmonicLikelihood = closeness;
        reg.harmonicExpectedHz = expectedHz;
        reg.harmonicDistanceCents = distCents;
    }
    // f0's own credit as "a genuine fundamental" scales with accumulated
    // evidence -- 2 clean matches (~2.0 evidence) is enough to be near-
    // certain; a single marginal match stays proportionally low, never a
    // flat 1.0 for "I found one thing that might be a harmonic".
    regionPool[(size_t) f0Region].harmonicLikelihood = juce::jlimit(0.0f, 1.0f, bestEvidence / 2.0f);
}

void ConfidenceEngine::computeConfidence()
{
    for (auto& reg : regionPool)
    {
        if (! reg.active) { reg.confidence = 0.0f; continue; }
        // Explicit, auditable sub-features, each already 0..1 or mapped here:
        // ~2dB->0, ~30dB->1 -- widened from an earlier 2-15dB range that
        // saturated too early: test material commonly reaches 15-45dB
        // prominence, and a too-narrow range made a genuinely elevated
        // partial (e.g. +8dB above its own already-loud harmonic siblings)
        // indistinguishable from an unboosted one once both saturated at 1.0.
        float prominenceEvidence = juce::jlimit(0.0f, 1.0f, (reg.peakProminenceDb - 2.0f) / 28.0f);
        float persistenceEvidence = reg.persistence;
        float stabilityEvidence = reg.stability;
        float widthEvidence = 1.0f - std::exp(-(float) reg.widthBins / 2.0f); // saturates quickly; a 1-bin spike scores low
        float harmonicPenalty = 1.0f - reg.harmonicLikelihood * harmonicMaxPenalty; // NEVER reaches 0 (harmonicMaxPenalty < 1)
        // Pure multiplication of 4+ independent [0,1] evidences collapses
        // too aggressively in practice (measured directly: a genuinely
        // isolated, persistent, stable, non-harmonic resonance only reached
        // ~0.26 confidence with 4-way multiplication, barely above a
        // legitimate harmonic partial's ~0.17-0.20 -- not enough
        // separation to be useful). combineEvidence uses a weighted mean
        // of the four independent evidences instead (gentler, avoids
        // multiplicative collapse), with harmonicPenalty kept as a
        // SEPARATE, deliberately multiplicative final suppression step --
        // that one genuinely should scale down confidence, capped so it
        // can never reach exactly 0 (see harmonicMaxPenalty).
        float combinedEvidence = 0.30f * prominenceEvidence + 0.30f * persistenceEvidence + 0.25f * stabilityEvidence + 0.15f * widthEvidence;
        reg.lastProminenceEvidence = prominenceEvidence; reg.lastPersistenceEvidence = persistenceEvidence;
        reg.lastStabilityEvidence = stabilityEvidence; reg.lastWidthEvidence = widthEvidence; reg.lastHarmonicPenalty = harmonicPenalty;
        reg.confidence = juce::jlimit(0.0f, 1.0f, combinedEvidence * harmonicPenalty);
    }
}

float ConfidenceEngine::selectivityToThreshold(float sel)
{
    return juce::jmap(juce::jlimit(0.0f, 10.0f, sel), 0.0f, 10.0f, 0.15f, 0.85f);
}
float ConfidenceEngine::passWeight(float confidence, float sel, float softness)
{
    float threshold = selectivityToThreshold(sel);
    float x = (confidence - threshold) / juce::jmax(0.001f, softness);
    return 1.0f / (1.0f + std::exp(-4.0f * x)); // smooth sigmoid, no hard switch
}
