// PHYSICAL D -- Transient Protection validation. Diagnostic only, no gain
// touched. PHYSICAL C (Method C, prominence, soft admission, top-K, Region
// Continuation, dual-source rescue, harmonic reasoning, Existence/Problem
// Confidence, Selectivity, F0/classification reliability) is NOT modified
// anywhere in this file or in TransientProtectionEngine itself -- this test
// only CONSUMES ConfidenceEngine's regions() alongside the new,
// independent TransientProtectionEngine.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include "DSP/TransientProtectionEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp, int startSample = 0, int endSample = -1)
{
    if (endSample < 0) endSample = (int) b.size();
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < (int) b.size(); ++i) { if (i >= startSample && i < endSample) b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f) { auto b = genSilence(n); for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1))); return b; }
static void addDecayingResonance(std::vector<float>& b, double sr, double freqHz, float amp, double decaySeconds, int startSample)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freqHz / sr; int n = (int) b.size();
    for (int i = startSample; i < n; ++i) { double t = (double) (i - startSample) / sr; float env = (float) std::exp(-t / decaySeconds); b[(size_t) i] += (float) std::sin(ph) * amp * env; ph += inc; }
}
static void addClick(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i)
    { float env = (float) std::exp(-(double) i / (lenSamples * 0.3)); b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp * env; }
}
static void addNoiseBurst(std::vector<float>& b, int startSample, int lenSamples, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < lenSamples && startSample + i < (int) b.size(); ++i) b[(size_t) (startSample + i)] += (rng.nextFloat() * 2.0f - 1.0f) * amp;
}

struct FrameRec { float problemConf=0, transEv=0, transProt=0; float centerHz=0, existenceConf=0, problemDecision=0, reliableProblemEv=0, unknownAnomaly=0, harmonicCtxRel=0; bool found=false; };
struct Pipeline
{
    SpectralProminenceEngineV5 prom; LowFrequencyHarmonicAnalyzer aux; ConfidenceEngine conf; TransientProtectionEngine trans;
    juce::dsp::FFT fft{11}; std::vector<float> window, scratch, magDb, promOut;
    int kFft = 2048, kHop = 512, bins = 1025;
    double sr = 48000.0;
    void prepare(double sampleRate)
    {
        sr = sampleRate; bins = kFft / 2 + 1;
        prom.prepare(bins, sr, kFft); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        aux.prepare(sr); conf.prepare(sr, kFft, kHop); trans.prepare(sr, kFft, kHop);
        window.assign((size_t) kFft, 0.0f);
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        scratch.assign((size_t) kFft * 2, 0.0f); magDb.assign((size_t) bins, -120.0f); promOut.assign((size_t) bins, 0.0f);
    }
    std::vector<FrameRec> run(const std::vector<float>& sig, float watchHz, int warmupFrames = 15)
    {
        std::vector<FrameRec> out;
        int n = (int) sig.size();
        for (int i = 0; i + kFft <= n; i += kHop)
        {
            for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            for (int b = 0; b < bins; ++b)
            { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
              magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
            prom.computeProminence(magDb, 4.0f, promOut);
            aux.pushSamples(sig.data() + i, kHop);
            conf.process(promOut, &aux, &magDb);
            trans.process(magDb);

            int fr = (int) (i / kHop);
            if (fr >= warmupFrames)
            {
                float target = std::log2(juce::jmax(1.0f, watchHz)); const ConfidenceEngine::Region* best = nullptr; float bestDist = 1e9f;
                for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - target); if (d < bestDist) { bestDist = d; best = &r; } }
                FrameRec rec;
                rec.found = best && bestDist <= 0.3f;
                rec.problemConf = rec.found ? best->confidence : 0.0f;
                rec.centerHz = rec.found ? best->centerHz : 0.0f;
                rec.existenceConf = rec.found ? best->existenceConfidence : 0.0f;
                rec.problemDecision = rec.found ? best->problemDecisionEvidence : 0.0f;
                rec.reliableProblemEv = rec.found ? best->reliableProblemEvidence : 0.0f;
                rec.unknownAnomaly = rec.found ? best->unknownAnomalySupport : 0.0f;
                rec.harmonicCtxRel = rec.found ? best->harmonicContextReliability : 0.0f;
                rec.transEv = trans.transientEvidenceFor(watchHz);
                rec.transProt = trans.transientProtectionFor(watchHz);
                out.push_back(rec);
            }
        }
        return out;
    }
};

