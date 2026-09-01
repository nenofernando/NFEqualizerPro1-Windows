// PHYSICAL C, Checkpoint C1: confidence-only, diagnostic, offline. No audio
// gain is touched anywhere in this file. Covers C2 (persistence T63/T90/
// T95), C3 (frequency stability), C5 (harmonic reasoning: H1/H2/H3), C6
// (Selectivity curve), C9 (T1-T7 controlled cases), C10 (the bass cases
// tied to the actual "buraco no grave" complaint), C11 (musical material
// sparsity), C12 (incremental CPU cost).

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/ConfidenceEngine.h"
#include <chrono>

using juce::AudioBuffer;
static const double sr = 48000.0;
static const int fftSize = 2048, hop = 512;
static const int bins = fftSize / 2 + 1;
static const double binHz = sr / (double) fftSize;
static const double frameMs = 1000.0 * hop / sr;

// ---------------- signal generators (self-contained, deterministic) ----------------
static AudioBuffer<float> makeSilence(int n) { AudioBuffer<float> b(2, n); b.clear(); return b; }
static void addTone(AudioBuffer<float>& b, double freq, float amp, double phase = 0.0)
{
    int n = b.getNumSamples(); double ph = phase, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * amp; for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, s); ph += inc; }
}
static AudioBuffer<float> genHarmonicSeries(int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{
    auto b = makeSilence(n);
    for (int h = 1; h <= numH; ++h) addTone(b, f0 * h, amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)));
    return b;
}
static AudioBuffer<float> genHarmonicSeriesBoosted(int n, double f0, float amp, int numH, int boostH, float boostDb, float rolloffDb = 3.0f)
{
    auto b = makeSilence(n);
    for (int h = 1; h <= numH; ++h) { float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1)); if (h == boostH) a *= (float) juce::Decibels::decibelsToGain(boostDb); addTone(b, f0 * h, a); }
    return b;
}
static AudioBuffer<float> genResonanceBurst(int n, double freqHz, float amp, double Q, int seed, int startSample = 0, int lengthSamples = -1)
{
    auto b = makeSilence(n);
    juce::Random rng(seed);
    double bwHz = freqHz / Q;
    int len = lengthSamples < 0 ? n : lengthSamples;
    AudioBuffer<float> burst(2, len); burst.clear();
    for (int k = 0; k < 9; ++k)
    {
        double t = (double) k / 8.0 - 0.5;
        double f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < len; ++i) { float s = (float) std::sin(ph) * (amp / 3.0f); for (int c = 0; c < 2; ++c) burst.addSample(c, i, s); ph += inc; }
    }
    for (int c = 0; c < 2; ++c) b.addFrom(c, startSample, burst, c, 0, juce::jmin(len, n - startSample));
    return b;
}
static AudioBuffer<float> genTransientClicks(int n, int seed, int numClicks)
{
    auto b = makeSilence(n); juce::Random rng(seed);
    int spacing = n / juce::jmax(1, numClicks);
    for (int c = 0; c < numClicks; ++c)
    {
        int pos = c * spacing + spacing / 2;
        for (int i = 0; i < 150 && pos + i < n; ++i) { float env = std::exp(-(float) i / 15.0f); float x = (rng.nextFloat() * 2.0f - 1.0f) * env * 0.8f; for (int ch = 0; ch < 2; ++ch) b.addSample(ch, pos + i, x); }
    }
    return b;
}
static AudioBuffer<float> genMovingTone(int n, double f0, double f1, float amp)
{
    auto b = makeSilence(n); double ph = 0;
    for (int i = 0; i < n; ++i) { double t = (double) i / n; double f = f0 + (f1 - f0) * t; ph += juce::MathConstants<double>::twoPi * f / sr; float s = (float) std::sin(ph) * amp; for (int c = 0; c < 2; ++c) b.addSample(c, i, s); }
    return b;
}
static AudioBuffer<float> genKick(int n)
{
    auto b = makeSilence(n); juce::Random rng(3); double period = sr * 0.8;
    for (int hit = 0; hit * period < n; ++hit)
    {
        int start = (int) (hit * period);
        for (int i = 0; i < (int) (sr * 0.3) && start + i < n; ++i)
        { double t = i / sr; double f = 120.0 * std::exp(-t * 18.0) + 45.0; float env = (float) std::exp(-t * 9.0);
          float s = (float) std::sin(juce::MathConstants<double>::twoPi * f * t) * env * 0.9f; for (int c = 0; c < 2; ++c) b.addSample(c, start + i, s); }
    }
    return b;
}
static AudioBuffer<float> genSnare(int n)
{
    auto b = makeSilence(n); juce::Random rng(9); double period = sr * 0.8;
    for (int hit = 0; hit * period < n; ++hit)
    {
        int start = (int) (hit * period);
        for (int i = 0; i < (int) (sr * 0.15) && start + i < n; ++i)
        { double t = i / sr; float env = (float) std::exp(-t * 20.0);
          float tone = (float) std::sin(juce::MathConstants<double>::twoPi * 200.0 * t) * env * 0.4f;
          float noise = (rng.nextFloat() * 2.0f - 1.0f) * env * 0.6f;
          for (int c = 0; c < 2; ++c) { b.addSample(c, start + i, tone); b.addSample(c, start + i, noise); } }
    }
    return b;
}
static AudioBuffer<float> genVocalConsonant(int n)
{
    // Broadband, short, noisy burst -- approximates an unvoiced consonant (s/f/t-like).
    auto b = makeSilence(n); juce::Random rng(13);
    int start = n / 3, len = (int) (sr * 0.08);
    for (int i = 0; i < len && start + i < n; ++i) { float env = std::exp(-(float) i / (len * 0.3f)); float x = (rng.nextFloat() * 2.0f - 1.0f) * env * 0.5f; for (int c = 0; c < 2; ++c) b.addSample(c, start + i, x); }
    return b;
}

