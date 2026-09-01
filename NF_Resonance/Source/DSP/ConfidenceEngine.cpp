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

// ---------------- PHYSICAL C2.3: low-frequency prominence assistance ----------------
float ConfidenceEngine::resolutionAdvantageWeight(double mainBinHz, double auxBinHz)
{
    if (mainBinHz <= 0.0) return 0.0f;
    double ratio = auxBinHz / mainBinHz; // <1 = aux resolves finer than main
    return (float) juce::jlimit(0.0, 1.0, 1.0 - ratio);
}

// Evidence-based (no hardcoded bin index): does magnitude's own local
// ranking (of this bin's neighbors) agree with prominence's own local
// ranking, how much does prominence's own asymmetry diverge from
// magnitude's own asymmetry, and how coarse is the resolution relative to
// this bin's own frequency.
float ConfidenceEngine::mainLowBinReliability(int bin, const float* magDb, const float* promOut, int size, double binHz)
{
    if (bin < 1 || bin >= size - 1) return 0.5f;
    float magL = magDb[bin - 1], magC = magDb[bin], magR = magDb[bin + 1];
    float promL = promOut[bin - 1], promC = promOut[bin], promR = promOut[bin + 1];
    auto bestOf3 = [](float l, float c, float r) { if (c >= l && c >= r) return 0; return (l > r) ? -1 : 1; };
    int magBest = bestOf3(magL, magC, magR), promBest = bestOf3(promL, promC, promR);
    float orderAgreement = (magBest == promBest) ? 1.0f : 0.0f;
    float magAsym = (magR - magL) / juce::jmax(1.0f, std::abs(magR) + std::abs(magL));
    float promAsym = (promR - promL) / juce::jmax(1.0f, std::abs(promR) + std::abs(promL));
    float asymDivergence = juce::jlimit(0.0f, 1.0f, std::abs(magAsym - promAsym));
    float freqHz = (float) (bin * binHz);
    float relRes = juce::jlimit(0.0f, 1.0f, 1.0f - (float) (binHz / juce::jmax(1.0, (double) freqHz)));
    return juce::jlimit(0.0f, 1.0f, relRes * orderAgreement * (1.0f - asymDivergence));
}

int ConfidenceEngine::localPeakBin(const float* data, int size, int approxBin, int radius)
{
    int best = juce::jlimit(1, size - 2, approxBin); float bestVal = data[best];
    for (int b = juce::jmax(1, approxBin - radius); b <= juce::jmin(size - 2, approxBin + radius); ++b)
        if (data[b] > bestVal) { bestVal = data[b]; best = b; }
    return best;
}

// Does THIS source's own prominence-peak bin agree with THIS source's own
// magnitude-peak bin? The dominant, near-gating signal distinguishing
// ordinary sub-bin interpolation noise (bins agree, a few cents off) from
// the known structural artifact (prominence lands on a DIFFERENT bin
// entirely) -- see C2.3c.
float ConfidenceEngine::physicalConsistencyFromBins(int promBin, int magBin)
{
    return juce::jmax(0.0f, 1.0f - 0.5f * std::abs((float) (promBin - magBin)));
}

