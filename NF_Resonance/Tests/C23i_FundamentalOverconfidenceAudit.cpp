// PHYSICAL C2.3i -- Fundamental Overconfidence Audit. Full decomposition,
// diagnostic only. Does NOT touch Method C, aux rescue, Region Continuation,
// peak floors, top-K, F0 sigma. Question: why does a genuine 80Hz harmonic
// fundamental still reach high finalConfidence/selectivityWeight even
// though harmonicLikelihood/reliability are computed as non-trivial?

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static std::vector<float> genHarmonicSeriesBoosted(double sr, int n, double f0, float amp, int numH, int boostH, float boostDb, float rolloffDb = 3.0f)
{
    auto b = genSilence(n);
    for (int h = 1; h <= numH; ++h) { float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)); if (h == boostH) a *= (float) juce::Decibels::decibelsToGain(boostDb); addTone(b, sr, f0 * h, a); }
    return b;
}
static void addNoiseFloor(std::vector<float>& b, float amp, int seed) { juce::Random rng(seed); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * amp; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}

struct Rec
{
    float prom=0, promEv=0, persEv=0, stabEv=0, widthEv=0, baseEv=0;
    float harmLike=0, effReliability=0, effLikelihood=0, excessFactor=0;
    int f0Matches=0; float f0Hz=0, f0Reliability=0;
    float protectionBeforeReliability=0, effProtection=0, finalConf=0;
    float selW0=0, selW25=0, selW5=0, selW75=0, selW10=0;
    float mainProtEv=0, auxProtEv=0, structuralProt=0;
    float existenceConf=0, harmonicCtxReliability=0, nonHarmSupport=0, excessiveHarmSupport=0, strongAnomaly=0, problemDecision=0, problemConf=0;
};

static float pct(std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; }

struct CaseResult { std::vector<Rec> hist; Rec last; bool everFound=false; };

static CaseResult runCase(double sr, const std::vector<float>& sig, float watchHz, int warmupFrames = 15)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::vector<float> scratch((size_t) kFft * 2), magDb((size_t) bins), promOut((size_t) bins);

    CaseResult out;
    int n = (int) sig.size();
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f);
        }
        prom.computeProminence(magDb, 4.0f, promOut);
        aux.pushSamples(sig.data() + i, kHop);
        conf.process(promOut, &aux, &magDb);

        // find region near watchHz
        float target = std::log2(juce::jmax(1.0f, watchHz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }

        int fr = (int) (i / kHop);
        if (best && bestDist <= 0.25f && fr >= warmupFrames)
        {
            Rec rec;
            rec.prom = best->peakProminenceDb; rec.promEv = best->lastProminenceEvidence; rec.persEv = best->lastPersistenceEvidence;
            rec.stabEv = best->lastStabilityEvidence; rec.widthEv = best->lastWidthEvidence; rec.baseEv = best->lastBaseEvidence;
            rec.harmLike = best->harmonicLikelihood; rec.effReliability = best->effectiveReliability; rec.effLikelihood = best->effectiveLikelihood;
            rec.excessFactor = best->excessFactor;
            int wIdx = conf.lastF0WinnerIndex();
            if (wIdx >= 0) { auto& fc = conf.lastF0Candidates()[(size_t) wIdx]; rec.f0Matches = fc.matches; rec.f0Hz = fc.centerHz; }
            rec.f0Reliability = conf.currentMainF0Reliability();
            rec.protectionBeforeReliability = juce::jlimit(0.0f, 1.0f, rec.effLikelihood * rec.excessFactor);
            rec.mainProtEv = best->mainProtectionEvidence; rec.auxProtEv = best->auxProtectionEvidence; rec.structuralProt = best->structuralHarmonicProtection;
            rec.effProtection = best->effectiveHarmonicProtection; rec.finalConf = best->confidence;
            rec.existenceConf = best->existenceConfidence; rec.harmonicCtxReliability = best->harmonicContextReliability;
            rec.nonHarmSupport = best->nonHarmonicSupportEvidence; rec.excessiveHarmSupport = best->excessiveHarmonicEvidence;
            rec.strongAnomaly = best->unknownAnomalySupport; rec.problemDecision = best->problemDecisionEvidence;
            rec.problemConf = best->problemConfidence;
            rec.selW0 = ConfidenceEngine::passWeight(rec.finalConf, 0.0f); rec.selW25 = ConfidenceEngine::passWeight(rec.finalConf, 2.5f);
            rec.selW5 = ConfidenceEngine::passWeight(rec.finalConf, 5.0f); rec.selW75 = ConfidenceEngine::passWeight(rec.finalConf, 7.5f);
            rec.selW10 = ConfidenceEngine::passWeight(rec.finalConf, 10.0f);
            out.hist.push_back(rec); out.last = rec; out.everFound = true;
        }
    }
    return out;
}

