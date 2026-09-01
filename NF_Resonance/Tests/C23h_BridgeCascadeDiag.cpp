// C2.3h targeted diagnostic: does the region continuation bridge alter
// ONLY the region being continued, or does it leak into other regions'
// state / matching / top-K via shared pool mechanics? Runs the EXACT same
// 80Hz+113.14Hz@48kHz signal from ConfidenceLayerCheck twice (bridge ON vs
// OFF), snapshotting the FULL region pool every frame, and reports the
// first frame+slot where the two runs diverge.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include "DSP/ConfidenceEngine.h"
#include <vector>
#include <array>
#include <cstdio>
#include <cmath>

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

struct SlotSnap
{
    bool active = false;
    float centerHz = 0, stability = 0, persistence = 0, candidateEvidence = 0, harmonicLikelihood = 0, confidence = 0;
    bool bridged = false;
    int framesAbsent = 0, framesPresent = 0;
    int generation = 0; // increments each time this slot transitions inactive->active (crude "region identity" proxy)
};

using FrameSnap = std::array<SlotSnap, ConfidenceEngine::kMaxRegions>;

static std::vector<FrameSnap> runAndCapture(bool bridgeOn, double sr, const std::vector<float>& sig)
{
    const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
    SpectralProminenceEngineV5 prom; prom.prepare(bins, sr, kFft);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    ConfidenceEngine conf; conf.prepare(sr, kFft, kHop); conf.setPersistenceTimeConstants(3.0f, 8.0f);
    if (! bridgeOn) conf.setContinuationBridgeTimeMs(0.0f);
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
    std::vector<float> scratch((size_t) kFft * 2), magDb((size_t) bins), promOut((size_t) bins);

    std::array<int, ConfidenceEngine::kMaxRegions> generation{}; generation.fill(0);
    std::array<bool, ConfidenceEngine::kMaxRegions> wasActive{}; wasActive.fill(false);

    std::vector<FrameSnap> out;
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
        conf.process(promOut); // no aux, matches ConfidenceLayerCheck's runPipeline exactly

        FrameSnap snap{};
        int idx = 0;
        for (auto& r : conf.regions())
        {
            if (r.active && ! wasActive[(size_t) idx]) generation[(size_t) idx]++;
            wasActive[(size_t) idx] = r.active;
            snap[(size_t) idx] = { r.active, r.centerHz, r.stability, r.persistence, r.candidateEvidence, r.harmonicLikelihood, r.confidence,
                                    r.lastBridged, r.framesAbsent, r.framesPresent, generation[(size_t) idx] };
            ++idx;
        }
        out.push_back(snap);
    }
    return out;
}

int main()
{
    const double sr = 48000.0;
    auto bass80 = genHarmonicSeries(sr, (int) (sr * 2.0), 80.0, 0.4f, 8);
    auto sig = bass80; addBurst(sig, sr, 113.14, 0.35f, 10.0, 300);

    auto on = runAndCapture(true, sr, sig);
    auto off = runAndCapture(false, sr, sig);

    std::printf("=== C2.3h cascade diagnostic: 80Hz+113.14Hz@48kHz, bridge ON vs OFF ===\n");
    std::printf("frames captured: ON=%zu OFF=%zu\n", on.size(), off.size());

    const float eps = 1.0e-4f;
    int firstDivFrame = -1, firstDivSlot = -1;
    for (size_t f = 0; f < on.size() && f < off.size() && firstDivFrame < 0; ++f)
    {
        for (int s = 0; s < ConfidenceEngine::kMaxRegions; ++s)
        {
            auto& a = on[f][(size_t) s]; auto& b = off[f][(size_t) s];
            bool differ = (a.active != b.active)
                       || (a.active && (std::abs(a.centerHz - b.centerHz) > 0.05f
                                      || std::abs(a.stability - b.stability) > eps
                                      || std::abs(a.persistence - b.persistence) > eps
                                      || std::abs(a.candidateEvidence - b.candidateEvidence) > eps
                                      || std::abs(a.confidence - b.confidence) > eps
                                      || a.generation != b.generation));
            if (differ) { firstDivFrame = (int) f; firstDivSlot = s; break; }
        }
    }

    if (firstDivFrame < 0) { std::printf("NO DIVERGENCE FOUND -- ON and OFF traces are identical throughout.\n"); return 0; }

    std::printf("\nFIRST DIVERGENCE: frame=%d slot=%d\n", firstDivFrame, firstDivSlot);
    int lo = juce::jmax(0, firstDivFrame - 3), hi = juce::jmin((int) on.size() - 1, firstDivFrame + 5);
    std::printf("\n-- context, slot %d, frames %d..%d --\n", firstDivSlot, lo, hi);
    std::printf("%-6s | %-45s | %-45s\n", "frame", "ON", "OFF");
    for (int f = lo; f <= hi; ++f)
    {
        auto& a = on[(size_t) f][(size_t) firstDivSlot]; auto& b = off[(size_t) f][(size_t) firstDivSlot];
        char bufA[128], bufB[128];
        if (a.active) std::snprintf(bufA, sizeof(bufA), "gen=%d hz=%.1f stab=%.3f pers=%.3f cE=%.3f conf=%.3f br=%d fA=%d fP=%d", a.generation, a.centerHz, a.stability, a.persistence, a.candidateEvidence, a.confidence, (int) a.bridged, a.framesAbsent, a.framesPresent);
        else std::snprintf(bufA, sizeof(bufA), "inactive");
        if (b.active) std::snprintf(bufB, sizeof(bufB), "gen=%d hz=%.1f stab=%.3f pers=%.3f cE=%.3f conf=%.3f br=%d fA=%d fP=%d", b.generation, b.centerHz, b.stability, b.persistence, b.candidateEvidence, b.confidence, (int) b.bridged, b.framesAbsent, b.framesPresent);
        else std::snprintf(bufB, sizeof(bufB), "inactive");
        std::printf("%-6d | %-45s | %-45s\n", f, bufA, bufB);
    }

    // Also: did ANY OTHER slot bridge on/around firstDivFrame in the ON run?
    // (helps distinguish "this slot's own bridge caused it" vs "another
    // slot's bridge perturbed shared state, e.g. f0 selection, that this
    // slot's harmonicLikelihood/penalty then reacted to.)
    std::printf("\n-- all bridged slots in ON run, frames %d..%d --\n", juce::jmax(0, firstDivFrame - 5), firstDivFrame);
    for (int f = juce::jmax(0, firstDivFrame - 5); f <= firstDivFrame; ++f)
        for (int s = 0; s < ConfidenceEngine::kMaxRegions; ++s)
            if (on[(size_t) f][(size_t) s].active && on[(size_t) f][(size_t) s].bridged)
                std::printf("  frame %d: slot %d bridged (hz=%.1f)\n", f, s, on[(size_t) f][(size_t) s].centerHz);

    // Count total active-slot count per frame around divergence -- did pool
    // occupancy/eviction pressure differ?
    std::printf("\n-- active region count, ON vs OFF, frames %d..%d --\n", lo, hi);
    for (int f = lo; f <= hi; ++f)
    {
        int cA = 0, cB = 0;
        for (int s = 0; s < ConfidenceEngine::kMaxRegions; ++s) { if (on[(size_t) f][(size_t) s].active) ++cA; if (off[(size_t) f][(size_t) s].active) ++cB; }
        std::printf("  frame %d: ON activeCount=%d  OFF activeCount=%d\n", f, cA, cB);
    }

    return 0;
}
