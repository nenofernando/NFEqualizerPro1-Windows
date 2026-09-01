// PHYSICAL C2.3g: Ground-truth validation of the 135Hz@48kHz CASE A finding.
// Question: did the DETECTOR lose a resonance that was still physically
// present, or did the SYNTHETIC signal itself stop presenting it (beating
// null) at the frames where the region died?
//
// Part 1 measures the OLD test signal (9-tone random-phase burst) directly
// from raw FFT magnitude -- NO SpectralProminenceEngineV5, NO ConfidenceEngine
// -- so the "ground truth" is the physical signal itself, not the detector.
// Part 2 runs the SAME signal through the real pipeline in parallel so
// physical-truth frames can be lined up against detector active/absent frames.
// Part 3 builds an alternative, physically stable 135Hz resonance (bandpass-
// filtered noise through a real 2-pole resonator, no multi-tone beating) at
// matched Q, and runs IT through the real pipeline for comparison.
//
// This file makes NO changes to DSP/ code. Diagnostic only.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <numeric>

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

// A physically stable resonance: white noise excitation through a real
// 2nd-order IIR bandpass resonator (constant Q, constant center freq).
// No multi-tone interference -- the physics of a single resonant mode.
static std::vector<float> genStableResonance(double sr, int n, double freqHz, double Q, float outAmp, int seed)
{
    juce::dsp::IIR::Filter<float> filt;
    filt.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freqHz, (float) Q);
    filt.prepare(juce::dsp::ProcessSpec{ sr, (juce::uint32) n, 1 });
    juce::Random rng(seed);
    std::vector<float> noise((size_t) n);
    for (auto& s : noise) s = (rng.nextFloat() * 2.0f - 1.0f);
    std::vector<float> out((size_t) n);
    double rms = 0.0;
    for (int i = 0; i < n; ++i) { float y = filt.processSample(noise[(size_t) i]); out[(size_t) i] = y; rms += (double) y * y; }
    rms = std::sqrt(rms / juce::jmax(1, n));
    float scale = rms > 1e-9 ? (outAmp / (float) rms) : 1.0f;
    for (auto& s : out) s *= scale;
    return out;
}

struct FrameMeas { float local135Db, contextDb, ratioDb, peakFreq; };

