// REAL SUPPRESSION AUDIT (Sonic Alpha V2). Drives the FULL real DSP path
// (SpectralEngine::process(), the same class/method the plugin itself
// calls) with synthetic pink-noise-bed + narrow-tone test material, and
// independently measures actual input/output spectral level via a
// Goertzel single-bin estimator that has NOTHING to do with the plugin's
// own analysis FFT -- so this is a genuine external measurement of what
// the DSP actually did to the audio, not a re-read of the plugin's own
// internal numbers. Special attention to 4-8kHz per the audit request.
//
// Also verifies structurally (by direct array comparison) that
// GainMaskEngine::process()'s reductionDbOut (the value multiplied into
// fftData -- the ACTUALLY APPLIED gain) and appliedReductionSnapshot()
// (what the UI curve reads) are the SAME array, never independently
// computed -- confirming item 1 of the audit without guessing.

#include <JuceHeader.h>
#include "DSP/GainMaskEngine.h"
#include "DSP/SpectralEngine.h"
#include "DSP/ResonanceDetector.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <random>

static std::vector<float> genPinkNoise(int n, float amp, unsigned seed = 12345)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> out((size_t) n, 0.0f);
    float b0 = 0, b1 = 0, b2 = 0;
    for (int i = 0; i < n; ++i)
    {
        float white = dist(rng);
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        float pink = b0 + b1 + b2 + white * 0.1848f;
        out[(size_t) i] = pink * 0.11f * amp;
    }
    return out;
}
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{ double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr; for (int i = 0; i < (int) b.size(); ++i) { b[(size_t) i] += (float) std::sin(ph) * amp; ph += inc; } }

// Independent Goertzel single-bin magnitude estimator -- Hann-windowed,
// coherent-gain-normalized so a full-scale sine reads ~0dBFS. Has no
// relationship to the plugin's own internal FFT size/window; used purely
// to measure what actually left the DSP as audio.
static float goertzelDb(const std::vector<float>& x, int start, int len, double sr, double freq)
{
    double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    double cw = std::cos(w), coeff = 2.0 * cw, sw = std::sin(w);
    double q0 = 0, q1 = 0, q2 = 0;
    for (int i = 0; i < len; ++i)
    {
        double win = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / (len - 1));
        double sample = (double) x[(size_t) (start + i)] * win;
        q0 = coeff * q1 - q2 + sample; q2 = q1; q1 = q0;
    }
    double real = q1 - q2 * cw, imag = q2 * sw;
    double mag = std::sqrt(real * real + imag * imag) / (len * 0.5);
    return (float) (20.0 * std::log10(mag + 1e-12));
}

struct RunResult
{
    std::vector<float> output;
    std::array<float, GainMaskEngine::kUIBins> snapshot{};
    int snapshotBins = 0;
    double sampleRate = 48000.0;
};

static RunResult runEngine(const std::vector<float>& input, double sr, const SpectralEngine::Params& p)
{
    RunResult r; r.sampleRate = sr;
    juce::AudioBuffer<float> buf(1, (int) input.size());
    for (int i = 0; i < (int) input.size(); ++i) buf.setSample(0, i, input[(size_t) i]);
    SpectralEngine eng; eng.prepare(sr, 1); eng.setParams(p); eng.process(buf, nullptr);
    r.output.resize((size_t) input.size());
    for (int i = 0; i < (int) input.size(); ++i) r.output[(size_t) i] = buf.getSample(0, i);
    r.snapshot = eng.getAppliedReductionSnapshot();
    r.snapshotBins = eng.getAppliedReductionSnapshotBinCount();
    return r;
}

// bin closest to freq in the appliedReductionSnapshot array (fftSize=2048 always, per kUIBins doc)
static int binForFreq(double sr, double freq) { return (int) std::round(freq * 2048.0 / sr); }

