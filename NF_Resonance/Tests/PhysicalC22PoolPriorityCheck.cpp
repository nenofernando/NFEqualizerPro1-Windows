// PHYSICAL C2.2: region pool priority (deterministic top-K, not first-come)
// and floor sweep. Diagnostic only -- does not touch ConfidenceEngine.cpp.
// Policy A (aux/main harmonic blending) is untouched; this investigation is
// scoped entirely to candidate admission / pool eviction, kept separate per
// item 11 of the C2 spec.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <array>
#include <string>

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
static std::vector<float> genWhiteNoise(int n, float amp) { std::vector<float> b((size_t) n); juce::Random rng(31); for (auto& s : b) s = (rng.nextFloat() * 2.0f - 1.0f) * amp; return b; }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{
    auto b = genSilence(n);
    for (int h = 1; h <= numH; ++h) addTone(b, sr, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)));
    return b;
}
static std::vector<float> genGuitar(double sr, int n)
{
    auto b = genSilence(n);
    for (int h = 1; h <= 10; ++h)
    {
        double ph = 0.0, inc = juce::MathConstants<double>::twoPi * (196.0 * h) / sr;
        float amp0 = 0.3f * (float) juce::Decibels::decibelsToGain(-2.5f * (h - 1));
        for (int i = 0; i < n; ++i) { double t = i / sr; float env = (float) std::exp(-t * 1.2); b[(size_t) i] += (float) std::sin(ph) * amp0 * env; ph += inc; }
    }
    return b;
}
static std::vector<float> genVocal(double sr, int n) { return genHarmonicSeries(sr, n, 140.0, 0.28f, 10, 2.5f); }
static std::vector<float> genDenseMix(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 62.0, 0.3f, 8, 2.5f);
    auto gtr = genGuitar(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += gtr[(size_t) i] * 0.5f;
    auto voc = genVocal(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += voc[(size_t) i] * 0.5f;
    juce::Random rng(11); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
    return b;
}

static void capturePromHistory(const std::vector<float>& sig, double sr, int fftSize, int hop, std::vector<std::vector<float>>& out)
{
    SpectralProminenceEngineV5 prom; prom.prepare(fftSize / 2 + 1, sr, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
    std::vector<float> scratch((size_t) fftSize * 2), magDb((size_t) (fftSize / 2 + 1)), promOut((size_t) (fftSize / 2 + 1));
    int n = (int) sig.size();
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
        out.push_back(promOut);
    }
}

// ---------------- experimental pool: parametric floors + priority policy ----------------
struct ExpCandidate { bool active = false; float centerHz = 0; float persistence = 0; float lastEvidence = 0; bool matchedThisFrame = false; };
struct ExpPool
{
    static constexpr int kMax = 32;
    std::array<ExpCandidate, kMax> pool;
    float lowFloorDb = 0.5f, strongFloorDb = 2.0f, continuationFloorDb = 1.0f;
    float riseTau = 3.0f, fallTau = 8.0f;
    int priorityPolicy = 1; // 0 = P1 (evidence priority, small continuity bonus), 1 = P2 (persistence*evidence)

    static float smoothstep(float lo, float hi, float x) { float t = juce::jlimit(0.0f, 1.0f, (x - lo) / juce::jmax(1.0e-6f, hi - lo)); return t * t * (3.0f - 2.0f * t); }
    void reset() { pool.fill(ExpCandidate{}); }
    float priorityScore(const ExpCandidate& c) const
    {
        if (priorityPolicy == 0) return 0.85f * c.lastEvidence + (c.matchedThisFrame ? 0.15f : 0.0f); // P1
        return c.persistence * c.lastEvidence; // P2
    }

    // Returns: number of new admissions, number of evictions, and (if
    // outEvictedIdx set) which slot was evicted this frame for a specific
    // caller-tracked scenario.
    struct FrameStats { int admissions = 0, evictions = 0; int evictedSlot = -1; float evictedOldScore = -1, newCandidateScore = -1; };
    FrameStats processFrame(const std::vector<float>& prom, double sr, int fftSize)
    {
        FrameStats stats;
        int n = (int) prom.size();
        struct Peak { int bin; float db; };
        std::array<Peak, kMax> peaks; int numPeaks = 0;
        for (int b = 1; b < n - 1 && numPeaks < kMax; ++b)
            if (prom[(size_t) b] > prom[(size_t) (b - 1)] && prom[(size_t) b] >= prom[(size_t) (b + 1)] && prom[(size_t) b] > lowFloorDb)
                { peaks[(size_t) numPeaks].bin = b; peaks[(size_t) numPeaks].db = prom[(size_t) b]; ++numPeaks; }

        for (auto& c : pool) c.matchedThisFrame = false;
        std::array<bool, kMax> peakMatched{}; peakMatched.fill(false);
        for (int p = 0; p < kMax; ++p)
        {
            if (! pool[(size_t) p].active) continue;
            float logHz = std::log2(juce::jmax(1.0f, pool[(size_t) p].centerHz));
            int best = -1; float bestDist = 1.0e9f;
            for (int k = 0; k < numPeaks; ++k)
            {
                if (peakMatched[(size_t) k]) continue;
                float hz = (float) (peaks[(size_t) k].bin * sr / fftSize);
                float d = std::abs(std::log2(juce::jmax(1.0f, hz)) - logHz);
                if (d < bestDist) { bestDist = d; best = k; }
            }
            if (best >= 0 && bestDist <= 0.36f)
            {
                peakMatched[(size_t) best] = true; pool[(size_t) p].matchedThisFrame = true;
                float db = peaks[(size_t) best].db;
                float evidence = smoothstep(lowFloorDb, strongFloorDb, db);
                bool sustaining = db >= continuationFloorDb;
                float target = sustaining ? 1.0f : 0.0f;
                float coeff = std::exp(-1.0f / (sustaining ? riseTau : fallTau));
                pool[(size_t) p].persistence = target + (pool[(size_t) p].persistence - target) * coeff;
                pool[(size_t) p].centerHz = (float) (peaks[(size_t) best].bin * sr / fftSize);
                pool[(size_t) p].lastEvidence = evidence;
            }
            else
            {
                float coeff = std::exp(-1.0f / fallTau);
                pool[(size_t) p].persistence *= coeff;
                if (pool[(size_t) p].persistence < 0.02f) pool[(size_t) p] = ExpCandidate{};
            }
        }
        for (int k = 0; k < numPeaks; ++k)
        {
            if (peakMatched[(size_t) k]) continue;
            float hz = (float) (peaks[(size_t) k].bin * sr / fftSize);
            float evidence = smoothstep(lowFloorDb, strongFloorDb, peaks[(size_t) k].db);
            int freeSlot = -1;
            for (int p = 0; p < kMax; ++p) if (! pool[(size_t) p].active) { freeSlot = p; break; }
            if (freeSlot < 0)
            {
                int weakest = -1; float weakestScore = 1.0e9f;
                for (int p = 0; p < kMax; ++p) { float score = priorityScore(pool[(size_t) p]); if (score < weakestScore) { weakestScore = score; weakest = p; } }
                float newScore = priorityPolicy == 0 ? evidence : evidence; // a brand-new candidate has persistence=0, so P2's newScore for the CANDIDATE itself is 0 unless we give new seeds an evidence-based admission chance (see below) -- comparison uses evidence directly as the "would-be" score for a fresh candidate under either policy at frame 1
                if (weakest >= 0 && newScore > weakestScore)
                {
                    stats.evictions++; stats.evictedSlot = weakest; stats.evictedOldScore = weakestScore; stats.newCandidateScore = newScore;
                    freeSlot = weakest;
                }
            }
            if (freeSlot >= 0)
            {
                pool[(size_t) freeSlot] = ExpCandidate{}; pool[(size_t) freeSlot].active = true;
                pool[(size_t) freeSlot].centerHz = hz; pool[(size_t) freeSlot].persistence = 0.0f; pool[(size_t) freeSlot].lastEvidence = evidence;
                pool[(size_t) freeSlot].matchedThisFrame = true;
                stats.admissions++;
            }
        }
        return stats;
    }
    int activeCount() const { int c = 0; for (auto& p : pool) if (p.active) ++c; return c; }
    int trackedCount(float thresh = 0.3f) const { int c = 0; for (auto& p : pool) if (p.active && p.persistence >= thresh) ++c; return c; }
    bool isTrackedNear(float hz, float thresh = 0.3f) const { for (auto& p : pool) if (p.active && p.persistence >= thresh && std::abs(std::log2(juce::jmax(1.0f, p.centerHz)) - std::log2(juce::jmax(1.0f, hz))) < 0.25f) return true; return false; }
};

int main()
{
    const double sr = 48000.0; const int fftSize = 2048, hop = 512;

    // ---------------- 2/1. P1 vs P2 priority comparison, using a "new strong resonance vs old weak noise" scenario ----------------
    std::printf("=== 1/2. PRIORITY POLICY COMPARISON: P1 (evidence) vs P2 (persistence*evidence) ===\n");
    std::printf("  scenario: pool pre-filled with weak/old noise-driven candidates, then a NEW strong resonance (+6dB) appears\n");
    for (int policy = 0; policy <= 1; ++policy)
    {
        int nPre = (int) (sr * 1.0);
        auto noise = genWhiteNoise(nPre, 0.2f);
        int nBurst = (int) (sr * 0.5);
        auto sig = genSilence(nPre + nBurst);
        for (int i = 0; i < nPre; ++i) sig[(size_t) i] = noise[(size_t) i];
        std::vector<float> burstOnly = genSilence(nBurst);
        addBurst(burstOnly, sr, 300.0, (float) juce::Decibels::decibelsToGain(6.0f) * 0.4f, 8.0, 7);
        for (int i = 0; i < nBurst; ++i) sig[(size_t) (nPre + i)] += burstOnly[(size_t) i];

        std::vector<std::vector<float>> promHist;
        capturePromHistory(sig, sr, fftSize, hop, promHist);
        int preFrames = nPre / hop;

        ExpPool pool; pool.reset(); pool.priorityPolicy = policy;
        int framesToAdmit300 = -1; int evictionsAfterBurst = 0; int occupancyJustBeforeBurst = -1;
        for (size_t f = 0; f < promHist.size(); ++f)
        {
            auto stats = pool.processFrame(promHist[f], sr, fftSize);
            if ((int) f == preFrames - 1) occupancyJustBeforeBurst = pool.activeCount();
            if ((int) f >= preFrames && stats.evictions > 0) ++evictionsAfterBurst;
            if ((int) f >= preFrames && framesToAdmit300 < 0 && pool.isTrackedNear(300.0f, 0.05f)) framesToAdmit300 = (int) f - preFrames;
        }
        std::printf("  Policy %s: pool occupancy just before burst=%d/32 | frames-after-burst-until-300Hz-tracked=%s | evictions triggered post-burst=%d\n",
            policy == 0 ? "P1" : "P2", occupancyJustBeforeBurst, framesToAdmit300 >= 0 ? std::to_string(framesToAdmit300).c_str() : "NEVER", evictionsAfterBurst);
    }

    // ---------------- A. FINAL ADVERSARIAL TEST: stationary weak regions (real accumulated persistence) vs a new stronger resonance ----------------
    std::printf("\n=== A. ADVERSARIAL: pool filled with 32 STATIONARY weak tones (real persistence), then a stronger NEW resonance appears ===\n");
    for (int policy = 0; policy <= 1; ++policy)
    {
        std::printf(" -- Policy %s --\n", policy == 0 ? "P1" : "P2");
        for (float newAmpDb : { 3.0f, 6.0f, 9.0f })
        {
            double preSec = 2.0; // long enough for the 32 stationary weak tones to build real persistence toward 1.0
            int nPre = (int) (sr * preSec);
            double burstSec = 0.6;
            int nBurst = (int) (sr * burstSec);
            // keep the 32 stationary tones running through the burst region too (realistic: they don't vanish)
            std::vector<float> full = genSilence(nPre + nBurst);
            for (int k = 0; k < 32; ++k)
            {
                double f = 200.0 + k * 250.0;
                addTone(full, sr, f, (float) juce::Decibels::decibelsToGain(2.5f) * 0.35f);
            }
            double newFreq = 200.0 + 32 * 250.0 + 500.0; // clearly unoccupied frequency, above all 32 stationary tones
            std::vector<float> burstOnly = genSilence(nBurst);
            addBurst(burstOnly, sr, newFreq, (float) juce::Decibels::decibelsToGain(newAmpDb) * 0.4f, 8.0, 17);
            for (int i = 0; i < nBurst; ++i) full[(size_t) (nPre + i)] += burstOnly[(size_t) i];

            std::vector<std::vector<float>> promHist;
            capturePromHistory(full, sr, fftSize, hop, promHist);
            int preFrames = nPre / hop;

            ExpPool pool; pool.reset(); pool.priorityPolicy = policy;
            int occBeforeBurst = -1; int framesToAdmitNew = -1; int evictedSlotIdx = -1;
            float evictedOldPersistence = -1, evictedOldScore = -1, newScore = -1;
            for (size_t f = 0; f < promHist.size(); ++f)
            {
                auto stats = pool.processFrame(promHist[f], sr, fftSize);
                if ((int) f == preFrames - 1) occBeforeBurst = pool.activeCount();
                if ((int) f >= preFrames && framesToAdmitNew < 0 && pool.isTrackedNear((float) newFreq, 0.05f))
                {
                    framesToAdmitNew = (int) f - preFrames;
                    if (stats.evictions > 0) { evictedSlotIdx = stats.evictedSlot; evictedOldScore = stats.evictedOldScore; newScore = stats.newCandidateScore; }
                }
            }
            std::printf("    newAmp=+%.0fdB newFreq=%.0fHz: occBeforeBurst=%d/32 framesToAdmit=%s evictedSlot=%s evictedOldScore=%.3f newCandidateScore=%.3f\n",
                newAmpDb, newFreq, occBeforeBurst, framesToAdmitNew >= 0 ? std::to_string(framesToAdmitNew).c_str() : "NEVER",
                evictedSlotIdx >= 0 ? std::to_string(evictedSlotIdx).c_str() : "n/a(free slot or none)", evictedOldScore, newScore);
            (void) evictedOldPersistence;
        }
    }

    // ---------------- 3. STARVATION TEST ----------------
    std::printf("\n=== 3. STARVATION TEST: pool pre-filled with 32 weak candidates, then known resonances introduced ===\n");
    for (int policy = 0; policy <= 1; ++policy)
    {
        std::printf(" -- Policy %s --\n", policy == 0 ? "P1" : "P2");
        for (float amp : { 3.0f, 6.0f, 9.0f })
            for (double f : { 300.0, 700.0, 1500.0 })
            {
                int nPre = (int) (sr * 1.0);
                auto noise = genWhiteNoise(nPre, 0.2f);
                int nBurst = (int) (sr * 0.6);
                auto sig = genSilence(nPre + nBurst);
                for (int i = 0; i < nPre; ++i) sig[(size_t) i] = noise[(size_t) i];
                std::vector<float> burstOnly = genSilence(nBurst);
                addBurst(burstOnly, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, 8.0, 13);
                for (int i = 0; i < nBurst; ++i) sig[(size_t) (nPre + i)] += burstOnly[(size_t) i];

                std::vector<std::vector<float>> promHist;
                capturePromHistory(sig, sr, fftSize, hop, promHist);
                int preFrames = nPre / hop;
                ExpPool pool; pool.reset(); pool.priorityPolicy = policy;
                int framesToAdmit = -1; bool admitted = false; int occupancyJustBeforeIntro = -1;
                for (size_t fr = 0; fr < promHist.size(); ++fr)
                {
                    pool.processFrame(promHist[fr], sr, fftSize);
                    if ((int) fr == preFrames - 1) occupancyJustBeforeIntro = pool.activeCount();
                    if ((int) fr >= preFrames && ! admitted && pool.isTrackedNear((float) f, 0.05f)) { admitted = true; framesToAdmit = (int) fr - preFrames; }
                }
                std::printf("    f=%5.0fHz amp=+%.0fdB: admitted=%s frames-after-intro=%s poolOccupancyBeforeIntro=%d/32\n",
                    f, amp, admitted ? "YES" : "NO", framesToAdmit >= 0 ? std::to_string(framesToAdmit).c_str() : "n/a", occupancyJustBeforeIntro);
            }
    }

    // ---------------- 7. FLOOR SWEEP ----------------
    std::printf("\n=== 7. FLOOR SWEEP: lowFloor / strongFloor / continuationFloor -- recall + false-candidate + occupancy ===\n");
    struct FloorCfg { float lo, strong, cont; };
    std::vector<FloorCfg> configs = {
        { 0.0f, 1.5f, 0.5f }, { 0.0f, 2.0f, 1.0f }, { 0.5f, 2.0f, 1.0f }, { 0.5f, 2.5f, 1.5f }, { 1.0f, 2.0f, 1.0f }, { 1.0f, 2.5f, 1.5f }
    };
    // recall subset: 5 freqs x 3 amps x medium width
    const double recallFreqs[] = { 120, 250, 500, 1000, 4000 };
    const float recallAmps[] = { 3.0f, 4.0f, 6.0f };
    auto noiseSig = genWhiteNoise((int) (sr * 1.0), 0.15f);
    std::vector<std::vector<float>> noisePromHist; capturePromHistory(noiseSig, sr, fftSize, hop, noisePromHist);
    for (auto& cfg : configs)
    {
        int total = 0, ok = 0;
        for (double f : recallFreqs) for (float amp : recallAmps)
        {
            int n = (int) (sr * 0.5);
            auto sig = genSilence(n);
            addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, 10.0, 5);
            std::vector<std::vector<float>> promHist; capturePromHistory(sig, sr, fftSize, hop, promHist);
            ExpPool pool; pool.reset(); pool.priorityPolicy = 1; pool.lowFloorDb = cfg.lo; pool.strongFloorDb = cfg.strong; pool.continuationFloorDb = cfg.cont;
            bool tracked = false;
            for (auto& pf : promHist) { pool.processFrame(pf, sr, fftSize); if (pool.isTrackedNear((float) f, 0.3f)) tracked = true; }
            ++total; if (tracked) ++ok;
        }
        ExpPool noisePool; noisePool.reset(); noisePool.priorityPolicy = 1; noisePool.lowFloorDb = cfg.lo; noisePool.strongFloorDb = cfg.strong; noisePool.continuationFloorDb = cfg.cont;
        int peakOcc = 0; long sumTracked = 0;
        for (auto& pf : noisePromHist) { noisePool.processFrame(pf, sr, fftSize); peakOcc = juce::jmax(peakOcc, noisePool.activeCount()); sumTracked += noisePool.trackedCount(); }
        std::printf("  lo=%.1f strong=%.1f cont=%.1f : recall=%3d%% (%d/%d) | noise: peakOcc=%2d meanTracked=%.2f\n",
            cfg.lo, cfg.strong, cfg.cont, total ? 100 * ok / total : 0, ok, total, peakOcc, (double) sumTracked / juce::jmax(1, (int) noisePromHist.size()));
    }

    // ---------------- 8. CONTROLS WITH FULL POOL (churn) ----------------
    std::printf("\n=== 8. CONTROLS: candidate/tracked/evictions/admissions per frame (Policy P2, current-matching floors) ===\n");
    {
        int n = (int) (sr * 1.2);
        struct Ctl { const char* name; std::vector<float> sig; };
        std::vector<Ctl> controls = {
            { "White noise", genWhiteNoise(n, 0.15f) }, { "Dense mix", genDenseMix(sr, n) },
            { "Guitar", genGuitar(sr, n) }, { "Vocal", genVocal(sr, n) }, { "Bass", genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f) }
        };
        for (auto& c : controls)
        {
            std::vector<std::vector<float>> promHist; capturePromHistory(c.sig, sr, fftSize, hop, promHist);
            ExpPool pool; pool.reset(); pool.priorityPolicy = 1;
            int peakOcc = 0; long sumTracked = 0, sumAdm = 0, sumEvict = 0;
            for (auto& pf : promHist) { auto st = pool.processFrame(pf, sr, fftSize); peakOcc = juce::jmax(peakOcc, pool.activeCount()); sumTracked += pool.trackedCount(); sumAdm += st.admissions; sumEvict += st.evictions; }
            int nf = (int) promHist.size();
            std::printf("  %-11s peakOcc=%2d meanTracked=%.2f admissions/frame=%.2f evictions/frame=%.2f (churn=%.1f%%)\n",
                c.name, peakOcc, (double) sumTracked / juce::jmax(1, nf), (double) sumAdm / juce::jmax(1, nf), (double) sumEvict / juce::jmax(1, nf),
                100.0 * sumEvict / juce::jmax(1L, sumAdm + sumEvict));
        }
    }

    std::printf("\nC2.2 report complete.\n");
    return 0;
}