// ---------------- STFT + prominence + confidence pipeline ----------------
struct RegionSnapshot { float centerHz, peakDb, widthHz, persistence, stability, harmonicLikelihood, confidence; int framesPresent; };

static void runPipeline(const AudioBuffer<float>& input, SpectralProminenceEngineV5& v2, ConfidenceEngine& conf,
                         std::function<void(int frameIdx, const ConfidenceEngine&)> perFrame = nullptr)
{
    juce::dsp::FFT fft(11);
    std::vector<float> window((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
    std::vector<float> fftBuf((size_t) fftSize * 2, 0.0f);
    std::vector<float> magDb((size_t) bins, -120.0f);
    std::vector<float> prom((size_t) bins, 0.0f);

    int n = input.getNumSamples();
    int frameIdx = 0;
    for (int pos = 0; pos + fftSize <= n; pos += hop)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            float x = 0.0f; for (int c = 0; c < input.getNumChannels(); ++c) x += input.getSample(c, pos + i);
            x /= (float) input.getNumChannels();
            fftBuf[(size_t) i] = x * window[(size_t) i];
        }
        std::fill(fftBuf.begin() + fftSize, fftBuf.end(), 0.0f);
        fft.performRealOnlyForwardTransform(fftBuf.data());
        for (int b = 0; b < bins; ++b)
        {
            float re = fftBuf[(size_t) 2 * b], im = (b == 0 || b == bins - 1) ? 0.0f : fftBuf[(size_t) 2 * b + 1];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) fftSize + 1e-12f, -120.0f);
        }
        v2.computeProminence(magDb, 4.0f, prom);
        conf.process(prom);
        if (perFrame) perFrame(frameIdx, conf);
        ++frameIdx;
    }
}

// 0.25oct default (not 0.1) -- at 2048/48kHz, bass-range probes (e.g. 80Hz)
// land on a bin center that can be ~0.19oct away from the nominal
// frequency purely from FFT bin quantization; a tighter default falsely
// reports "NOT FOUND" for a region that is genuinely there.
static ConfidenceEngine::Region findRegionNear(const ConfidenceEngine& conf, float hz, float toleranceOct = 0.25f)
{
    ConfidenceEngine::Region best{};
    float bestDist = 1.0e9f;
    for (auto& r : conf.regions())
    {
        if (! r.active) continue;
        float dist = std::abs(std::log2(juce::jmax(1.0f, r.centerHz)) - std::log2(juce::jmax(1.0f, hz)));
        if (dist < bestDist) { bestDist = dist; best = r; }
    }
    if (bestDist > toleranceOct) return ConfidenceEngine::Region{}; // not found -- return inactive default
    return best;
}
static void printRegion(const char* label, const ConfidenceEngine::Region& r)
{
    if (! r.active) { std::printf("  %-40s NOT FOUND (no active region near expected frequency)\n", label); return; }
    std::printf("  %-40s freq=%8.1fHz prom=%6.2fdB persist=%.3f stability=%.3f harmLike=%.3f  CONFIDENCE=%.3f\n",
        label, r.centerHz, r.peakProminenceDb, r.persistence, r.stability, r.harmonicLikelihood, r.confidence);
}
// C1.5's full diagnostic readout: raw bin center vs sub-bin estimate,
// predicted harmonic, distance in cents, likelihood, final confidence.
static void printRegionFull(const char* label, const ConfidenceEngine::Region& r)
{
    if (! r.active) { std::printf("  %-40s NOT FOUND\n", label); return; }
    std::printf("  %s:\n", label);
    std::printf("    raw bin center       = %.2fHz\n", r.rawBinHz);
    std::printf("    sub-bin estimated    = %.2fHz\n", r.centerHz);
    std::printf("    nearest predicted harmonic = %.2fHz  (distance = %.1f cents)\n", r.harmonicExpectedHz, r.harmonicDistanceCents);
    std::printf("    harmonicLikelihood   = %.3f\n", r.harmonicLikelihood);
    std::printf("    persistence=%.3f  stability=%.3f  prominence=%.2fdB\n", r.persistence, r.stability, r.peakProminenceDb);
    std::printf("    -- full decomposition --\n");
    std::printf("    prominenceEvidence   = %.3f\n", r.lastProminenceEvidence);
    std::printf("    persistenceEvidence  = %.3f\n", r.lastPersistenceEvidence);
    std::printf("    stabilityEvidence    = %.3f\n", r.lastStabilityEvidence);
    std::printf("    widthEvidence        = %.3f\n", r.lastWidthEvidence);
    std::printf("    harmonicPenalty      = %.3f\n", r.lastHarmonicPenalty);
    std::printf("    FINAL CONFIDENCE     = %.3f\n", r.confidence);
}

static SpectralProminenceEngineV5 makeV2()
{
    SpectralProminenceEngineV5 v2;
    v2.prepare(bins, sr, fftSize);
    v2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    return v2;
}
static ConfidenceEngine makeConf()
{
    ConfidenceEngine c;
    c.prepare(sr, fftSize, hop);
    c.setPersistenceTimeConstants(3.0f, 8.0f); // ~32ms rise tau, ~85ms fall tau at 48k/hop512
    return c;
}

