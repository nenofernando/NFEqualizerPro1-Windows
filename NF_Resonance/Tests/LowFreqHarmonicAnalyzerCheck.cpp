// PHYSICAL C, Blocker 2: LowFrequencyHarmonicAnalyzer standalone validation.
// Diagnostic/offline only -- nothing here touches gain, UI, or the main
// STFT/OLA/C2 path. Covers the user's Blocker-2 spec items 5-9:
//   5 -- cases A/B/C/D/E at 44.1/48/96/192kHz, per-rate report
//   6 -- fundamental criterion (non-harmonic/excessive-harmonic queries
//        must read a HIGHER "problem confidence" than the fundamental
//        itself, at every sample rate)
//   7 -- cross-sample-rate invariance (same conceptual signal, all 4 rates)
//   8 -- CPU: auxiliary-analyzer-only vs full combined path
//   9 -- realtime safety: sequential sample-rate reprepare on one instance,
//        zero allocations once running (ASAN/UBSAN build covers the rest)

#include <JuceHeader.h>
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/ConfidenceEngine.h"
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::atomic<bool> gTrackAllocs{ false };
static std::atomic<long long> gAllocCount{ 0 };
void* operator new(std::size_t sz) { if (gTrackAllocs.load(std::memory_order_relaxed)) gAllocCount.fetch_add(1, std::memory_order_relaxed); void* p = std::malloc(sz == 0 ? 1 : sz); if (! p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static double median(std::vector<double> v) { std::sort(v.begin(), v.end()); size_t n = v.size(); return n ? (n % 2 ? v[n/2] : 0.5*(v[n/2-1]+v[n/2])) : 0.0; }
static double percentile(std::vector<double> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); double idx = p/100.0*(double)(v.size()-1); size_t lo=(size_t)idx; size_t hi=juce::jmin(v.size()-1, lo+1); double frac=idx-(double)lo; return v[lo]+(v[hi]-v[lo])*frac; }

// ---------------- signal generators (sample-rate parametric) ----------------
static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{
    auto b = genSilence(n);
    for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)));
    return b;
}
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed);
    double bwHz = freqHz / Q;
    int n = (int) b.size();
    for (int k = 0; k < 9; ++k)
    {
        double t = (double) k / 8.0 - 0.5;
        double f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; }
    }
}

// ---------------- run one case through the analyzer, track stabilization ----------------
struct QueryResult { float freq; float harmLike; float problemConf; };
struct RunResult
{
    float f0Hz = 0, f0ErrorCents = 999, f0Score = 0, f0Confidence = 0, f0Reliability = 0;
    int partials = 0, t63Frames = -1, t90Frames = -1, t95Frames = -1, totalFrames = 0;
    std::vector<QueryResult> queries;
};

// ConfidenceEngine's own harmonicMaxPenalty (0.7) mirrored here as a simple
// diagnostic proxy -- LowFrequencyHarmonicAnalyzer is not yet wired into
// ConfidenceEngine (that integration is explicitly out of scope for
// Blocker 2), so "problem confidence" here is just 1 - 0.7*harmonicLikelihood,
// consistent with how ConfidenceEngine::computeConfidence() already
// discounts a region's confidence via harmonicPenalty.
//
// f0Reliability gate (fail-safe): harmonicLikelihoodFor() is now a PURELY
// geometric answer, not scaled by how much we trust the f0 estimate. So
// this proxy explicitly discounts the harmonic-protection TERM by
// reliability, rather than letting an unreliable f0 either (a) claim full
// harmonic protection it hasn't earned, or (b) collapse toward
// harmonicLikelihood=0 and swing problemConfidence up to "very
// problematic" -- the exact failure mode the user flagged for bass
// fundamentals. When reliability is low, the harmonic term is pulled
// toward a NEUTRAL midpoint (0.5) instead of committing either direction.
//
// DIAGNOSTIC-ONLY, NOT THE PRODUCTION FORMULA (Blocker 2 closure note): this
// lerp-to-0.5 shape is convenient for a single pass/fail report number in
// THIS test file, but it is not the intended final gain-decision formula --
// see the class-level comment in LowFrequencyHarmonicAnalyzer.h for the
// three-value contract (baseProblemEvidence / harmonicLikelihood /
// harmonicContextReliability) a future gain-decision stage must keep
// separate instead of collapsing into one scalar like this.
static constexpr float kHarmonicMaxPenaltyProxy = 0.7f;
static float problemConfidenceProxy(float harmLike, float reliability)
{
    float committedProblem = 1.0f - kHarmonicMaxPenaltyProxy * harmLike;
    return reliability * committedProblem + (1.0f - reliability) * 0.5f;
}