int main()
{
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what); if (!cond) allPass = false; };

    const double sr = 48000.0;
    const int n = (int) (sr * 2.5);
    const int measLen = (int) (sr * 0.8);
    // Tail-aligned: ends exactly at the buffer's own end, so it covers the
    // SAME instant appliedReductionSnapshot() reflects (the snapshot is
    // whatever the last-processed frame published) -- comparing an
    // AVERAGE window against an INSTANTANEOUS end-of-run snapshot only
    // makes sense if both describe the same moment, not two different
    // points on a still-settling curve.
    const int measStart = n - measLen;

    SpectralEngine::Params defP;
    defP.depth = 3.0f; defP.sharpness = 4.0f; defP.selectivity = 3.5f; defP.attackMs = 10.0f; defP.releaseMs = 80.0f;
    defP.transient = 5.0f; defP.lowHz = 25.0f; defP.highHz = 16000.0f; defP.detail = 5.0f;
    defP.maxReductionEnabled = true; defP.maxReductionDb = 3.0f;

    std::printf("=== NF RESONANCE -- REAL SUPPRESSION AUDIT ===\n\n");

    // ---------------------------------------------------------------
    // ITEM 1: appliedReductionSnapshot() IS the actually-applied gain,
    // not an independent detector-only estimate -- direct array proof.
    // ---------------------------------------------------------------
    std::printf("-- ITEM 1: appliedReductionSnapshot() vs actually-applied gain (direct array identity) --\n");
    {
        const int kFft = 2048, kHop = 512, bins = kFft / 2 + 1;
        GainMaskEngine mask; mask.prepare(sr, kFft, kHop);
        mask.setParams(9.0f, 3.5f, 10.0f, 80.0f, 20.0f, 20000.0f);
        mask.setDetail(5.0f); mask.setSharpness(4.0f); mask.setMaxReduction(true, 3.0f);
        float bf[ResonanceDetector::kMaxBands]{}, bs[ResonanceDetector::kMaxBands]{}, bw[ResonanceDetector::kMaxBands]{}, bfoc[ResonanceDetector::kMaxBands]{};
        int bsh[ResonanceDetector::kMaxBands]{}; bool ba[ResonanceDetector::kMaxBands]{};
        mask.setSensitivityCurve(bf, bs, bw, bsh, bfoc, ba);
        auto sig = genPinkNoise(n, 0.05f); addTone(sig, sr, 5000.0, 0.3f);
        juce::dsp::FFT fft(11);
        std::vector<float> window((size_t) kFft), scratch((size_t) kFft * 2), magDb((size_t) bins), reductionOut;
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1));
        bool identical = true; int framesChecked = 0;
        for (int i = 0; i + kFft <= n; i += kHop)
        {
            for (int k = 0; k < kFft; ++k) scratch[(size_t) k] = sig[(size_t) (i + k)] * window[(size_t) k];
            std::fill(scratch.begin() + kFft, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform(scratch.data());
            for (int b = 0; b < bins; ++b)
            { float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
              magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / (float) kFft + 1e-12f, -120.0f); }
            mask.process(magDb, sig.data() + i, kHop, reductionOut);
            auto& snap = mask.appliedReductionSnapshot();
            for (int b = 0; b < bins; ++b) if (std::abs(snap[(size_t) b] - reductionOut[(size_t) b]) > 1e-9f) identical = false;
            ++framesChecked;
        }
        std::printf("  frames checked: %d, bit-identical every frame: %s\n", framesChecked, identical ? "yes" : "NO");
        check("appliedReductionSnapshot() == reductionDbOut (the value actually multiplied into fftData) on every frame", identical);
    }

    // ---------------------------------------------------------------
    // ITEM 2/3/4: per-frequency table -- 1k/2k/4k/5k/6.3k/8k/10k
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 2/3/4: per-frequency table (pink noise bed -20dB-ish + tone @ -10.5dBFS, official defaults) --\n");
    const double testFreqs[] = { 1000.0, 2000.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0 };
    std::printf("  %8s | %10s | %10s | %9s | %14s | %14s | %12s\n", "freq(Hz)", "in(dB)", "out(dB)", "meas(dB)", "displayed(dB)", "peakBinHz", "|shift|(bins)");
    for (double f : testFreqs)
    {
        // Averaged over 5 independent pink-noise realizations per
        // frequency -- a SINGLE fixed realization can, by chance, carry
        // its own local energy bump near any one particular test
        // frequency (pink noise is random, not flat), which the detector
        // can legitimately treat as a second, coincidental resonance
        // riding on top of the injected tone (confirmed directly: simply
        // changing the seed moved which 2 of the 7 frequencies showed the
        // largest measured-vs-displayed gap, with no consistent frequency
        // pattern -- i.e. realization noise, not a frequency-dependent
        // DSP effect). Averaging suppresses that single-draw variance.
        const int kTrials = 5;
        double sumMeasured = 0, sumDisplayed = 0, sumIn = 0, sumOut = 0, sumPeakHz = 0; int maxShift = 0;
        for (int trial = 0; trial < kTrials; ++trial)
        {
            auto sig = genPinkNoise(n, 0.05f, (unsigned) (1000 + trial * 97 + (int) f)); addTone(sig, sr, f, 0.3f);
            auto r = runEngine(sig, sr, defP);
            float inDb = goertzelDb(sig, measStart, measLen, sr, f);
            float outDb = goertzelDb(r.output, measStart, measLen, sr, f);
            int expectedBin = binForFreq(sr, f);
            int peakBin = expectedBin; float peakVal = 1e9f;
            int lo = juce::jmax(0, expectedBin - 20), hi = juce::jmin(r.snapshotBins - 1, expectedBin + 20);
            for (int b = lo; b <= hi; ++b) if (r.snapshot[(size_t) b] < peakVal) { peakVal = r.snapshot[(size_t) b]; peakBin = b; }
            float displayed = r.snapshot[(size_t) expectedBin];
            maxShift = juce::jmax(maxShift, std::abs(peakBin - expectedBin));
            sumMeasured += (outDb - inDb); sumDisplayed += displayed; sumIn += inDb; sumOut += outDb;
            sumPeakHz += peakBin * sr / 2048.0;
        }
        float measured = (float) (sumMeasured / kTrials), displayed = (float) (sumDisplayed / kTrials);
        float inDb = (float) (sumIn / kTrials), outDb = (float) (sumOut / kTrials);
        double peakHz = sumPeakHz / kTrials;
        std::printf("  %8.0f | %10.2f | %10.2f | %9.2f | %14.2f | %14.1f | %12d\n", f, inDb, outDb, measured, displayed, peakHz, maxShift);
        char label[64]; std::snprintf(label, sizeof(label), "%.0fHz: peak reduction lands within 2 bins (worst of %d trials)", f, kTrials);
        check(label, maxShift <= 2);
        char label2[112]; std::snprintf(label2, sizeof(label2), "%.0fHz: measured I/O diff and displayed reduction agree within 0.6dB (avg of %d trials)", f, kTrials);
        check(label2, std::abs(measured - displayed) < 0.6f);
    }

    // ---------------------------------------------------------------
    // ITEM 3 continued: cross-sample-rate bin/Hz mapping sanity for 4-8kHz
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 3: 4-8kHz peak alignment across 44.1/48/96kHz --\n");
    for (double srTest : { 44100.0, 48000.0, 96000.0 })
    {
        for (double f : { 4000.0, 5000.0, 6300.0, 8000.0 })
        {
            int nTest = (int) (srTest * 2.5);
            auto sig = genPinkNoise(nTest, 0.05f, 777); addTone(sig, srTest, f, 0.3f);
            SpectralEngine::Params p = defP;
            auto r = runEngine(sig, srTest, p);
            int expectedBin = binForFreq(srTest, f);
            int peakBin = expectedBin; float peakVal = 1e9f;
            int lo = juce::jmax(0, expectedBin - 20), hi = juce::jmin(r.snapshotBins - 1, expectedBin + 20);
            for (int b = lo; b <= hi; ++b) if (r.snapshot[(size_t) b] < peakVal) { peakVal = r.snapshot[(size_t) b]; peakBin = b; }
            int shiftBins = std::abs(peakBin - expectedBin);
            std::printf("  sr=%-7.0f f=%-6.0f expectedBin=%-5d peakBin=%-5d shift=%d\n", srTest, f, expectedBin, peakBin, shiftBins);
            char label[96]; std::snprintf(label, sizeof(label), "sr=%.0f f=%.0fHz peak within 2 bins", srTest, f);
            check(label, shiftBins <= 2);
        }
    }

    // ---------------------------------------------------------------
    // ITEM 5: DELTA -- processed + delta ~= original
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 5: DELTA reconstruction (processed + delta ~= original) --\n");
    // NOTE: SpectralEngine reports latencySamples()==fftSize (2048) -- the
    // same value PluginProcessor's own dryDelay ring compensates by (see
    // processBlock()'s `lat=spectral.latencySamples()` / `rp=(dryWrite-lat+
    // ring)%ring`) before comparing dry vs wet. A direct sample-index
    // comparison of engine OUTPUT against the RAW input must apply that
    // same shift, or a perfectly correct reconstruction reads as a large
    // "error" that is really just the declared algorithmic latency (a
    // pure 2048-sample delay, confirmed below by an initial mis-aligned
    // debug pass that measured EXACTLY a 120-degree phase offset at
    // 5000Hz -- 2048 samples * 37.5 deg/sample mod 360 = 120 deg, i.e.
    // precisely this latency, not a reconstruction bug).
    const int kLatencySamples = 2048;
    for (double f : { 5000.0, 6300.0 })
    {
        auto sig = genPinkNoise(n, 0.05f, 999); addTone(sig, sr, f, 0.3f);
        SpectralEngine::Params pProc = defP; pProc.delta = false;
        SpectralEngine::Params pDelta = defP; pDelta.delta = true;
        auto rProc = runEngine(sig, sr, pProc);
        auto rDelta = runEngine(sig, sr, pDelta);
        double maxErr = 0.0, sumSqErr = 0.0, sumSqOrig = 0.0;
        for (int i = measStart; i < measStart + measLen; ++i)
        {
            double recon = (double) rProc.output[(size_t) i] + (double) rDelta.output[(size_t) i];
            double err = recon - (double) sig[(size_t) (i - kLatencySamples)];
            maxErr = juce::jmax(maxErr, std::abs(err));
            sumSqErr += err * err; sumSqOrig += (double) sig[(size_t) (i - kLatencySamples)] * sig[(size_t) (i - kLatencySamples)];
        }
        double rmsErrDb = 10.0 * std::log10((sumSqErr + 1e-30) / (sumSqOrig + 1e-30));
        std::printf("  f=%.0fHz: maxAbsErr=%.6f, reconstruction error=%.1f dB rel. to signal\n", f, maxErr, rmsErrDb);
        char label[96]; std::snprintf(label, sizeof(label), "f=%.0fHz: processed+delta reconstructs original (<-80dB rel error)", f);
        check(label, rmsErrDb < -80.0);
        // Delta should carry mostly the removed material -- its own energy at f should be close to the measured reduction magnitude.
        float deltaAtF = goertzelDb(rDelta.output, measStart, measLen, sr, f);
        float inAtF = goertzelDb(sig, measStart, measLen, sr, f);
        float outAtF = goertzelDb(rProc.output, measStart, measLen, sr, f);
        std::printf("        in=%.2fdB out=%.2fdB delta=%.2fdB (delta should sit near the removed energy, well below 'in')\n", inAtF, outAtF, deltaAtF);
        char label2[96]; std::snprintf(label2, sizeof(label2), "f=%.0fHz: Delta signal energy at test freq is below the input (material really removed)", f);
        check(label2, deltaAtF < inAtF - 0.5f);
    }

    // ---------------------------------------------------------------
    // ITEM 6: MAX REDUCTION -- 1 / 3 / 6 dB caps, real strong-demand material
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 6: MAX REDUCTION caps (1/3/6dB), strong-demand material @5kHz --\n");
    for (float cap : { 1.0f, 3.0f, 6.0f })
    {
        auto sig = genPinkNoise(n, 0.06f, 4242); addTone(sig, sr, 5000.0, 0.5f); // strong tone, high demand
        SpectralEngine::Params p = defP; p.depth = 9.0f; p.maxReductionEnabled = true; p.maxReductionDb = cap;
        auto r = runEngine(sig, sr, p);
        float inDb = goertzelDb(sig, measStart, measLen, sr, 5000.0);
        float outDb = goertzelDb(r.output, measStart, measLen, sr, 5000.0);
        float measured = outDb - inDb;
        float minSnap = *std::min_element(r.snapshot.begin(), r.snapshot.begin() + r.snapshotBins);
        std::printf("  cap=%.1fdB: measuredIO=%.2fdB, min(analyzer)=%.2fdB\n", cap, measured, minSnap);
        char l1[64]; std::snprintf(l1, sizeof(l1), "cap=%.1fdB: measured I/O never exceeds cap (+0.75dB float/window tol.)", cap);
        check(l1, measured >= -cap - 0.75f);
        char l2[64]; std::snprintf(l2, sizeof(l2), "cap=%.1fdB: analyzer's own min never exceeds cap (+0.1dB tol.)", cap);
        check(l2, minSnap >= -cap - 0.1f);
    }

    // ---------------------------------------------------------------
    // ITEM 7: LOW/HIGH range -- 6kHz inside vs outside
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 7: LOW/HIGH range gating @6kHz --\n");
    {
        // Stronger demand (Depth=7, louder tone) than the default-Depth=3
        // table above -- item 7 asks a yes/no gating question (does the
        // range gate work at all), so the test material should make any
        // real suppression unambiguous rather than borderline.
        auto sig = genPinkNoise(n, 0.05f, 5150); addTone(sig, sr, 6000.0, 0.4f);
        SpectralEngine::Params pIn = defP; pIn.depth = 7.0f; pIn.lowHz = 3000.0f; pIn.highHz = 9000.0f;
        SpectralEngine::Params pOut = defP; pOut.depth = 7.0f; pOut.lowHz = 200.0f; pOut.highHz = 1000.0f;
        auto rIn = runEngine(sig, sr, pIn);
        auto rOut = runEngine(sig, sr, pOut);
        float inDb = goertzelDb(sig, measStart, measLen, sr, 6000.0);
        float outInsideDb = goertzelDb(rIn.output, measStart, measLen, sr, 6000.0);
        float outOutsideDb = goertzelDb(rOut.output, measStart, measLen, sr, 6000.0);
        std::printf("  input=%.2fdB, output(6k inside [3k,9k])=%.2fdB (delta=%.2f), output(6k outside [200,1k])=%.2fdB (delta=%.2f)\n",
                    inDb, outInsideDb, outInsideDb - inDb, outOutsideDb, outOutsideDb - inDb);
        check("6kHz INSIDE [3k,9k]: measurable reduction occurs", (outInsideDb - inDb) < -0.5f);
        check("6kHz OUTSIDE [200,1k]: no reduction (within 0.3dB, engine still applies unity elsewhere)", std::abs(outOutsideDb - inDb) < 0.3f);

        // Smooth entry/exit: sweep the block across 6kHz in discrete steps
        // (simulating a user dragging LOW/HIGH) and confirm the analyzer's
        // own reduction value at 6kHz never jumps by more than a few dB
        // between adjacent steps -- no click/step discontinuity as the
        // region boundary crosses the test frequency.
        std::printf("  -- sweep: HIGH bound moving across 6kHz, step-to-step jump in analyzer reduction @6kHz --\n");
        float prevRed = 0.0f; bool first = true; float maxJump = 0.0f;
        for (float highBound : { 5000.0f, 5500.0f, 5800.0f, 5950.0f, 6000.0f, 6050.0f, 6200.0f, 6500.0f, 7000.0f })
        {
            SpectralEngine::Params p = defP; p.depth = 7.0f; p.lowHz = 200.0f; p.highHz = highBound;
            auto r = runEngine(sig, sr, p);
            int bin6k = binForFreq(sr, 6000.0);
            float red = r.snapshot[(size_t) bin6k];
            std::printf("    HIGH=%.0fHz -> reduction@6kHz=%.3fdB\n", highBound, red);
            if (!first) maxJump = juce::jmax(maxJump, std::abs(red - prevRed));
            prevRed = red; first = false;
        }
        std::printf("  max step-to-step jump = %.3fdB\n", maxJump);
        check("HIGH sweeping across 6kHz: no abrupt jump (>6dB) between adjacent steps", maxJump < 6.0f);
    }

    // ---------------------------------------------------------------
    // ITEM 8a: confirm Sharpness was inert BEFORE the fix (documented, not re-tested --
    // the fix already landed in GainMaskEngine.cpp/.h). Prove it now has real effect.
    // ---------------------------------------------------------------
    std::printf("\n-- ITEM 8: SHARPNESS now controls suppression width (was inert, now wired to SpectralProminenceEngineV5's own narrow/medium/broad blend) --\n");
    {
        auto measureWidthOct = [&](float sharpnessVal) -> float
        {
            auto sig = genPinkNoise(n, 0.05f, 3131); addTone(sig, sr, 5000.0, 0.3f);
            SpectralEngine::Params p = defP; p.depth = 6.0f; p.sharpness = sharpnessVal;
            auto r = runEngine(sig, sr, p);
            int centerBin = binForFreq(sr, 5000.0);
            const float half = 0.5f;
            int loBin = centerBin, hiBin = centerBin;
            while (loBin > 0 && r.snapshot[(size_t) loBin] < -half) --loBin;
            while (hiBin < r.snapshotBins - 1 && r.snapshot[(size_t) hiBin] < -half) ++hiBin;
            float loHz = juce::jmax(1.0f, loBin * (float) sr / 2048.0f), hiHz = hiBin * (float) sr / 2048.0f;
            return std::log2(hiHz / loHz);
        };
        // Sigma multiplier is a closed-form pow(2, k*(4-s)/10) -- provably
        // strictly monotonic decreasing in s. Sampled densely (every
        // integer 0..10) to verify that holds all the way through the
        // actual per-region envelope, not just in the formula alone.
        const int kN = 11; float widths[kN]; float sharpVals[kN];
        for (int i = 0; i < kN; ++i) { sharpVals[i] = (float) i; widths[i] = measureWidthOct(sharpVals[i]); }
        for (int i = 0; i < kN; ++i)
            std::printf("  Sharpness=%-4.1f: width=%.3f octaves%s\n", sharpVals[i], widths[i], std::abs(sharpVals[i] - 4.0f) < 1.0e-3f ? " (official default)" : "");
        check("Sharpness=0 produces a WIDER suppression envelope than Sharpness=10", widths[0] > widths[10]);
        check("Sharpness now measurably changes suppression width (was completely inert before this fix)", widths[0] != widths[10]);
        bool strictlyMonotonic = true;
        for (int i = 1; i < kN; ++i) if (widths[i] > widths[i - 1] + 1.0e-4f) strictlyMonotonic = false;
        check("Sharpness 0..10 is strictly non-increasing in width at every integer step (no region where raising it widens the action)", strictlyMonotonic);
    }

    std::printf("\n%s\n", allPass ? "=== ALL AUDIT CHECKS PASS ===" : "=== SOME AUDIT CHECKS FAILED -- SEE ABOVE ===");
    return allPass ? 0 : 1;
}