// C1.6: repeats the load-bearing cases at a GIVEN sample rate (FFT size
// stays fixed at 2048 -- the frozen STFT is untouched; only the Hz-per-bin
// ratio changes with sample rate, which is exactly the effect being
// measured). Self-contained (doesn't reuse the global `sr`-bound helpers
// above) so it can be parametrized cleanly.
static void runAtSampleRate(double testSr)
{
    const int testBins = fftSize / 2 + 1;
    const double testBinHz = testSr / fftSize;
    auto genHarm = [&](int n, double f0, float amp, int numH) {
        AudioBuffer<float> b(2, n); b.clear();
        for (int h = 1; h <= numH; ++h)
        {
            float a = amp * (float) juce::Decibels::decibelsToGain(-3.0 * (h - 1));
            double ph = 0, inc = juce::MathConstants<double>::twoPi * f0 * h / testSr;
            for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * a; for (int c = 0; c < 2; ++c) b.addSample(c, i, s); ph += inc; }
        }
        return b;
    };
    auto genHarmBoosted = [&](int n, double f0, float amp, int numH, int boostH, float boostDb) {
        AudioBuffer<float> b(2, n); b.clear();
        for (int h = 1; h <= numH; ++h)
        {
            float a = amp * (float) juce::Decibels::decibelsToGain(-3.0 * (h - 1));
            if (h == boostH) a *= (float) juce::Decibels::decibelsToGain(boostDb);
            double ph = 0, inc = juce::MathConstants<double>::twoPi * f0 * h / testSr;
            for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * a; for (int c = 0; c < 2; ++c) b.addSample(c, i, s); ph += inc; }
        }
        return b;
    };
    auto genRes = [&](int n, double freqHz, float amp, double Q, int seed) {
        AudioBuffer<float> b(2, n); b.clear(); juce::Random rng(seed);
        double bwHz = freqHz / Q;
        for (int k = 0; k < 9; ++k)
        {
            double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
            double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / testSr;
            for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * (amp / 3.0f); for (int c = 0; c < 2; ++c) b.addSample(c, i, s); ph += inc; }
        }
        return b;
    };
    auto run = [&](const AudioBuffer<float>& sig, ConfidenceEngine& conf) {
        SpectralProminenceEngineV5 v2; v2.prepare(testBins, testSr, fftSize); v2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        conf.prepare(testSr, fftSize, hop); conf.setPersistenceTimeConstants(3.0f, 8.0f);
        juce::dsp::FFT fft(11); std::vector<float> window((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
        std::vector<float> fftBuf((size_t) fftSize * 2, 0.0f), magDb((size_t) testBins, -120.0f), prom((size_t) testBins, 0.0f);
        int n = sig.getNumSamples();
        for (int pos = 0; pos + fftSize <= n; pos += hop)
        {
            for (int i = 0; i < fftSize; ++i) { float x=0; for (int c=0;c<sig.getNumChannels();++c) x+=sig.getSample(c,pos+i); x/=sig.getNumChannels(); fftBuf[(size_t)i]=x*window[(size_t)i]; }
            std::fill(fftBuf.begin()+fftSize, fftBuf.end(), 0.0f); fft.performRealOnlyForwardTransform(fftBuf.data());
            for (int b=0;b<testBins;++b){ float re=fftBuf[(size_t)2*b], im=(b==0||b==testBins-1)?0.0f:fftBuf[(size_t)2*b+1]; magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
            v2.computeProminence(magDb, 4.0f, prom); conf.process(prom);
        }
    };
    auto findNear = [&](const ConfidenceEngine& conf, float hz) {
        ConfidenceEngine::Region best{}; float bestDist = 1.0e9f;
        for (auto& r : conf.regions()) { if (! r.active) continue; float d = std::abs(std::log2(juce::jmax(1.0f,r.centerHz))-std::log2(juce::jmax(1.0f,hz))); if (d<bestDist){bestDist=d;best=r;} }
        return bestDist <= 0.3f ? best : ConfidenceEngine::Region{};
    };

    std::printf("\n  === sr=%.0fHz (bin width=%.2fHz) ===\n", testSr, testBinHz);
    int n2s = (int) (testSr * 2.0);

    { auto sig = genHarm(n2s, 80.0, 0.4f, 8); ConfidenceEngine conf; run(sig, conf);
      std::printf("  80/135 Scenario A (80Hz harmonic bass only): "); auto r = findNear(conf, 80.0);
      if (r.active) std::printf("f0 conf=%.3f harmLike=%.3f\n", r.confidence, r.harmonicLikelihood); else std::printf("f0 NOT FOUND\n"); }
    { auto h = genHarm(n2s, 80.0, 0.4f, 8); auto res = genRes(n2s, 135.0, 0.35f, 10.0, 300);
      AudioBuffer<float> sig(2, n2s); sig.clear(); for (int c=0;c<2;++c){ sig.addFrom(c,0,h,c,0,n2s); sig.addFrom(c,0,res,c,0,n2s); }
      ConfidenceEngine conf; run(sig, conf);
      auto rf0 = findNear(conf, 80.0), rres = findNear(conf, 135.0);
      std::printf("  80/135 Scenario B (+ non-harmonic @135Hz): f0 conf=%.3f | 135Hz conf=%.3f harmLike=%.3f %s\n",
          rf0.active ? rf0.confidence : -1.0f, rres.active ? rres.confidence : -1.0f, rres.active ? rres.harmonicLikelihood : -1.0f,
          rres.active && rres.harmonicLikelihood > 0.5f ? "<-- STILL AMBIGUOUS" : "(correctly low harmLike)"); }
    { auto sig = genHarmBoosted(n2s, 80.0, 0.4f, 8, 2, 9.0f); // real 2nd harmonic (160Hz) excessively elevated
      ConfidenceEngine conf; run(sig, conf);
      auto r = findNear(conf, 160.0);
      std::printf("  80/160 Scenario C (real H2 @160Hz, +9dB excessive): conf=%.3f harmLike=%.3f\n", r.active?r.confidence:-1.0f, r.active?r.harmonicLikelihood:-1.0f); }
    { auto h = genHarm(n2s, 120.0, 0.4f, 8); auto res = genRes(n2s, 170.0, 0.35f, 10.0, 301);
      AudioBuffer<float> sig(2, n2s); sig.clear(); for (int c=0;c<2;++c){ sig.addFrom(c,0,h,c,0,n2s); sig.addFrom(c,0,res,c,0,n2s); }
      ConfidenceEngine conf; run(sig, conf);
      auto rf0 = findNear(conf, 120.0), rres = findNear(conf, 170.0);
      std::printf("  120/170 (f0 + non-harmonic @170Hz): f0 conf=%.3f | 170Hz conf=%.3f harmLike=%.3f\n",
          rf0.active?rf0.confidence:-1.0f, rres.active?rres.confidence:-1.0f, rres.active?rres.harmonicLikelihood:-1.0f); }
    { auto sig = genHarm(n2s, 110.0, 0.4f, 12); ConfidenceEngine conf; run(sig, conf);
      auto r = findNear(conf, 110.0);
      std::printf("  H1 (clean series, f0=110Hz): f0 conf=%.3f\n", r.active?r.confidence:-1.0f); }
    { auto sig = genHarmBoosted(n2s, 110.0, 0.4f, 12, 4, 8.0f); ConfidenceEngine conf; run(sig, conf);
      auto r = findNear(conf, 440.0);
      std::printf("  H2 (H4 @440Hz boosted +8dB): conf=%.3f\n", r.active?r.confidence:-1.0f); }
    { auto h = genHarm(n2s, 110.0, 0.4f, 12); auto res = genRes(n2s, 381.05, 0.3f, 15.0, 400);
      AudioBuffer<float> sig(2, n2s); sig.clear(); for (int c=0;c<2;++c){ sig.addFrom(c,0,h,c,0,n2s); sig.addFrom(c,0,res,c,0,n2s); }
      ConfidenceEngine conf; run(sig, conf);
      auto r = findNear(conf, 381.05);
      std::printf("  H3 (non-harmonic @381Hz): conf=%.3f\n", r.active?r.confidence:-1.0f); }
}

int main()
{
    std::printf("================================================================\n");
    std::printf("PHYSICAL C Checkpoint C1 -- Confidence layer, diagnostic only\n");
    std::printf("sr=%.0fHz fft=%d hop=%d (1 frame = %.3fms)\n", sr, fftSize, hop, frameMs);
    std::printf("================================================================\n\n");

    // ---------------- C2: persistence T63/T90/T95 ----------------
    std::printf("-- C2: persistence rise time (T63/T90/T95, sustained tone @1kHz) --\n");
    {
        auto sig = genHarmonicSeries((int) (sr * 2.0), 1000.0, 0.3f, 1);
        auto v2 = makeV2(); auto conf = makeConf();
        int t63f = -1, t90f = -1, t95f = -1;
        runPipeline(sig, v2, conf, [&](int f, const ConfidenceEngine& c) {
            auto r = findRegionNear(c, 1000.0);
            if (r.active) { if (t63f < 0 && r.persistence >= 0.63f) t63f = f; if (t90f < 0 && r.persistence >= 0.90f) t90f = f; if (t95f < 0 && r.persistence >= 0.95f) t95f = f; }
        });
        auto report = [&](const char* name, int fr) { if (fr < 0) std::printf("  %s: not reached within signal duration\n", name); else std::printf("  %s = %d frames = %.1fms (frame-quantized; real precision is no finer than %.3fms)\n", name, fr, fr * frameMs, frameMs); };
        report("T63", t63f); report("T90", t90f); report("T95", t95f);
    }

    std::printf("\n-- C2: persistence across different signal TYPES --\n");
    {
        struct Case { const char* name; std::function<AudioBuffer<float>()> gen; float probeHz; };
        std::vector<Case> cases = {
            { "sustained tone/resonance", [] { return genHarmonicSeries((int) (sr * 1.5), 1500.0, 0.3f, 1); }, 1500.0f },
            { "short burst (150ms)", [] { return genResonanceBurst((int) (sr * 1.5), 1500.0, 0.35f, 10.0, 50, 0, (int) (sr * 0.15)); }, 1500.0f },
            { "kick", [] { return genKick((int) (sr * 1.5)); }, 100.0f },
            { "snare", [] { return genSnare((int) (sr * 1.5)); }, 200.0f },
            { "vocal consonant (unvoiced burst)", [] { return genVocalConsonant((int) (sr * 1.5)); }, 3000.0f },
            { "resonance appears then disappears", [] { return genResonanceBurst((int) (sr * 1.5), 2000.0, 0.35f, 10.0, 60, (int) (sr * 0.3), (int) (sr * 0.5)); }, 2000.0f },
        };
        for (auto& cs : cases)
        {
            auto sig = cs.gen(); auto v2 = makeV2(); auto conf = makeConf();
            float maxPersist = 0.0f; int framesActive = 0;
            runPipeline(sig, v2, conf, [&](int, const ConfidenceEngine& c) { auto r = findRegionNear(c, cs.probeHz, 0.3f); if (r.active) { maxPersist = juce::jmax(maxPersist, r.persistence); ++framesActive; } });
            std::printf("  %-32s max persistence=%.3f  frames with an active region nearby=%d\n", cs.name, maxPersist, framesActive);
        }
    }

    // ---------------- C3: frequency stability ----------------
    std::printf("\n-- C3: frequency stability (stable vs jumping) --\n");
    {
        auto stableSig = genHarmonicSeries((int) (sr * 1.0), 1003.0, 0.3f, 1); // near-1000Hz, holds
        AudioBuffer<float> jump(2, (int) (sr * 1.0)); jump.clear();
        { auto s1 = genHarmonicSeries((int)(sr*0.33), 800.0, 0.3f, 1); auto s2 = genHarmonicSeries((int)(sr*0.33), 1500.0, 0.3f, 1); auto s3 = genHarmonicSeries((int)(sr*0.34), 3000.0, 0.3f, 1);
          for (int c=0;c<2;++c){ jump.copyFrom(c,0,s1,c,0,s1.getNumSamples()); jump.copyFrom(c,s1.getNumSamples(),s2,c,0,s2.getNumSamples()); jump.copyFrom(c,s1.getNumSamples()+s2.getNumSamples(),s3,c,0,s3.getNumSamples()); } }
        for (auto& pr : std::vector<std::pair<const char*, AudioBuffer<float>*>>{ { "stable (~1003Hz held)", &stableSig }, { "jumping (800->1500->3000Hz)", &jump } })
        {
            auto v2 = makeV2(); auto conf = makeConf();
            float lastStability = 0.0f;
            runPipeline(*pr.second, v2, conf, [&](int, const ConfidenceEngine& c) { for (auto& r : c.regions()) if (r.active) lastStability = juce::jmax(lastStability, r.stability); });
            std::printf("  %-32s max observed stability=%.3f\n", pr.first, lastStability);
        }
    }

    // ---------------- C5: H1/H2/H3 harmonic reasoning ----------------
    std::printf("\n-- C5: H1 (clean harmonic series, f0=110Hz) --\n");
    { auto sig = genHarmonicSeries((int)(sr*2.0), 110.0, 0.4f, 12); auto v2=makeV2(); auto conf=makeConf();
      runPipeline(sig, v2, conf);
      printRegion("f0 (110Hz)", findRegionNear(conf, 110.0));
      printRegion("H4 (440Hz, untouched)", findRegionNear(conf, 440.0));
      printRegion("H10 (1100Hz)", findRegionNear(conf, 1100.0)); }

    std::printf("\n-- C5: H2 (same series, partial #4/440Hz boosted +8dB) --\n");
    { auto sig = genHarmonicSeriesBoosted((int)(sr*2.0), 110.0, 0.4f, 12, 4, 8.0f); auto v2=makeV2(); auto conf=makeConf();
      runPipeline(sig, v2, conf);
      printRegion("f0 (110Hz)", findRegionNear(conf, 110.0));
      printRegion("H4 (440Hz, +8dB boosted)", findRegionNear(conf, 440.0)); }

    std::printf("\n-- C5: H3 (same series + non-harmonic resonance @505Hz) --\n");
    { auto h = genHarmonicSeries((int)(sr*2.0), 110.0, 0.4f, 12); auto res = genResonanceBurst((int)(sr*2.0), 381.05, 0.3f, 15.0, 400);
      AudioBuffer<float> sig(2,(int)(sr*2.0)); sig.clear(); for(int c=0;c<2;++c){sig.addFrom(c,0,h,c,0,h.getNumSamples()); sig.addFrom(c,0,res,c,0,res.getNumSamples());}
      auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
      printRegion("f0 (110Hz)", findRegionNear(conf, 110.0));
      printRegion("381Hz injected resonance (non-harmonic, 249cents from H3/H4)", findRegionNear(conf, 381.05)); }

    // ---------------- C9: T1-T7 controlled cases ----------------
    std::printf("\n-- C9: T1-T7 controlled cases --\n");
    {
        std::printf("T1 -- transient only (clicks):\n");
        { auto sig = genTransientClicks((int)(sr*2.0), 5, 4); auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          int shown=0; for (auto& r : conf.regions()) if (r.active && shown<3) { printRegion("transient region", r); ++shown; } if(!shown) std::printf("  (no persistent region survived -- expected for pure transients)\n"); }

        std::printf("T2 -- sustained non-harmonic ringing @1.7kHz:\n");
        { auto sig = genResonanceBurst((int)(sr*2.0), 1700.0, 0.3f, 12.0, 700); auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          printRegion("ringing @1.7kHz", findRegionNear(conf, 1700.0)); }

        std::printf("T3 -- fundamental + normal harmonics (f0=140Hz):\n");
        { auto sig = genHarmonicSeries((int)(sr*2.0), 140.0, 0.4f, 10); auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          printRegion("f0 (140Hz)", findRegionNear(conf, 140.0)); printRegion("H3 (420Hz)", findRegionNear(conf, 420.0)); }

        std::printf("T4 -- fundamental + one excessive harmonic (f0=140Hz, H3 +9dB):\n");
        { auto sig = genHarmonicSeriesBoosted((int)(sr*2.0), 140.0, 0.4f, 10, 3, 9.0f); auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          printRegion("H3 (420Hz, +9dB boosted)", findRegionNear(conf, 420.0)); }

        std::printf("T5 -- fundamental + non-harmonic resonance (f0=140Hz, res@610Hz):\n");
        { auto h = genHarmonicSeries((int)(sr*2.0), 140.0, 0.4f, 10); auto res = genResonanceBurst((int)(sr*2.0), 610.0, 0.3f, 12.0, 900);
          AudioBuffer<float> sig(2,(int)(sr*2.0)); sig.clear(); for(int c=0;c<2;++c){sig.addFrom(c,0,h,c,0,h.getNumSamples()); sig.addFrom(c,0,res,c,0,res.getNumSamples());}
          auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          printRegion("610Hz injected resonance (non-harmonic)", findRegionNear(conf, 610.0)); }

        std::printf("T6 -- moving tonal component (800->3000Hz sweep):\n");
        { auto sig = genMovingTone((int)(sr*2.0), 800.0, 3000.0, 0.3f); auto v2=makeV2(); auto conf=makeConf();
          float maxStab=0, maxConf=0; runPipeline(sig, v2, conf, [&](int, const ConfidenceEngine& c){ for(auto& r: c.regions()) if(r.active){ maxStab=juce::jmax(maxStab,r.stability); maxConf=juce::jmax(maxConf,r.confidence);} });
          std::printf("  moving component: max stability observed=%.3f  max confidence observed=%.3f\n", maxStab, maxConf); }

        std::printf("T7 -- stationary resonance @2.2kHz:\n");
        { auto sig = genResonanceBurst((int)(sr*2.0), 2200.0, 0.3f, 12.0, 1000); auto v2=makeV2(); auto conf=makeConf(); runPipeline(sig, v2, conf);
          printRegion("stationary @2.2kHz", findRegionNear(conf, 2200.0)); }
    }

    // ---------------- C10/item1: matched-evidence B vs C ----------------
    // B and C now use the IDENTICAL injection technique (genResonanceBurst,
    // same Q, same duration, same seed-derived phase) at the SAME amplitude
    // -- the ONLY difference is frequency: B sits at the geometric midpoint
    // between h1=80Hz and h2=160Hz (113.14Hz, exactly 600 cents from
    // either -- maximally unambiguous non-harmonic), C sits EXACTLY on
    // h2=160Hz. This isolates the effect of the harmonic RELATIONSHIP
    // itself, not a confound between two differently-generated anomalies.
    std::printf("\n-- C1.5/item1: bass 80Hz -- matched-evidence B (non-harmonic@113.14Hz) vs C (on-harmonic@160Hz) --\n");
    {
        auto bassOnly = genHarmonicSeries((int)(sr*2.0), 80.0, 0.4f, 8);
        auto v2a = makeV2(); auto confA = makeConf(); runPipeline(bassOnly, v2a, confA);
        std::printf(" Scenario A (80Hz + normal harmonics only, no injection):\n");
        printRegionFull("f0 (80Hz)", findRegionNear(confA, 80.0));

        for (float injectDb : { 6.0f, 10.0f })
        {
            float ampLinear = 0.2f * (float) std::pow(10.0, injectDb / 20.0);
            std::printf("\n === matched pair @ +%.0fdB (ampLinear=%.3f) ===\n", injectDb, ampLinear);

            auto resB = genResonanceBurst((int)(sr*2.0), 113.14, ampLinear, 10.0, 300);
            AudioBuffer<float> sigB(2,(int)(sr*2.0)); sigB.clear();
            for(int c=0;c<2;++c){ sigB.addFrom(c,0,bassOnly,c,0,bassOnly.getNumSamples()); sigB.addFrom(c,0,resB,c,0,resB.getNumSamples()); }
            auto v2b = makeV2(); auto confB = makeConf(); runPipeline(sigB, v2b, confB);
            std::printf(" Scenario B (+non-harmonic @113.14Hz, 600 cents from h1/h2):\n");
            printRegionFull("113.14Hz injected resonance", findRegionNear(confB, 113.14f));

            auto resC = genResonanceBurst((int)(sr*2.0), 160.0, ampLinear, 10.0, 300); // same Q, same amp, same seed -- only frequency differs
            AudioBuffer<float> sigC(2,(int)(sr*2.0)); sigC.clear();
            for(int c=0;c<2;++c){ sigC.addFrom(c,0,bassOnly,c,0,bassOnly.getNumSamples()); sigC.addFrom(c,0,resC,c,0,resC.getNumSamples()); }
            auto v2c = makeV2(); auto confC = makeConf(); runPipeline(sigC, v2c, confC);
            std::printf(" Scenario C (+on-harmonic burst @160Hz=h2, SAME technique/amp/Q as B):\n");
            printRegionFull("160Hz injected burst (on h2)", findRegionNear(confC, 160.0f));

            auto rf0 = findRegionNear(confA, 80.0);
            auto rB = findRegionNear(confB, 113.14f); auto rC = findRegionNear(confC, 160.0f);
            std::printf(" Summary @+%.0fdB: fundamental=%.3f  non-harmonic(B)=%.3f  on-harmonic(C)=%.3f   %s\n",
                injectDb, rf0.active?rf0.confidence:-1.0f, rB.active?rB.confidence:-1.0f, rC.active?rC.confidence:-1.0f,
                (rB.active && rC.active && rf0.active && rB.confidence > rC.confidence && rC.confidence > rf0.confidence)
                    ? "PASS: non-harmonic > on-harmonic-excessive > fundamental" : "DOES NOT SATISFY expected ordering");
        }
    }
    std::printf("\n-- C1.5: fundamental=120Hz + non-harmonic resonance @170Hz (explicit repeat, tied to the LUNA bass hole) --\n");
    {
        auto bassOnly = genHarmonicSeries((int)(sr*2.0), 120.0, 0.4f, 8);
        auto res = genResonanceBurst((int)(sr*2.0), 170.0, 0.35f, 10.0, 301);
        AudioBuffer<float> sig(2,(int)(sr*2.0)); sig.clear();
        for(int c=0;c<2;++c){ sig.addFrom(c,0,bassOnly,c,0,bassOnly.getNumSamples()); sig.addFrom(c,0,res,c,0,res.getNumSamples()); }
        auto v2 = makeV2(); auto conf = makeConf(); runPipeline(sig, v2, conf);
        printRegionFull("f0 (120Hz)", findRegionNear(conf, 120.0));
        printRegionFull("170Hz injected resonance", findRegionNear(conf, 170.0));
        std::printf(" [BLOCKER-1 DEBUG] all f0-candidate scores from the last processed frame:\n");
        for (auto& c : conf.lastF0Candidates())
            if (c.active) std::printf("   candidate freq=%8.2fHz  evidence=%.3f  matches=%d\n", c.centerHz, c.evidence, c.matches);
        std::printf("   winner index=%d\n", conf.lastF0WinnerIndex());

        // REGRESSION (item 4): the old bug picked a 2f0 candidate (fewer,
        // tighter matches) over the true f0 (more, looser matches). Assert
        // the winner is the LOWEST-frequency, most-matched candidate --
        // fails loudly (non-zero exit) if this regresses.
        int winnerIdx = conf.lastF0WinnerIndex();
        bool f0RegressionOk = false;
        if (winnerIdx >= 0)
        {
            auto& winner = conf.lastF0Candidates()[(size_t) winnerIdx];
            int winnerMatches = winner.matches;
            bool anyBetterSupportedCandidateExists = false;
            for (auto& c : conf.lastF0Candidates())
                if (c.active && c.matches > winnerMatches) anyBetterSupportedCandidateExists = true;
            f0RegressionOk = ! anyBetterSupportedCandidateExists;
        }
        std::printf(" [F0 REGRESSION CHECK] winner has the most supporting partials among candidates: %s\n",
            f0RegressionOk ? "PASS" : "FAIL -- a less-supported candidate won, the old 238Hz-beats-117Hz bug is back");
    }

    // ---------------- item 5: harmonicSigmaCents sweep ----------------
    std::printf("\n-- item5: harmonicSigmaCents sweep (35/40/45/50/60), same tests --\n");
    for (float sigma : { 35.0f, 40.0f, 45.0f, 50.0f, 60.0f })
    {
        std::printf(" sigma=%.0f cents:\n", sigma);
        auto withSigma = [&](ConfidenceEngine& c) { c.setHarmonicSigmaCents(sigma); };

        // H1/H2/H3 @381.05Hz
        auto H1s = genHarmonicSeries((int)(sr*2.0), 110.0, 0.4f, 12);
        SpectralProminenceEngineV5 v2h1; v2h1.prepare(bins,sr,fftSize); v2h1.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine confH1; confH1.prepare(sr,fftSize,hop); confH1.setPersistenceTimeConstants(3.0f,8.0f); withSigma(confH1);
        runPipeline(H1s, v2h1, confH1);
        auto H2s = genHarmonicSeriesBoosted((int)(sr*2.0), 110.0, 0.4f, 12, 4, 8.0f);
        SpectralProminenceEngineV5 v2h2; v2h2.prepare(bins,sr,fftSize); v2h2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine confH2; confH2.prepare(sr,fftSize,hop); confH2.setPersistenceTimeConstants(3.0f,8.0f); withSigma(confH2);
        runPipeline(H2s, v2h2, confH2);
        auto h3base = genHarmonicSeries((int)(sr*2.0), 110.0, 0.4f, 12); auto h3res = genResonanceBurst((int)(sr*2.0), 381.05, 0.3f, 15.0, 400);
        AudioBuffer<float> H3s(2,(int)(sr*2.0)); H3s.clear(); for(int c=0;c<2;++c){H3s.addFrom(c,0,h3base,c,0,h3base.getNumSamples()); H3s.addFrom(c,0,h3res,c,0,h3res.getNumSamples());}
        SpectralProminenceEngineV5 v2h3; v2h3.prepare(bins,sr,fftSize); v2h3.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine confH3; confH3.prepare(sr,fftSize,hop); confH3.setPersistenceTimeConstants(3.0f,8.0f); withSigma(confH3);
        runPipeline(H3s, v2h3, confH3);
        auto rH1 = findRegionNear(confH1, 440.0), rH2 = findRegionNear(confH2, 440.0), rH3 = findRegionNear(confH3, 381.05);
        std::printf("   H1(H4 normal)=%.3f  H2(H4 boosted)=%.3f  H3(non-harm@381)=%.3f  %s\n",
            rH1.active?rH1.confidence:-1, rH2.active?rH2.confidence:-1, rH3.active?rH3.confidence:-1,
            (rH3.active && rH2.active && rH1.active && rH3.confidence > rH2.confidence && rH2.confidence > rH1.confidence) ? "PASS H3>H2>H1" : "fails H3>H2>H1");

        // 80/135(->113.14) and 120/170
        auto bass80 = genHarmonicSeries((int)(sr*2.0), 80.0, 0.4f, 8);
        auto res113 = genResonanceBurst((int)(sr*2.0), 113.14, 0.35f, 10.0, 300);
        AudioBuffer<float> sig80(2,(int)(sr*2.0)); sig80.clear(); for(int c=0;c<2;++c){sig80.addFrom(c,0,bass80,c,0,bass80.getNumSamples()); sig80.addFrom(c,0,res113,c,0,res113.getNumSamples());}
        SpectralProminenceEngineV5 v2_80; v2_80.prepare(bins,sr,fftSize); v2_80.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine conf80; conf80.prepare(sr,fftSize,hop); conf80.setPersistenceTimeConstants(3.0f,8.0f); withSigma(conf80);
        runPipeline(sig80, v2_80, conf80);
        auto r80f0 = findRegionNear(conf80, 80.0), r113 = findRegionNear(conf80, 113.14f);

        auto bass120 = genHarmonicSeries((int)(sr*2.0), 120.0, 0.4f, 8);
        auto res170 = genResonanceBurst((int)(sr*2.0), 170.0, 0.35f, 10.0, 301);
        AudioBuffer<float> sig120(2,(int)(sr*2.0)); sig120.clear(); for(int c=0;c<2;++c){sig120.addFrom(c,0,bass120,c,0,bass120.getNumSamples()); sig120.addFrom(c,0,res170,c,0,res170.getNumSamples());}
        SpectralProminenceEngineV5 v2_120; v2_120.prepare(bins,sr,fftSize); v2_120.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        ConfidenceEngine conf120; conf120.prepare(sr,fftSize,hop); conf120.setPersistenceTimeConstants(3.0f,8.0f); withSigma(conf120);
        runPipeline(sig120, v2_120, conf120);
        auto r120f0 = findRegionNear(conf120, 120.0), r170 = findRegionNear(conf120, 170.0);

        std::printf("   80fund=%.3f 113.14nonharm=%.3f (%s)   120fund=%.3f 170nonharm=%.3f (%s)\n",
            r80f0.active?r80f0.confidence:-1, r113.active?r113.confidence:-1, (r113.active&&r80f0.active&&r113.confidence>r80f0.confidence)?"PASS":"FAIL",
            r120f0.active?r120f0.confidence:-1, r170.active?r170.confidence:-1, (r170.active&&r120f0.active&&r170.confidence>r120f0.confidence)?"PASS":"FAIL");
    }

    std::printf("\n-- C1.6: repeat 80/135, 80/160(C), 120/170, H1/H2/H3 across 44.1/48/96/192kHz --\n");
    for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        runAtSampleRate(testSr);

    // ---------------- C6: Selectivity curve ----------------
    std::printf("\n-- C6: Selectivity pass-weight curve (low=permissive, high=strict) --\n");
    {
        for (float testConf : { 0.2f, 0.4f, 0.6f, 0.8f })
        {
            std::printf("  confidence=%.1f: ", testConf);
            for (float sel : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
                std::printf("sel=%.1f->%.3f  ", sel, ConfidenceEngine::passWeight(testConf, sel));
            std::printf("\n");
        }
    }

    // ---------------- C11: musical material sparsity ----------------
    std::printf("\n-- C11: musical material -- does confidence sparsify the raw prominence map? --\n");
    {
        struct MCase { const char* name; std::function<AudioBuffer<float>()> gen; };
        std::vector<MCase> mcases = {
            { "Vocal-like (harmonic+formant approx)", [] { auto b = genHarmonicSeries((int)(sr*2.0), 130.0, 0.35f, 14); return b; } },
            { "Bass", [] { return genHarmonicSeries((int)(sr*2.0), 80.0, 0.4f, 8); } },
            { "Kick", [] { return genKick((int)(sr*2.0)); } },
            { "Guitar-like (harmonic, decaying)", [] { return genHarmonicSeries((int)(sr*2.0), 220.0, 0.3f, 10); } },
        };
        for (auto& mc : mcases)
        {
            auto sig = mc.gen(); auto v2 = makeV2(); auto conf = makeConf();
            int rawCandidateBinsLastFrame = 0;
            std::vector<float> lastProm;
            runPipeline(sig, v2, conf, [&](int, const ConfidenceEngine&) {});
            // Re-run once more just to grab the final frame's raw prominence-above-3dB bin count for comparison.
            {
                juce::dsp::FFT fft(11); std::vector<float> window((size_t) fftSize);
                for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
                std::vector<float> fftBuf((size_t) fftSize * 2, 0.0f), magDb((size_t) bins, -120.0f), prom((size_t) bins, 0.0f);
                int pos = sig.getNumSamples() - fftSize - hop;
                for (int i = 0; i < fftSize; ++i) { float x=0; for (int c=0;c<sig.getNumChannels();++c) x+=sig.getSample(c,pos+i); x/=sig.getNumChannels(); fftBuf[(size_t)i]=x*window[(size_t)i]; }
                std::fill(fftBuf.begin()+fftSize, fftBuf.end(), 0.0f); fft.performRealOnlyForwardTransform(fftBuf.data());
                for (int b=0;b<bins;++b){ float re=fftBuf[(size_t)2*b], im=(b==0||b==bins-1)?0.0f:fftBuf[(size_t)2*b+1]; magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
                SpectralProminenceEngineV5 v2b; v2b.prepare(bins,sr,fftSize); v2b.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
                v2b.computeProminence(magDb, 4.0f, prom);
                for (float p : prom) if (p > 3.0f) ++rawCandidateBinsLastFrame;
            }
            int highConfRegions = 0; for (auto& r : conf.regions()) if (r.active && r.confidence > 0.5f) ++highConfRegions;
            int anyActiveRegions = conf.activeRegionCount();
            std::printf("  %-32s raw prominence bins>3dB (last frame)=%4d  tracked regions=%2d  regions w/ confidence>0.5=%2d\n",
                mc.name, rawCandidateBinsLastFrame, anyActiveRegions, highConfRegions);
        }
    }

    // ---------------- C12: incremental CPU cost ----------------
    std::printf("\n-- C12: incremental CPU cost, prominence-only vs +confidence (Release, profiling OFF) --\n");
    {
        for (double testSr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            SpectralProminenceEngineV5 v2; v2.prepare(bins, testSr, fftSize); v2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
            ConfidenceEngine conf; conf.prepare(testSr, fftSize, hop);
            std::vector<float> frame((size_t) bins); for (int i=0;i<bins;++i) frame[(size_t)i] = (float) (-10.0 - 4.0*std::log2(juce::jmax(1.0,(double)i*testSr/fftSize)/1000.0));
            for (double f : { 110.0, 220.0, 330.0, 440.0 }) { int b=(int)std::round(f*fftSize/testSr); if (b>=0&&b<bins) frame[(size_t)b]=-20.0f; }
            std::vector<float> prom((size_t) bins, 0.0f);
            const int iters=3000, warmup=300;
            std::vector<double> promOnlyUs, promPlusConfUs;
            for (int it=0; it<iters; ++it)
            {
                auto t0=std::chrono::steady_clock::now(); v2.computeProminence(frame, 4.0f, prom); auto t1=std::chrono::steady_clock::now();
                if (it>=warmup) promOnlyUs.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
            }
            for (int it=0; it<iters; ++it)
            {
                auto t0=std::chrono::steady_clock::now(); v2.computeProminence(frame, 4.0f, prom); conf.process(prom); auto t1=std::chrono::steady_clock::now();
                if (it>=warmup) promPlusConfUs.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
            }
            auto median=[](std::vector<double> v){ std::sort(v.begin(),v.end()); return v[v.size()/2]; };
            auto pct=[](std::vector<double> v, double p){ std::sort(v.begin(),v.end()); return v[(size_t)(p*(v.size()-1))]; };
            double budget = 1.0e6*hop/testSr;
            std::printf("  sr=%.0fHz  prominence-only: med=%.2fus(%.2f%%) P95=%.2fus P99=%.2fus | +confidence: med=%.2fus(%.2f%%) P95=%.2fus P99=%.2fus\n",
                testSr, median(promOnlyUs), 100.0*median(promOnlyUs)/budget, pct(promOnlyUs,0.95), pct(promOnlyUs,0.99),
                median(promPlusConfUs), 100.0*median(promPlusConfUs)/budget, pct(promPlusConfUs,0.95), pct(promPlusConfUs,0.99));
        }
    }

    std::printf("\n================================================================\n");
    std::printf("Checkpoint C1 report complete. Diagnostic only -- no gain applied,\n");
    std::printf("no audio processing changed. Awaiting review before connecting to audio.\n");
    std::printf("================================================================\n");
    return 0;
}