static void printTimeSeries(const char* label, const std::vector<FrameRec>& hist, double hopMs, int maxFrames = 40)
{
    std::printf("  %s:\n", label);
    for (int i = 0; i < (int) hist.size() && i < maxFrames; ++i)
        std::printf("    t=%6.1fms  problemConf=%.3f transEv=%.3f transProt=%.3f\n", i * hopMs, hist[(size_t) i].problemConf, hist[(size_t) i].transEv, hist[(size_t) i].transProt);
}

// Finds the peak transProt frame, then measures how many ms it takes to
// decay to 37%/10% of that peak (T63/T90 of the DECAY, i.e. release timing).
static void measureDecay(const std::vector<FrameRec>& hist, double hopMs, float& outT63Ms, float& outT90Ms, float& outPeak)
{
    outT63Ms = -1.0f; outT90Ms = -1.0f; outPeak = 0.0f;
    int peakIdx = -1;
    for (int i = 0; i < (int) hist.size(); ++i) if (hist[(size_t) i].transProt > outPeak) { outPeak = hist[(size_t) i].transProt; peakIdx = i; }
    if (peakIdx < 0 || outPeak <= 0.0f) return;
    for (int i = peakIdx; i < (int) hist.size(); ++i)
    {
        if (outT63Ms < 0.0f && hist[(size_t) i].transProt <= outPeak * 0.37f) outT63Ms = (float) ((i - peakIdx) * hopMs);
        if (outT90Ms < 0.0f && hist[(size_t) i].transProt <= outPeak * 0.10f) outT90Ms = (float) ((i - peakIdx) * hopMs);
    }
}

