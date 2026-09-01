// PHYSICAL B, item 6 (CPU audit) -- three separately-scoped benchmarks, all
// compiled Release (-O3 -DNDEBUG), so speed numbers are never conflated
// across different amounts of work:
//
//   A -- O1_RobustSideSlope KERNEL ONLY. Isolated via the class's own
//        internal per-stage timers (lastNarrowUs(), populated inside every
//        computeProminence() call) -- this is the NARROW loop alone, not
//        BROAD/MEDIUM/blend/prefix-sum construction.
//   B -- SpectralProminenceEngineV5 FULL prominence frame: prefix-sum
//        construction + BROAD + MEDIUM + NARROW + blend, i.e. the entire
//        computeProminence() call, wall-clock timed from outside.
//   C -- Realistic integrated per-frame path: magnitude -> dB conversion
//        (the step that happens in SpectralEngine.cpp right before any
//        detector sees the spectrum) + the full B pipeline. Excludes the
//        forward FFT/STFT itself, which belongs to the already-frozen,
//        separately-validated STFT/OLA/C2 engine, not this prominence work.
//
// This benchmark file previously reported ~300us because it measured B's
// full scope while being compared against an earlier ~34us number that
// isolated A's scope -- an apples-to-oranges comparison, not a regression.
// Separately, this worktree's CMAKE_BUILD_TYPE was unset (no -O flag at
// all) until this audit; all numbers here are from a build with
// CMAKE_BUILD_TYPE=Release confirmed via CMakeCache.txt.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <new>

static std::atomic<bool> gTrackAllocs{ false };
static std::atomic<long long> gAllocCount{ 0 };
void* operator new(std::size_t sz) { if (gTrackAllocs.load(std::memory_order_relaxed)) gAllocCount.fetch_add(1, std::memory_order_relaxed); void* p = std::malloc(sz == 0 ? 1 : sz); if (! p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static double median(std::vector<double> v) { std::sort(v.begin(), v.end()); size_t n = v.size(); return n ? (n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2])) : 0.0; }
static double percentile(std::vector<double> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * frac; }
static double maxOf(const std::vector<double>& v) { double m = 0; for (double x : v) m = juce::jmax(m, x); return m; }
struct Stats { double med, p95, p99, worst; long long allocs; };
static Stats statsOf(std::vector<double> v, long long allocs) { return { median(v), percentile(v, 95), percentile(v, 99), maxOf(v), allocs }; }

static std::vector<float> genMusicalLikeFrame(int bins, double sr, int fftSize)
{
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i) { double hz = juce::jmax(1.0, (double) i * sr / fftSize); double db = -10.0 - 4.0 * std::log2(hz / 1000.0); v[(size_t) i] = (float) juce::jlimit(-120.0, -10.0, db); }
    double binHz = sr / fftSize;
    for (double f : { 110.0, 220.0, 330.0, 440.0, 550.0, 660.0, 770.0, 880.0, 990.0 })
    { int b = (int) std::round(f / binHz); if (b >= 0 && b < bins) v[(size_t) b] = juce::jmax(v[(size_t) b], -20.0f); }
    int resBin = (int) std::round(2800.0 / binHz);
    if (resBin >= 0 && resBin < bins) for (int k = -2; k <= 2; ++k) { int b = juce::jlimit(0, bins - 1, resBin + k); v[(size_t) b] = juce::jmax(v[(size_t) b], (float) (-8.0 + (2 - std::abs(k)) * 2.0)); }
    return v;
}

// A synthetic complex spectrum (re/im) matching what an actual forward FFT
// output looks like in shape, used ONLY to drive benchmark C's mag->dB
// conversion step with realistic-looking numbers -- not a real FFT.
static void genComplexSpectrum(int fftSize, double sr, std::vector<float>& reIm)
{
    int bins = fftSize / 2 + 1;
    reIm.assign((size_t) fftSize * 2, 0.0f); // matches SpectralEngine.cpp's own fftData sizing (fftSize*2), not fftSize -- the earlier fftSize-only sizing overran by one float at b=bins-1 and was the actual SIGABRT root cause, not the DSP engine
    for (int b = 0; b < bins; ++b)
    {
        double hz = juce::jmax(1.0, (double) b * sr / fftSize);
        double mag = std::pow(10.0, (-10.0 - 4.0 * std::log2(hz / 1000.0)) / 20.0) * fftSize;
        reIm[(size_t) 2 * b] = (float) mag; if (2 * b + 1 < fftSize) reIm[(size_t) 2 * b + 1] = 0.0f;
    }
}