// PHYSICAL C2.3e -- see the header comment for the full rationale (retires
// the bin-by-bin prominence blend, which was PROVEN via a direct local-
// maxima dump to alter main's own peak topology). Main's detectedScratch
// entries [0, numMainDetected) are read-only here except for the evidence/
// location fields of an existing entry being MERGED into -- no entry's
// underlying peakDb/widthBins/etc (main's own detected values) is ever
// rewritten from aux, only candidateEvidence and (conditionally) centerHz.
int ConfidenceEngine::appendAuxRescueCandidates(const std::vector<float>& mainProminence, const std::vector<float>& mainMagDb, const LowFrequencyHarmonicAnalyzer& aux, int numMainDetected)
{
    if (mainMagDb.size() != mainProminence.size()) return numMainDetected;
    double mainBinHz = sampleRate / (double) fftSize;
    double auxBinHz = aux.analysisBinHz();
    float resAdv = resolutionAdvantageWeight(mainBinHz, auxBinHz);
    if (resAdv <= 1.0e-4f) return numMainDetected; // no structural advantage at all (44.1/48kHz) -- nothing to rescue with, zero added cost there

    const auto& auxProm = aux.debugProminence();
    const auto& auxMag = aux.debugMagDb();
    int auxSize = (int) auxProm.size();
    int mainSize = (int) mainProminence.size();
    int auxScanLimit = juce::jmin(auxSize - 2, (int) std::ceil((double) promCrossoverHighHz / auxBinHz) + 2);

    int count = numMainDetected;
    for (int ab = 1; ab <= auxScanLimit && count < kMaxDetections; ++ab)
    {
        bool isAuxPeak = auxProm[(size_t) ab] > auxProm[(size_t) (ab - 1)] && auxProm[(size_t) ab] >= auxProm[(size_t) (ab + 1)] && auxProm[(size_t) ab] > lowFloorDb;
        if (! isAuxPeak) continue;

        float auxHzApprox = (float) (ab * auxBinHz);
        float freqWeight = crossoverWeight(auxHzApprox, promCrossoverLowHz, promCrossoverHighHz);
        if (freqWeight <= 1.0e-4f) continue;

        // Location ALWAYS from aux's own raw magnitude (never prominence).
        int auxMagBin = localPeakBin(auxMag.data(), auxSize, ab, 2);
        float auxPhysCons = physicalConsistencyFromBins(ab, auxMagBin);
        float lM = auxMag[(size_t) juce::jmax(0, auxMagBin - 1)], cM = auxMag[(size_t) auxMagBin], rM = auxMag[(size_t) juce::jmin(auxSize - 1, auxMagBin + 1)];
        float auxHz = (float) ((auxMagBin + parabolicDelta(lM, cM, rM)) * auxBinHz);

        float auxEst = 0, auxValRel = 0, auxFreqRel = 0;
        float auxDb = aux.auxProminenceFor(auxHzApprox, &auxEst, &auxValRel, &auxFreqRel);
        (void) auxEst; // superseded by the direct per-bin location computed above

        // Does a MAIN candidate already exist near aux's estimate?
        float auxLogHz = std::log2(juce::jmax(1.0f, auxHz));
        int mergeTarget = -1; float bestDist = 1.0e9f;
        for (int k = 0; k < numMainDetected; ++k)
        {
            float d = std::abs(std::log2(juce::jmax(1.0f, detectedScratch[(size_t) k].centerHz)) - auxLogHz);
            if (d < bestDist) { bestDist = d; mergeTarget = k; }
        }
        bool hasMergeTarget = mergeTarget >= 0 && bestDist <= stabilityToleranceOct * 2.0f;

        // mainPhysicalConsistency AT this spot -- evaluated whether or not
        // main formally detected a candidate there (a spot main's topology
        // never even registered as a local max reads as fully inconsistent
        // here, which is exactly the "main found nothing" rescue trigger).
        int mainApproxBin = juce::jlimit(1, mainSize - 2, (int) std::round((double) auxHz / mainBinHz));
        int mainPeakBinHere = localPeakBin(mainProminence.data(), mainSize, mainApproxBin, 2);
        int mainMagBinHere = localPeakBin(mainMagDb.data(), mainSize, mainApproxBin, 2);
        float mainPhysConsHere = physicalConsistencyFromBins(mainPeakBinHere, mainMagBinHere);

        float auxRescueAuthority = freqWeight * resAdv * auxPhysCons * juce::jlimit(0.0f, 1.0f, auxValRel) * (1.0f - mainPhysConsHere);

        if (hasMergeTarget)
        {
            // MERGE: contribute evidence to the SAME region, never a second
            // one. Main's own detected value/width/etc is never rewritten.
            auto& tgt = detectedScratch[(size_t) mergeTarget];
            float boost = juce::jlimit(0.0f, 1.0f, resAdv * auxPhysCons * auxValRel);
            tgt.candidateEvidence = juce::jlimit(0.0f, 1.0f, tgt.candidateEvidence + (1.0f - tgt.candidateEvidence) * boost * 0.5f);
            tgt.rescueAuthority = juce::jmax(tgt.rescueAuthority, boost); // diagnostic-only
            // Steer location toward aux ONLY when aux is clearly more
            // physically/frequency consistent than main is at this spot --
            // otherwise main's own (already-validated) location stands.
            if (auxPhysCons > mainPhysConsHere + 0.3f && auxFreqRel > 0.5f)
            {
                float w = juce::jlimit(0.0f, 1.0f, auxPhysCons - mainPhysConsHere);
                float logMain = std::log2(juce::jmax(1.0f, tgt.centerHz));
                float logAux = std::log2(juce::jmax(1.0f, auxHz));
                tgt.centerHz = std::pow(2.0f, w * logAux + (1.0f - w) * logMain);
            }
            continue;
        }

        // AUX_RESCUE: only admit with real, non-trivial authority --
        // continuous, no hard cliff, but a near-zero score shouldn't
        // manufacture a candidate main had no structural reason to miss.
        if (auxRescueAuthority <= 0.05f) continue;

        auto& d = detectedScratch[(size_t) count];
        d.centerBin = mainApproxBin; d.rawBinHz = (float) (mainApproxBin * mainBinHz); d.centerHz = auxHz; d.peakDb = auxDb;
        d.widthBins = 1; d.widthHz = (float) auxBinHz; d.integratedDb = auxDb;
        d.candidateEvidence = admissionSmoothstep(lowFloorDb, strongFloorDb, auxDb) * juce::jlimit(0.0f, 1.0f, auxRescueAuthority);
        d.source = CandidateSource::AuxRescue;
        d.rescueAuthority = auxRescueAuthority;
        ++count;
    }
    return count;
}

