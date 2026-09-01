// V2-A5 CPU non-determinism isolation microbenchmark.
// Instantiates ONLY SpectralProminenceEngineV5 (no AudioProcessor, no FFT, no
// file I/O, no logging inside the measured region). prepare() is called once.
// Every scenario processes the exact SAME pre-generated frame 10,000 times,
// discarding the first 500 as warm-up, 5 independent runs each. Only the
// compute() call itself is timed (std::chrono::steady_clock), into a
// preallocated buffer -- statistics are computed only after the loop.
// Read-only investigation: no prominence formulas are touched here.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <new>

//==============================================================================
// Global new/delete interception -- proves compute() allocates nothing, by
// instrumentation rather than visual inspection. Only counts while armed.
static std::atomic<bool> gTrackAllocs{false};
static std::atomic<long long> gAllocCount{0};
static std::atomic<long long> gAllocBytes{0};

void* operator new(std::size_t sz)
{
    if (gTrackAllocs.load(std::memory_order_relaxed)) { gAllocCount.fetch_add(1, std::memory_order_relaxed); gAllocBytes.fetch_add((long long) sz, std::memory_order_relaxed); }
    void* p = std::malloc(sz == 0 ? 1 : sz);
    if (! p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

//==============================================================================
static double median(std::vector<double> v) { std::sort(v.begin(), v.end()); size_t n = v.size(); return n ? (n % 2 ? v[n/2] : 0.5*(v[n/2-1]+v[n/2])) : 0.0; }
static double percentile(std::vector<double> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); double idx = p/100.0 * (double)(v.size()-1); size_t lo=(size_t)idx; size_t hi=juce::jmin(v.size()-1, lo+1); double frac=idx-(double)lo; return v[lo]+(v[hi]-v[lo])*frac; }
static double maxOf(const std::vector<double>& v) { double m=0; for (double x : v) m = juce::jmax(m, x); return m; }

struct Stats { double med=0, p95=0, p99=0, p999=0, mx=0; };
static Stats statsOf(std::vector<double> v) { Stats s; s.med=median(v); s.p95=percentile(v,95); s.p99=percentile(v,99); s.p999=percentile(v,99.9); s.mx=maxOf(v); return s; }

static void printStatRow(const char* label, const Stats& s)
{
    std::printf("    %-12s median=%9.3f  P95=%9.3f  P99=%9.3f  P99.9=%9.3f  max=%9.3f  (us)\n",
                label, s.med, s.p95, s.p99, s.p999, s.mx);
}

//==============================================================================
// Deterministic, content-fixed test frames (no per-call randomness -- the
// SAME frame is reused for every one of the 10,000 iterations).
static std::vector<float> genSilence(int bins) { return std::vector<float>((size_t) bins, -120.0f); }
static std::vector<float> genFlat(int bins) { return std::vector<float>((size_t) bins, -20.0f); }
static std::vector<float> genPinkLike(int bins, double sr, int fftSize)
{
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i) { double hz = juce::jmax(1.0, (double) i * sr / fftSize); double db = -6.0 - 4.5 * std::log2(hz / 1000.0); v[(size_t) i] = (float) juce::jlimit(-120.0, -6.0, db); }
    return v;
}
static std::vector<float> genHarmonic(int bins, double sr, int fftSize)
{
    std::vector<float> v((size_t) bins, -90.0f);
    double f0 = 180.0;
    for (int h = 1; h <= 60; ++h) { double hz = f0 * h; if (hz >= sr * 0.5) break; int bin = (int) std::round(hz * fftSize / sr); if (bin < bins) v[(size_t) bin] = (float) (-6.0 - 12.0 * std::log2((double) h + 1)); }
    return v;
}
static std::vector<float> genDense(int bins, double sr, int fftSize)
{
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i) { double hz = juce::jmax(1.0, (double) i * sr / fftSize); double base = -40.0 - 8.0 * std::log2(hz / 1000.0 + 1.0); double ripple = 6.0 * std::sin(0.35 * i) + 3.0 * std::sin(1.7 * i + 0.5); v[(size_t) i] = (float) juce::jlimit(-120.0, -3.0, base + ripple); }
    return v;
}
static std::vector<float> genRandom(int bins)
{
    juce::Random rng(777);
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i) v[(size_t) i] = (float) juce::jlimit(-120.0, -6.0, -60.0 + rng.nextDouble() * 54.0);
    return v;
}

//==============================================================================
struct ScenarioResult
{
    std::vector<double> total, prefix, broad, medium, narrow, blend;
    long long nanCount = 0, infCount = 0, subnormalCount = 0;
};

