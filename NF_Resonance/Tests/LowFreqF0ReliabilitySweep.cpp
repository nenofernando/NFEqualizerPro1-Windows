// PHYSICAL C, Blocker 2 -- short characterization sweep to confirm the
// "supportingPartials >= 2" reliability gate doesn't break legitimate
// cases. NOT a big investigation -- just enough coverage per the user's
// request: fundamentals {60,70,80,90,100,120,150,200}Hz x 5 signal
// configs x {44.1,48,96,192}kHz, reporting supportingPartials, f0Estimated,
// f0Confidence, f0Reliability, problemConfidence(at f0 itself).

#include <JuceHeader.h>
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
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

static constexpr float kHarmonicMaxPenaltyProxy = 0.7f;
static float problemConfidenceProxy(float harmLike, float reliability)
{
    float committed = 1.0f - kHarmonicMaxPenaltyProxy * harmLike;
    return reliability * committed + (1.0f - reliability) * 0.5f;
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double f0s[] = { 60, 70, 80, 90, 100, 120, 150, 200 };
    const char* configNames[] = { "isolated", "f0+H2", "f0+H2+H3", "full series(6H)", "full series+non-harm" };

    int totalRuns = 0;
    int fullSeriesReliableCount = 0, fullSeriesTotal = 0;
    int fullSeriesNonHarmReliableAndCorrectCount = 0, fullSeriesNonHarmTotal = 0;

    for (double sr : rates)
    {
        std::printf(" -- Sample rate %.0f Hz --\n", sr);
        for (double f0 : f0s)
        {
            for (int cfg = 0; cfg < 5; ++cfg)
            {
                int n = (int) (sr * 1.2);
                auto sig = genSilence(n);
                if (cfg == 0) addTone(sig, sr, f0, 0.35f);
                else if (cfg == 1) { addTone(sig, sr, f0, 0.35f); addTone(sig, sr, f0 * 2, 0.30f); }
                else if (cfg == 2) { addTone(sig, sr, f0, 0.35f); addTone(sig, sr, f0 * 2, 0.30f); addTone(sig, sr, f0 * 3, 0.26f); }
                else if (cfg == 3) { for (int h = 1; h <= 6; ++h) addTone(sig, sr, f0 * h, 0.35f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1))); }
                else { for (int h = 1; h <= 6; ++h) addTone(sig, sr, f0 * h, 0.35f * (float) juce::Decibels::decibelsToGain(-3.0f * (h - 1))); addBurst(sig, sr, f0 * 1.63, 0.5f, 8.0, 7); }

                LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
                const int block = 512;
                for (int i = 0; i < n; i += block) az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
                auto ctx = az.currentContext();
                float hl = az.harmonicLikelihoodFor((float) f0);
                float pc = problemConfidenceProxy(hl, ctx.f0Reliability);
                float errCents = ctx.f0Hz > 0 ? 1200.0f * std::log2(ctx.f0Hz / (float) f0) : 9999.0f;

                std::printf("  f0=%5.0fHz %-22s: partials=%d f0Est=%7.2fHz(err=%+7.1fc) f0conf=%.3f f0Reliable=%.3f problemConf(f0)=%.3f\n",
                    f0, configNames[cfg], ctx.supportingPartials, ctx.f0Hz, errCents, ctx.f0Confidence, ctx.f0Reliability, pc);

                ++totalRuns;
                if (cfg == 3) { ++fullSeriesTotal; if (ctx.f0Reliability >= 0.5f) ++fullSeriesReliableCount; }
                if (cfg == 4) { ++fullSeriesNonHarmTotal; if (ctx.f0Reliability >= 0.5f && errCents > -200.0f && errCents < 200.0f) ++fullSeriesNonHarmReliableAndCorrectCount; }
            }
        }
    }

    std::printf("\n=== SWEEP SUMMARY (%d runs) ===\n", totalRuns);
    std::printf("  full series (6H, no interference): reliable (f0Reliability>=0.5) in %d/%d cases\n", fullSeriesReliableCount, fullSeriesTotal);
    std::printf("  full series + non-harmonic burst: reliable AND f0 within +-200c in %d/%d cases\n", fullSeriesNonHarmReliableAndCorrectCount, fullSeriesNonHarmTotal);
    std::printf("  (isolated/H2/H2+H3 configs are EXPECTED to often show low reliability -- fewer\n");
    std::printf("   than 2 independent harmonic matches is architecturally correct there, not a bug)\n");
    return 0;
}