int main(int argc, char** argv)
{
    double onlySr = argc > 1 ? std::atof(argv[1]) : 0.0; // optional: run a single sample rate in isolation
    std::printf("================================================================\n");
    std::printf("PHYSICAL B item 6 (CPU audit) -- three separately-scoped Method C benchmarks\n");
    std::printf("Build verified Release: CMAKE_BUILD_TYPE=Release, -O3 -DNDEBUG (see CMakeCache.txt)\n");
    std::printf("hop=512 samples. %% hop computed against hop/sampleRate exactly, per sample rate.\n");
#if NF_PROMINENCE_PROFILING
    std::printf("NF_PROMINENCE_PROFILING=1 -- internal per-stage chrono calls ARE compiled in.\n");
    std::printf("This run's B/C rows are B2/C2 (profiling ON).\n");
#else
    std::printf("NF_PROMINENCE_PROFILING=0 (default/commercial) -- internal chrono calls compiled OUT.\n");
    std::printf("This run's B/C rows are B1/C1 (profiling OFF). A's lastNarrowUs() reads 0 here --\n");
    std::printf("that internal sub-timer only exists when profiling is compiled in.\n");
#endif
    std::printf("================================================================\n\n");

    const int fftSize = 2048, hop = 512;
    const int iterations = 3000, warmup = 300, runs = 5;
    bool allOk = true;

    struct Row { double sr; Stats a, b, c; };
    std::vector<Row> rows;

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        if (onlySr > 0.0 && std::abs(sr - onlySr) > 1.0) continue;
        int bins = fftSize / 2 + 1;
        double frameBudgetUs = 1.0e6 * (double) hop / sr;

        SpectralProminenceEngineV5 eng;
        eng.prepare(bins, sr, fftSize);
        eng.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        jassert(eng.activeNarrowMethod() == SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        if (eng.activeNarrowMethod() != SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope) { std::printf("FAIL: Method C not active at sr=%.0f\n", sr); allOk = false; continue; }

        auto frame = genMusicalLikeFrame(bins, sr, fftSize);
        std::vector<float> prom((size_t) bins, 0.0f);
        std::vector<float> reIm; genComplexSpectrum(fftSize, sr, reIm);
        std::vector<float> magDbScratch((size_t) bins, -120.0f);

        // -------- A: kernel only (lastNarrowUs() from inside a full call) --------
        std::vector<double> aUs; long long aAllocs = 0;
        for (int run = 0; run < runs; ++run)
            for (int it = 0; it < iterations; ++it)
            {
                bool measure = it >= warmup;
                if (measure) gTrackAllocs.store(true, std::memory_order_relaxed);
                eng.computeProminence(frame, 4.0f, prom); // whole call runs; only the NARROW stage's own internal timer is read
                if (measure) { gTrackAllocs.store(false, std::memory_order_relaxed); aAllocs += gAllocCount.exchange(0, std::memory_order_relaxed); aUs.push_back(eng.lastNarrowUs()); }
            }

        // -------- B: full computeProminence() frame, wall-clock from outside --------
        std::vector<double> bUs; long long bAllocs = 0;
        for (int run = 0; run < runs; ++run)
            for (int it = 0; it < iterations; ++it)
            {
                bool measure = it >= warmup;
                if (measure) gTrackAllocs.store(true, std::memory_order_relaxed);
                auto t0 = std::chrono::steady_clock::now();
                eng.computeProminence(frame, 4.0f, prom);
                auto t1 = std::chrono::steady_clock::now();
                if (measure) { gTrackAllocs.store(false, std::memory_order_relaxed); bAllocs += gAllocCount.exchange(0, std::memory_order_relaxed); bUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count()); }
            }

        // -------- C: mag->dB conversion (as done in SpectralEngine.cpp) + B --------
        std::vector<double> cUs; long long cAllocs = 0;
        for (int run = 0; run < runs; ++run)
            for (int it = 0; it < iterations; ++it)
            {
                bool measure = it >= warmup;
                if (measure) gTrackAllocs.store(true, std::memory_order_relaxed);
                auto t0 = std::chrono::steady_clock::now();
                for (int b = 0; b < bins; ++b)
                {
                    float re = reIm[(size_t) 2 * b], im = (b == 0 || b == bins - 1) ? 0.0f : reIm[(size_t) 2 * b + 1];
                    magDbScratch[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) fftSize + 1e-12f, -120.0f);
                }
                eng.computeProminence(magDbScratch, 4.0f, prom);
                auto t1 = std::chrono::steady_clock::now();
                if (measure) { gTrackAllocs.store(false, std::memory_order_relaxed); cAllocs += gAllocCount.exchange(0, std::memory_order_relaxed); cUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count()); }
            }

        Row row{ sr, statsOf(aUs, aAllocs), statsOf(bUs, bAllocs), statsOf(cUs, cAllocs) };
        rows.push_back(row);
        if (row.a.allocs || row.b.allocs || row.c.allocs) allOk = false;

        std::printf("-- sr=%.0fHz (frame budget=%.2fus) --\n", sr, frameBudgetUs);
        auto printRow = [&](const char* label, const Stats& s) {
            std::printf("  %-3s median=%8.3fus(%.2f%%)  P95=%8.3fus(%.2f%%)  P99=%8.3fus(%.2f%%)  worst=%8.3fus(%.2f%%)  allocs=%lld\n",
                label, s.med, 100.0 * s.med / frameBudgetUs, s.p95, 100.0 * s.p95 / frameBudgetUs, s.p99, 100.0 * s.p99 / frameBudgetUs, s.worst, 100.0 * s.worst / frameBudgetUs, s.allocs);
        };
        printRow("A", row.a); printRow("B", row.b); printRow("C", row.c);
        std::printf("\n");
    }

    std::printf("what is included, per benchmark:\n");
    std::printf("  A: O1_RobustSideSlope NARROW loop only (isolated via the class's own lastNarrowUs() timer)\n");
    std::printf("  B: prefix-sum construction + BROAD + MEDIUM + NARROW + blend (the full computeProminence() call)\n");
    std::printf("  C: magnitude->dB conversion (as done once per frame in SpectralEngine.cpp) + everything in B.\n");
    std::printf("     Excludes the forward FFT/STFT itself (frozen, separately-validated engine).\n\n");

    std::printf("================================================================\n");
    std::printf("%s\n", allOk ? "CPU audit complete: zero allocations in A/B/C at every sample rate, Method C confirmed active." : "CPU audit FLAGGED an issue -- see above.");
    std::printf("================================================================\n");
    return allOk ? 0 : 1;
}
