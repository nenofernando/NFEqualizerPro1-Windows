// TRANSIENT -> PHYSICAL D authority validation (Sonic Alpha V2). Confirms:
// (1) Transient=5 reproduces today's validated PHYSICAL D baseline
// exactly (amount=1.0, GainMaskV2Check's own bit-exact regression already
// covers this at the full-plugin level -- this file focuses on the
// per-material attack/tail behaviour); (2) Transient=0 removes protection
// authority entirely; (3) Transient=10 preserves attacks more without
// touching sustained material; (4) raising Transient never increases
// reduction beyond the Transient=5 baseline anywhere. No PHYSICAL C/D/
// ConfidenceEngine/gamma/Detail/Sensitivity-Curve file touched by this
// feature -- only GainMaskEngine's own actionWeight->action step.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/ResonanceDetector.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp, int startSample = 0)
{ double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (int i = startSample; i < (int) b.size(); ++i) { b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; } }
static void addClick(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{ juce::Random rng(seed); for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) { float env = (float) std::exp(-(double) i / (lenSamples * 0.3)); b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp * env; } }
static void addNoiseBurst(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{ juce::Random rng(seed); for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp; }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f, int startSample = 0)
{ auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)), startSample); return b; }
static void addDecayingResonance(std::vector<float>& b, double sr, double freqHz, float amp, double decaySeconds, int startSample)
{ double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freqHz / sr; int n = (int) b.size();
  for (int i = startSample; i < n; ++i) { double t = (double) (i - startSample) / sr; float env = (float) std::exp(-t / decaySeconds); b[(size_t) i] += (float) std::sin(ph) * amp * env; ph += inc; } }

// Each signal: a real attack at startSample, followed by a resonant/
// sustaining tail so both "onset" and "tail" windows have real content to
// measure. All ride a light harmonic bed so PHYSICAL C's prominence
// machinery has local context (a bare click has nothing to look prominent
// against).
static std::vector<float> genKick(double sr, int n) { auto b = genHarmonicSeries(sr, n, 60.0, 0.06f, 3); addClick(b, 0, (int) (sr * 0.006), 0.7f, 3); addDecayingResonance(b, sr, 60.0, 0.5f, 0.25, 0); return b; }
static std::vector<float> genSnare(double sr, int n) { auto b = genHarmonicSeries(sr, n, 180.0, 0.05f, 3); addNoiseBurst(b, 0, (int) (sr * 0.02), 0.5f, 2); addDecayingResonance(b, sr, 200.0, 0.35f, 0.12, 0); return b; }
static std::vector<float> genGuitarPick(double sr, int n) { auto b = genHarmonicSeries(sr, n, 110.0, 0.15f, 6, 2.5f); addClick(b, 0, (int) (sr * 0.003), 0.5f, 5); return b; }
static std::vector<float> genVocalConsonant(double sr, int n) { auto b = genHarmonicSeries(sr, n, 140.0, 0.15f, 6, 2.5f); addNoiseBurst(b, 0, (int) (sr * 0.03), 0.4f, 4); return b; }
static std::vector<float> genBassPluck(double sr, int n) { auto b = genHarmonicSeries(sr, n, 55.0, 0.2f, 8, 2.0f); addClick(b, 0, (int) (sr * 0.004), 0.4f, 6); return b; }
// A digital instant on/off IS a genuine (if artificial) transient -- PHYSICAL
// D correctly flags it, which is not a "sustained, no attack" case at all.
// A real sustained tone has no attack because it fades in gently; a short
// linear ramp (~80ms) avoids manufacturing a fake onset while still analysis
// windows start well after the ramp is complete.
static std::vector<float> fadeIn(std::vector<float> b, double sr, double rampSeconds)
{ int rampSamples = (int) (sr * rampSeconds); for (int i = 0; i < rampSamples && i < (int) b.size(); ++i) b[(size_t) i] *= (float) i / (float) rampSamples; return b; }
static std::vector<float> genSustainedVocal(double sr, int n) { return fadeIn(genHarmonicSeries(sr, n, 220.0, 0.2f, 8, 2.5f), sr, 0.08); }
static std::vector<float> genSustainedGuitar(double sr, int n) { return fadeIn(genHarmonicSeries(sr, n, 196.0, 0.2f, 7, 2.5f), sr, 0.08); }
static std::vector<float> genDenseMix(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 62.0, 0.14f, 5, 2.5f);
    auto vocal = genHarmonicSeries(sr, n, 220.0, 0.1f, 6, 2.5f); for (size_t i = 0; i < b.size(); ++i) b[i] += vocal[i];
    addClick(b, 0, (int) (sr * 0.004), 0.5f, 9); addDecayingResonance(b, sr, 62.0, 0.4f, 0.15, 0);
    addNoiseBurst(b, (int) (sr * 0.05), (int) (sr * 0.02), 0.25f, 10);
    return b;
}

