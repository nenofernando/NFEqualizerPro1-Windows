// PHYSICAL C2.3 item 3: side-by-side comparison of raw magnitude / main
// V2-A5C prominence / aux prominence (LowFrequencyHarmonicAnalyzer's own,
// independent of harmonic context), BEFORE any blending. Diagnostic only.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
}

struct MainProm
{
    SpectralProminenceEngineV5 prom;
    juce::dsp::FFT fft{ 11 };
    std::array<float, 2048> window{};
    std::array<float, 4096> scratch{};
    std::vector<float> magDb, promOut;
    void prepare(double sr) { prom.prepare(2048 / 2 + 1, sr, 2048); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f);
        magDb.assign(2048 / 2 + 1, -120.0f); promOut.assign(2048 / 2 + 1, 0.0f); }
    void processFrame(const float* samples) // samples: 2048 contiguous
    {
        for (int k = 0; k < 2048; ++k) scratch[(size_t) k] = samples[k] * window[(size_t) k];
        std::fill(scratch.begin() + 2048, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins = 2048 / 2 + 1;
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / 2048.0f + 1e-12f, -120.0f);
        }
        prom.computeProminence(magDb, 4.0f, promOut);
    }
};

static float parabolicDelta(float l, float c, float r) { float denom = l - 2.0f * c + r; if (std::abs(denom) < 1.0e-6f) return 0.0f; return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom); }

int main()
{
    const double freqs[] = { 80, 100, 110, 115, 120, 125, 130, 150, 200, 250, 300, 400, 500, 800 };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const float ampDb = 4.0f;
    const double Q = 10.0;

    int totalRows = 0, mainWrongOrder = 0, auxWrongOrder = 0;
    for (double sr : rates)
    {
        std::printf(" -- Sample rate %.0f Hz --\n", sr);
        for (double f : freqs)
        {
            int n = (int) (sr * 0.6);
            auto sig = genSilence(n);
            {
                juce::Random rng(5);
                double bwHz = f / Q;
                for (int k = 0; k < 9; ++k)
                {
                    double t = (double) k / 8.0 - 0.5, ff = f + t * bwHz;
                    double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * ff / sr;
                    float amp = (float) juce::Decibels::decibelsToGain(ampDb) * 0.4f;
                    for (int i = 0; i < n; ++i) { sig[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; }
                }
            }

            MainProm mp; mp.prepare(sr);
            LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
            const int hop = 512;
            float rawMagPeakHz = 0, rawMagPeakDb = -999; int rawBin = -1;
            float mainPeakHz = 0, mainPeakDb = -999; int mainBin = -1;
            float auxPeakHz = 0, auxPeakDb = -999, auxReliability = 0;
            double hostBinHz = sr / 2048.0;
            for (int i = 0; i + 2048 <= n; i += hop)
            {
                mp.processFrame(sig.data() + i);
                aux.pushSamples(sig.data() + i, hop);

                int approxBin = juce::jlimit(1, 2048 / 2 - 2, (int) std::round(f / hostBinHz));
                for (int b = juce::jmax(1, approxBin - 2); b <= juce::jmin(2048 / 2 - 2, approxBin + 2); ++b)
                {
                    if (mp.magDb[(size_t) b] > rawMagPeakDb) { rawMagPeakDb = mp.magDb[(size_t) b]; rawBin = b; }
                    if (mp.promOut[(size_t) b] > mainPeakDb) { mainPeakDb = mp.promOut[(size_t) b]; mainBin = b; }
                }
                float estHz, rel; float p = aux.auxProminenceFor((float) f, &estHz, &rel);
                if (p > auxPeakDb) { auxPeakDb = p; auxPeakHz = estHz; auxReliability = rel; }
            }
            if (rawBin >= 0)
            {
                float l = mp.magDb[(size_t) (rawBin - 1)], c = mp.magDb[(size_t) rawBin], r = mp.magDb[(size_t) (rawBin + 1)];
                float delta = parabolicDelta(l, c, r);
                rawMagPeakHz = (float) ((rawBin + delta) * hostBinHz);
            }
            if (mainBin >= 0)
            {
                float l = mp.promOut[(size_t) (mainBin - 1)], c = mp.promOut[(size_t) mainBin], r = mp.promOut[(size_t) (mainBin + 1)];
                float delta = parabolicDelta(l, c, r);
                mainPeakHz = (float) ((mainBin + delta) * hostBinHz);
            }

            float mainErrHz = mainPeakHz - rawMagPeakHz, mainErrCents = rawMagPeakHz > 0 && mainPeakHz > 0 ? 1200.0f * std::log2(mainPeakHz / rawMagPeakHz) : 9999.0f;
            float auxErrHz = auxPeakHz - rawMagPeakHz, auxErrCents = rawMagPeakHz > 0 && auxPeakHz > 0 ? 1200.0f * std::log2(auxPeakHz / rawMagPeakHz) : 9999.0f;
            bool mainOrderOk = std::abs(mainErrCents) < 150.0f; // "physically correct" = within ~1.5 semitone of the true magnitude peak
            bool auxOrderOk = std::abs(auxErrCents) < 150.0f;
            ++totalRows; if (! mainOrderOk) ++mainWrongOrder; if (! auxOrderOk) ++auxWrongOrder;

            std::printf("  f=%6.1fHz: rawMag=%7.1fHz(%.1fdB) | main=%7.1fHz(%.1fdB) err=%+6.1fc %-7s | aux=%7.1fHz(%.1fdB) rel=%.2f err=%+6.1fc %-7s\n",
                f, rawMagPeakHz, rawMagPeakDb, mainPeakHz, mainPeakDb, mainErrCents, mainOrderOk ? "OK" : "WRONG", auxPeakHz, auxPeakDb, auxReliability, auxErrCents, auxOrderOk ? "OK" : "WRONG");
        }
    }
    std::printf("\n=== SUMMARY ===\n");
    std::printf("  total=%d  main WRONG ordering=%d (%.1f%%)  aux WRONG ordering=%d (%.1f%%)\n",
        totalRows, mainWrongOrder, 100.0 * mainWrongOrder / totalRows, auxWrongOrder, 100.0 * auxWrongOrder / totalRows);
    return 0;
}
