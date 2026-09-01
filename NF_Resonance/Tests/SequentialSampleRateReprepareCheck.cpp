// PHYSICAL B blocker 1: root-cause investigation for the SIGABRT seen when
// running all 4 sample rates inside ONE process (the earlier
// MethodCCpuBenchmark, before it was split into per-process runs). This is
// NOT a benchmark -- correctness/crash only. Tests BOTH architectures a
// real host could exercise:
//   1) REPREPARE the SAME instance across a sample-rate sequence (this is
//      what a DAW actually does: releaseResources()/prepareToPlay() again).
//   2) Construct/destroy a fresh instance per sample rate in the same
//      process (what the original crashing benchmark actually did).
// Processes several thousand frames at each step, not just prepare().

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <vector>

using NM = SpectralProminenceEngineV5::NarrowMethod;

static std::vector<float> genFrame(int bins, double sr, int fftSize, int seed)
{
    std::vector<float> v((size_t) bins);
    juce::Random rng(seed);
    for (int i = 0; i < bins; ++i)
    {
        double hz = juce::jmax(1.0, (double) i * sr / fftSize);
        double db = -10.0 - 4.0 * std::log2(hz / 1000.0) + (rng.nextFloat() - 0.5f) * 2.0f;
        v[(size_t) i] = (float) juce::jlimit(-120.0, -6.0, db);
    }
    return v;
}

static void processFrames(SpectralProminenceEngineV5& eng, int bins, double sr, int fftSize, int count, const char* label)
{
    std::vector<float> prom((size_t) bins, 0.0f);
    for (int i = 0; i < count; ++i)
    {
        auto frame = genFrame(bins, sr, fftSize, i);
        eng.computeProminence(frame, 4.0f, prom);
        // Sanity: every value must be finite -- a NaN/Inf here would point at
        // a geometry bug (e.g. division by a zero-sized sideband) rather
        // than a pure crash, useful context even if nothing aborts.
        for (float p : prom)
            if (! std::isfinite(p)) { std::printf("  FAIL [%s]: non-finite prominence value encountered (frame %d)\n", label, i); std::exit(1); }
    }
    std::printf("  OK [%s]: %d frames processed, all finite.\n", label, count);
}

int main()
{
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    const int framesPerStep = 4000;
    const double sequence[] = { 44100.0, 48000.0, 96000.0, 192000.0, 48000.0, 44100.0 };

    std::printf("================================================================\n");
    std::printf("Sequential sample-rate reprepare check (PHYSICAL B blocker 1)\n");
    std::printf("================================================================\n\n");

    std::printf("-- Architecture 1: SAME instance, reprepare()'d across the sequence --\n");
    std::printf("sequence: 44.1k -> 48k -> 96k -> 192k -> 48k -> 44.1k\n");
    {
        SpectralProminenceEngineV5 eng;
        eng.setNarrowMethod(NM::O1_RobustSideSlope);
        for (double sr : sequence)
        {
            eng.prepare(bins, sr, fftSize);
            if (eng.activeNarrowMethod() != NM::O1_RobustSideSlope) { std::printf("  FAIL: narrowMethod reset by prepare() at sr=%.0f\n", sr); return 1; }
            juce::String label = "reprepare sr=" + juce::String(sr, 0);
            processFrames(eng, bins, sr, fftSize, framesPerStep, label.toRawUTF8());
        }
    }
    std::printf("Architecture 1 PASSED: no crash across %d total frames, single reused instance.\n\n", (int) (framesPerStep * (sizeof(sequence) / sizeof(sequence[0]))));

    std::printf("-- Architecture 2: construct/destroy a FRESH instance per sample rate --\n");
    for (double sr : sequence)
    {
        SpectralProminenceEngineV5 eng;
        eng.setNarrowMethod(NM::O1_RobustSideSlope);
        eng.prepare(bins, sr, fftSize);
        juce::String label = "fresh-instance sr=" + juce::String(sr, 0);
        processFrames(eng, bins, sr, fftSize, framesPerStep, label.toRawUTF8());
    } // eng destructed here, every iteration
    std::printf("Architecture 2 PASSED: no crash across construct/destroy per sample rate.\n\n");

    std::printf("-- Architecture 3: MANY rapid construct/destroy cycles at a FIXED rate --\n");
    std::printf("(isolates whether repeated construction/destruction alone, independent\n");
    std::printf(" of sample-rate changes, is what triggers instability)\n");
    for (int i = 0; i < 50; ++i)
    {
        SpectralProminenceEngineV5 eng;
        eng.setNarrowMethod(NM::O1_RobustSideSlope);
        eng.prepare(bins, 48000.0, fftSize);
        std::vector<float> prom((size_t) bins, 0.0f);
        auto frame = genFrame(bins, 48000.0, fftSize, i);
        eng.computeProminence(frame, 4.0f, prom);
    }
    std::printf("Architecture 3 PASSED: 50 rapid construct/destroy cycles at sr=48000, no crash.\n\n");

    std::printf("================================================================\n");
    std::printf("ALL SEQUENTIAL SAMPLE-RATE CHECKS PASSED (no SIGABRT reproduced)\n");
    std::printf("================================================================\n");
    return 0;
}
