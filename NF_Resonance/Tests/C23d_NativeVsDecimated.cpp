// PHYSICAL C2.3d: does LowFrequencyHarmonicAnalyzer's internal analysis
// domain (kAnalysisFftSize=2048 @ ~decimatedRate) produce EQUIVALENT
// results for the same conceptual signal whether it arrives native at
// ~48kHz (decimation=1, no filter) or decimated from 192kHz (4:1) /
// 96kHz (2:1)? If yes at every stage, 120Hz@192kHz's residual failure is a
// genuine physical-ambiguity limitation, not a decimator/alignment bug.
//
// NOTE on scope: SpectralProminenceEngineV5 does not expose its internal
// BROAD/MEDIUM/NARROW arrays publicly (only timing accessors) -- per
// standing instruction not to touch/recalibrate Method C, this comparison
// stops at raw magnitude (pre-prominence) and final prominence (post
// full blend), which IS what the aux analyzer's own debugMagDb()/
// debugProminence() expose. A true intermediate-stage breakdown would
// require adding read-only diagnostic accessors to that frozen class,
// which is out of scope without explicit approval.

#include <JuceHeader.h>
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

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

static float parabolicDelta(float l, float c, float r) { float denom = l - 2.0f * c + r; if (std::abs(denom) < 1.0e-6f) return 0.0f; return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom); }

struct SteadyState
{
    std::array<float, 13> magDb{};      // bins 0..12
    std::array<float, 13> promDb{};
    float estimatedHz = 0;
    float peakDb = -999;
    int peakBin = -1;
    float decRms = 0, decPeak = 0;
};

static SteadyState runToSteadyState(double hostSr, const std::vector<float>& hostSig, double approxFreqHz)
{
    // BUG FOUND AND FIXED (C2.3d): this used to cap at a fixed NUMBER of
    // pushSamples() calls, each covering 512 HOST samples -- at 192kHz that
    // is 2.67ms of real time per call vs 10.67ms at 48kHz, a 4x real-time
    // mismatch that starved the high-SR path of settling time and produced
    // an APPARENT divergence that was actually a test-harness artifact, not
    // a decimator bug. Now processes the ENTIRE buffer (same real-time
    // duration, generated identically in both callers) for both paths.
    LowFrequencyHarmonicAnalyzer az; az.prepare(hostSr);
    int n = (int) hostSig.size();
    const int hostHop = 512;
    for (int i = 0; i + hostHop <= n; i += hostHop)
        az.pushSamples(hostSig.data() + i, hostHop);
    SteadyState s;
    double binHz = az.analysisBinHz();
    int approxBin = juce::jlimit(1, (int) az.debugMagDb().size() - 2, (int) std::round(approxFreqHz / binHz));
    for (int b = 0; b <= 12 && b < (int) az.debugMagDb().size(); ++b) { s.magDb[(size_t) b] = az.debugMagDb()[(size_t) b]; s.promDb[(size_t) b] = az.debugProminence()[(size_t) b]; }
    int bestBin = approxBin; float bestVal = -999;
    for (int b = juce::jmax(1, approxBin - 2); b <= juce::jmin((int) az.debugMagDb().size() - 2, approxBin + 2); ++b) if (az.debugMagDb()[(size_t) b] > bestVal) { bestVal = az.debugMagDb()[(size_t) b]; bestBin = b; }
    float l = az.debugMagDb()[(size_t) (bestBin - 1)], c = az.debugMagDb()[(size_t) bestBin], r = az.debugMagDb()[(size_t) (bestBin + 1)];
    s.estimatedHz = (float) ((bestBin + parabolicDelta(l, c, r)) * binHz);
    s.peakDb = c; s.peakBin = bestBin;

    // time-domain decimated ring RMS/peak over the last analysis window
    double sumSq = 0; float pk = 0; int start = (az.debugRingWrite() - LowFrequencyHarmonicAnalyzer::kAnalysisFftSize + LowFrequencyHarmonicAnalyzer::kRingCapacity) % LowFrequencyHarmonicAnalyzer::kRingCapacity;
    for (int i = 0; i < LowFrequencyHarmonicAnalyzer::kAnalysisFftSize; ++i)
    {
        int idx = (start + i) % LowFrequencyHarmonicAnalyzer::kRingCapacity;
        float v = az.debugRing()[(size_t) idx];
        sumSq += (double) v * v; pk = juce::jmax(pk, std::abs(v));
    }
    s.decRms = (float) std::sqrt(sumSq / LowFrequencyHarmonicAnalyzer::kAnalysisFftSize);
    s.decPeak = pk;
    return s;
}

