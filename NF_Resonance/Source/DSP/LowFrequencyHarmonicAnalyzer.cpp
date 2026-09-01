#include "LowFrequencyHarmonicAnalyzer.h"
#include <cmath>
#include <algorithm>

void LowFrequencyHarmonicAnalyzer::prepare(double hostSampleRate, float f0SearchLowHz, float f0SearchHighHz, float harmonicEvidenceHighHz)
{
    hostRate = hostSampleRate;
    f0Lo = f0SearchLowHz; f0Hi = f0SearchHighHz; harmHi = harmonicEvidenceHighHz;

    // Decimation factor: keep the internal analysis rate close to 44.1-48kHz
    // regardless of host rate (item 2).
    if (hostRate <= 60000.0) decimation = 1;
    else if (hostRate <= 120000.0) decimation = 2;
    else decimation = 4;
    decimatedRate = hostRate / (double) decimation;

    designAntiAlias();
    for (auto& bq : aaFilter) bq.reset();

    // Host hop is assumed 512 (matches SpectralEngine's own hop everywhere
    // else in this codebase) -- the analyzer updates at the SAME cadence in
    // real time, at any host rate, so it never adds extra delay relative
    // to the main engine's own frame rate.
    const int hostHop = 512;
    hopDecimatedSamples = juce::jmax(1, hostHop / decimation);

    for (int i = 0; i < kAnalysisFftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kAnalysisFftSize - 1));

    prom.prepare(kAnalysisFftSize / 2 + 1, decimatedRate, kAnalysisFftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    magDbVecReused.assign((size_t) (kAnalysisFftSize / 2 + 1), -120.0f);
    promVecReused.assign((size_t) (kAnalysisFftSize / 2 + 1), 0.0f);

    reset();
}

void LowFrequencyHarmonicAnalyzer::reset()
{
    ring.fill(0.0f); ringWrite = 0; ringFilled = 0;
    decimationPhase = 0; samplesSinceLastAnalysis = 0;
    for (auto& bq : aaFilter) bq.reset();
    rawContext = Context{}; smoothedContext = Context{};
    f0StabilityLastHz = 0.0f; f0StableFrameCount = 0;
}

// RBJ cookbook lowpass biquad, cutoff placed comfortably below the NEW
// (post-decimation) Nyquist -- 0.42x the decimated Nyquist gives real
// anti-alias margin without being so conservative it eats into the
// 40-800Hz search range's own upper harmonics.
void LowFrequencyHarmonicAnalyzer::designAntiAlias()
{
    if (decimation <= 1) { for (auto& bq : aaFilter) { bq = Biquad{}; } return; } // no filtering needed, 1:1

    double cutoffHz = 0.42 * (decimatedRate * 0.5);
    double q = 0.7071; // Butterworth-ish per stage; 2 stages cascaded = 4th order overall
    double w0 = juce::MathConstants<double>::twoPi * cutoffHz / hostRate;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * q);
    double b0 = (1 - cosw0) / 2, b1 = 1 - cosw0, b2 = (1 - cosw0) / 2;
    double a0 = 1 + alpha, a1 = -2 * cosw0, a2 = 1 - alpha;
    Biquad stage;
    stage.b0 = (float) (b0 / a0); stage.b1 = (float) (b1 / a0); stage.b2 = (float) (b2 / a0);
    stage.a1 = (float) (a1 / a0); stage.a2 = (float) (a2 / a0);
    aaFilter[0] = stage; aaFilter[1] = stage; // identical cascaded stages
}

void LowFrequencyHarmonicAnalyzer::pushSamples(const float* monoIn, int numHostSamples)
{
    for (int i = 0; i < numHostSamples; ++i)
    {
        float x = monoIn[i];
        if (decimation > 1) { x = aaFilter[0].process(x); x = aaFilter[1].process(x); }

        ++decimationPhase;
        if (decimationPhase < decimation) continue; // drop the (already-filtered-band-limited) in-between samples
        decimationPhase = 0;

        ring[(size_t) ringWrite] = x;
        ringWrite = (ringWrite + 1) % kRingCapacity;
        ringFilled = juce::jmin(kRingCapacity, ringFilled + 1);
        ++samplesSinceLastAnalysis;

        if (samplesSinceLastAnalysis >= hopDecimatedSamples && ringFilled >= kAnalysisFftSize)
        {
            samplesSinceLastAnalysis = 0;
            runAnalysisFrame();
        }
    }
}