int main()
{
    std::printf("=== PHYSICAL D: Transient Protection Validation ===\n");
    const double sr = 48000.0; const double hopMs = 1000.0 * 512.0 / sr;

    // ---- item 5: CENTRAL PROOF -- impulse/attack + narrow decaying resonance ----
    std::printf("\n########## item 5: attack + decaying resonance (central proof, 48kHz) ##########\n");
    {
        int n = (int) (sr * 1.5);
        auto sig = genSilence(n);
        int attackStart = (int) (sr * 0.3);
        addClick(sig, attackStart, (int) (sr * 0.004), 0.9f, 7); // ~4ms broadband click = the "attack"
        addDecayingResonance(sig, sr, 300.0, 0.5, 0.6, attackStart); // 300Hz ringing, 0.6s decay constant, starts at the same instant as the click

        Pipeline p; p.prepare(sr);
        auto hist = p.run(sig, 300.0f, 0);
        // align warmup: report starting a few frames before the attack for context
        int attackFrame = attackStart / 512;
        int reportStart = juce::jmax(0, attackFrame - 3);
        std::vector<FrameRec> windowed(hist.begin() + reportStart, hist.end());
        printTimeSeries("300Hz region (attack at t=0 in this listing)", windowed, hopMs, 50);

        float t63, t90, peak;
        std::vector<FrameRec> fromAttack(hist.begin() + attackFrame, hist.end());
        measureDecay(fromAttack, hopMs, t63, t90, peak);
        std::printf("  transientProtection: peak=%.3f  decay T63=%.1fms  T90=%.1fms\n", peak, t63, t90);
        // problemConfidence should REMAIN elevated through the ringing tail
        // while transientProtection has already decayed -- the central
        // divergence this checkpoint exists to prove.
        int tailFrame = attackFrame + (int) (300.0 / hopMs); // ~300ms after attack
        if (tailFrame < (int) hist.size())
            std::printf("  at t=+300ms after attack: problemConfidence=%.3f transientProtection=%.3f (want: problemConf still meaningful, transProt low)\n",
                hist[(size_t) tailFrame].problemConf, hist[(size_t) tailFrame].transProt);

        std::printf("  -- diagnostic: frames 28-34 post-attack (around the jump) --\n");
        for (int i = 28; i <= 34 && i < (int) fromAttack.size(); ++i)
        {
            auto& r = fromAttack[(size_t) i];
            std::printf("    t=%6.1fms found=%d centerHz=%.1f existenceConf=%.3f harmonicCtxRel=%.3f problemDecision=%.3f reliableProblemEv=%.3f unknownAnomaly=%.3f problemConf=%.3f\n",
                i * hopMs, (int) r.found, r.centerHz, r.existenceConf, r.harmonicCtxRel, r.problemDecision, r.reliableProblemEv, r.unknownAnomaly, r.problemConf);
        }
    }

    // ---- item 6: musical material ----
    std::printf("\n########## item 6: musical material (48kHz) ##########\n");
    {
        int n = (int) (sr * 1.0);
        Pipeline p;

        // Kick: 60Hz decaying thump + click
        { auto sig = genSilence(n); int st = (int)(sr*0.1); addClick(sig, st, (int)(sr*0.003), 0.9f, 1); addDecayingResonance(sig, sr, 60.0, 0.8, 0.25, st);
          p.prepare(sr); auto h = p.run(sig, 60.0f, 0); float t63,t90,peak; measureDecay(std::vector<FrameRec>(h.begin()+(st/512), h.end()), hopMs, t63, t90, peak);
          std::printf("  Kick (60Hz): transProt peak=%.3f T63=%.1fms T90=%.1fms | last problemConf=%.3f\n", peak, t63, t90, h.back().problemConf); }

        // Snare: noise burst + ~200Hz body
        { auto sig = genSilence(n); int st = (int)(sr*0.1); addNoiseBurst(sig, st, (int)(sr*0.02), 0.6f, 2); addDecayingResonance(sig, sr, 200.0, 0.4, 0.08, st);
          p.prepare(sr); auto h = p.run(sig, 200.0f, 0); float t63,t90,peak; measureDecay(std::vector<FrameRec>(h.begin()+(st/512), h.end()), hopMs, t63, t90, peak);
          std::printf("  Snare (200Hz body): transProt peak=%.3f T63=%.1fms T90=%.1fms | last problemConf=%.3f\n", peak, t63, t90, h.back().problemConf); }

        // Percussion: short broadband click, higher freq ring
        { auto sig = genSilence(n); int st = (int)(sr*0.1); addClick(sig, st, (int)(sr*0.002), 0.8f, 3); addDecayingResonance(sig, sr, 3000.0, 0.3, 0.05, st);
          p.prepare(sr); auto h = p.run(sig, 3000.0f, 0); float t63,t90,peak; measureDecay(std::vector<FrameRec>(h.begin()+(st/512), h.end()), hopMs, t63, t90, peak);
          std::printf("  Percussion (3kHz): transProt peak=%.3f T63=%.1fms T90=%.1fms | last problemConf=%.3f\n", peak, t63, t90, h.back().problemConf); }

        // Vocal consonant: short noise burst (fricative-like), no sustained tail
        { auto sig = genSilence(n); int st = (int)(sr*0.1); addNoiseBurst(sig, st, (int)(sr*0.06), 0.4f, 4);
          p.prepare(sr); auto h = p.run(sig, 4000.0f, 0); float t63,t90,peak; measureDecay(std::vector<FrameRec>(h.begin()+(st/512), h.end()), hopMs, t63, t90, peak);
          std::printf("  Vocal consonant (~4kHz frication): transProt peak=%.3f T63=%.1fms T90=%.1fms\n", peak, t63, t90); }

        // Guitar pick attack + sustained note (110Hz harmonic series)
        { auto sig = genHarmonicSeries(sr, n, 110.0, 0.25f, 8, 2.5f); addClick(sig, 0, (int)(sr*0.003), 0.5f, 5);
          p.prepare(sr); auto h = p.run(sig, 110.0f, 0);
          std::printf("  Guitar pick+sustain (110Hz): transProt onset=%.3f (frame0) -> steady-state=%.3f (last) | problemConf steady=%.3f\n", h.front().transProt, h.back().transProt, h.back().problemConf);
          std::printf("    time series (every 5th frame): ");
          for (size_t i = 0; i < h.size(); i += 5) std::printf("t=%.0fms:%.3f ", i * hopMs, h[i].transProt);
          std::printf("\n"); }

        // Bass pluck: attack + sustained 55Hz fundamental
        { auto sig = genHarmonicSeries(sr, n, 55.0, 0.3f, 8, 2.0f); addClick(sig, 0, (int)(sr*0.004), 0.5f, 6);
          p.prepare(sr); auto h = p.run(sig, 55.0f, 0);
          std::printf("  Bass pluck+sustain (55Hz): transProt onset=%.3f -> steady-state=%.3f | problemConf steady=%.3f\n", h.front().transProt, h.back().transProt, h.back().problemConf); }

        // Sustained vocal vowel: pure harmonic series, no attack at all
        { auto sig = genHarmonicSeries(sr, n, 220.0, 0.28f, 10, 2.5f);
          p.prepare(sr); auto h = p.run(sig, 220.0f, 15);
          float maxProt = 0; for (auto& r : h) maxProt = juce::jmax(maxProt, r.transProt);
          std::printf("  Sustained vocal vowel (220Hz, no attack): max transProt over run=%.3f (want ~0) | problemConf=%.3f\n", maxProt, h.back().problemConf); }

        // Sustained guitar resonance: same, different f0
        { auto sig = genHarmonicSeries(sr, n, 330.0, 0.28f, 10, 2.0f);
          p.prepare(sr); auto h = p.run(sig, 330.0f, 15);
          float maxProt = 0; for (auto& r : h) maxProt = juce::jmax(maxProt, r.transProt);
          std::printf("  Sustained guitar resonance (330Hz, no attack): max transProt over run=%.3f (want ~0) | problemConf=%.3f\n", maxProt, h.back().problemConf); }

        // Dense mix: kick + bass + vocal-like + guitar-like, all together
        { auto sig = genHarmonicSeries(sr, n, 62.0, 0.2f, 6, 2.5f); // bass bed
          auto vocal = genHarmonicSeries(sr, n, 220.0, 0.15f, 8, 2.5f); for (size_t i=0;i<sig.size();++i) sig[i]+=vocal[i];
          int st = (int)(sr*0.1); addClick(sig, st, (int)(sr*0.003), 0.7f, 9); addDecayingResonance(sig, sr, 60.0, 0.6, 0.2, st);
          addNoiseBurst(sig, (int)(sr*0.4), (int)(sr*0.02), 0.4f, 10); // snare-like hit mid-way
          p.prepare(sr); auto h = p.run(sig, 60.0f, 0);
          std::printf("  Dense mix (watching 60Hz kick band): transProt at attack=%.3f, +300ms later=%.3f | problemConf steady=%.3f\n",
              h[(size_t) (st/512)].transProt, h[juce::jmin(h.size()-1,(size_t)(st/512 + (int)(300.0/hopMs)))].transProt, h.back().problemConf); }
    }

    // ---- item 10: cross-SR ----
    std::printf("\n########## item 10: cross-SR (attack + decaying resonance, same signal design) ##########\n");
    for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        double hMs = 1000.0 * 512.0 / testSr;
        int n = (int) (testSr * 1.5);
        auto sig = genSilence(n);
        int attackStart = (int) (testSr * 0.3);
        addClick(sig, attackStart, (int) (testSr * 0.004), 0.9f, 7);
        addDecayingResonance(sig, testSr, 300.0, 0.5, 0.6, attackStart);
        Pipeline p; p.prepare(testSr);
        auto hist = p.run(sig, 300.0f, 0);
        int attackFrame = attackStart / 512;
        float t63, t90, peak;
        std::vector<FrameRec> fromAttack(hist.begin() + attackFrame, hist.end());
        measureDecay(fromAttack, hMs, t63, t90, peak);
        std::printf("  sr=%.0f: hop=%.3fms peak=%.3f T63=%.1fms T90=%.1fms\n", testSr, hMs, peak, t63, t90);
    }

    // ---- item 11: realtime / CPU (standalone TransientProtectionEngine::process cost) ----
    std::printf("\n########## item 11: realtime safety + CPU (standalone process() cost) ##########\n");
    for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
        TransientProtectionEngine trans; trans.prepare(testSr, kFft, kHop);
        std::vector<float> magDb((size_t) bins);
        juce::Random rng(42);
        for (auto& v : magDb) v = -60.0f + rng.nextFloat() * 40.0f;

        const int kIters = 2000;
        std::vector<double> timesUs; timesUs.reserve(kIters);
        for (int it = 0; it < kIters; ++it)
        {
            for (auto& v : magDb) v += (rng.nextFloat() - 0.5f) * 2.0f; // small frame-to-frame variation
            auto t0 = std::chrono::high_resolution_clock::now();
            trans.process(magDb);
            auto t1 = std::chrono::high_resolution_clock::now();
            timesUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(timesUs.begin(), timesUs.end());
        double med = timesUs[(size_t) (kIters * 0.50)], p95 = timesUs[(size_t) (kIters * 0.95)], p99 = timesUs[(size_t) (kIters * 0.99)];
        double hopBudgetUs = 1.0e6 * kHop / testSr;
        std::printf("  sr=%.0f: med=%.2fus(%.3f%%) P95=%.2fus(%.3f%%) P99=%.2fus(%.3f%%)  [hop budget=%.1fus]\n",
            testSr, med, 100.0*med/hopBudgetUs, p95, 100.0*p95/hopBudgetUs, p99, 100.0*p99/hopBudgetUs, hopBudgetUs);
    }

    return 0;
}
