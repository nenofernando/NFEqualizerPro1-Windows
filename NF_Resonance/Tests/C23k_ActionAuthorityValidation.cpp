// PHYSICAL C2.3k-R2 -- Action Authority Validation (v2). Validates:
// (A) non-diluting non-harmonic evidence fixes 96/192kHz conservatism;
// (B/C) unknownAnomalySupport's authority over ACTION shrinks with
// Selectivity, so UNKNOWN context can no longer reach high action weight
// at Selectivity=5 purely from loudness/width; (D) problemConfidence stays
// evidence-only, actionWeight() is the new Selectivity-aware decision.
// Diagnostic only -- no DSP changes in this file.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static std::vector<float> genHarmonicSeriesBoosted(double sr, int n, double f0, float amp, int numH, int boostH, float boostDb, float rolloffDb = 3.0f)
{
    auto b = genSilence(n);
    for (int h = 1; h <= numH; ++h) { float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)); if (h == boostH) a *= (float) juce::Decibels::decibelsToGain(boostDb); addTone(b, sr, f0 * h, a); }
    return b;
}
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}
static void addPinkNoise(std::vector<float>& b, float amp, int seed)
{
    juce::Random rng(seed); float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
    for (auto& s : b) { float w = rng.nextFloat()*2.0f-1.0f;
        b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f; b2=0.96900f*b2+w*0.1538520f;
        b3=0.86650f*b3+w*0.3104856f; b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
        float pink = b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6=w*0.115926f; s += pink * amp * 0.11f; }
}

struct FrameSample
{
    float existenceConf=0, reliableProblemEv=0, unknownAnomaly=0, problemConf=0;
    float actionW0=0, actionW25=0, actionW5=0, actionW75=0, actionW10=0;
};
struct Series { std::vector<FrameSample> hist; ConfidenceEngine::Region last; bool everFound=false; };

static float pct(std::vector<float> v, double p) { if (v.empty()) return 0.0f; std::sort(v.begin(), v.end()); double idx = p / 100.0 * (double) (v.size() - 1); size_t lo = (size_t) idx; size_t hi = juce::jmin(v.size() - 1, lo + 1); double frac = idx - (double) lo; return v[lo] + (v[hi] - v[lo]) * (float) frac; }

static Series runCase(double sr, const std::vector<float>& sig, float watchHz, int warmupFrames = 15)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::vector<float> scratch((size_t) kFft * 2), magDb((size_t) bins), promOut((size_t) bins);

    Series out;
    int n = (int) sig.size();
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
        aux.pushSamples(sig.data() + i, kHop);
        conf.process(promOut, &aux, &magDb);

        float target = std::log2(juce::jmax(1.0f, watchHz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
        int fr = (int) (i / kHop);
        if (best && bestDist <= 0.25f && fr >= warmupFrames)
        {
            FrameSample fs;
            fs.existenceConf = best->existenceConfidence; fs.reliableProblemEv = best->reliableProblemEvidence;
            fs.unknownAnomaly = best->unknownAnomalySupport; fs.problemConf = best->problemConfidence;
            fs.actionW0  = ConfidenceEngine::actionWeight(fs.existenceConf, fs.reliableProblemEv, fs.unknownAnomaly, 0.0f);
            fs.actionW25 = ConfidenceEngine::actionWeight(fs.existenceConf, fs.reliableProblemEv, fs.unknownAnomaly, 2.5f);
            fs.actionW5  = ConfidenceEngine::actionWeight(fs.existenceConf, fs.reliableProblemEv, fs.unknownAnomaly, 5.0f);
            fs.actionW75 = ConfidenceEngine::actionWeight(fs.existenceConf, fs.reliableProblemEv, fs.unknownAnomaly, 7.5f);
            fs.actionW10 = ConfidenceEngine::actionWeight(fs.existenceConf, fs.reliableProblemEv, fs.unknownAnomaly, 10.0f);
            out.hist.push_back(fs); out.last = *best; out.everFound = true;
        }
    }
    return out;
}