struct RunStats { float maxRedOnsetDb = 0, maxRedTailDb = 0, meanRawTP = 0, meanEffTP = 0, outRms = 0; };

static RunStats runMaterial(const std::vector<float>& sig, double sr, float depth, float transientVal)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
    mask.setParams(depth, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f);
    mask.setTransientAmount(transientVal);
    float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
    int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
    mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);

    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));

    RunStats st; double sumOutSq = 0;
    double onsetEndSec = 0.03, tailStartSec = 0.15; // onset = first 30ms, tail = after 150ms
    int n = (int) sig.size();
    double rawTPSum = 0, effTPSum = 0; int tpCount = 0;
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
          magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
        mask.process(magDb, sig.data() + i, kHop, reductionOut);

        double frameTimeSec = (double) i / sr;
        float worst = 0.0f; for (float v : reductionOut) worst = juce::jmin(worst, v);
        if (frameTimeSec < onsetEndSec) st.maxRedOnsetDb = juce::jmin(st.maxRedOnsetDb, worst);
        if (frameTimeSec >= tailStartSec) st.maxRedTailDb = juce::jmin(st.maxRedTailDb, worst);

        for (auto& d : mask.lastRegionActionDebug())
            if (d.active) { rawTPSum += d.transientProt; effTPSum += d.effectiveTransientProt; ++tpCount; }

    }
    st.meanRawTP = tpCount ? (float) (rawTPSum / tpCount) : 0.0f;
    st.meanEffTP = tpCount ? (float) (effTPSum / tpCount) : 0.0f;
    for (float x : sig) sumOutSq += (double) x * x;
    st.outRms = (float) std::sqrt(sumOutSq / juce::jmax(1, n));
    return st;
}