static void printSummary(const char* label, const CaseResult& c)
{
    if (! c.everFound) { std::printf("  %s: NEVER FOUND\n", label); return; }
    std::vector<float> promV, promEvV, persEvV, stabEvV, widthEvV, baseEvV, harmLikeV, effRelV, effProtV, confV, selW5V;
    for (auto& r : c.hist) { promV.push_back(r.prom); promEvV.push_back(r.promEv); persEvV.push_back(r.persEv); stabEvV.push_back(r.stabEv); widthEvV.push_back(r.widthEv); baseEvV.push_back(r.baseEv); harmLikeV.push_back(r.harmLike); effRelV.push_back(r.effReliability); effProtV.push_back(r.effProtection); confV.push_back(r.finalConf); selW5V.push_back(r.selW5); }
    std::printf("  %s (n=%d frames):\n", label, (int) c.hist.size());
    std::printf("    prom(dB)      P50=%.2f P90=%.2f\n", pct(promV,50), pct(promV,90));
    std::printf("    promEvidence  P50=%.3f P90=%.3f\n", pct(promEvV,50), pct(promEvV,90));
    std::printf("    persEvidence  P50=%.3f P90=%.3f\n", pct(persEvV,50), pct(persEvV,90));
    std::printf("    stabEvidence  P50=%.3f P90=%.3f\n", pct(stabEvV,50), pct(stabEvV,90));
    std::printf("    widthEvidence P50=%.3f P90=%.3f\n", pct(widthEvV,50), pct(widthEvV,90));
    std::printf("    baseEvidence  P50=%.3f P90=%.3f\n", pct(baseEvV,50), pct(baseEvV,90));
    std::printf("    harmLikelihood P50=%.3f P90=%.3f\n", pct(harmLikeV,50), pct(harmLikeV,90));
    std::printf("    effReliability P50=%.3f P90=%.3f\n", pct(effRelV,50), pct(effRelV,90));
    std::printf("    effProtection  P50=%.3f P90=%.3f\n", pct(effProtV,50), pct(effProtV,90));
    std::printf("    finalConfidence P50=%.3f P90=%.3f\n", pct(confV,50), pct(confV,90));
    std::printf("    selWeight@5    P50=%.3f P90=%.3f\n", pct(selW5V,50), pct(selW5V,90));
    auto& r = c.last;
    std::printf("    -- last frame -- prom=%.2f promEv=%.3f persEv=%.3f stabEv=%.3f widthEv=%.3f baseEv=%.3f\n", r.prom, r.promEv, r.persEv, r.stabEv, r.widthEv, r.baseEv);
    std::printf("       harmLike=%.3f effReliability=%.3f f0Matches=%d f0Hz=%.1f f0Reliability=%.3f\n", r.harmLike, r.effReliability, r.f0Matches, r.f0Hz, r.f0Reliability);
    std::printf("       mainProtEv=%.3f auxProtEv=%.3f structuralProt(max)=%.3f effProtection=%.3f finalConf(old)=%.3f\n", r.mainProtEv, r.auxProtEv, r.structuralProt, r.effProtection, r.finalConf);
    std::printf("       [C2.3k] existenceConf=%.3f harmonicCtxReliability=%.3f nonHarmSupport=%.3f excessiveHarmSupport=%.3f strongAnomaly=%.3f problemDecision=%.3f PROBLEM_CONF=%.3f\n",
        r.existenceConf, r.harmonicCtxReliability, r.nonHarmSupport, r.excessiveHarmSupport, r.strongAnomaly, r.problemDecision, r.problemConf);
    std::printf("       selW: 0->%.3f 2.5->%.3f 5->%.3f 7.5->%.3f 10->%.3f\n", r.selW0, r.selW25, r.selW5, r.selW75, r.selW10);
}