static ScenarioResult runScenario(SpectralProminenceEngineV5& eng, const std::vector<float>& frame, int bins,
                                   int iterations, int warmup, bool withScopedNoDenormals)
{
    ScenarioResult res;
    int kept = iterations - warmup;
    res.total.reserve((size_t) kept); res.prefix.reserve((size_t) kept); res.broad.reserve((size_t) kept);
    res.medium.reserve((size_t) kept); res.narrow.reserve((size_t) kept); res.blend.reserve((size_t) kept);

    std::vector<float> out((size_t) bins, 0.0f); // preallocated once, reused every call
    std::vector<double> rawTotalUs((size_t) iterations, 0.0);

    std::unique_ptr<juce::ScopedNoDenormals> noDenorm;
    if (withScopedNoDenormals) noDenorm = std::make_unique<juce::ScopedNoDenormals>();

    for (int it = 0; it < iterations; ++it)
    {
        auto t0 = std::chrono::steady_clock::now();
        eng.computeProminence(frame, 5.0f, out);
        auto t1 = std::chrono::steady_clock::now();
        rawTotalUs[(size_t) it] = std::chrono::duration<double, std::micro>(t1 - t0).count();

        if (it >= warmup)
        {
            res.total.push_back(rawTotalUs[(size_t) it]);
            res.prefix.push_back(eng.lastPrefixUs());
            res.broad.push_back(eng.lastBroadUs());
            res.medium.push_back(eng.lastMediumUs());
            res.narrow.push_back(eng.lastNarrowUs());
            res.blend.push_back(eng.lastBlendUs());

            for (float v : out)
            {
                if (std::isnan(v)) ++res.nanCount;
                else if (std::isinf(v)) ++res.infCount;
                else if (v != 0.0f && std::fpclassify(v) == FP_SUBNORMAL) ++res.subnormalCount;
            }
        }
    }
    return res;
}