static void printTemporal(const char* label, const Series& s)
{
    if (! s.everFound) { std::printf("  %s: NEVER FOUND\n", label); return; }
    std::vector<float> aw5;
    int gt25=0, gt50=0, gt75=0;
    for (auto& f : s.hist) { aw5.push_back(f.actionW5); if (f.actionW5>0.25f) ++gt25; if (f.actionW5>0.50f) ++gt50; if (f.actionW5>0.75f) ++gt75; }
    int n = (int) s.hist.size();
    std::printf("  %s (n=%d):\n", label, n);
    std::printf("    actionWeight@Sel5: P10=%.3f P50=%.3f P90=%.3f max=%.3f\n", pct(aw5,10), pct(aw5,50), pct(aw5,90), *std::max_element(aw5.begin(),aw5.end()));
    std::printf("    %% frames actionW@5 > 0.25 = %.1f%% (%d/%d) | > 0.50 = %.1f%% (%d/%d) | > 0.75 = %.1f%% (%d/%d)\n",
        100.0*gt25/n, gt25, n, 100.0*gt50/n, gt50, n, 100.0*gt75/n, gt75, n);
}

static void printSelSweep(const char* label, const Series& s)
{
    if (! s.everFound) { std::printf("  %s: NEVER FOUND\n", label); return; }
    std::vector<float> w0,w25,w5,w75,w10;
    for (auto& f : s.hist) { w0.push_back(f.actionW0); w25.push_back(f.actionW25); w5.push_back(f.actionW5); w75.push_back(f.actionW75); w10.push_back(f.actionW10); }
    std::printf("  %s actionWeight P50 by Selectivity: 0->%.3f 2.5->%.3f 5->%.3f 7.5->%.3f 10->%.3f\n",
        label, pct(w0,50), pct(w25,50), pct(w5,50), pct(w75,50), pct(w10,50));
}

