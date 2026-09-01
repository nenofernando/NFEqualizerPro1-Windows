// PHYSICAL C2.1: candidate admission / peak floor investigation. Diagnostic
// only -- does not modify ConfidenceEngine.cpp (Policy A stays exactly as
// committed). Runs the REAL production pipeline for "current hard floor"
// numbers, and a separate, standalone, offline reimplementation (this file
// only) of a two-stage soft-admission + hysteresis scheme for comparison.
// No gain reduction, no UI, no STFT/OLA/C2 change.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/ConfidenceEngine.h"
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
static std::vector<float> genKick(double sr, int n)
{
    auto b = genSilence(n);
    for (int i = 0; i < n; ++i) { double t = i / sr; double f = 120.0 * std::exp(-t * 18.0) + 45.0; float env = (float) std::exp(-t * 7.0); b[(size_t) i] = (float) std::sin(juce::MathConstants<double>::twoPi * f * t) * env * 0.9f; }
    return b;
}
static std::vector<float> genVocal(double sr, int n) { return genHarmonicSeries(sr, n, 140.0, 0.28f, 10, 2.5f); }
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
static std::vector<float> genDenseMix(double sr, int n)
{
    auto b = genHarmonicSeries(sr, n, 62.0, 0.3f, 8, 2.5f);
    auto kick = genKick(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += kick[(size_t) i] * 0.7f;
    auto voc = genVocal(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += voc[(size_t) i] * 0.6f;
    auto gtr = genGuitar(sr, n); for (int i = 0; i < n; ++i) b[(size_t) i] += gtr[(size_t) i] * 0.5f;
    juce::Random rng(11); for (auto& s : b) s += (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
    return b;
}
static std::vector<float> genWhiteNoise(int n, float amp) { std::vector<float> b((size_t) n); juce::Random rng(21); for (auto& s : b) s = (rng.nextFloat() * 2.0f - 1.0f) * amp; return b; }
static std::vector<float> genPinkNoise(int n, float amp)
{
    std::vector<float> b((size_t) n); juce::Random rng(22);
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
    for (int i = 0; i < n; ++i)
    {
        float white = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99886f*b0 + white*0.0555179f; b1 = 0.99332f*b1 + white*0.0750759f;
        b2 = 0.96900f*b2 + white*0.1538520f; b3 = 0.86650f*b3 + white*0.3104856f;
        b4 = 0.55000f*b4 + white*0.5329522f; b5 = -0.7616f*b5 - white*0.0168980f;
        float pink = b0+b1+b2+b3+b4+b5+b6+white*0.5362f; b6 = white*0.115926f;
        b[(size_t) i] = pink * amp * 0.11f;
    }
    return b;
}

// ---------------- prominence-only capture (bypasses ConfidenceEngine entirely) ----------------
struct PromFrame { std::vector<float> prom; };
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

// ---------------- "current" production admission, run for real via ConfidenceEngine ----------------
struct CurrentResult { bool tracked = false; int framesToAcquire = -1; float peakProminence = -999.0f; float confidenceIfTracked = 0.0f; };
static CurrentResult runCurrentAdmission(const std::vector<std::vector<float>>& promHist, double sr, int fftSize, int hop, float targetHz)
{
    ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);
    CurrentResult r;
    for (size_t f = 0; f < promHist.size(); ++f)
    {
        conf.process(promHist[f], nullptr);
        int b = (int) std::round(targetHz * fftSize / sr);
        if (b >= 0 && b < (int) promHist[f].size()) r.peakProminence = juce::jmax(r.peakProminence, promHist[f][(size_t) b]);
        if (! r.tracked)
        {
            for (auto& reg : conf.regions())
                if (reg.active && std::abs(std::log2(juce::jmax(1.0f, reg.centerHz)) - std::log2(juce::jmax(1.0f, targetHz))) < 0.25f)
                { r.tracked = true; r.framesToAcquire = (int) f; r.confidenceIfTracked = reg.confidence; break; }
        }
        else
        {
            for (auto& reg : conf.regions())
                if (reg.active && std::abs(std::log2(juce::jmax(1.0f, reg.centerHz)) - std::log2(juce::jmax(1.0f, targetHz))) < 0.25f)
                { r.confidenceIfTracked = reg.confidence; break; }
        }
    }
    return r;
}
static void runCurrentControls(const std::vector<std::vector<float>>& promHist, double sr, int fftSize, int hop, int& peakOccupancy, double& meanActive)
{
    ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);
    peakOccupancy = 0; long sum = 0;
    for (auto& pf : promHist) { conf.process(pf, nullptr); int n = conf.activeRegionCount(); peakOccupancy = juce::jmax(peakOccupancy, n); sum += n; }
    meanActive = promHist.empty() ? 0.0 : (double) sum / (double) promHist.size();
}

// ---------------- EXPERIMENTAL: two-stage soft admission + hysteresis + priority eviction ----------------
// Entirely standalone -- reimplemented here for comparison only, never
// touches ConfidenceEngine.cpp. Fixed-size pool (32), zero heap allocation.
struct ExpCandidate { bool active = false; float centerHz = 0; float persistence = 0; float lastEvidence = 0; };
struct ExpPool
{
    static constexpr int kMax = 32;
    std::array<ExpCandidate, kMax> pool;
    float lowFloorDb = 0.5f;      // Stage 1: any local max above this can SEED a weak candidate
    float strongFloorDb = 2.0f;   // Stage 1: prominence at/above this -> full seed evidence (matches current hard floor's value, for comparability)
    float continuationFloorDb = 1.0f; // hysteresis: an EXISTING tracked region can survive a frame with prominence as low as this (below strongFloor, above lowFloor)
    float riseTau = 3.0f, fallTau = 8.0f;
    float trackedPersistenceThreshold = 0.3f; // "tracked" = Stage 2 has accumulated enough persistence

    static float smoothstep(float lo, float hi, float x) { float t = juce::jlimit(0.0f, 1.0f, (x - lo) / juce::jmax(1.0e-6f, hi - lo)); return t * t * (3.0f - 2.0f * t); }

    void reset() { pool.fill(ExpCandidate{}); }

    // Returns per-frame: number of NEW seeds admitted, number evicted, and whether targetHz became tracked this frame.
    void processFrame(const std::vector<float>& prom, double sr, int fftSize, int candidateFloorDbForSeed)
    {
        (void) candidateFloorDbForSeed;
        int n = (int) prom.size();
        struct Peak { int bin; float db; };
        std::array<Peak, kMax> peaks; int numPeaks = 0;
        for (int b = 1; b < n - 1 && numPeaks < kMax; ++b)
        {
            if (prom[(size_t) b] > prom[(size_t) (b - 1)] && prom[(size_t) b] >= prom[(size_t) (b + 1)] && prom[(size_t) b] > lowFloorDb)
                { peaks[(size_t) numPeaks].bin = b; peaks[(size_t) numPeaks].db = prom[(size_t) b]; ++numPeaks; }
        }
        std::array<bool, kMax> peakMatched{}; peakMatched.fill(false);
        std::array<bool, kMax> poolMatched{}; poolMatched.fill(false);
        // match existing pool entries to nearest peak within tolerance
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
                peakMatched[(size_t) best] = true; poolMatched[(size_t) p] = true;
                float db = peaks[(size_t) best].db;
                float evidence = smoothstep(lowFloorDb, strongFloorDb, db);
                // hysteresis: continuing region only needs to clear continuationFloorDb, not strongFloorDb, to keep RISING persistence
                bool sustaining = db >= continuationFloorDb;
                float target = sustaining ? 1.0f : 0.0f;
                float tau = sustaining ? riseTau : fallTau;
                float coeff = std::exp(-1.0f / tau);
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
        // unmatched peaks -> new seeds (Stage 1, permissive: any local max above lowFloorDb seeds SOMETHING, evidence scales continuously)
        for (int k = 0; k < numPeaks; ++k)
        {
            if (peakMatched[(size_t) k]) continue;
            float hz = (float) (peaks[(size_t) k].bin * sr / fftSize);
            float evidence = smoothstep(lowFloorDb, strongFloorDb, peaks[(size_t) k].db);
            int freeSlot = -1;
            for (int p = 0; p < kMax; ++p) if (! pool[(size_t) p].active) { freeSlot = p; break; }
            if (freeSlot < 0)
            {
                // pool full: deterministic priority eviction -- evict the
                // weakest occupant (persistence*lastEvidence) if the new
                // seed's OWN evidence beats it; otherwise the new seed is
                // dropped. No heap allocation, no randomness.
                int weakest = -1; float weakestScore = 1.0e9f;
                for (int p = 0; p < kMax; ++p) { float score = pool[(size_t) p].persistence * pool[(size_t) p].lastEvidence; if (score < weakestScore) { weakestScore = score; weakest = p; } }
                if (weakest >= 0 && evidence > weakestScore) freeSlot = weakest;
            }
            if (freeSlot >= 0) { pool[(size_t) freeSlot] = ExpCandidate{}; pool[(size_t) freeSlot].active = true; pool[(size_t) freeSlot].centerHz = hz; pool[(size_t) freeSlot].persistence = 0.0f; pool[(size_t) freeSlot].lastEvidence = evidence; }
        }
    }
    int activeCount() const { int c = 0; for (auto& p : pool) if (p.active) ++c; return c; }
    int trackedCount() const { int c = 0; for (auto& p : pool) if (p.active && p.persistence >= trackedPersistenceThreshold) ++c; return c; }
    bool isTrackedNear(float hz) const { for (auto& p : pool) if (p.active && p.persistence >= trackedPersistenceThreshold && std::abs(std::log2(juce::jmax(1.0f, p.centerHz)) - std::log2(juce::jmax(1.0f, hz))) < 0.25f) return true; return false; }
};

struct ExpResult { bool tracked = false; int framesToAcquire = -1; };
static ExpResult runExperimentalAdmission(const std::vector<std::vector<float>>& promHist, double sr, int fftSize, float targetHz)
{
    ExpPool pool; pool.reset();
    ExpResult r;
    for (size_t f = 0; f < promHist.size(); ++f)
    {
        pool.processFrame(promHist[f], sr, fftSize, 0);
        if (! r.tracked && pool.isTrackedNear(targetHz)) { r.tracked = true; r.framesToAcquire = (int) f; }
    }
    return r;
}
static void runExperimentalControls(const std::vector<std::vector<float>>& promHist, double sr, int fftSize, int& peakOccupancy, double& meanTracked)
{
    ExpPool pool; pool.reset();
    peakOccupancy = 0; long sum = 0;
    for (auto& pf : promHist) { pool.processFrame(pf, sr, fftSize, 0); peakOccupancy = juce::jmax(peakOccupancy, pool.activeCount()); sum += pool.trackedCount(); }
    meanTracked = promHist.empty() ? 0.0 : (double) sum / (double) promHist.size();
}

int main()
{
    const double freqs[] = { 80, 120, 170, 250, 500, 1000, 2000, 4000, 8000 };
    const float amps[] = { 2.0f, 3.0f, 4.0f, 6.0f, 9.0f, 12.0f };
    const double qs[] = { 20.0, 10.0, 5.0 }; // narrow/medium/broad
    const char* qNames[] = { "narrow", "medium", "broad" };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int fftSize = 2048, hop = 512;

    std::printf("=== 1. AUDIT: see report text -- path confirmed by direct code read, not re-derived here ===\n\n");

    std::printf("=== 3. DETECTION SWEEP: freq x amp x width x SR, current(hard floor) vs experimental(soft+hysteresis) ===\n");
    struct Row { double freq, amp, sr; const char* q; bool curTracked, expTracked; int curFrames, expFrames; float promPeak; bool resolutionLimited; };
    std::vector<Row> rows;
    for (double sr : rates)
        for (double f : freqs)
            for (int qi = 0; qi < 3; ++qi)
                for (float amp : amps)
                {
                    int n = (int) (sr * 0.5);
                    auto sig = genSilence(n);
                    addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(amp) * 0.4f, qs[(size_t) qi], 5);
                    std::vector<std::vector<float>> promHist;
                    capturePromHistory(sig, sr, fftSize, hop, promHist);
                    auto cur = runCurrentAdmission(promHist, sr, fftSize, hop, (float) f);
                    auto exp = runExperimentalAdmission(promHist, sr, fftSize, (float) f);
                    double binHz = sr / fftSize;
                    double signalBw = f / qs[(size_t) qi];
                    bool resLimited = signalBw < binHz;
                    rows.push_back({ f, amp, sr, qNames[qi], cur.tracked, exp.tracked, cur.framesToAcquire, exp.framesToAcquire, cur.peakProminence, resLimited });
                    if (amp == 2.0f && std::string(qNames[qi]) == "narrow" && ! cur.tracked && exp.tracked)
                        std::printf("    [DIVERGENCE] f=%.0fHz sr=%.0fHz amp=+2dB narrow: current=NOT-tracked experimental=tracked peakProm=%.2fdB resLimited=%s\n",
                            f, sr, cur.peakProminence, resLimited ? "yes" : "no");
                }

    std::printf("  -- recall table: amplitude | width | recall(current) | recall(experimental) -- averaged over freq+SR --\n");
    for (float amp : amps)
        for (int qi = 0; qi < 3; ++qi)
        {
            int total = 0, curOk = 0, expOk = 0;
            for (auto& r : rows) if (r.amp == amp && std::string(r.q) == qNames[qi]) { ++total; if (r.curTracked) ++curOk; if (r.expTracked) ++expOk; }
            std::printf("    amp=+%2.0fdB width=%-7s : recall current=%3d%%  recall experimental=%3d%%  (n=%d)\n",
                amp, qNames[qi], total ? 100 * curOk / total : 0, total ? 100 * expOk / total : 0, total);
        }

    std::printf("  -- false candidate check: any RESOLUTION LIMITED cases where even experimental scheme can't help? --\n");
    int resLimitedTotal = 0, resLimitedCurOk = 0, resLimitedExpOk = 0;
    for (auto& r : rows) if (r.resolutionLimited) { ++resLimitedTotal; if (r.curTracked) ++resLimitedCurOk; if (r.expTracked) ++resLimitedExpOk; }
    std::printf("    RESOLUTION LIMITED cases: %d/%d total rows. current recall there=%d%%  experimental recall there=%d%%\n",
        resLimitedTotal, (int) rows.size(), resLimitedTotal ? 100 * resLimitedCurOk / resLimitedTotal : 0, resLimitedTotal ? 100 * resLimitedExpOk / resLimitedTotal : 0);

    std::printf("  -- per-frequency recall (amp=+4dB, width=medium, averaged over SR) -- checks item 8 (freq consistency) --\n");
    for (double f : freqs)
    {
        int total = 0, curOk = 0, expOk = 0, resLim = 0;
        for (auto& r : rows) if (r.freq == f && r.amp == 4.0f && std::string(r.q) == "medium") { ++total; if (r.curTracked) ++curOk; if (r.expTracked) ++expOk; if (r.resolutionLimited) ++resLim; }
        std::printf("    freq=%6.0fHz: recall current=%3d%% recall experimental=%3d%% (n=%d, resolutionLimited=%d)\n", f, total ? 100*curOk/total : 0, total ? 100*expOk/total : 0, total, resLim);
    }

    std::printf("\n=== 4. NO-RESONANCE CONTROLS: candidate/tracked regions per frame, peak occupancy ===\n");
    for (double sr : rates)
    {
        int n = (int) (sr * 1.2);
        struct Ctl { const char* name; std::vector<float> sig; };
        std::vector<Ctl> controls = {
            { "White noise", genWhiteNoise(n, 0.15f) }, { "Pink noise", genPinkNoise(n, 0.3f) },
            { "Bass clean", genHarmonicSeries(sr, n, 62.0, 0.33f, 8, 2.5f) }, { "Kick", genKick(sr, n) },
            { "Vocal", genVocal(sr, n) }, { "Guitar", genGuitar(sr, n) }, { "Dense mix", genDenseMix(sr, n) }
        };
        std::printf(" -- sr=%.0fHz --\n", sr);
        for (auto& c : controls)
        {
            std::vector<std::vector<float>> promHist;
            capturePromHistory(c.sig, sr, fftSize, hop, promHist);
            int curPeak; double curMean; runCurrentControls(promHist, sr, fftSize, hop, curPeak, curMean);
            int expPeak; double expMean; runExperimentalControls(promHist, sr, fftSize, expPeak, expMean);
            std::printf("   %-11s current: peakActive=%2d meanActive=%.2f  | experimental: peakActive=%2d meanTracked=%.2f\n",
                c.name, curPeak, curMean, expPeak, expMean);
        }
    }

    std::printf("\n=== 9. REGION POOL: peak occupancy across ALL runs above (current vs experimental) ===\n");
    std::printf("  (see per-control peakActive columns above -- if experimental peakActive approaches 32 anywhere, note it)\n");

    std::printf("\n=== RECOMMENDATION ===\n");
    std::printf("  See recall table (item 10) and control false-candidate/occupancy numbers above.\n");
    std::printf("  This harness makes NO change to ConfidenceEngine.cpp. Policy A / aux-main blending untouched.\n");
    return 0;
}