int main()
{
    std::printf("=== C2.3i: Fundamental Overconfidence Audit ===\n");

    // ---- 48kHz primary cases ----
    {
        double sr = 48000.0; int n = (int) (sr * 2.0);
        std::printf("\n########## 48kHz ##########\n");

        std::printf("\n-- Case A: 80Hz clean harmonic fundamental --\n");
        auto sigA = genHarmonicSeries(sr, n, 80.0, 0.3f, 8);
        printSummary("80Hz (clean fundamental)", runCase(sr, sigA, 80.0f));

        std::printf("\n-- Case B: 80Hz + 135Hz non-harmonic --\n");
        auto sigB = genHarmonicSeries(sr, n, 80.0, 0.3f, 8); addBurst(sigB, sr, 135.0, 0.5f, 8.0, 1);
        printSummary("80Hz (fundamental)", runCase(sr, sigB, 80.0f));
        printSummary("135Hz (non-harmonic)", runCase(sr, sigB, 135.0f));

        std::printf("\n-- Case C: 120Hz clean fundamental --\n");
        auto sigC = genHarmonicSeries(sr, n, 120.0, 0.3f, 8);
        printSummary("120Hz (clean fundamental)", runCase(sr, sigC, 120.0f));

        std::printf("\n-- Case D: 120Hz + 170Hz non-harmonic --\n");
        auto sigD = genHarmonicSeries(sr, n, 120.0, 0.3f, 8); addBurst(sigD, sr, 170.0, 0.5f, 8.0, 2);
        printSummary("120Hz (fundamental)", runCase(sr, sigD, 120.0f));
        printSummary("170Hz (non-harmonic)", runCase(sr, sigD, 170.0f));

        std::printf("\n-- Case E: excessive harmonic (80Hz series, h2@160Hz boosted +9dB, on a legitimate partial) --\n");
        auto sigE = genHarmonicSeriesBoosted(sr, n, 80.0, 0.3f, 8, 2, 9.0f);
        printSummary("80Hz (f0)", runCase(sr, sigE, 80.0f));
        printSummary("160Hz (h2, excessive)", runCase(sr, sigE, 160.0f));

        std::printf("\n-- Case E2: second excessive-harmonic control (300Hz series, h3@900Hz boosted +12dB) --\n");
        auto sigE2 = genHarmonicSeriesBoosted(sr, n, 300.0, 0.25f, 6, 3, 12.0f);
        printSummary("300Hz (f0)", runCase(sr, sigE2, 300.0f));
        printSummary("900Hz (h3, excessive)", runCase(sr, sigE2, 900.0f));

        // NOTE: SpectralProminenceEngineV5's prominence is RELATIVE to local
        // spectral context, not an absolute level -- an isolated tone
        // against near-silence reads as maximally prominent regardless of
        // its own absolute amplitude (confirmed directly: amp=0.3 and
        // amp=0.12 both read ~52dB here). So "moderate, isolated, low-
        // evidence" and "extreme, isolated, low-evidence" cannot be
        // distinguished by amplitude alone in a near-silent synthetic
        // signal -- both naturally saturate prominenceEvidence. Kept as a
        // low-amplitude variant for an honest side-by-side against Case G.
        std::printf("\n-- Case F: UNKNOWN harmonic context, musical/persistent, isolated (sparse 2-partial series, 300Hz, low amplitude) --\n");
        auto sigF = genHarmonicSeries(sr, n, 300.0, 0.05f, 2, 3.0f);
        printSummary("300Hz (sparse f0, low-evidence context, low amplitude)", runCase(sr, sigF, 300.0f));

        std::printf("\n-- Case G: UNKNOWN harmonic context + extremely strong narrow anomaly (isolated, very loud, no harmonic context) --\n");
        auto sigG = genSilence(n); addTone(sigG, sr, 450.0, 0.95f);
        printSummary("450Hz (isolated, extreme amplitude, no context)", runCase(sr, sigG, 450.0f));
    }

    // ---- 96kHz spot check ----
    {
        double sr = 96000.0; int n = (int) (sr * 2.0);
        std::printf("\n########## 96kHz spot-check ##########\n");
        auto sigB = genHarmonicSeries(sr, n, 80.0, 0.3f, 8); addBurst(sigB, sr, 135.0, 0.5f, 8.0, 1);
        printSummary("80Hz (fundamental)", runCase(sr, sigB, 80.0f));
        printSummary("135Hz (non-harmonic)", runCase(sr, sigB, 135.0f));
        auto sigD = genHarmonicSeries(sr, n, 120.0, 0.3f, 8); addBurst(sigD, sr, 170.0, 0.5f, 8.0, 2);
        printSummary("120Hz (fundamental)", runCase(sr, sigD, 120.0f));
        printSummary("170Hz (non-harmonic)", runCase(sr, sigD, 170.0f));
    }

    // ---- 44.1k / 192k spot-check ----
    for (double sr : { 44100.0, 192000.0 })
    {
        int n = (int) (sr * 2.0);
        std::printf("\n########## %.0fHz spot-check ##########\n", sr);
        auto sigB = genHarmonicSeries(sr, n, 80.0, 0.3f, 8); addBurst(sigB, sr, 135.0, 0.5f, 8.0, 1);
        printSummary("80Hz (fundamental)", runCase(sr, sigB, 80.0f));
        printSummary("135Hz (non-harmonic)", runCase(sr, sigB, 135.0f));
    }

    // ---- musical material spot-check (bass/vocal/guitar) ----
    {
        double sr = 48000.0; int n = (int) (sr * 2.0);
        std::printf("\n########## Musical material spot-check (48kHz) ##########\n");
        auto bass = genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f);
        printSummary("Bass (62Hz f0)", runCase(sr, bass, 62.0f));
        auto vocal = genHarmonicSeries(sr, n, 140.0, 0.28f, 10, 2.5f);
        printSummary("Vocal-like (140Hz f0)", runCase(sr, vocal, 140.0f));
        auto guitar = genHarmonicSeries(sr, n, 220.0, 0.25f, 10, 2.0f);
        printSummary("Guitar-like (220Hz f0)", runCase(sr, guitar, 220.0f));
    }

    return 0;
}
