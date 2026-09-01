// Ad-hoc diagnostic: trace the 113.14Hz region frame-by-frame to find why
// C2.3h's bridge lowered its final-frame confidence in ConfidenceLayerCheck.
#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <string>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}

int main(int argc, char** argv)
{
    double sr = 48000.0; const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    int n = (int) (sr * 2.0);
    auto sig = genHarmonicSeries(sr, n, 80.0, 0.4f, 8);
    addBurst(sig, sr, 113.14, 0.35f, 10.0, 300);

    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop); conf.setPersistenceTimeConstants(3.0f, 8.0f);
    if (argc > 1 && std::string(argv[1]) == "nobridge") conf.setContinuationBridgeTimeMs(0.0f);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::vector<float> scratch((size_t) kFft * 2), magDb((size_t) bins), promOut((size_t) bins);

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
        conf.process(promOut, nullptr, nullptr); // NOTE: this section of ConfidenceLayerCheck never passes aux/magDb

        float target = std::log2(113.14f); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
        int fr = (int) (i / kHop);
        if (best && bestDist <= 0.25f)
            std::printf("frame %3d: centerHz=%7.2f prom=%6.2f candEv=%.3f persist=%.3f stab=%.3f harmLike=%.3f harmPen=%.3f conf=%.3f bridged=%d framesAbsent=%d framesPresent=%d activeRegions=%d\n",
                fr, best->centerHz, best->peakProminenceDb, best->candidateEvidence, best->persistence, best->stability, best->harmonicLikelihood, best->lastHarmonicPenalty, best->confidence, (int) best->lastBridged, best->framesAbsent, best->framesPresent, conf.activeRegionCount());
        else
            std::printf("frame %3d: NOT FOUND near 113.14\n", fr);
    }
    return 0;
}