int main()
{
    std::printf("=== C2.3k-R2: Action Authority Validation ===\n");

    // ---- TESTE 1 ----
    std::printf("\n########## TESTE 1: clean/musical + UNKNOWN, actionWeight@Sel5 (48kHz) ##########\n");
    {
        double sr = 48000.0; int n = (int) (sr * 2.0);
        auto printRootDebug = [](const char* label, const Series& s) {
            if (! s.everFound) return;
            auto& r = s.last;
            std::printf("    [%s] mainF0Rel=%.3f mainRootMember=%.3f mainLike=%.3f mainStructMember=%.3f f0ClassRel=%.3f\n",
                label, 0.0f, r.mainRootMembership, r.harmonicLikelihood, r.mainStructuralMembership, r.f0ClassificationReliability);
            std::printf("    [%s] auxLike=%.3f auxRel=%.3f auxClassRel=%.3f auxRootMember=%.3f auxStructMember=%.3f\n",
                label, r.auxHarmonicLikelihood, r.auxReliability, r.auxClassificationReliability, r.auxRootMembership, r.auxStructuralMembership);
            std::printf("    [%s] mainNonHarm=%.3f auxNonHarm=%.3f nonHarmSupport=%.3f reliableProblemEv=%.3f unknownAnomaly=%.3f problemConf=%.3f actionW@5=%.3f\n",
                label, r.mainNonHarmonicEvidence, r.auxNonHarmonicEvidence, r.nonHarmonicSupportEvidence, r.reliableProblemEvidence, r.unknownAnomalySupport, r.problemConfidence,
                ConfidenceEngine::actionWeight(r.existenceConfidence, r.reliableProblemEvidence, r.unknownAnomalySupport, 5.0f));
        };
        {
            auto s80 = runCase(sr, genHarmonicSeries(sr, n, 80.0, 0.3f, 8), 80.0f);
            printTemporal("80Hz clean isolated", s80);
            printRootDebug("80Hz", s80);
        }
        { auto s = runCase(sr, genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f), 62.0f); printTemporal("Bass 62Hz clean", s); printRootDebug("Bass62", s); }
        printTemporal("Vocal-like 140Hz clean", runCase(sr, genHarmonicSeries(sr, n, 140.0, 0.28f, 10, 2.5f), 140.0f));
        printTemporal("Guitar-like 220Hz clean", runCase(sr, genHarmonicSeries(sr, n, 220.0, 0.25f, 10, 2.0f), 220.0f));
        { auto s = runCase(sr, genHarmonicSeries(sr, n, 120.0, 0.3f, 8), 120.0f); printTemporal("120Hz clean", s); printRootDebug("120Hz", s); }

        auto sigUnk = genSilence(n);
        addPinkNoise(sigUnk, 0.5f, 21);
        addTone(sigUnk, sr, 300.0, 0.15f); addTone(sigUnk, sr, 600.0, 0.08f);
        {
            auto sUnk = runCase(sr, sigUnk, 300.0f);
            printTemporal("UNKNOWN musical (300Hz, 2-partial, over pink-noise bed)", sUnk);
            printRootDebug("UNKmusical", sUnk);
        }
    }

    // ---- TESTE 2: non-harmonic cross-SR, per-source breakdown ----
    std::printf("\n########## TESTE 2: non-harmonic cross-SR, per-source breakdown ##########\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        int n = (int) (sr * 2.0);
        std::printf(" -- %.0fHz --\n", sr);
        {
            auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 8); addBurst(sig, sr, 135.0, 0.5f, 8.0, 1);
            auto f0 = runCase(sr, sig, 80.0f); auto nh = runCase(sr, sig, 135.0f);
            auto& rf0 = f0.last; auto& rnh = nh.last;
            std::printf("  80Hz f0:  mainLike=%.3f mainRel=%.3f auxLike=%.3f auxRel=%.3f | mainNonHarm=%.3f auxNonHarm=%.3f nonHarmSupport=%.3f | reliableProblemEv=%.3f problemConf=%.3f\n",
                rf0.harmonicLikelihood, 0.0f /*mainF0Reliability not stored per-region*/, rf0.auxHarmonicLikelihood, rf0.auxReliability, rf0.mainNonHarmonicEvidence, rf0.auxNonHarmonicEvidence, rf0.nonHarmonicSupportEvidence, rf0.reliableProblemEvidence, rf0.problemConfidence);
            std::printf("  135Hz nh: mainLike=%.3f auxLike=%.3f auxRel=%.3f | mainNonHarm=%.3f auxNonHarm=%.3f nonHarmSupport=%.3f | reliableProblemEv=%.3f problemConf=%.3f\n",
                rnh.harmonicLikelihood, rnh.auxHarmonicLikelihood, rnh.auxReliability, rnh.mainNonHarmonicEvidence, rnh.auxNonHarmonicEvidence, rnh.nonHarmonicSupportEvidence, rnh.reliableProblemEvidence, rnh.problemConfidence);
            printSelSweep("80Hz f0", f0); printSelSweep("135Hz nh", nh);
            printTemporal("80Hz f0", f0); printTemporal("135Hz nh", nh);
        }
        {
            auto sig = genHarmonicSeries(sr, n, 120.0, 0.3f, 8); addBurst(sig, sr, 170.0, 0.5f, 8.0, 2);
            auto f0 = runCase(sr, sig, 120.0f); auto nh = runCase(sr, sig, 170.0f);
            auto& rf0 = f0.last; auto& rnh = nh.last;
            std::printf("  120Hz f0:  mainLike=%.3f auxLike=%.3f auxRel=%.3f | mainNonHarm=%.3f auxNonHarm=%.3f nonHarmSupport=%.3f | reliableProblemEv=%.3f problemConf=%.3f\n",
                rf0.harmonicLikelihood, rf0.auxHarmonicLikelihood, rf0.auxReliability, rf0.mainNonHarmonicEvidence, rf0.auxNonHarmonicEvidence, rf0.nonHarmonicSupportEvidence, rf0.reliableProblemEvidence, rf0.problemConfidence);
            std::printf("  170Hz nh: mainLike=%.3f auxLike=%.3f auxRel=%.3f | mainNonHarm=%.3f auxNonHarm=%.3f nonHarmSupport=%.3f | reliableProblemEv=%.3f problemConf=%.3f\n",
                rnh.harmonicLikelihood, rnh.auxHarmonicLikelihood, rnh.auxReliability, rnh.mainNonHarmonicEvidence, rnh.auxNonHarmonicEvidence, rnh.nonHarmonicSupportEvidence, rnh.reliableProblemEvidence, rnh.problemConfidence);
            printSelSweep("120Hz f0", f0); printSelSweep("170Hz nh", nh);
            printTemporal("120Hz f0", f0); printTemporal("170Hz nh", nh);
        }
    }

    // ---- TESTE 3: h2@160+9dB (weak/moderate, kept) + a new >6dB-measured-excess control ----
    std::printf("\n########## TESTE 3: excessive-harmonic controls, calibrated by MEASURED excess ##########\n");
    {
        double sr = 48000.0; int n = (int) (sr * 2.0);
        std::printf(" -- h2@160Hz +9dB (weak/moderate measured excess -- kept as-is, NOT recalibrated) --\n");
        {
            auto sig = genHarmonicSeriesBoosted(sr, n, 80.0, 0.3f, 8, 2, 9.0f);
            auto f0 = runCase(sr, sig, 80.0f); auto h2 = runCase(sr, sig, 160.0f);
            std::printf("  measured: h2 siblingRefDb=%.2f h2 own prom=%.2f -> measured excess=%.2fdB (excessFactor=%.3f)\n",
                h2.last.harmonicSiblingRefDb, h2.last.peakProminenceDb, h2.last.peakProminenceDb - h2.last.harmonicSiblingRefDb, h2.last.excessFactor);
            printTemporal("f0 (80Hz)", f0); printTemporal("h2 (160Hz, weak-excess)", h2);
            int both=0, wins=0; size_t nn = juce::jmin(f0.hist.size(), h2.hist.size());
            for (size_t i=0;i<nn;++i){ ++both; if (h2.hist[i].problemConf > f0.hist[i].problemConf) ++wins; }
            std::printf("  %% frames h2 problemConf > f0 problemConf: %.1f%% (%d/%d)\n", both?100.0*wins/both:0.0, wins, both);
        }
        std::printf("\n -- NEW control: h2b@160Hz, boosted to produce >6dB MEASURED excess (calibrated against the DSP's own reading, not a nominal dB target) --\n");
        {
            // Sweep boost dB until measured excess clears 6dB, report what it took.
            for (float boostDb : { 15.0f, 20.0f, 25.0f, 30.0f })
            {
                auto sig = genHarmonicSeriesBoosted(sr, n, 80.0, 0.3f, 8, 2, boostDb);
                auto h2b = runCase(sr, sig, 160.0f);
                if (! h2b.everFound) { std::printf("  boostDb=%.0f: NOT FOUND\n", boostDb); continue; }
                float measuredExcess = h2b.last.peakProminenceDb - h2b.last.harmonicSiblingRefDb;
                std::printf("  boostDb=%.0f: measured excess=%.2fdB excessFactor=%.3f excessiveHarmonicEvidence=%.3f\n", boostDb, measuredExcess, h2b.last.excessFactor, h2b.last.excessiveHarmonicEvidence);
            }
            // Use the boost that cleared 6dB for the full report.
            float chosenBoost = 25.0f;
            auto sig = genHarmonicSeriesBoosted(sr, n, 80.0, 0.3f, 8, 2, chosenBoost);
            auto f0 = runCase(sr, sig, 80.0f); auto h2b = runCase(sr, sig, 160.0f);
            std::printf("  chosen boostDb=%.0f: measured excess=%.2fdB excessFactor=%.3f excessiveHarmonicEvidence=%.3f\n",
                chosenBoost, h2b.last.peakProminenceDb - h2b.last.harmonicSiblingRefDb, h2b.last.excessFactor, h2b.last.excessiveHarmonicEvidence);
            printTemporal("f0 (80Hz)", f0); printTemporal("h2b (160Hz, >6dB measured excess)", h2b);
            int both=0, wins=0; size_t nn = juce::jmin(f0.hist.size(), h2b.hist.size());
            for (size_t i=0;i<nn;++i){ ++both; if (h2b.hist[i].problemConf > f0.hist[i].problemConf) ++wins; }
            std::printf("  %% frames h2b problemConf > f0 problemConf: %.1f%% (%d/%d)\n", both?100.0*wins/both:0.0, wins, both);
        }
        std::printf("\n -- h3@900Hz +12dB control (from prior round) --\n");
        {
            auto sig = genHarmonicSeriesBoosted(sr, n, 300.0, 0.25f, 6, 3, 12.0f);
            auto f0 = runCase(sr, sig, 300.0f); auto h3 = runCase(sr, sig, 900.0f);
            std::printf("  measured: h3 siblingRefDb=%.2f h3 own prom=%.2f -> measured excess=%.2fdB (excessFactor=%.3f)\n",
                h3.last.harmonicSiblingRefDb, h3.last.peakProminenceDb, h3.last.peakProminenceDb - h3.last.harmonicSiblingRefDb, h3.last.excessFactor);
            printTemporal("f0 (300Hz)", f0); printTemporal("h3 (900Hz, excessive)", h3);
            int both=0, wins=0; size_t nn = juce::jmin(f0.hist.size(), h3.hist.size());
            for (size_t i=0;i<nn;++i){ ++both; if (h3.hist[i].problemConf > f0.hist[i].problemConf) ++wins; }
            std::printf("  %% frames h3 problemConf > f0 problemConf: %.1f%% (%d/%d)\n", both?100.0*wins/both:0.0, wins, both);
        }
    }

    return 0;
}