//==============================================================================
int main()
{
    const double sr = 192000.0;
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    const int iterations = 10000;
    const int warmup = 500;
    const int runsPerScenario = 5;

    std::printf("================================================================\n");
    std::printf("V2-A5 SpectralProminenceEngineV5 -- isolated CPU microbenchmark\n");
    std::printf("sr=%.0f fftSize=%d bins=%d iterations=%d warmup=%d runs=%d\n",
                 sr, fftSize, bins, iterations, warmup, runsPerScenario);
#ifdef NDEBUG
    std::printf("Build: NDEBUG defined (Release)\n");
#else
    std::printf("Build: *** NDEBUG NOT DEFINED -- this is a DEBUG build, results are NOT valid for the CPU diagnosis ***\n");
#endif
    std::printf("================================================================\n\n");

    struct Scenario { const char* name; std::vector<float> frame; };
    std::vector<Scenario> scenarios;
    scenarios.push_back({ "silence", genSilence(bins) });
    scenarios.push_back({ "flat", genFlat(bins) });
    scenarios.push_back({ "pink-like", genPinkLike(bins, sr, fftSize) });
    scenarios.push_back({ "harmonic", genHarmonic(bins, sr, fftSize) });
    scenarios.push_back({ "dense", genDense(bins, sr, fftSize) });
    scenarios.push_back({ "random", genRandom(bins) });

    SpectralProminenceEngineV5 eng;
    eng.prepare(bins, sr, fftSize); // single prepare() call for the whole run

    // hop budget at sr=192000, hop=512 (fftSize/4): 1e6*512/192000 = 2666.67 us
    const double hopUs = 1000000.0 * (fftSize / 4) / sr;
    std::printf("Hop budget at sr=%.0f, hop=%d: %.3f us. All %% figures below are %% of this hop.\n\n", sr, fftSize / 4, hopUs);

    struct Method { const char* name; SpectralProminenceEngineV5::NarrowMethod m; };
    std::vector<Method> methods = {
        { "current-WinsorizedSideband", SpectralProminenceEngineV5::NarrowMethod::WinsorizedSideband },
        { "A-O1_LeftRightInterp",       SpectralProminenceEngineV5::NarrowMethod::O1_LeftRightInterp },
        { "B-O1_BlockTrimmedMean6",     SpectralProminenceEngineV5::NarrowMethod::O1_BlockTrimmedMean6 },
        { "C-O1_RobustSideSlope",       SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope },
    };

    // ---- Main timed battery: for each method, each scenario, 5 independent runs ----
    for (auto& meth : methods)
    {
        eng.setNarrowMethod(meth.m);
        std::printf("================ METHOD: %s ================\n", meth.name);
        for (auto& sc : scenarios)
        {
            std::printf("---- Scenario: %s ----\n", sc.name);
            std::vector<Stats> totalPerRun;
            Stats lastNarrow, lastTotal;
            for (int run = 0; run < runsPerScenario; ++run)
            {
                auto r = runScenario(eng, sc.frame, bins, iterations, warmup, false);
                Stats tStats = statsOf(r.total);
                totalPerRun.push_back(tStats);
                std::printf("  run %d/%d: TOTAL median=%.3f(%.1f%%) P99=%.3f(%.1f%%) P99.9=%.3f max=%.3f us | NaN=%lld Inf=%lld subnormal=%lld\n",
                            run + 1, runsPerScenario, tStats.med, 100.0*tStats.med/hopUs, tStats.p99, 100.0*tStats.p99/hopUs, tStats.p999, tStats.mx,
                            r.nanCount, r.infCount, r.subnormalCount);
                if (run == runsPerScenario - 1) { lastNarrow = statsOf(r.narrow); lastTotal = tStats; }
            }
            double sum = 0; for (auto& s : totalPerRun) sum += s.med;
            double mean = sum / (double) totalPerRun.size();
            double var = 0; for (auto& s : totalPerRun) var += (s.med - mean) * (s.med - mean);
            double sd = std::sqrt(var / (double) totalPerRun.size());
            double maxOfMax = 0; for (auto& s : totalPerRun) maxOfMax = juce::jmax(maxOfMax, s.mx);
            std::printf("  SUMMARY  NARROW: med=%.3f P95=%.3f P99=%.3f P99.9=%.3f max=%.3f us | TOTAL: med=%.3f(%.1f%%) P95=%.3f P99=%.3f(%.1f%%) P99.9=%.3f max=%.3f us\n",
                        lastNarrow.med, lastNarrow.p95, lastNarrow.p99, lastNarrow.p999, lastNarrow.mx,
                        lastTotal.med, 100.0*lastTotal.med/hopUs, lastTotal.p95, lastTotal.p99, 100.0*lastTotal.p99/hopUs, lastTotal.p999, lastTotal.mx);
            std::printf("  across %d runs: median-of-medians mean=%.3f us, std=%.3f us | worst max seen=%.3f us\n\n",
                        runsPerScenario, mean, sd, maxOfMax);
        }
    }
    eng.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::WinsorizedSideband); // restore for the rest of this run

    // ---- Denormal comparison: normal vs ScopedNoDenormals, on the "dense" and "pink-like" scenarios ----
    std::printf("---- Denormal comparison (dense, pink-like): normal vs ScopedNoDenormals ----\n");
    for (auto& sc : scenarios)
    {
        if (juce::String(sc.name) != "dense" && juce::String(sc.name) != "pink-like") continue;
        auto rNormal = runScenario(eng, sc.frame, bins, iterations, warmup, false);
        auto rGuarded = runScenario(eng, sc.frame, bins, iterations, warmup, true);
        Stats sN = statsOf(rNormal.total), sG = statsOf(rGuarded.total);
        std::printf("  %-10s normal:    median=%.3f P99=%.3f max=%.3f us | NaN=%lld Inf=%lld subnormal=%lld\n",
                    sc.name, sN.med, sN.p99, sN.mx, rNormal.nanCount, rNormal.infCount, rNormal.subnormalCount);
        std::printf("  %-10s NoDenorm:  median=%.3f P99=%.3f max=%.3f us | NaN=%lld Inf=%lld subnormal=%lld\n",
                    sc.name, sG.med, sG.p99, sG.mx, rGuarded.nanCount, rGuarded.infCount, rGuarded.subnormalCount);
    }
    std::printf("\n");

    // ---- Zero-allocation proof: separate pass, allocation tracking armed ----
    std::printf("---- Zero-allocation proof (armed operator-new/delete counter) ----\n");
    {
        std::vector<float> out((size_t) bins, 0.0f);
        const auto& frame = scenarios[3].frame; // harmonic
        // one untracked warmup call to make sure no lazy-init allocation leaks into the tracked window
        eng.computeProminence(frame, 5.0f, out);
        gAllocCount.store(0); gAllocBytes.store(0);
        gTrackAllocs.store(true);
        const int trackedCalls = 2000;
        for (int i = 0; i < trackedCalls; ++i)
            eng.computeProminence(frame, 5.0f, out);
        gTrackAllocs.store(false);
        std::printf("  %d calls to computeProminence(): heap allocations observed = %lld (%lld bytes)\n",
                    trackedCalls, gAllocCount.load(), gAllocBytes.load());
    }
    std::printf("\n");

    std::printf("================================================================\n");
    std::printf("Microbenchmark complete. No prominence formulas were modified by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