void LowFrequencyHarmonicAnalyzer::runAnalysisFrame()
{
    // Copy the most recent kAnalysisFftSize decimated samples out of the
    // ring, oldest-to-newest, windowed straight into the FFT scratch --
    // fixed-size, no allocation.
    int start = (ringWrite - kAnalysisFftSize + kRingCapacity) % kRingCapacity;
    for (int i = 0; i < kAnalysisFftSize; ++i)
    {
        int idx = (start + i) % kRingCapacity;
        fftScratch[(size_t) i] = ring[(size_t) idx] * window[(size_t) i];
    }
    std::fill(fftScratch.begin() + kAnalysisFftSize, fftScratch.end(), 0.0f);
    fft.performRealOnlyForwardTransform(fftScratch.data());

    const int bins = kAnalysisFftSize / 2 + 1;
    for (int b = 0; b < bins; ++b)
    {
        float re = fftScratch[(size_t) 2 * b], im = (b == 0 || b == bins - 1) ? 0.0f : fftScratch[(size_t) 2 * b + 1];
        magDbScratch[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kAnalysisFftSize + 1e-12f, -120.0f);
    }
    std::copy(magDbScratch.begin(), magDbScratch.begin() + bins, magDbVecReused.begin()); // reused buffer, no allocation
    prom.computeProminence(magDbVecReused, 4.0f, promVecReused); // promVecReused already sized correctly -- computeProminence only resizes if the size mismatches, which it never will here
    std::copy(promVecReused.begin(), promVecReused.end(), promScratch.begin());

    // Peak-pick within [f0Lo, harmHi] (need to see partials above f0Hi to
    // confirm a bass fundamental, per item 3 -- f0 SEARCH range vs
    // HARMONIC EVIDENCE range are deliberately different).
    double binHz = decimatedRate / kAnalysisFftSize;
    int loBin = juce::jmax(1, (int) std::floor(f0Lo / binHz));
    int hiBin = juce::jmin(bins - 2, (int) std::ceil(harmHi / binHz));
    int numPeaks = 0;
    for (int b = loBin; b <= hiBin && numPeaks < kMaxF0Candidates; ++b)
    {
        // Prominence decides WHERE a peak is (it says "how much this stands
        // out"), but the sub-bin LOCATION comes from the raw magnitude
        // curve, not from the already baseline-subtracted prominence curve
        // -- root-caused (Blocker 2, F0 bias investigation): interpolating
        // on prominence itself carries a real, always-positive systematic
        // bias (confirmed via a dedicated pure-tone/sweep diagnostic,
        // ~0.02-0.25 bins even for an isolated tone), because the BROAD/
        // MEDIUM baseline subtraction is not symmetric around a true peak.
        // Raw magnitude has no such baseline subtraction and interpolates
        // near-zero-bias on an isolated tone.
        if (promScratch[(size_t) b] > 2.0f && promScratch[(size_t) b] > promScratch[(size_t) (b - 1)] && promScratch[(size_t) b] >= promScratch[(size_t) (b + 1)])
        {
            float delta = 0.0f;
            float l = magDbScratch[(size_t) (b - 1)], c = magDbScratch[(size_t) b], r = magDbScratch[(size_t) (b + 1)];
            float denom = l - 2.0f * c + r;
            if (std::abs(denom) > 1.0e-6f) delta = juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom);
            peakScratch[(size_t) numPeaks].hz = (float) ((b + delta) * binHz);
            peakScratch[(size_t) numPeaks].db = promScratch[(size_t) b];
            ++numPeaks;
        }
    }

    // Same match-count-first F0 scoring as ConfidenceEngine (Blocker 1 fix)
    // -- only candidates within the F0 SEARCH range compete to be f0;
    // partials up to harmHi may support them.
    lastNumPeaks = numPeaks;
    for (auto& d : f0CandidateDebug) d = F0CandidateInfo{};
    int bestIdx = -1, bestMatches = 0; float bestEvidence = 0.0f;
    for (int c = 0; c < numPeaks; ++c)
    {
        if (peakScratch[(size_t) c].hz < f0Lo || peakScratch[(size_t) c].hz > f0Hi) continue;
        float f0Hz = peakScratch[(size_t) c].hz;
        float evidence = 0.0f; int matches = 0;
        for (int r = 0; r < numPeaks; ++r)
        {
            if (r == c) continue;
            float ratio = peakScratch[(size_t) r].hz / f0Hz;
            if (ratio < 1.4f) continue;
            int n = juce::jmax(1, (int) std::round(ratio));
            float expected = f0Hz * (float) n;
            float centsOff = 1200.0f * std::log2(peakScratch[(size_t) r].hz / expected);
            float sigma = 60.0f; // matches ConfidenceEngine's own C1 provisional calibration
            float gaussian = std::exp(-0.5f * (centsOff / sigma) * (centsOff / sigma));
            float binWidthCentsAtF0 = 1200.0f * std::log2(1.0f + (float) binHz / juce::jmax(1.0f, f0Hz));
            float cap = juce::jlimit(0.15f, 1.0f, 1.0f - binWidthCentsAtF0 / 1200.0f);
            float closeness = gaussian * cap;
            if (closeness > 0.05f) { evidence += closeness; ++matches; }
        }
        f0CandidateDebug[(size_t) c] = { true, f0Hz, evidence, matches };
        bool better = (matches > bestMatches) || (matches == bestMatches && evidence > bestEvidence + 1.0e-6f);
        if (bestIdx < 0 || better) { bestIdx = c; bestMatches = matches; bestEvidence = evidence; }
    }

    rawContext.valid = bestIdx >= 0 && bestMatches > 0;
    rawContext.f0Hz = rawContext.valid ? peakScratch[(size_t) bestIdx].hz : 0.0f;
    rawContext.supportingPartials = bestMatches;
    rawContext.f0Score = bestEvidence;
    rawContext.f0Confidence = rawContext.valid ? juce::jlimit(0.0f, 1.0f, bestEvidence / 2.0f) : 0.0f;

    // Structural reliability gate: a winner with 0 or 1 supporting partials
    // contributes ZERO reliability, however high its raw evidence/confidence
    // looks -- this is what stops a single ambiguous match (e.g. a
    // resolution collision between an injected non-harmonic resonance and a
    // real harmonic, both landing in the same FFT bin) from being declared
    // a confident-but-wrong F0. >=2 matches unlocks a reliability that then
    // grows with evidence density (how good the matches are, not just how
    // many) and with temporal stability (the same winner recurring across
    // consecutive frames, tracked below). Deliberately conservative for
    // this first implementation, per spec: 0/1 matches always => 0.
    float matchTerm = bestMatches <= 1 ? 0.0f : juce::jlimit(0.0f, 1.0f, (float) (bestMatches - 1) / 3.0f); // 2->0.33, 3->0.67, 4+->1.0
    float evidenceDensity = bestMatches > 0 ? juce::jlimit(0.0f, 1.0f, bestEvidence / (float) bestMatches) : 0.0f;

    bool sameWinnerAsBefore = rawContext.valid && f0StableFrameCount > 0
        && std::abs(1200.0f * std::log2(rawContext.f0Hz / juce::jmax(1.0f, f0StabilityLastHz))) < 50.0f;
    if (rawContext.valid) { f0StableFrameCount = sameWinnerAsBefore ? juce::jmin(20, f0StableFrameCount + 1) : 1; f0StabilityLastHz = rawContext.f0Hz; }
    else { f0StableFrameCount = 0; }
    float stabilityTerm = juce::jlimit(0.0f, 1.0f, (float) f0StableFrameCount / 5.0f); // 5 consecutive frames on the same winner -> full stability contribution

    float rawReliability = matchTerm * juce::jlimit(0.0f, 1.0f, 0.7f * evidenceDensity + 0.3f * stabilityTerm);

    // Persistence-smooth f0Confidence AND f0Reliability independently
    // (C1.4/item 4: measure T63/T90/T95 for how long context takes to
    // stabilize -- causal only, no added lookahead).
    float target = rawContext.f0Confidence;
    float coeff = std::exp(-1.0f / (target > smoothedContext.f0Confidence ? riseTau : fallTau));
    smoothedContext.f0Confidence = target + (smoothedContext.f0Confidence - target) * coeff;

    float relCoeff = std::exp(-1.0f / (rawReliability > smoothedContext.f0Reliability ? riseTau : fallTau));
    smoothedContext.f0Reliability = rawReliability + (smoothedContext.f0Reliability - rawReliability) * relCoeff;

    if (rawContext.valid) { smoothedContext.f0Hz = rawContext.f0Hz; smoothedContext.supportingPartials = rawContext.supportingPartials; smoothedContext.f0Score = rawContext.f0Score; }
    smoothedContext.valid = smoothedContext.f0Confidence > 0.05f;
}