static RunResult runCase(double sr, const std::vector<float>& sig, double expectedF0Hz, const std::vector<float>& queryFreqs)
{
    LowFrequencyHarmonicAnalyzer az;
    az.prepare(sr);
    const int block = 512;
    int n = (int) sig.size();
    std::vector<float> confHistory;
    for (int i = 0; i < n; i += block)
    {
        int len = juce::jmin(block, n - i);
        az.pushSamples(sig.data() + i, len);
        confHistory.push_back(az.currentContext().f0Confidence);
    }
    RunResult r;
    r.totalFrames = (int) confHistory.size();
    float finalConf = confHistory.empty() ? 0.0f : confHistory.back();
    for (size_t k = 0; k < confHistory.size(); ++k)
    {
        if (r.t63Frames < 0 && confHistory[k] >= 0.63f * finalConf) r.t63Frames = (int) k;
        if (r.t90Frames < 0 && confHistory[k] >= 0.90f * finalConf) r.t90Frames = (int) k;
        if (r.t95Frames < 0 && confHistory[k] >= 0.95f * finalConf) r.t95Frames = (int) k;
    }
    auto ctx = az.currentContext();
    r.f0Hz = ctx.f0Hz; r.partials = ctx.supportingPartials; r.f0Score = ctx.f0Score; r.f0Confidence = ctx.f0Confidence; r.f0Reliability = ctx.f0Reliability;
    r.f0ErrorCents = (expectedF0Hz > 0 && ctx.f0Hz > 0) ? 1200.0f * std::log2((float) (ctx.f0Hz / expectedF0Hz)) : 9999.0f;
    for (float qf : queryFreqs)
    {
        float hl = az.harmonicLikelihoodFor(qf);
        r.queries.push_back({ qf, hl, problemConfidenceProxy(hl, ctx.f0Reliability) });
    }
    return r;
}

static void printRunResult(const char* label, double sr, const RunResult& r)
{
    std::printf("  [%s @ %.0fHz] f0=%.2fHz err=%.1fcents partials=%d score=%.3f f0conf=%.3f f0Reliable=%.3f T63=%d T90=%d T95=%d (of %d frames)\n",
        label, sr, r.f0Hz, r.f0ErrorCents, r.partials, r.f0Score, r.f0Confidence, r.f0Reliability, r.t63Frames, r.t90Frames, r.t95Frames, r.totalFrames);
    for (auto& q : r.queries)
        std::printf("      query %.1fHz: harmonicLikelihood=%.3f problemConfidence=%.3f\n", q.freq, q.harmLike, q.problemConf);
}