int main()
{
    const double sr = 48000.0; const int kFft = 2048, kHop = 512;
    const int n = (int) (sr * 2.0);
    const double binHz = sr / (double) kFft;
    const int bins = kFft / 2 + 1;

    auto sigOld = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
    addBurst(sigOld, sr, 135.0, 0.5f, 8.0, 1);

    // ---- Part 1+2: raw physical measurement + real pipeline, same loop ----
    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop);
    juce::dsp::FFT fft(11);
    std::array<float, kFft> window{};
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::array<float, kFft * 2> scratch{};
    std::vector<float> magDb((size_t) bins), promOut((size_t) bins);

    auto findRegionNear = [&](float hz, float tol = 0.25f) -> ConfidenceEngine::Region {
        float target = std::log2(juce::jmax(1.0f, hz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
        if (best && bestDist <= tol) return *best; return ConfidenceEngine::Region{};
    };

    auto binOf = [&](double hz) { return (int) std::lround(hz / binHz); };
    const int b135lo = binOf(117.0), b135hi = binOf(164.0);   // ~bins 5-7
    const int bCtxLo = binOf(211.0), bCtxHi = binOf(258.0);   // bins 9-11, away from 80Hz's h1/h2 and 135Hz
    const int bSearchLo = binOf(94.0), bSearchHi = binOf(188.0);

    auto measure = [&]() -> FrameMeas {
        double localPow = 0.0; for (int b = b135lo; b <= b135hi; ++b) localPow += std::pow(10.0, magDb[(size_t) b] / 10.0);
        double ctxPow = 0.0; int ctxN = 0; for (int b = bCtxLo; b <= bCtxHi; ++b) { ctxPow += std::pow(10.0, magDb[(size_t) b] / 10.0); ++ctxN; }
        double ctxAvgDb = 10.0 * std::log10(juce::jmax(1e-12, ctxPow / juce::jmax(1, ctxN)));
        double localDb = 10.0 * std::log10(juce::jmax(1e-12, localPow));
        int peakBin = bSearchLo; float peakVal = magDb[(size_t) bSearchLo];
        for (int b = bSearchLo; b <= bSearchHi; ++b) if (magDb[(size_t) b] > peakVal) { peakVal = magDb[(size_t) b]; peakBin = b; }
        float l = (peakBin > 0) ? magDb[(size_t) (peakBin - 1)] : peakVal, c = magDb[(size_t) peakBin], r = (peakBin < bins - 1) ? magDb[(size_t) (peakBin + 1)] : peakVal;
        float denom = (l - 2.0f * c + r); float delta = std::abs(denom) > 1e-6f ? 0.5f * (l - r) / denom : 0.0f;
        delta = juce::jlimit(-0.5f, 0.5f, delta);
        float peakFreq = (float) ((peakBin + delta) * binHz);
        return { (float) localDb, (float) ctxAvgDb, (float) (localDb - ctxAvgDb), peakFreq };
    };

    std::vector<FrameMeas> measHist; std::vector<bool> detActiveHist; std::vector<float> candEvHist;
    const int warmupFrames = 15;
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sigOld[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f);
        }
        auto m = measure();
        prom.computeProminence(magDb, 4.0f, promOut);
        aux.pushSamples(sigOld.data() + i, kHop);
        conf.process(promOut, &aux, &magDb);
        auto r135 = findRegionNear(135.0f);

        int fr = (int) (i / kHop);
        if (fr >= warmupFrames) { measHist.push_back(m); detActiveHist.push_back(r135.active); candEvHist.push_back(r135.candidateEvidence); }
    }

    std::printf("=== C2.3g: Ground-truth validation, 135Hz@48kHz (old multi-tone burst signal) ===\n");
    std::printf("\n-- item 1/2: physical measurement vs detector state, first 60 post-warmup frames --\n");
    std::printf("  (local135Db = power-sum bins %.0f-%.0fHz; contextDb = avg bins %.0f-%.0fHz; peakFreq = parabolic-interp local max search %.0f-%.0fHz)\n",
        b135lo * binHz, b135hi * binHz, bCtxLo * binHz, bCtxHi * binHz, bSearchLo * binHz, bSearchHi * binHz);
    for (size_t idx = 0; idx < measHist.size() && idx < 60; ++idx)
    {
        auto& m = measHist[idx];
        std::printf("  frame %3zu: local=%.2fdB ctx=%.2fdB ratio=%6.2fdB peakFreq=%.1fHz | detector=%s candEv=%.3f\n",
            idx, m.local135Db, m.contextDb, m.ratioDb, m.peakFreq, detActiveHist[idx] ? "ACTIVE" : "absent", candEvHist[idx]);
    }

    double sumRatioPresent = 0, sumRatioAbsent = 0; int nPresent = 0, nAbsent = 0;
    for (size_t idx = 0; idx < measHist.size(); ++idx)
    {
        if (detActiveHist[idx]) { sumRatioPresent += measHist[idx].ratioDb; ++nPresent; }
        else { sumRatioAbsent += measHist[idx].ratioDb; ++nAbsent; }
    }
    std::printf("\n-- summary: physical local/context ratio, detector-ACTIVE frames vs detector-ABSENT frames --\n");
    std::printf("  ACTIVE frames (n=%d): avg physical ratio = %.2f dB\n", nPresent, nPresent ? (float) (sumRatioPresent / nPresent) : 0.0f);
    std::printf("  ABSENT frames (n=%d): avg physical ratio = %.2f dB\n", nAbsent, nAbsent ? (float) (sumRatioAbsent / nAbsent) : 0.0f);

    auto gapStats = [&](const char* label, const std::vector<bool>& hist, double hopMs)
    {
        std::vector<int> gaps; int cur = 0;
        for (bool a : hist) { if (! a) ++cur; else { if (cur > 0) gaps.push_back(cur); cur = 0; } }
        if (cur > 0) gaps.push_back(cur);
        if (gaps.empty()) { std::printf("  %s: no gaps observed\n", label); return; }
        auto p = [&](double pct) { std::vector<int> s = gaps; std::sort(s.begin(), s.end()); double idx = pct / 100.0 * (double) (s.size() - 1); return s[(size_t) std::lround(idx)]; };
        int mx = *std::max_element(gaps.begin(), gaps.end());
        std::printf("  %s: n_gaps=%d P50=%d frames(%.1fms) P90=%d frames(%.1fms) max=%d frames(%.1fms)\n",
            label, (int) gaps.size(), p(50), p(50) * hopMs, p(90), p(90) * hopMs, mx, mx * hopMs);
    };
    double hopMs48 = 1000.0 * kHop / sr;
    std::printf("\n-- gap distribution (consecutive detector-ABSENT frame runs, post-warmup) --\n");
    gapStats("135Hz old (beating) signal", detActiveHist, hopMs48);

    // ---- Part 3: stable resonator control ----
    std::printf("\n=== item 3: stable resonator control (noise -> 2-pole bandpass, Q=8, 135Hz, no multi-tone beating) ===\n");
    auto sigStable = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
    auto res = genStableResonance(sr, n, 135.0, 8.0, 0.5f, 7);
    for (int i = 0; i < n; ++i) sigStable[(size_t) i] += res[(size_t) i];

    SpectralProminenceEngineV5 prom2; prom2.prepare(bins, sr, kFft);
    prom2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux2; aux2.prepare(sr);
    ConfidenceEngine conf2; conf2.prepare(sr, kFft, kHop);
    std::vector<float> magDb2((size_t) bins), promOut2((size_t) bins);
    std::vector<float> c135stable, promStable; int presentStable = 0, absentStable = 0, totalStable = 0;
    std::vector<bool> detActiveStable;
    ConfidenceEngine::Region lastStable;
    auto findRegionNear2 = [&](float hz, float tol = 0.25f) -> ConfidenceEngine::Region {
        float target = std::log2(juce::jmax(1.0f, hz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1.0e9f;
        for (auto& rr : conf2.regions()) { if (! rr.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, rr.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &rr; } }
        if (best && bestDist <= tol) return *best; return ConfidenceEngine::Region{};
    };
    for (int i = 0; i + kFft <= n; i += kHop)
    {
        for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sigStable[(size_t) (i + k)] * window[(size_t) k];
        std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb2[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f);
        }
        prom2.computeProminence(magDb2, 4.0f, promOut2);
        aux2.pushSamples(sigStable.data() + i, kHop);
        conf2.process(promOut2, &aux2, &magDb2);
        auto r = findRegionNear2(135.0f);
        int fr = (int) (i / kHop);
        if (fr >= warmupFrames) { ++totalStable; detActiveStable.push_back(r.active); if (r.active) { ++presentStable; c135stable.push_back(r.confidence); promStable.push_back(r.peakProminenceDb); } else ++absentStable; }
        lastStable = r;
    }
    auto pct = [](std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; };
    std::printf("  135Hz(stable resonator) post-warmup: present=%d absent=%d / %d frames\n", presentStable, absentStable, totalStable);
    if (! c135stable.empty())
        std::printf("  confidence: P50=%.3f P90=%.3f max=%.3f\n", pct(c135stable, 50), pct(c135stable, 90), *std::max_element(c135stable.begin(), c135stable.end()));
    if (! promStable.empty())
        std::printf("  mainPeakProm (active frames): min=%.2fdB avg=%.2fdB max=%.2fdB (n=%d)\n",
            *std::min_element(promStable.begin(), promStable.end()),
            std::accumulate(promStable.begin(), promStable.end(), 0.0f) / (float) promStable.size(),
            *std::max_element(promStable.begin(), promStable.end()), (int) promStable.size());
    gapStats("135Hz stable resonator", detActiveStable, hopMs48);
    if (lastStable.active)
        std::printf("  last frame: candEv=%.3f persist=%.3f stability=%.3f effReliability=%.3f finalConfidence=%.3f mainPeakProm=%.2f\n",
            lastStable.candidateEvidence, lastStable.persistence, lastStable.stability, lastStable.effectiveReliability, lastStable.confidence, lastStable.peakProminenceDb);
    else
        std::printf("  last frame: NOT FOUND\n");

    std::printf("\n=== decision inputs ===\n");
    std::printf("  CASE 1 check: old-signal ABSENT frames show physical ratio near/below zero AND stable resonator is reliably detected -> harness/ground-truth ambiguity, no DSP change.\n");
    std::printf("  CASE 2 check: old-signal ABSENT frames still show clearly elevated physical ratio -> real tracking/admission gap at 48kHz.\n");
    std::printf("  CASE 3 check: stable resonator ALSO fails to be reliably detected -> stop, report, do not calibrate.\n");

    return 0;
}