float LowFrequencyHarmonicAnalyzer::harmonicLikelihoodFor(float queryHz) const
{
    if (! smoothedContext.valid || smoothedContext.f0Hz < 1.0f) return 0.0f;
    float f0Hz = smoothedContext.f0Hz;
    float ratio = queryHz / f0Hz;
    if (ratio < 0.6f) return 0.0f; // well below f0, not a harmonic relationship
    int n = juce::jmax(1, (int) std::round(ratio));
    float expected = f0Hz * (float) n;
    float centsOff = 1200.0f * std::log2(juce::jmax(1.0f, queryHz) / expected);
    float sigma = 60.0f;
    float gaussian = std::exp(-0.5f * (centsOff / sigma) * (centsOff / sigma));
    float binWidthCentsAtF0 = 1200.0f * (float) std::log2(1.0 + analysisBinHz() / juce::jmax(1.0, (double) f0Hz));
    float cap = juce::jlimit(0.15f, 1.0f, 1.0f - binWidthCentsAtF0 / 1200.0f);
    // Deliberately NOT scaled by f0Confidence/f0Reliability here -- see the
    // header comment. Callers combine this geometric answer with
    // currentContext().f0Reliability themselves, so "unreliable f0" and
    // "confirmed non-harmonic" never collapse into the same low number.
    return gaussian * cap;
}