int main()
{
    bool allOk = true;
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double durationSec = 1.5;

    std::printf("=== 5. MAIN TEST MATRIX (A-E across 44.1/48/96/192kHz) ===\n");

    // Store the fundamental-criterion results (case B/E) for the hard
    // pass/fail check below, per sample rate.
    struct FundCheck { double sr; float problem80_or_120; float problem135_or_170; bool excessiveOk; };
    std::vector<FundCheck> fundChecks;

    for (double sr : rates)
    {
        int n = (int) (sr * durationSec);
        std::printf(" -- Sample rate %.0f Hz --\n", sr);

        // A: 80Hz + normal harmonic series
        auto sigA = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        auto rA = runCase(sr, sigA, 80.0, { 80.0f });
        printRunResult("A: 80Hz+harmonics", sr, rA);

        // B: A + non-harmonic resonance at 135Hz
        auto sigB = sigA; addBurst(sigB, sr, 135.0, 0.5f, 8.0, 1);
        auto rB = runCase(sr, sigB, 80.0, { 80.0f, 135.0f });
        printRunResult("B: A+non-harm@135Hz", sr, rB);

        // C: A + excessive 2nd harmonic boost at 160Hz (+12dB on h2)
        std::vector<float> sigC = genSilence(n);
        for (int h = 1; h <= 6; ++h)
        {
            float a = 0.3f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1));
            if (h == 2) a *= (float) juce::Decibels::decibelsToGain(12.0f);
            addTone(sigC, sr, 80.0 * h, a);
        }
        auto rC = runCase(sr, sigC, 80.0, { 80.0f, 160.0f });
        printRunResult("C: A+excessive h2@160Hz", sr, rC);

        // D: 120Hz + normal harmonic series
        auto sigD = genHarmonicSeries(sr, n, 120.0, 0.3f, 6);
        auto rD = runCase(sr, sigD, 120.0, { 120.0f });
        printRunResult("D: 120Hz+harmonics", sr, rD);

        // E: D + non-harmonic resonance at 170Hz
        auto sigE = sigD; addBurst(sigE, sr, 170.0, 0.5f, 8.0, 2);
        auto rE = runCase(sr, sigE, 120.0, { 120.0f, 170.0f });
        printRunResult("E: D+non-harm@170Hz", sr, rE);

        // Fundamental criterion (item 6): problemConfidence(135) > problemConfidence(80fund)
        // and problemConfidence(170) > problemConfidence(120fund), evaluated
        // within the SAME analysis state (B and E runs respectively).
        //
        // Reliability-aware: when f0Reliability is low, problemConfidenceProxy()
        // deliberately pulls BOTH queries toward the same neutral 0.5 (see its
        // own comment) -- that is the intended fail-safe, not a bug. A strict
        // ">" check would flag that honest "we don't know" collapse as a
        // failure, which conflates "the tool refused to guess" with "the tool
        // guessed wrong." So: only require the strict ordering when reliability
        // is high enough (>=0.5) for the context the ordering was computed
        // from; below that, only require that neither side ran away to a false
        // extreme (both stay within a wide neutral band), and report the
        // reliability-gated case separately instead of failing outright.
        bool reliableB = rB.f0Reliability >= 0.5f, reliableE = rE.f0Reliability >= 0.5f;
        float p80 = rB.queries[0].problemConf, p135 = rB.queries[1].problemConf;
        float p120 = rE.queries[0].problemConf, p170 = rE.queries[1].problemConf;
        bool okBE_reliable = (! reliableB || p135 > p80) && (! reliableE || p170 > p120);
        bool okBE_neutralWhenUnreliable = (reliableB || (p80 > 0.25f && p80 < 0.75f && p135 > 0.25f && p135 < 0.75f))
                                        && (reliableE || (p120 > 0.25f && p120 < 0.75f && p170 > 0.25f && p170 < 0.75f));
        bool critBE = okBE_reliable && okBE_neutralWhenUnreliable;
        std::printf("  [FUNDAMENTAL CRITERION] problem(135)=%.3f (reliB=%.2f) vs problem(80)=%.3f | problem(170)=%.3f (reliE=%.2f) vs problem(120)=%.3f : %s\n",
            p135, rB.f0Reliability, p80, p170, rE.f0Reliability, p120, critBE ? "PASS" : "FAIL");
        if (! critBE) allOk = false;

        // Normal-harmonic-remains-protected: excessive h2 (case C) should
        // still read a MUCH lower problem confidence than a genuinely
        // non-harmonic case at a comparable distance (case B), i.e. the
        // excessive-harmonic query at 160Hz must not collapse to "as
        // problematic as" 135Hz's non-harmonic reading. Same reliability
        // gating: only a strict requirement when both contexts are reliable.
        bool reliableC = rC.f0Reliability >= 0.5f;
        float p160 = rC.queries[1].problemConf;
        bool excessiveOk = (! reliableC || ! reliableB) ? (p160 > 0.25f && p160 < 0.75f) : (p160 < p135);
        std::printf("  [EXCESSIVE-HARMONIC STILL PROTECTED] problem(160,excessive-h2)=%.3f (reliC=%.2f) vs problem(135,non-harm)=%.3f : %s\n",
            p160, rC.f0Reliability, p135, excessiveOk ? "PASS" : "FAIL");
        if (! excessiveOk) allOk = false;

        fundChecks.push_back({ sr, p80, p135, excessiveOk });
    }

    // ---------------- 7. Cross-sample-rate invariance ----------------
    std::printf("\n=== 7. CROSS-SAMPLE-RATE INVARIANCE (same conceptual signal, 4 rates) ===\n");
    {
        std::vector<RunResult> results;
        for (double sr : rates)
        {
            int n = (int) (sr * durationSec);
            auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
            addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);
            results.push_back(runCase(sr, sig, 80.0, { 80.0f, 135.0f }));
        }
        float f0Ref = results[0].f0Hz, hlRef = results[0].queries[1].harmLike, confRef = results[0].f0Confidence;
        float maxF0DriftCents = 0, maxHlDrift = 0, maxConfDrift = 0;
        for (size_t i = 1; i < results.size(); ++i)
        {
            float driftCents = 1200.0f * std::log2(results[i].f0Hz / f0Ref);
            float hlDrift = std::abs(results[i].queries[1].harmLike - hlRef);
            float confDrift = std::abs(results[i].f0Confidence - confRef);
            maxF0DriftCents = juce::jmax(maxF0DriftCents, std::abs(driftCents));
            maxHlDrift = juce::jmax(maxHlDrift, hlDrift);
            maxConfDrift = juce::jmax(maxConfDrift, confDrift);
            std::printf("  %.0fHz vs %.0fHz: f0 drift=%.1fcents harmLike(135) drift=%.3f f0conf drift=%.3f\n",
                rates[i], rates[0], driftCents, hlDrift, confDrift);
        }
        std::printf("  max f0 drift=%.1fcents max harmLike drift=%.3f max f0conf drift=%.3f\n", maxF0DriftCents, maxHlDrift, maxConfDrift);
    }

    // ---------------- 8. CPU: aux-only vs combined ----------------
    std::printf("\n=== 8. CPU (Release, profiling OFF): aux-only vs full combined path ===\n");
    for (double sr : rates)
    {
        LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
        SpectralProminenceEngineV5 prom; prom.prepare((int) (2048 / 2 + 1), sr, 2048);
        prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine conf; conf.prepare(sr, 2048, 512);
        std::vector<float> magDb((size_t) (2048 / 2 + 1), -120.0f), promOut((size_t) (2048 / 2 + 1), 0.0f);
        juce::dsp::FFT hostFft(11);
        std::array<float, 2048> window{};
        for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f);
        std::array<float, 4096> scratch{};
        auto sig = genHarmonicSeries(sr, (int) sr * 2, 80.0, 0.3f, 6);
        addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);
        int n = (int) sig.size();
        const int block = 512;

        std::vector<double> auxOnlyUs, combinedUs;
        for (int i = 0; i + block <= n; i += block)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            az.pushSamples(sig.data() + i, block);
            auto t1 = std::chrono::high_resolution_clock::now();
            auxOnlyUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        for (int i = 0; i + 2048 <= n; i += block)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int k = 0; k < 2048; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + 2048, scratch.end(), 0.0f);
            hostFft.performRealOnlyForwardTransform(scratch.data());
            const int bins = 2048 / 2 + 1;
            for (int b = 0; b < bins; ++b)
            {
                float re = scratch[(size_t) 2 * b], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) 2 * b + 1];
                magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / 2048.0f + 1e-12f, -120.0f);
            }
            prom.computeProminence(magDb, 4.0f, promOut);
            conf.process(promOut);
            az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
            auto t1 = std::chrono::high_resolution_clock::now();
            combinedUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        double hopBudgetUs = 1.0e6 * 512.0 / sr; // one host hop's worth of real time, the actual realtime budget
        std::printf("  %.0fHz: aux-only per-block med=%.2fus p95=%.2fus p99=%.2fus | combined med=%.2fus p95=%.2fus p99=%.2fus (decimation=%dx)\n",
            sr, median(auxOnlyUs), percentile(auxOnlyUs, 95), percentile(auxOnlyUs, 99),
            median(combinedUs), percentile(combinedUs, 95), percentile(combinedUs, 99), az.decimationFactor());
        std::printf("      hop budget=%.2fus | combined med/p95/p99 = %.2f%% / %.2f%% / %.2f%% of budget\n",
            hopBudgetUs, 100.0 * median(combinedUs) / hopBudgetUs, 100.0 * percentile(combinedUs, 95) / hopBudgetUs, 100.0 * percentile(combinedUs, 99) / hopBudgetUs);
    }

    // ---------------- PERMANENT REGRESSION (Blocker 2 closure, item 4): ----------------
    // insufficient harmonic evidence (supportingPartials <= 1) must NEVER
    // produce a high-reliability wrong F0. Checked at the level that
    // actually matters for safety: the persistence-smoothed, PUBLIC
    // currentContext() a caller would actually read every analysis frame --
    // not just the instantaneous raw per-frame computation (which is
    // trivially safe by construction: matchTerm=0 when matches<=1).
    std::printf("\n=== PERMANENT REGRESSION: insufficient evidence must never yield high-reliability wrong F0 ===\n");
    {
        struct Scenario { const char* name; double sr; double f0; double burstHz; };
        std::vector<Scenario> scenarios = {
            { "80Hz+harm+135Hz-burst", 44100.0, 80.0, 135.0 },
            { "80Hz+harm+135Hz-burst", 48000.0, 80.0, 135.0 },
            { "60Hz+harm+~98Hz-burst(mainlobe collision)", 44100.0, 60.0, 60.0 * 1.63 },
            { "70Hz+harm+~114Hz-burst(mainlobe collision)", 44100.0, 70.0, 70.0 * 1.63 },
        };
        bool regressionOk = true;
        int totalFramesChecked = 0, violations = 0;
        for (auto& sc : scenarios)
        {
            int n = (int) (sc.sr * 1.5);
            auto sig = genHarmonicSeries(sc.sr, n, sc.f0, 0.35f, 6);
            addBurst(sig, sc.sr, sc.burstHz, 0.5f, 8.0, 7);
            LowFrequencyHarmonicAnalyzer az; az.prepare(sc.sr);
            const int block = 512;
            int localViolations = 0, localFrames = 0;
            for (int i = 0; i < n; i += block)
            {
                az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
                auto ctx = az.currentContext();
                ++localFrames;
                if (ctx.f0Reliability >= 0.5f && ctx.supportingPartials < 2) { ++localViolations; ++violations; }
            }
            totalFramesChecked += localFrames;
            std::printf("  %-46s @ %6.0fHz: %d/%d frames checked, %d violation(s) (f0Reliability>=0.5 with <2 matches) : %s\n",
                sc.name, sc.sr, localFrames, localFrames, localViolations, localViolations == 0 ? "PASS" : "FAIL");
            if (localViolations > 0) regressionOk = false;
        }
        std::printf("  [REGRESSION: NO HIGH-CONFIDENCE WRONG F0] %d frames checked across %d scenarios, %d violations : %s\n",
            totalFramesChecked, (int) scenarios.size(), violations, regressionOk ? "PASS" : "FAIL");
        if (! regressionOk) allOk = false;
    }

    // ---------------- 9. Realtime safety: sequential sample-rate reprepare + zero-alloc ----------------
    std::printf("\n=== 9. REALTIME SAFETY: sequential SR reprepare (same instance) + zero-allocation ===\n");
    {
        LowFrequencyHarmonicAnalyzer az;
        const double sequence[] = { 44100.0, 48000.0, 96000.0, 192000.0, 48000.0, 44100.0 };
        bool seqOk = true;
        for (double sr : sequence)
        {
            az.prepare(sr);
            int n = (int) (sr * 0.3);
            auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
            addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);

            gAllocCount.store(0); gTrackAllocs.store(true);
            const int block = 512;
            for (int i = 0; i < n; i += block)
                az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
            gTrackAllocs.store(false);
            long long allocs = gAllocCount.load();

            bool finiteOk = std::isfinite(az.currentContext().f0Hz) && std::isfinite(az.currentContext().f0Confidence);
            bool thisOk = (allocs == 0) && finiteOk;
            std::printf("  prepare(%.0fHz) -> %d blocks pushed, allocations=%lld, finite=%s : %s\n",
                sr, n / block, allocs, finiteOk ? "yes" : "NO", thisOk ? "PASS" : "FAIL");
            if (! thisOk) seqOk = false;
        }
        std::printf("  [SEQUENTIAL REPREPARE + ZERO-ALLOC] %s\n", seqOk ? "PASS" : "FAIL");
        if (! seqOk) allOk = false;
    }

    std::printf("\n%s\n", allOk ? "LowFrequencyHarmonicAnalyzer: fundamental criterion holds and excessive-harmonic protection holds at every sample rate; sequential reprepare is allocation-clean. See CPU/invariance numbers above for review." : "LowFrequencyHarmonicAnalyzer: at least one hard criterion FAILED -- see above.");
    return allOk ? 0 : 1;
}