int main()
{
    const double sr = 48000.0;
    int n = (int) (sr * 0.8);
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  %s: %s\n", what, cond ? "PASS" : "FAIL"); if (!cond) allPass = false; };

    std::printf("=== TRANSIENT -> PHYSICAL D authority validation ===\n\n");

    struct Mat { const char* name; std::vector<float> sig; bool hasRealOnset; };
    std::vector<Mat> mats = {
        { "Kick", genKick(sr, n), true },
        { "Snare", genSnare(sr, n), true },
        { "GuitarPick", genGuitarPick(sr, n), true },
        { "VocalConsonant", genVocalConsonant(sr, n), true },
        { "BassPluck", genBassPluck(sr, n), true },
        { "SustainedVocal", genSustainedVocal(sr, n), false },
        { "SustainedGuitar", genSustainedGuitar(sr, n), false },
        { "DenseMix", genDenseMix(sr, n), true },
    };

    for (auto& m : mats)
    {
        auto s0 = runMaterial(m.sig, sr, 7.0f, 0.0f);
        auto s5 = runMaterial(m.sig, sr, 7.0f, 5.0f);
        auto s10 = runMaterial(m.sig, sr, 7.0f, 10.0f);
        std::printf("-- %s --\n", m.name);
        std::printf("  T=0 : onset=%.2fdB tail=%.2fdB rawTP=%.3f effTP=%.3f outRms=%.4f\n", s0.maxRedOnsetDb, s0.maxRedTailDb, s0.meanRawTP, s0.meanEffTP, s0.outRms);
        std::printf("  T=5 : onset=%.2fdB tail=%.2fdB rawTP=%.3f effTP=%.3f outRms=%.4f\n", s5.maxRedOnsetDb, s5.maxRedTailDb, s5.meanRawTP, s5.meanEffTP, s5.outRms);
        std::printf("  T=10: onset=%.2fdB tail=%.2fdB rawTP=%.3f effTP=%.3f outRms=%.4f\n", s10.maxRedOnsetDb, s10.maxRedTailDb, s10.meanRawTP, s10.meanEffTP, s10.outRms);

        check("effTP(T=0) == 0 (no protection authority at all)", std::abs(s0.meanEffTP) < 1.0e-6f);
        check("effTP(T=5) == rawTP(T=5) (amount=1.0, unmodified)", std::abs(s5.meanEffTP - s5.meanRawTP) < 1.0e-5f);
        check("effTP(T=10) >= effTP(T=5) (more authority, never less)", s10.meanEffTP >= s5.meanEffTP - 1.0e-5f);
        check("rawTP identical across T=0/5/10 (PHYSICAL D itself untouched)", std::abs(s0.meanRawTP - s5.meanRawTP) < 1.0e-5f && std::abs(s5.meanRawTP - s10.meanRawTP) < 1.0e-5f);
        if (m.hasRealOnset)
        {
            check("T=0 reduces the onset AT LEAST as much as T=5 (less/no protection -> more or equal onset reduction)", s0.maxRedOnsetDb <= s5.maxRedOnsetDb + 1.0e-3f);
            check("T=10 reduces the onset NO MORE than T=5 (more protection -> onset preserved at least as well)", s10.maxRedOnsetDb >= s5.maxRedOnsetDb - 1.0e-3f);
        }
        else
        {
            // Sustained material: transientProtection is near-zero throughout, so onset/tail should barely move across T=0/5/10.
            check("sustained material: T=0 vs T=10 tail difference is small (<0.3dB, no real transient evidence to act on)", std::abs(s0.maxRedTailDb - s10.maxRedTailDb) < 0.3f);
        }
        // Fundamental requirement: raising Transient can only PRESERVE more, never increase reduction anywhere.
        check("T=10 never reduces MORE than T=5 in the tail either (raising Transient never increases reduction)", s10.maxRedTailDb >= s5.maxRedTailDb - 1.0e-3f);
        std::printf("\n");
    }

    std::printf("-- CPU: incremental cost of setTransientAmount() itself, 192kHz --\n");
    {
        const double srHi = 192000.0; int nHi = (int) (srHi * 0.5);
        auto sig = genDenseMix(srHi, nHi);
        const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
        for (float tval : { 0.0f, 10.0f })
        {
            GainMaskEngine mask; mask.prepare(srHi, kFft, kHop);
            mask.setParams(7.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f); mask.setTransientAmount(tval);
            float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
            int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
            mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);
            juce::dsp::FFT fft(11);
            std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
            for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
            std::vector<double> times; int nn = (int) sig.size();
            for (int i = 0; i + kFft <= nn; i += kHop)
            {
                for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
                std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform(scratch.data());
                for (int b = 0; b < bins; ++b)
                { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
                  magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
                auto t0 = std::chrono::high_resolution_clock::now();
                mask.process(magDb, sig.data() + i, kHop, reductionOut);
                auto t1 = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
            }
            std::sort(times.begin(), times.end());
            double med = times[times.size() / 2], hopBudgetUs = 1.0e6 * (double) kHop / srHi;
            std::printf("  transient=%-4.0f med=%.2fus (%.2f%% of hop budget)\n", tval, med, 100.0 * med / hopBudgetUs);
        }
    }

    std::printf("\n%s\n", allPass ? "=== ALL TRANSIENT CHECKS PASS ===" : "=== TRANSIENT CHECKS FAILED ===");
    return allPass ? 0 : 1;
}
