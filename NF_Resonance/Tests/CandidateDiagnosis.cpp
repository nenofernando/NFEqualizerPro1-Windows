// NF Resonance -- diagnose exactly how many bins become "candidates" under
// different content types and threshold/margin settings, before deciding
// how to fix the CPU regression found in V2-A2.
#include <JuceHeader.h>
#include <chrono>
#include "DSP/SpectralProminenceEngineV2.h"

static const double sr = 48000.0;
static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin) { return bin * sr / fftSize; }

static std::vector<float> genSilence() { return std::vector<float>((size_t)bins, -100.0f); }

static std::vector<float> genPinkNoise(int seed, float noiseDb)
{
    std::vector<float> m((size_t)bins);
    juce::Random rng(seed);
    for (int i=0;i<bins;++i)
    {
        double hz = juce::jmax(1.0, binHz(i));
        m[(size_t)i] = (float)(-40.0 - 3.0*std::log2(hz/1000.0)) + (rng.nextFloat()-0.5f)*noiseDb;
    }
    return m;
}

// Voice-like: a handful of harmonic partials over a smooth spectral envelope + light noise floor.
static std::vector<float> genVoiceLike(int seed)
{
    std::vector<float> m((size_t)bins, -60.0f);
    juce::Random rng(seed);
    double f0 = 180.0;
    for (int h=1; h<=25; ++h)
    {
        double f = f0*h;
        if (f > 18000) break;
        double envDb = -10.0 - 8.0*std::log2(f/f0); // rolling off harmonics
        for (int i=0;i<bins;++i)
        {
            double hz = juce::jmax(1.0, binHz(i));
            double d = std::log2(hz/f);
            m[(size_t)i] = juce::jmax(m[(size_t)i], (float)(envDb - 60.0*std::abs(d)));
        }
    }
    for (int i=0;i<bins;++i) m[(size_t)i] += (rng.nextFloat()-0.5f)*2.0f;
    return m;
}

int main()
{
    struct Content{ const char* name; std::vector<float> frame; };
    Content contents[] = {
        {"silence", genSilence()},
        {"pink noise (+-1dB jitter)", genPinkNoise(1, 2.0f)},
        {"pink noise (+-3dB jitter)", genPinkNoise(2, 6.0f)},
        {"voice-like (harmonic stack)", genVoiceLike(3)},
    };

    struct ThreshCfg{ double thr, margin; };
    ThreshCfg configs[] = { {1.0,4.0}, {2.5,3.0}, {4.0,2.0}, {6.0,2.0}, {4.0,1.0} };

    std::cout << "==================== CANDIDATE COUNT DIAGNOSIS (1025 bins total) ====================\n\n";
    std::printf("%-30s | %14s | %10s | %10s\n","content","threshold/margin","candidates","%ofBins");
    SpectralProminenceEngineV2 eng; eng.prepare(bins, sr, fftSize, 48);
    std::vector<float> prom;
    for (auto& c : contents)
    {
        for (auto& cfg : configs)
        {
            eng.setCandidateThresholdDb(cfg.thr, cfg.margin);
            eng.computeProminenceHybrid(c.frame, 4.0f, prom);
            int cand = eng.numCandidateBinsLastCall();
            std::printf("%-30s | %6.1f/%6.1f | %10d | %10.1f\n", c.name, cfg.thr, cfg.margin, cand, 100.0*cand/bins);
        }
        std::cout << "\n";
    }

    std::cout << "==================== CPU vs candidate count (threshold sweep, pink noise +-3dB) ====================\n";
    std::printf("%14s | %10s | %10s | %12s\n","threshold","margin","candidates","us/frame(1ch)");
    auto pink = genPinkNoise(2, 6.0f);
    for (auto& cfg : configs)
    {
        eng.setCandidateThresholdDb(cfg.thr, cfg.margin);
        eng.computeProminenceHybrid(pink, 4.0f, prom); // warm
        int cand = eng.numCandidateBinsLastCall();
        const int iters = 100;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i=0;i<iters;++i) eng.computeProminenceHybrid(pink, 4.0f, prom);
        auto t1 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double,std::micro>(t1-t0).count()/iters;
        std::printf("%14.1f | %10.1f | %10d | %12.2f\n", cfg.thr, cfg.margin, cand, us);
    }

    return 0;
}