// PHYSICAL C2.2: fixed-size, no heap allocation. Scans for local maxima
// above lowFloorDb (NOT the old hard peakFloorDb cliff) -- every local max
// down to this permissive floor becomes a candidate with a CONTINUOUS
// candidateEvidence in [0,1] (smoothstep between lowFloorDb/strongFloorDb),
// grown into a contiguous region while prominence stays above (peakDb-6dB)
// or the array ends, capped at kMaxDetections/frame. Whether a candidate
// actually gets a pool slot is decided later, by matching/admission in
// process() -- "candidate ≠ problem": this function only finds shapes in
// the prominence curve, it does not judge relevance.
int ConfidenceEngine::detectPeaks(const std::vector<float>& prominence, int maxPeaks, const std::vector<float>* magDb)
{
    int n = (int) prominence.size();
    int count = 0;
    int b = 1;
    while (b < n - 1 && count < maxPeaks)
    {
        bool isPeak = prominence[(size_t) b] > prominence[(size_t) (b - 1)]
                    && prominence[(size_t) b] >= prominence[(size_t) (b + 1)]
                    && prominence[(size_t) b] > lowFloorDb;
        if (! isPeak) { ++b; continue; }
        float peakDb = prominence[(size_t) b];
        float half = peakDb - 6.0f;
        int lo = b, hi = b;
        while (lo > 0 && prominence[(size_t) (lo - 1)] > half) --lo;
        while (hi < n - 1 && prominence[(size_t) (hi + 1)] > half) ++hi;
        float integrated = 0.0f; for (int i = lo; i <= hi; ++i) integrated += juce::jmax(0.0f, prominence[(size_t) i]);
        // C1.1 (original): sub-bin refinement via 3-point parabolic
        // interpolation on the prominence array. C2.3 item 6 (validated):
        // when magDb is supplied, LOCATION comes from raw magnitude instead
        // -- prominence answers "how much", magnitude answers "where". This
        // is what the aux analyzer's own auxProminenceFor() already does;
        // applying it here too closes the same failure mode in the main
        // engine's own sub-bin estimate (a contaminated neighbor pulling
        // the parabolic fit off-target). Falls back to the original
        // prominence-domain interpolation when magDb is null, preserving
        // exact prior behaviour for any caller that hasn't been updated to
        // supply it yet.
        float delta = 0.0f;
        if (b > 0 && b < n - 1)
        {
            if (magDb != nullptr && magDb->size() == prominence.size())
                delta = parabolicDelta((*magDb)[(size_t) (b - 1)], (*magDb)[(size_t) b], (*magDb)[(size_t) (b + 1)]);
            else
                delta = parabolicDelta(prominence[(size_t) (b - 1)], prominence[(size_t) b], prominence[(size_t) (b + 1)]);
        }
        auto& d = detectedScratch[(size_t) count];
        d.centerBin = b; d.rawBinHz = binToHz(b); d.centerHz = (float) ((b + delta) * sampleRate / fftSize); d.peakDb = peakDb;
        d.widthBins = hi - lo + 1; d.widthHz = d.widthBins * (float) (sampleRate / fftSize);
        d.integratedDb = integrated;
        d.candidateEvidence = admissionSmoothstep(lowFloorDb, strongFloorDb, peakDb);
        ++count;
        b = hi + 1; // skip past this region, avoids re-detecting its own tail as a new peak
    }
    return count;
}

