// Checkpoint A closure, item 7: CPU cost of ResonanceDetector::compute()
// (the new per-bin sum-of-shaped-band-contributions model) at 0/1/7/16/32
// active bands. No sort/compact step exists any more -- every active band
// is evaluated directly per bin, so cost should scale roughly linearly with
// active band count. Isolated ResonanceDetector-only benchmark, no FFT/host.

#include <JuceHeader.h>
#include "DSP/ResonanceDetector.h"
#include <chrono>

static constexpr int kMaxBands = ResonanceDetector::kMaxBands;

int main()
{
    const double sr = 192000.0; // heaviest realistic case: most bins (fftSize/2+1 at 2048 is fixed regardless of SR, but frame rate is what matters for hop budget)
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    const double hopUs = 1000000.0 * (fftSize / 4) / sr;

    std::vector<float> magDb((size_t) bins, -40.0f);
    juce::Random rng(99);
    for (auto& v : magDb) v += rng.nextFloat() * 6.0f;

    std::printf("================================================================\n");
    std::printf("Multiband CPU benchmark -- ResonanceDetector::compute()\n");
    std::printf("sr=%.0f bins=%d hop budget=%.3f us\n", sr, bins, hopUs);
    std::printf("================================================================\n\n");

    for (int count : { 0, 1, 7, 16, 32 })
    {
        float freq[kMaxBands] = {}; float sens[kMaxBands] = {}; float width[kMaxBands] = {}; float focus[kMaxBands] = {}; int shape[kMaxBands] = {}; bool active[kMaxBands] = {};
        for (int i = 0; i < count; ++i)
        {
            float t = (float) i / (float) juce::jmax(1, count - 1);
            float logHz = std::log10(20.0f) + t * (std::log10(20000.0f) - std::log10(20.0f));
            freq[i] = std::pow(10.0f, logHz);
            sens[i] = (i % 2 == 0) ? 5.0f : -5.0f;
            width[i] = 1.7f;
            shape[i] = i % 5;
            focus[i] = 0.5f;
            active[i] = true;
        }

        ResonanceDetector det; det.prepare(bins, sr, fftSize); det.reset();
        std::vector<float> reduction;
        const int iterations = 2000, warmup = 100;
        std::vector<double> times; times.reserve((size_t) (iterations - warmup));
        for (int it = 0; it < iterations; ++it)
        {
            auto t0 = std::chrono::steady_clock::now();
            det.compute(magDb, reduction, 5.0f, 4.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f, 1.5f, freq, sens, width, shape, focus, active, 1.0f);
            auto t1 = std::chrono::steady_clock::now();
            if (it >= warmup) times.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(times.begin(), times.end());
        double med = times[times.size() / 2];
        double p99 = times[(size_t) (times.size() * 0.99)];
        double mx = times.back();
        std::printf("  bands=%-3d  median=%7.3f us (%.2f%% of hop)  P99=%7.3f us (%.2f%%)  max=%7.3f us\n",
                    count, med, 100.0 * med / hopUs, p99, 100.0 * p99 / hopUs, mx);
    }

    std::printf("\n================================================================\n");
    std::printf("Benchmark complete. No production defaults changed by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
