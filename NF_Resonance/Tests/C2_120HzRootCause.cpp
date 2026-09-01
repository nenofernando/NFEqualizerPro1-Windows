// PHYSICAL C2.2 item 5: exact root-cause diagnosis for the 120Hz recall
// stagnation found in the C2.1 sweep (75% -> 75%, unchanged by soft
// admission). Diagnostic only.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
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

int main()
{
    const double freqs[] = { 110, 115, 120, 125, 130 };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int fftSize = 2048, hop = 512;
    const double Q = 10.0; // "medium"
    const float ampDb = 4.0f;

    for (double sr : rates)
    {
        std::printf(" -- Sample rate %.0f Hz (binHz=%.4f) --\n", sr, sr / fftSize);
        for (double f : freqs)
        {
            int n = (int) (sr * 0.5);
            auto sig = genSilence(n);
            addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(ampDb) * 0.4f, Q, 5);

            SpectralProminenceEngineV5 prom; prom.prepare(fftSize / 2 + 1, sr, fftSize);
            prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
            juce::dsp::FFT fft(11);
            std::vector<float> window((size_t) fftSize);
            for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
            std::vector<float> scratch((size_t) fftSize * 2), magDb((size_t) (fftSize / 2 + 1)), promOut((size_t) (fftSize / 2 + 1));

            double binHz = sr / fftSize;
            double exactBin = f / binHz;
            int nearestBin = (int) std::round(exactBin);
            double fracBin = exactBin - std::floor(exactBin);

            float maxPromAtBin = -999.0f, maxPromNeighborLo = -999.0f, maxPromNeighborHi = -999.0f;
            bool everLocalMax = false; int localMaxFrameCount = 0; int totalFrames = 0;
            float maxCandidateEvidence = 0.0f;

            for (int i = 0; i + fftSize <= n; i += hop)
            {
                for (int k = 0; k < fftSize; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
                std::fill(scratch.begin() + fftSize, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform(scratch.data());
                int bins = fftSize / 2 + 1;
                for (int b = 0; b < bins; ++b)
                {
                    float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
                    magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) fftSize + 1e-12f, -120.0f);
                }
                prom.computeProminence(magDb, 4.0f, promOut);
                ++totalFrames;

                float c = promOut[(size_t) nearestBin];
                float l = nearestBin > 0 ? promOut[(size_t) (nearestBin - 1)] : -999.0f;
                float r = nearestBin < bins - 1 ? promOut[(size_t) (nearestBin + 1)] : -999.0f;
                maxPromAtBin = juce::jmax(maxPromAtBin, c);
                maxPromNeighborLo = juce::jmax(maxPromNeighborLo, l);
                maxPromNeighborHi = juce::jmax(maxPromNeighborHi, r);
                bool isLocalMax = c > l && c >= r;
                if (isLocalMax) { everLocalMax = true; ++localMaxFrameCount; }
                float evidence = juce::jlimit(0.0f, 1.0f, (c - 0.5f) / 1.5f); // smoothstep-ish proxy vs the experimental lowFloor..strongFloor range
                maxCandidateEvidence = juce::jmax(maxCandidateEvidence, evidence);
            }

            double signalBw = f / Q;
            bool resLimited = signalBw < binHz;
            const char* reason = "tracked (local max found, prominence>2.0dB)";
            if (! everLocalMax) reason = "NOT a local maximum at ANY frame (neighbor bin(s) equal or higher) -- admission never even gets a candidate to evaluate";
            else if (maxPromAtBin <= 2.0f) reason = "local max exists but prominence never exceeds 2.0dB hard floor";

            std::printf("  f=%6.1fHz nearestBin=%4d fracBin=%.3f promAtBin=%6.2fdB (L=%6.2f R=%6.2f) localMax=%s(%d/%d frames) maxCandEvidence=%.3f resLimited=%s\n",
                f, nearestBin, fracBin, maxPromAtBin, maxPromNeighborLo, maxPromNeighborHi,
                everLocalMax ? "yes" : "NO", localMaxFrameCount, totalFrames, maxCandidateEvidence, resLimited ? "yes" : "no");
            std::printf("      reason: %s\n", reason);
        }
    }
    return 0;
}