// Shared per-detection update, used both for a detection that matched an
// already-tracked region and for one that just got admitted into a
// free/evicted slot -- keeps the field-update/hysteresis/stability logic in
// exactly one place.
void ConfidenceEngine::applyDetectionToRegion(Region& reg, const DetectedPeak& d)
{
    reg.centerBin = d.centerBin; reg.centerHz = d.centerHz; reg.rawBinHz = d.rawBinHz; reg.peakProminenceDb = d.peakDb;
    reg.widthBins = d.widthBins; reg.widthHz = d.widthHz; reg.integratedEvidenceDb = d.integratedDb;
    reg.candidateEvidence = d.candidateEvidence;
    reg.lastCandidateSource = d.source; reg.lastAuxRescueAuthority = d.rescueAuthority; // C2.3e diagnostics
    reg.lastBridged = false; // C2.3h: a real detectPeaks()/rescue match, not a continuation-bridge frame
    reg.matchedThisFrame = true;
    // Hysteresis (C2.2 item 7): an ALREADY-tracked region only needs
    // continuationFloorDb (lower than strongFloorDb) to keep RISING in
    // persistence -- avoids creation/destruction flicker for a region that
    // dips slightly below the creation-strength floor but is still clearly
    // present. A brand-new region (persistence starts at 0) goes through
    // this exact same formula, so its first frame behaves identically to
    // before C2.2 when its own peakDb already clears continuationFloorDb.
    bool sustaining = d.peakDb >= continuationFloorDb;
    float target = sustaining ? 1.0f : 0.0f;
    float coeff = std::exp(-1.0f / (sustaining ? riseTau : fallTau));
    reg.persistence = target + (reg.persistence - target) * coeff;
    reg.framesPresent++; reg.framesAbsent = 0;
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

// C2.3h -- see header comment for full rationale/provenance.
float ConfidenceEngine::continuationProminenceAt(const std::vector<float>& prominence, float expectedHz, float& outFoundHz) const
{
    double mainBinHz = sampleRate / (double) fftSize;
    int n = (int) prominence.size();
    int approxBin = juce::jlimit(1, n - 2, (int) std::round((double) expectedHz / mainBinHz));
    int bestBin = localPeakBin(prominence.data(), n, approxBin, 2);
    float l = prominence[(size_t) juce::jmax(0, bestBin - 1)];
    float c = prominence[(size_t) bestBin];
    float r = prominence[(size_t) juce::jmin(n - 1, bestBin + 1)];
    outFoundHz = (float) ((bestBin + parabolicDelta(l, c, r)) * mainBinHz);
    return c;
}

void ConfidenceEngine::process(const std::vector<float>& prominence, const LowFrequencyHarmonicAnalyzer* aux, const std::vector<float>* magDb)
{
    currentAux = aux;
    // C2.3e: main's own topology is ALWAYS detected on the raw, unmodified
    // prominence array -- never passing magDb here, so this is
    // byte-identical to before C2.3 regardless of aux/magDb availability.
    int numDetected = detectPeaks(prominence, kMaxDetections);
    if (aux != nullptr && magDb != nullptr)
        numDetected = appendAuxRescueCandidates(prominence, *magDb, *aux, numDetected);

    // Match each detection to an existing active region by log-frequency
    // proximity (closest within tolerance wins); unmatched detections are
    // admitted (or not) below. matchedRegion[k] = -1 if unmatched.
    std::array<int, kMaxDetections> matchedRegion; matchedRegion.fill(-1);
    std::array<bool, kMaxRegions> regionWasMatched; regionWasMatched.fill(false);
    for (auto& reg : regionPool) reg.matchedThisFrame = false;

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

    // Decay/free active regions that got NO detection at all this frame.
    // C2.3h region continuation bridge: BEFORE falling back to blind decay,
    // check whether the physical structure that sustained this region is
    // still measurably present at its own expected location -- not a new
    // local maximum (detectPeaks() already tried that this frame and
    // failed), just the prominence array's own value there. Ground-truth-
    // validated (C2.3g) that this genuinely happens (physical energy
    // present, strict local-max test failing) rather than being a case of
    // the resonance actually having stopped. Time-budgeted in ms (not a
    // fixed frame count) so it means the same physical duration at every
    // sample rate; gated by log-frequency distance so a region can never be
    // bridged onto unrelated content that happens to be nearby.
    int bridgeMaxFrames = juce::jmax(1, (int) std::round(bridgeMaxTimeMs * (float) sampleRate / (float) hopSize / 1000.0f));
    for (int r = 0; r < kMaxRegions; ++r)
    {
        auto& reg = regionPool[(size_t) r];
        if (! reg.active || regionWasMatched[(size_t) r]) continue;

        bool bridged = false;
        if (bridgeMaxTimeMs > 0.0f && reg.framesAbsent < bridgeMaxFrames)
        {
            float foundHz = 0.0f;
            float contProm = continuationProminenceAt(prominence, reg.centerHz, foundHz);
            float logDist = std::abs(std::log2(juce::jmax(1.0f, foundHz)) - std::log2(juce::jmax(1.0f, reg.centerHz)));
            if (logDist <= stabilityToleranceOct)
            {
                // Reuses the ALREADY-calibrated lowFloorDb/continuationFloorDb
                // hysteresis pair (item 6: the same strongFloor-vs-
                // continuationFloor concept, now applied to continuity
                // instead of just initial admission) -- no new arbitrary
                // threshold introduced.
                float currentLocalEvidence = admissionSmoothstep(lowFloorDb, continuationFloorDb, contProm);
                if (currentLocalEvidence > 0.02f)
                {
                    // Confidence is never frozen (item 4): continuedEvidence
                    // tracks the CURRENT physical measurement, scaled down
                    // further the longer this region has gone without a real
                    // detection -- so a genuinely-ended resonance still
                    // fades out over the bridge window even if some residual
                    // energy briefly lingers there, and a resonance that
                    // truly returns to full strength recovers immediately
                    // (temporalContinuityFactor resets to 1 the moment
                    // framesAbsent resets to 0 on the next real match).
                    float temporalContinuityFactor = juce::jlimit(0.0f, 1.0f, 1.0f - (float) reg.framesAbsent / (float) bridgeMaxFrames);
                    float continuedEvidence = currentLocalEvidence * temporalContinuityFactor;
                    reg.candidateEvidence = continuedEvidence;
                    reg.peakProminenceDb = contProm;
                    reg.centerHz = foundHz;
                    float target = continuedEvidence;
                    float coeff = std::exp(-1.0f / fallTau);
                    reg.persistence = target + (reg.persistence - target) * coeff;
                    reg.framesAbsent++;
                    reg.framesPresent = 0;
                    reg.lastBridged = true;
                    bridged = true;
                }
            }
        }
        if (! bridged)
        {
            float fallCoeff = std::exp(-1.0f / fallTau);
            reg.persistence *= fallCoeff;
            reg.candidateEvidence *= fallCoeff; // eviction-priority signal fades too, not just persistence
            reg.framesAbsent++;
            reg.framesPresent = 0;
            reg.lastBridged = false;
        }
        if (reg.persistence < 0.02f) reg = Region{}; // free back to the pool
    }

    // Matched detections update their already-tracked region in place.
    for (int k = 0; k < numDetected; ++k)
    {
        int r = matchedRegion[(size_t) k];
        if (r < 0) continue;
        applyDetectionToRegion(regionPool[(size_t) r], detectedScratch[(size_t) k]);
    }

    // PHYSICAL C2.2: unmatched detections are admitted via a free slot or
    // via DETERMINISTIC priority-based eviction of the current weakest
    // occupant (Policy P1: mostly candidateEvidence, small continuity
    // bonus -- chosen after the C2.2 adversarial comparison found P1 and
    // P2 equivalent in every test, so the simpler/more auditable one was
    // kept). Strongest new candidates are processed FIRST (sorted
    // descending by candidateEvidence, fixed-size insertion sort, no heap
    // allocation) so a genuinely strong new resonance never loses a slot to
    // a weaker one purely because of bin order -- this replaces the old
    // "first bin index wins, pool full = silently dropped" behaviour.
    std::array<int, kMaxDetections> unmatchedIdx; int numUnmatched = 0;
    for (int k = 0; k < numDetected; ++k) if (matchedRegion[(size_t) k] < 0) unmatchedIdx[(size_t) numUnmatched++] = k;
    for (int i = 1; i < numUnmatched; ++i)
    {
        int key = unmatchedIdx[(size_t) i]; float keyEv = detectedScratch[(size_t) key].candidateEvidence; int j = i - 1;
        while (j >= 0 && detectedScratch[(size_t) unmatchedIdx[(size_t) j]].candidateEvidence < keyEv) { unmatchedIdx[(size_t) (j + 1)] = unmatchedIdx[(size_t) j]; --j; }
        unmatchedIdx[(size_t) (j + 1)] = key;
    }
    for (int ui = 0; ui < numUnmatched; ++ui)
    {
        int k = unmatchedIdx[(size_t) ui];
        auto& d = detectedScratch[(size_t) k];
        int r = -1;
        for (int f = 0; f < kMaxRegions; ++f) if (! regionPool[(size_t) f].active) { r = f; break; }
        if (r < 0)
        {
            int weakest = -1; float weakestScore = 1.0e9f;
            for (int p = 0; p < kMaxRegions; ++p)
            {
                float score = admissionPriorityScore(regionPool[(size_t) p].candidateEvidence, regionPool[(size_t) p].matchedThisFrame);
                if (score < weakestScore) { weakestScore = score; weakest = p; }
            }
            float newScore = admissionPriorityScore(d.candidateEvidence, true);
            if (weakest >= 0 && newScore > weakestScore) r = weakest;
        }
        if (r < 0) continue; // deterministically the lowest-priority contender THIS frame (not "arrived last") -- dropped, no allocation
        regionPool[(size_t) r] = Region{};
        regionPool[(size_t) r].active = true;
        applyDetectionToRegion(regionPool[(size_t) r], d);
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
    for (auto& reg : regionPool) { reg.harmonicLikelihood = 0.0f; reg.harmonicExpectedHz = 0.0f; reg.harmonicDistanceCents = 0.0f; reg.harmonicSiblingRefDb = 0.0f; reg.harmonicSiblingRefValid = false; }

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
    // Same structural reliability gate as LowFrequencyHarmonicAnalyzer's
    // own (Blocker 2): 0 or 1 supporting partial can never claim reliability
    // above 0, however clean that single match looks -- a lone match is
    // exactly the ambiguous case (e.g. a resolution collision) that should
    // read as "insufficient evidence", not "confident winner".
    // Note on evidenceDensity's rescale: bestEvidence/bestMatches (the
    // average per-match closeness) rarely approaches 1.0 even for an
    // obviously genuine, richly-supported harmonic series, because
    // harmonicClosenessFor()'s gaussian(sigma=60 cents) x resolutionCap
    // model is deliberately conservative (measured directly: an 11-match
    // clean series scored evidenceDensity~0.62, not ~1.0). Used as a
    // multiplicative reliability WEIGHT (unlike the aux analyzer's version,
    // which only gates a >=0.5 reporting cutoff), that would systematically
    // under-credit exactly the well-supported cases this gate is supposed
    // to trust -- so the density term is rescaled against ~0.5 ("a decent
    // real match") as its practical ceiling, not the unreachable
    // theoretical 1.0.
    { float matchTerm = bestMatches <= 1 ? 0.0f : juce::jlimit(0.0f, 1.0f, (float) (bestMatches - 1) / 3.0f);
      float evidenceDensity = bestMatches > 0 ? juce::jlimit(0.0f, 1.0f, (bestEvidence / (float) bestMatches) / 0.5f) : 0.0f;
      mainF0Reliability = matchTerm * evidenceDensity; }
    if (f0Region < 0) { harmonicSupportCoherence = 0.0f; return; } // no region has ANY harmonic evidence from another region -- nothing to anchor a series on

    float f0Hz = regionPool[(size_t) f0Region].centerHz;
    // C2.3k-R3 item C: harmonicSupportCoherence -- reuses ALREADY-existing,
    // already-temporally-smoothed per-region state (persistence, stability)
    // to answer "have the partials supporting this f0 actually persisted
    // and stayed frequency-stable over time, or did they just happen to
    // line up this one frame?" No new history buffer: persistence (EMA
    // rise/fall) and stability (log-Hz variance over the existing fixed-size
    // ring) already ARE the temporal-continuity signal, just never
    // previously folded into the classification-reliability question. A
    // genuine harmonic series's siblings have been matched/tracked
    // frame-to-frame (high persistence+stability); noise-coincidence
    // "partials" are freshly admitted or frequency-jittery (low
    // persistence and/or stability) -- zero heap, zero locks, O(kMaxRegions).
    float coherenceSum = 0.0f; int coherenceCount = 0;
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
        if (closeness > 0.05f) { coherenceSum += reg.persistence * reg.stability; ++coherenceCount; }
    }
    harmonicSupportCoherence = coherenceCount > 0 ? juce::jlimit(0.0f, 1.0f, coherenceSum / (float) coherenceCount) : 0.0f;
    // f0's own credit as "a genuine fundamental" scales with accumulated
    // evidence -- 2 clean matches (~2.0 evidence) is enough to be near-
    // certain; a single marginal match stays proportionally low, never a
    // flat 1.0 for "I found one thing that might be a harmonic".
    regionPool[(size_t) f0Region].harmonicLikelihood = juce::jlimit(0.0f, 1.0f, bestEvidence / 2.0f);

    // PHYSICAL C2, item 4: excess-prominence reference. A harmonic's
    // "expected" level is the mean prominence of ITS OWN siblings (other
    // regions matched as harmonics of the same f0 this frame) -- RELATIVE,
    // never a fixed dB constant, since a normal harmonic series can sit
    // anywhere from ~15dB to ~45dB of prominence depending on signal level
    // alone. Needs >=2 siblings (excluding self) to be a meaningful
    // reference; with fewer, excess discount stays disabled for that region
    // this frame (harmonicSiblingRefValid stays false).
    int siblingCount = 0; float siblingSum = 0.0f;
    for (int r = 0; r < kMaxRegions; ++r)
    {
        if (r == f0Region) continue;
        auto& reg = regionPool[(size_t) r];
        if (! reg.active || reg.harmonicLikelihood <= 0.05f) continue;
        ++siblingCount; siblingSum += reg.peakProminenceDb;
    }
    if (siblingCount >= 2)
    {
        for (int r = 0; r < kMaxRegions; ++r)
        {
            if (r == f0Region) continue;
            auto& reg = regionPool[(size_t) r];
            if (! reg.active || reg.harmonicLikelihood <= 0.05f) continue;
            // Leave-one-out mean: this region's own prominence excluded
            // from its own reference, so a single loud outlier doesn't
            // partly drag its own baseline up with it.
            reg.harmonicSiblingRefDb = (siblingSum - reg.peakProminenceDb) / (float) (siblingCount - 1 >= 1 ? siblingCount - 1 : 1);
            reg.harmonicSiblingRefValid = siblingCount >= 3; // need >=2 OTHER siblings for a leave-one-out mean to mean anything
        }
    }
}

float ConfidenceEngine::crossoverWeight(float hz, float lowHz, float highHz)
{
    if (hz <= lowHz) return 1.0f;
    if (hz >= highHz) return 0.0f;
    float t = (hz - lowHz) / (highHz - lowHz);
    return 1.0f - (t * t * (3.0f - 2.0f * t)); // smoothstep: continuous value AND derivative at both edges
}

float ConfidenceEngine::computeExcessFactor(float peakProminenceDb, float siblingRefDb, bool siblingRefValid) const
{
    if (! siblingRefValid) return 1.0f; // no meaningful "expected level" this frame -- don't discount
    float excessDb = peakProminenceDb - siblingRefDb;
    if (excessDb <= excessProminenceCeilingDb) return 1.0f;
    float t = (excessDb - excessProminenceCeilingDb) / excessProminenceRangeDb;
    return juce::jlimit(0.0f, 1.0f, 1.0f - t);
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
        // Pure multiplication of 4+ independent [0,1] evidences collapses
        // too aggressively in practice (measured directly: a genuinely
        // isolated, persistent, stable, non-harmonic resonance only reached
        // ~0.26 confidence with 4-way multiplication, barely above a
        // legitimate harmonic partial's ~0.17-0.20 -- not enough
        // separation to be useful). combinedEvidence uses a weighted mean
        // of the four independent evidences instead (gentler, avoids
        // multiplicative collapse). This IS baseProblemEvidence -- kept
        // explicit and untouched by harmonic reasoning until the final step.
        float combinedEvidence = 0.30f * prominenceEvidence + 0.30f * persistenceEvidence + 0.25f * stabilityEvidence + 0.15f * widthEvidence;

        // PHYSICAL C2 (confidence-aware blend, revised after the Case-B
        // @44.1/48kHz finding): auxWeight is now scaled by the aux
        // analyzer's OWN reliability, not just frequency -- so an aux f0
        // that is geometrically present but structurally untrustworthy
        // (e.g. a resolution collision) cannot silently zero out (or
        // falsely certify) a region's harmonic likelihood. When aux is
        // uncertain, mainWeight grows and the host-rate reasoning regains
        // authority -- but ONLY as much as mainF0Reliability (this frame's
        // OWN structural gate, mirroring the aux analyzer's >=2-partial
        // requirement) actually earns; a lone weak host match doesn't get
        // treated as certain either. effectiveHarmonicReliability is the
        // reliability-weighted blend of BOTH sources' own reliabilities --
        // if aux AND host are both uncertain this frame, it stays low, so
        // the region's protection authority stays low too (neither
        // artificially "harmonic" nor "non-harmonic"), rather than
        // defaulting to host=1.0 the moment aux looks shaky.
        float mainLikelihood = reg.harmonicLikelihood;
        float mainReliability = mainF0Reliability;
        float auxLikelihood = 0.0f, auxReliabilityRaw = 0.0f;
        if (currentAux != nullptr) { auxLikelihood = currentAux->harmonicLikelihoodFor(reg.centerHz); auxReliabilityRaw = currentAux->currentContext().f0Reliability; }
        // C2.3j -- NON-DILUTING HARMONIC PROTECTION FUSION. The previous
        // reliability-weighted convex blend (auxWeight*auxLikelihood +
        // mainWeight*mainLikelihood) let an aux source that is merely
        // UNCERTAIN at this frequency dilute a MAIN likelihood that was
        // already confidently 1.000 with 6 supporting partials -- e.g. an
        // 80Hz fundamental with mainLikelihood=1.000, mainReliability=0.774
        // still ended up with effectiveLikelihood=0.534 purely because
        // aux's own (lower, independent) geometric estimate at that
        // frequency pulled it down. That's semantically wrong: UNKNOWN !=
        // NON-HARMONIC. Each source's own protection evidence is now kept
        // separate and combined via max() -- never averaged -- so a weak
        // source can never subtract from a strong one's already-earned
        // protection. auxStructuralAuthority keeps the existing crossover
        // gating (aux only has real say in its own low-frequency operating
        // range); it does not, by itself, discount main's own evidence.
        float auxStructuralAuthority = (currentAux != nullptr) ? crossoverWeight(reg.centerHz, auxCrossoverLowHz, auxCrossoverHighHz) : 0.0f;
        float mainProtectionEvidence = juce::jlimit(0.0f, 1.0f, mainLikelihood * mainReliability);
        float auxProtectionEvidence = juce::jlimit(0.0f, 1.0f, auxLikelihood * auxReliabilityRaw * auxStructuralAuthority);
        float structuralHarmonicProtection = juce::jmax(mainProtectionEvidence, auxProtectionEvidence);
        // effectiveLikelihood/effectiveReliability kept as diagnostics,
        // reporting whichever source's own numbers actually won the max()
        // this frame (not a blend) -- so existing test/reporting code that
        // reads these fields still sees a coherent, attributable pair.
        float effectiveLikelihood, effectiveReliability;
        if (mainProtectionEvidence >= auxProtectionEvidence) { effectiveLikelihood = mainLikelihood; effectiveReliability = mainReliability; }
        else { effectiveLikelihood = auxLikelihood; effectiveReliability = auxReliabilityRaw * auxStructuralAuthority; }
        float excessFactor = computeExcessFactor(reg.peakProminenceDb, reg.harmonicSiblingRefDb, reg.harmonicSiblingRefValid);
        // effectiveHarmonicProtection: reliability scales how much
        // AUTHORITY the harmonic-protection term gets, it does NOT pull
        // baseProblemEvidence (combinedEvidence) toward a neutral guess --
        // when reliability is low, protection just weakens, and confidence
        // rises toward whatever combinedEvidence already says on its own.
        // harmonicMaxPenalty (0.7) UNCHANGED this round (C2.3j scope: fusion
        // only) -- excessFactor still applies AFTER the structural fusion,
        // so excessive harmonics remain distinguishable from normal ones.
        float effectiveHarmonicProtection = juce::jlimit(0.0f, 1.0f, structuralHarmonicProtection * excessFactor);
        float harmonicPenalty = 1.0f - effectiveHarmonicProtection * harmonicMaxPenalty; // kept, computed exactly as before (diagnostic continuity) -- NOT what drives confidence any more, see C2.3k below

        // ---------------- C2.3k: EXISTENCE vs PROBLEM confidence ----------------
        // existenceConfidence answers "does this structure really exist and
        // persist?" -- exactly the old combinedEvidence formula, completely
        // untouched (prominence/persistence/stability/width weights
        // unchanged). This alone must NEVER be what selectivity acts on.
        float existenceConfidence = combinedEvidence;

        // "Knownness" of the harmonic verdict this frame -- whichever source
        // has higher reliability -- kept only as a diagnostic/gate for the
        // UNKNOWN fallback (Source C below), NOT used any more to pick a
        // single source's likelihood for Source A (see C2.3k-R2 item A).
        float chosenReliability = juce::jmax(mainReliability, auxReliabilityRaw * auxStructuralAuthority);

        // C2.3k-R5 -- ROOT MEMBERSHIP (n=1), evaluated for BOTH sources
        // against a SHARED harmonic-grid reference: MAIN's own f0Hz, used
        // only when MAIN's f0 is valid, partial-supported, and
        // f0ClassificationReliability is sufficient. R4 tried evaluating
        // each source strictly against its OWN independent f0 estimate
        // (aux against aux's own currentContext().f0Hz) -- measured
        // directly, this made things WORSE: aux's own f0 estimate for an
        // isolated 80Hz fundamental is itself imprecise enough that
        // aux's-own-root-membership came out barely different from aux's
        // raw (already-low) likelihood, undoing the R3 improvement. This is
        // NOT source dilution -- mainLikelihood/auxLikelihood,
        // mainReliability/auxReliability, and
        // mainNonHarmonicEvidence/auxNonHarmonicEvidence all stay fully
        // separate below. Only the F0/harmonic-GRID hypothesis itself is
        // shared -- the single best available answer to "where is n=1",
        // used as a geometric reference point for BOTH sources' own
        // membership questions, exactly as one already-vetted F0 estimate
        // should be used for any harmonic-grid geometry regardless of which
        // analyzer is asking.
        float mainF0Hz = (f0WinnerIndex >= 0) ? regionPool[(size_t) f0WinnerIndex].centerHz : 0.0f;
        float f0ClassificationReliability = mainF0Reliability * harmonicSupportCoherence;
        float harmonicGridF0Hz = (mainF0Hz > 1.0f && f0ClassificationReliability > 0.0f) ? mainF0Hz : 0.0f;
        float rootMembership = 0.0f;
        if (harmonicGridF0Hz > 1.0f)
        {
            float ratio = reg.centerHz / harmonicGridF0Hz;
            if (ratio > 0.6f && ratio < 1.4f) // genuinely near n=1, not some other integer multiple
            {
                float expectedHz, distCents;
                float closeness = harmonicClosenessFor(reg.centerHz, harmonicGridF0Hz, harmonicSigmaCents, (float) sampleRate, fftSize, expectedHz, distCents);
                rootMembership = closeness * f0ClassificationReliability;
            }
        }
        float mainRootMembership = rootMembership, auxRootMembership = rootMembership; // same shared grid hypothesis, diagnostics kept separate for reporting
        float mainStructuralMembership = juce::jmax(mainLikelihood, rootMembership);
        float auxStructuralMembership = juce::jmax(auxLikelihood, rootMembership);

        // C2.3k-R4 item 2 (kept) -- auxClassificationReliability: aux's own
        // f0Reliability (currentContext().f0Reliability) already requires
        // >=2 supporting partials and already blends evidence density with
        // f0StableFrameCount-based temporal stability (see
        // LowFrequencyHarmonicAnalyzer::runAnalysisFrame) -- i.e. it already
        // answers "do I have enough COHERENT, temporally-persistent context
        // to classify with", as opposed to a hypothetical raw "does some
        // peak exist here" detection reliability (that would be
        // auxProminenceFor()'s own outValueReliability, a different
        // accessor never mixed into this). No new state needed inside
        // LowFrequencyHarmonicAnalyzer; this just names the ALREADY-correct
        // quantity explicitly rather than treating it as an undifferentiated
        // "auxReliabilityRaw".
        float auxClassificationReliability = auxReliabilityRaw;

        // C2.3k-R2 item A / R3 item E / R4 item 1 -- Source A (non-harmonic
        // evidence) non-diluting fusion: each source's classification
        // reliability (NOT a raw/undifferentiated one) times (1 -
        // structuralMembership), computed SEPARATELY, combined via max().
        // A source with low classification reliability now correctly
        // contributes ~0 -- UNKNOWN, not NON-HARMONIC -- regardless of what
        // its raw likelihood happened to read.
        float mainNonHarmonicEvidence = juce::jlimit(0.0f, 1.0f, f0ClassificationReliability * (1.0f - mainStructuralMembership));
        float auxNonHarmonicEvidence = juce::jlimit(0.0f, 1.0f, auxClassificationReliability * (1.0f - auxStructuralMembership) * auxStructuralAuthority);
        float nonHarmonicSupportEvidence = juce::jmax(mainNonHarmonicEvidence, auxNonHarmonicEvidence);
        // Source B -- excessive-harmonic evidence: structure IS recognized as
        // harmonic (structuralHarmonicProtection, before the excess
        // discount) but this partial is anomalously loud relative to its own
        // siblings (1-excessFactor). A normal, unboosted harmonic has
        // excessFactor~1 -> ~0 evidence here; only genuine excess registers.
        float excessiveHarmonicEvidence = juce::jlimit(0.0f, 1.0f, structuralHarmonicProtection * (1.0f - excessFactor));
        // reliableProblemEvidence: the two AFFIRMATIVE, source-attributable
        // problem signals -- "confidently non-harmonic" or "confidently an
        // excessive partial". Selectivity (see actionWeight()) can trust
        // this fully at any strictness level.
        float reliableProblemEvidence = juce::jmax(nonHarmonicSupportEvidence, excessiveHarmonicEvidence);

        // C2.3k-R2 item B -- renamed (was strongIndependentAnomalyEvidence):
        // this is the UNKNOWN-context fallback, not a general-purpose extra
        // problem signal, and STRONG PROMINENCE != PROBLEM PROOF -- a loud,
        // narrow, perfectly musical tone can look identical to this feature
        // set. It must not fire just because a confidently-harmonic region
        // is loud and wide (gated by (1-chosenReliability), unchanged from
        // C2.3k). Its remaining authority over ACTION is further reduced by
        // Selectivity itself in actionWeight() below -- unlike
        // reliableProblemEvidence, this signal alone should never justify
        // high action authority once Selectivity demands real certainty.
        float unknownAnomalySupport = admissionSmoothstep(0.85f, 0.98f, prominenceEvidence) * widthEvidence * (1.0f - chosenReliability);

        // problemDecisionEvidence / problemConfidence stay SELECTIVITY-FREE
        // (item D): this is a description of evidence, not yet an action
        // decision -- combines reliableProblemEvidence and
        // unknownAnomalySupport via max() (never summed), same non-diluting
        // principle throughout. Selectivity's own discount of the UNKNOWN
        // component happens separately, in actionWeight().
        float problemDecisionEvidence = juce::jmax(reliableProblemEvidence, unknownAnomalySupport);
        float problemConfidence = juce::jlimit(0.0f, 1.0f, existenceConfidence * problemDecisionEvidence);

        reg.lastProminenceEvidence = prominenceEvidence; reg.lastPersistenceEvidence = persistenceEvidence;
        reg.lastStabilityEvidence = stabilityEvidence; reg.lastWidthEvidence = widthEvidence; reg.lastHarmonicPenalty = harmonicPenalty;
        reg.lastBaseEvidence = combinedEvidence;
        reg.auxHarmonicLikelihood = auxLikelihood; reg.auxReliability = auxReliabilityRaw;
        reg.effectiveLikelihood = effectiveLikelihood; reg.effectiveReliability = effectiveReliability;
        reg.excessFactor = excessFactor; reg.effectiveHarmonicProtection = effectiveHarmonicProtection;
        reg.mainProtectionEvidence = mainProtectionEvidence; reg.auxProtectionEvidence = auxProtectionEvidence;
        reg.structuralHarmonicProtection = structuralHarmonicProtection;
        reg.existenceConfidence = existenceConfidence; reg.harmonicContextReliability = chosenReliability;
        reg.rootMembership = rootMembership; reg.mainStructuralMembership = mainStructuralMembership;
        reg.auxStructuralMembership = auxStructuralMembership; reg.f0ClassificationReliability = f0ClassificationReliability;
        reg.mainRootMembership = mainRootMembership; reg.auxRootMembership = auxRootMembership;
        reg.auxClassificationReliability = auxClassificationReliability;
        reg.mainNonHarmonicEvidence = mainNonHarmonicEvidence; reg.auxNonHarmonicEvidence = auxNonHarmonicEvidence;
        reg.nonHarmonicSupportEvidence = nonHarmonicSupportEvidence; reg.excessiveHarmonicEvidence = excessiveHarmonicEvidence;
        reg.reliableProblemEvidence = reliableProblemEvidence; reg.unknownAnomalySupport = unknownAnomalySupport;
        reg.problemDecisionEvidence = problemDecisionEvidence; reg.problemConfidence = problemConfidence;

        reg.confidence = problemConfidence; // evidence description, selectivity-free (see actionWeight() for the action decision)
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