static void printCompare(const char* label, double freq, const SteadyState& a, const SteadyState& b)
{
    std::printf(" -- %s (target ~%.0fHz) --\n", label, freq);
    std::printf("    native48:   peakBin=%2d estHz=%7.2f peakDb=%6.2f decRMS=%.4f decPeak=%.4f\n", a.peakBin, a.estimatedHz, a.peakDb, a.decRms, a.decPeak);
    std::printf("    decimated:  peakBin=%2d estHz=%7.2f peakDb=%6.2f decRMS=%.4f decPeak=%.4f\n", b.peakBin, b.estimatedHz, b.peakDb, b.decRms, b.decPeak);
    float freqErrCents = (a.estimatedHz > 0 && b.estimatedHz > 0) ? 1200.0f * std::log2(b.estimatedHz / a.estimatedHz) : 9999.0f;
    std::printf("    freq diff = %+.1f cents | RMS ratio = %.4f | peakDb diff = %+.2f dB\n", freqErrCents, b.decRms / juce::jmax(0.0001f, a.decRms), b.peakDb - a.peakDb);
    std::printf("    magDb bins 0..12:  native=[");
    for (int i = 0; i <= 12; ++i) std::printf("%.1f%s", a.magDb[(size_t) i], i < 12 ? "," : "");
    std::printf("]\n                       decim =[");
    for (int i = 0; i <= 12; ++i) std::printf("%.1f%s", b.magDb[(size_t) i], i < 12 ? "," : "");
    std::printf("]\n");
    std::printf("    promDb bins 0..12: native=[");
    for (int i = 0; i <= 12; ++i) std::printf("%.1f%s", a.promDb[(size_t) i], i < 12 ? "," : "");
    std::printf("]\n                       decim =[");
    for (int i = 0; i <= 12; ++i) std::printf("%.1f%s", b.promDb[(size_t) i], i < 12 ? "," : "");
    std::printf("]\n");
}

int main()
{
    const double freqs[] = { 80, 100, 120, 170 };
    const double srHigh[] = { 96000.0, 192000.0 };

    for (double srH : srHigh)
    {
        std::printf("======== native 48kHz vs %.0fkHz->48k-equivalent (decimation %dx) ========\n", srH, srH == 96000.0 ? 2 : 4);
        for (double f : freqs)
        {
            // 1a: pure sine
            {
                int n48 = (int) (48000.0 * 0.6), nH = (int) (srH * 0.6);
                auto sig48 = genSilence(n48); addTone(sig48, 48000.0, f, 0.4f);
                auto sigH = genSilence(nH); addTone(sigH, srH, f, 0.4f);
                auto a = runToSteadyState(48000.0, sig48, f);
                auto b = runToSteadyState(srH, sigH, f);
                printCompare("PURE SINE", f, a, b);
            }
            // 1b: fundamental + harmonic series
            {
                int n48 = (int) (48000.0 * 0.6), nH = (int) (srH * 0.6);
                auto sig48 = genHarmonicSeries(48000.0, n48, f, 0.3f, 6);
                auto sigH = genHarmonicSeries(srH, nH, f, 0.3f, 6);
                auto a = runToSteadyState(48000.0, sig48, f);
                auto b = runToSteadyState(srH, sigH, f);
                printCompare("HARMONIC SERIES", f, a, b);
            }
            // 1c: fundamental + artificial non-harmonic resonance
            {
                int n48 = (int) (48000.0 * 0.6), nH = (int) (srH * 0.6);
                auto sig48 = genHarmonicSeries(48000.0, n48, f, 0.3f, 6); addBurst(sig48, 48000.0, f * 1.63, 0.4f, 8.0, 3);
                auto sigH = genHarmonicSeries(srH, nH, f, 0.3f, 6); addBurst(sigH, srH, f * 1.63, 0.4f, 8.0, 3);
                auto a = runToSteadyState(48000.0, sig48, f);
                auto b = runToSteadyState(srH, sigH, f);
                printCompare("+ARTIFICIAL RESONANCE", f, a, b);
            }
        }
    }
    return 0;
}
