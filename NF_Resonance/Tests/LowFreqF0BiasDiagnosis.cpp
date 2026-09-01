// PHYSICAL C, Blocker 2 -- ROOT-CAUSE diagnosis of the -45..-115 cents
// systematic negative F0 bias found by LowFreqHarmonicAnalyzerCheck.
// Diagnostic/offline only. No production behaviour is changed by this file
// (the two debug* accessors added to LowFrequencyHarmonicAnalyzer are
// read-only and do not alter any computation).
//
// Reports errorHz/errorCents/errorInBins SEPARATELY (never converts cents
// directly into "fraction of a bin" -- errorInBins = errorHz / binWidthHz,
// computed independently) at four pipeline stages:
//   A -- raw signal, pre-decimation, host-rate FFT (ground truth if no
//        decimation/analyzer pipeline existed at all)
//   B/C -- post-decimation/anti-alias magnitude (production's own
//        debugMagDb(), i.e. exactly what feeds computeProminence())
//   D -- post-prominence-transform (production's own debugProminence(),
//        i.e. exactly what the current peak-picker in runAnalysisFrame()
//        actually interpolates over today)
// and compares parabolic interpolation on LINEAR magnitude vs LOG/dB
// magnitude at each stage where that distinction applies (item 8).

#include <JuceHeader.h>
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

static std::vector<float> genSine(double sr, int n, double freq, float amp)
{
    std::vector<float> b((size_t) n, 0.0f);
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s = (float) std::sin(ph) * amp; ph += inc; }
    return b;
}
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb = 3.0f)
{
    std::vector<float> b((size_t) n, 0.0f);
    for (int h = 1; h <= numH; ++h)
    {
        double ph = 0.0, inc = juce::MathConstants<double>::twoPi * (f0 * h) / sr;
        float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDb * (h - 1));
        for (auto& s : b) { s += (float) std::sin(ph) * a; ph += inc; }
    }
    return b;
}

static float parabolicDelta(float l, float c, float r)
{
    float denom = l - 2.0f * c + r;
    if (std::abs(denom) < 1.0e-9f) return 0.0f;
    return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom);
}

// Search [loBin,hiBin] for the local-max bin, interpolate in BOTH linear
// magnitude and dB/log-power domain from the SAME three points (so the two
// methods are directly comparable, not confounded by different peak picks).
struct PeakEstimate { bool found = false; int bin = -1; float estHzLinear = 0, estHzLog = 0; };
static PeakEstimate findAndInterpolate(const std::vector<float>& linMag, double binHz, int loBin, int hiBin)
{
    PeakEstimate pe;
    int bins = (int) linMag.size();
    loBin = juce::jmax(1, loBin); hiBin = juce::jmin(bins - 2, hiBin);
    int bestBin = -1; float bestVal = -1.0f;
    for (int b = loBin; b <= hiBin; ++b) if (linMag[(size_t) b] > bestVal) { bestVal = linMag[(size_t) b]; bestBin = b; }
    if (bestBin < 0) return pe;
    pe.found = true; pe.bin = bestBin;
    float l = linMag[(size_t) (bestBin - 1)], c = linMag[(size_t) bestBin], r = linMag[(size_t) (bestBin + 1)];
    float deltaLin = parabolicDelta(l, c, r);
    pe.estHzLinear = (float) ((bestBin + deltaLin) * binHz);
    float ldb = 20.0f * std::log10(juce::jmax(1.0e-9f, l)), cdb = 20.0f * std::log10(juce::jmax(1.0e-9f, c)), rdb = 20.0f * std::log10(juce::jmax(1.0e-9f, r));
    float deltaLog = parabolicDelta(ldb, cdb, rdb);
    pe.estHzLog = (float) ((bestBin + deltaLog) * binHz);
    return pe;
}
// Same, but interpolating directly on an ALREADY-dB (or prominence, which
// is dB-like) array -- this is what production's runAnalysisFrame() does
// today at stage D.
static PeakEstimate findAndInterpolateDbArray(const std::array<float, LowFrequencyHarmonicAnalyzer::kAnalysisFftSize / 2 + 1>& dbArr, double binHz, int loBin, int hiBin, float floorDb)
{
    PeakEstimate pe;
    int bins = (int) dbArr.size();
    loBin = juce::jmax(1, loBin); hiBin = juce::jmin(bins - 2, hiBin);
    int bestBin = -1; float bestVal = -1.0e9f;
    for (int b = loBin; b <= hiBin; ++b) if (dbArr[(size_t) b] > bestVal) { bestVal = dbArr[(size_t) b]; bestBin = b; }
    if (bestBin < 0 || bestVal < floorDb) return pe;
    pe.found = true; pe.bin = bestBin;
    float delta = parabolicDelta(dbArr[(size_t) (bestBin - 1)], dbArr[(size_t) bestBin], dbArr[(size_t) (bestBin + 1)]);
    pe.estHzLog = (float) ((bestBin + delta) * binHz);
    pe.estHzLinear = pe.estHzLog; // no separate linear variant for an already-dB-only array
    return pe;
}

struct StageReport { const char* name; float trueHz, estHz, errorHz, errorCents, binWidthHz, errorInBins; };
static StageReport makeReport(const char* name, float trueHz, float estHz, double binWidthHz)
{
    float errHz = estHz - trueHz;
    float errCents = (trueHz > 0 && estHz > 0) ? 1200.0f * std::log2(estHz / trueHz) : 9999.0f;
    return { name, trueHz, estHz, errHz, errCents, (float) binWidthHz, errHz / (float) binWidthHz };
}
static void printReport(const StageReport& r)
{
    std::printf("    %-18s trueHz=%8.3f estHz=%8.3f errorHz=%+8.3f errorCents=%+7.1f binWidthHz=%7.3f errorInBins=%+6.3f\n",
        r.name, r.trueHz, r.estHz, r.errorHz, r.errorCents, r.binWidthHz, r.errorInBins);
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double testFreqs[] = { 60, 80, 100, 120, 160, 200, 250, 400 };
    const int kFft = LowFrequencyHarmonicAnalyzer::kAnalysisFftSize;

    std::printf("=== 1/2. PURE-TONE ISOLATION: stage-by-stage F0 estimate, separated units ===\n");
    std::printf("(A = pre-decimation host-rate FFT; B/C = post-decimation raw magnitude (production's own debugMagDb());\n");
    std::printf(" D = post-prominence-transform (production's own debugProminence(), what today's peak-picker actually uses))\n\n");

    // accumulate stats for the aggregate bias summary at the end
    struct Row { double sr; float trueHz; StageReport a_lin, a_log, bc_lin, bc_log, d; };
    std::vector<Row> allRows;

    for (double sr : rates)
    {
        std::printf(" -- Sample rate %.0f Hz --\n", sr);
        for (double f : testFreqs)
        {
            int hostSettle = (int) (sr * 0.5); // 0.5s settle, well past filter warmup and ring fill
            auto hostSig = genSine(sr, hostSettle, f, 0.4f);

            LowFrequencyHarmonicAnalyzer az;
            az.prepare(sr);
            const int block = 512;
            for (int i = 0; i < hostSettle; i += block)
                az.pushSamples(hostSig.data() + i, juce::jmin(block, hostSettle - i));

            double decRate = az.analysisRate();
            double decBinHz = az.analysisBinHz();
            double hostBinHz = sr / kFft;

            // Stage A: last kFft host-rate samples, windowed identically (Hann, 0.5-0.5cos over N-1), own FFT at HOST rate.
            std::vector<float> winA((size_t) kFft);
            for (int i = 0; i < kFft; ++i) winA[(size_t) i] = hostSig[(size_t) (hostSettle - kFft + i)] * (0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (kFft - 1)));
            std::vector<float> scratchA((size_t) (kFft * 2), 0.0f);
            std::copy(winA.begin(), winA.end(), scratchA.begin());
            juce::dsp::FFT fftA(11);
            fftA.performRealOnlyForwardTransform(scratchA.data());
            int binsCount = kFft / 2 + 1;
            std::vector<float> magA((size_t) binsCount);
            for (int b = 0; b < binsCount; ++b)
            {
                float re = scratchA[(size_t) (2 * b)], im = (b == 0 || b == binsCount - 1) ? 0.0f : scratchA[(size_t) (2 * b + 1)];
                magA[(size_t) b] = std::sqrt(re * re + im * im) / (float) kFft;
            }
            int expectedBinA = (int) std::round(f / hostBinHz);
            auto peA = findAndInterpolate(magA, hostBinHz, expectedBinA - 2, expectedBinA + 2);

            // Stage B/C: production's own post-decimation magnitude (debugMagDb(), dB already).
            int expectedBinBC = (int) std::round(f / decBinHz);
            auto peBC_db = findAndInterpolateDbArray(az.debugMagDb(), decBinHz, expectedBinBC - 2, expectedBinBC + 2, -200.0f);
            // also reconstruct linear magnitude from the dB array for the linear-domain comparison (item 8)
            std::vector<float> magBC_lin(az.debugMagDb().size());
            for (size_t b = 0; b < magBC_lin.size(); ++b) magBC_lin[b] = juce::Decibels::decibelsToGain(az.debugMagDb()[b]);
            auto peBC_lin = findAndInterpolate(magBC_lin, decBinHz, expectedBinBC - 2, expectedBinBC + 2);

            // Stage D: production's own post-prominence array (what today's peak-picker uses).
            auto peD = findAndInterpolateDbArray(az.debugProminence(), decBinHz, expectedBinBC - 2, expectedBinBC + 2, 0.0f);

            Row row{ sr, (float) f,
                makeReport("A(host,linear)", (float) f, peA.estHzLinear, hostBinHz),
                makeReport("A(host,log)",    (float) f, peA.estHzLog,    hostBinHz),
                makeReport("B/C(dec,linear)",(float) f, peBC_lin.found ? peBC_lin.estHzLinear : 0.0f, decBinHz),
                makeReport("B/C(dec,log)",   (float) f, peBC_db.found  ? peBC_db.estHzLog     : 0.0f, decBinHz),
                makeReport("D(prominence)",  (float) f, peD.found      ? peD.estHzLog          : 0.0f, decBinHz) };
            std::printf("  f=%.0fHz (decimation=%dx, decBinHz=%.3f, hostBinHz=%.3f):\n", f, az.decimationFactor(), decBinHz, hostBinHz);
            printReport(row.a_lin); printReport(row.a_log); printReport(row.bc_lin); printReport(row.bc_log); printReport(row.d);
            allRows.push_back(row);
        }
    }

    // ---------------- aggregate bias summary (item 8/10) ----------------
    std::printf("\n=== AGGREGATE BIAS SUMMARY (mean errorInBins across all pure-tone cases above) ===\n");
    auto meanErrInBins = [&](auto getter) {
        double sum = 0; int cnt = 0; for (auto& r : allRows) { sum += getter(r).errorInBins; ++cnt; } return cnt ? sum / cnt : 0.0;
    };
    auto allNeg = [&](auto getter) {
        for (auto& r : allRows) if (getter(r).errorInBins > 0.02f) return false; return true;
    };
    std::printf("  A (host, linear mag):        mean errorInBins=%+.4f  all-negative=%s\n", meanErrInBins([](const Row& r){ return r.a_lin; }), allNeg([](const Row& r){ return r.a_lin; }) ? "yes" : "no");
    std::printf("  A (host, log/dB mag):        mean errorInBins=%+.4f  all-negative=%s\n", meanErrInBins([](const Row& r){ return r.a_log; }), allNeg([](const Row& r){ return r.a_log; }) ? "yes" : "no");
    std::printf("  B/C (decimated, linear mag): mean errorInBins=%+.4f  all-negative=%s\n", meanErrInBins([](const Row& r){ return r.bc_lin; }), allNeg([](const Row& r){ return r.bc_lin; }) ? "yes" : "no");
    std::printf("  B/C (decimated, log/dB mag): mean errorInBins=%+.4f  all-negative=%s\n", meanErrInBins([](const Row& r){ return r.bc_log; }), allNeg([](const Row& r){ return r.bc_log; }) ? "yes" : "no");
    std::printf("  D (prominence, production):  mean errorInBins=%+.4f  all-negative=%s\n", meanErrInBins([](const Row& r){ return r.d; }), allNeg([](const Row& r){ return r.d; }) ? "yes" : "no");

    // ---------------- 5. continuous sweep 50-300Hz, Stage B/C log vs Stage D ----------------
    std::printf("\n=== 5. CONTINUOUS SWEEP 50->300Hz (step 2Hz): bin-phase behaviour, Stage B/C(log) vs Stage D(prominence) ===\n");
    for (double sr : rates)
    {
        LowFrequencyHarmonicAnalyzer azProbe; azProbe.prepare(sr);
        double decBinHz = azProbe.analysisBinHz();
        double sumBC = 0, sumD = 0; int cnt = 0; float minBC = 1e9f, maxBC = -1e9f, minD = 1e9f, maxD = -1e9f;
        int negCountBC = 0, negCountD = 0;
        // near-DC bin (bin index < 3) vs higher-bin behaviour, tracked separately
        double sumBC_nearDC = 0; int cntNearDC = 0; double sumBC_far = 0; int cntFar = 0;
        for (double f = 50.0; f <= 300.0; f += 2.0)
        {
            int hostSettle = (int) (sr * 0.4);
            auto hostSig = genSine(sr, hostSettle, f, 0.4f);
            LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
            const int block = 512;
            for (int i = 0; i < hostSettle; i += block) az.pushSamples(hostSig.data() + i, juce::jmin(block, hostSettle - i));
            int expectedBin = (int) std::round(f / decBinHz);
            std::vector<float> magLin(az.debugMagDb().size());
            for (size_t b = 0; b < magLin.size(); ++b) magLin[b] = juce::Decibels::decibelsToGain(az.debugMagDb()[b]);
            auto peBC = findAndInterpolate(magLin, decBinHz, expectedBin - 2, expectedBin + 2);
            auto peD = findAndInterpolateDbArray(az.debugProminence(), decBinHz, expectedBin - 2, expectedBin + 2, 0.0f);
            if (! peBC.found || ! peD.found) continue;
            float errBC = (peBC.estHzLog - (float) f) / (float) decBinHz;
            float errD = (peD.estHzLog - (float) f) / (float) decBinHz;
            sumBC += errBC; sumD += errD; ++cnt;
            minBC = juce::jmin(minBC, errBC); maxBC = juce::jmax(maxBC, errBC);
            minD = juce::jmin(minD, errD); maxD = juce::jmax(maxD, errD);
            if (errBC < -0.02f) ++negCountBC;
            if (errD < -0.02f) ++negCountD;
            if (expectedBin <= 4) { sumBC_nearDC += errBC; ++cntNearDC; } else { sumBC_far += errBC; ++cntFar; }
        }
        std::printf("  %.0fHz (decBinHz=%.3f): B/C errorInBins mean=%+.4f min=%+.4f max=%+.4f negative=%d/%d | D errorInBins mean=%+.4f min=%+.4f max=%+.4f negative=%d/%d\n",
            sr, decBinHz, sumBC/cnt, minBC, maxBC, negCountBC, cnt, sumD/cnt, minD, maxD, negCountD, cnt);
        std::printf("      B/C near-DC (bin<=4) mean errorInBins=%+.4f (n=%d) vs far-from-DC mean=%+.4f (n=%d)\n",
            cntNearDC ? sumBC_nearDC/cntNearDC : 0.0, cntNearDC, cntFar ? sumBC_far/cntFar : 0.0, cntFar);
    }

    // ---------------- 6. DC blocker ----------------
    std::printf("\n=== 6. DC BLOCKER ===\n");
    std::printf("  grep of Source/ found no DC-blocking stage anywhere in this codebase (main engine or\n");
    std::printf("  LowFrequencyHarmonicAnalyzer's own pushSamples() path). There is nothing to toggle --\n");
    std::printf("  this item is N/A for THIS component; the bias, if present, is not coming from a DC blocker.\n");

    // ---------------- 7. Decimator: pre- vs post-decimation shift ----------------
    std::printf("\n=== 7. DECIMATOR: does anti-alias/decimation itself shift the estimate (96/192kHz only, decimation>1)? ===\n");
    for (double sr : { 96000.0, 192000.0 })
    {
        for (double f : testFreqs)
        {
            int hostSettle = (int) (sr * 0.5);
            auto hostSig = genSine(sr, hostSettle, f, 0.4f);
            LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
            const int block = 512;
            for (int i = 0; i < hostSettle; i += block) az.pushSamples(hostSig.data() + i, juce::jmin(block, hostSettle - i));
            double decBinHz = az.analysisBinHz();
            int expectedBin = (int) std::round(f / decBinHz);
            std::vector<float> magLin(az.debugMagDb().size());
            for (size_t b = 0; b < magLin.size(); ++b) magLin[b] = juce::Decibels::decibelsToGain(az.debugMagDb()[b]);
            auto peBC = findAndInterpolate(magLin, decBinHz, expectedBin - 2, expectedBin + 2);
            float errInBinsAfter = (peBC.estHzLog - (float) f) / (float) decBinHz;
            std::printf("  f=%.0fHz sr=%.0f dec=%dx: post-decimation errorInBins=%+.4f (compare against Stage-A host-rate value printed in section 1/2 above for the same f/sr)\n",
                f, sr, az.decimationFactor(), errInBinsAfter);
        }
    }

    // ---------------- 9. Harmonic series: F0 candidate table ----------------
    std::printf("\n=== 9. HARMONIC SERIES: F0 candidate scoring table (80Hz+harmonics, 120Hz+harmonics) ===\n");
    for (double sr : rates)
    {
        for (double f0 : { 80.0, 120.0 })
        {
            int n = (int) (sr * 1.0);
            auto sig = genHarmonicSeries(sr, n, f0, 0.3f, 6);
            LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
            const int block = 512;
            for (int i = 0; i < n; i += block) az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
            auto ctx = az.currentContext();
            std::printf(" -- sr=%.0f f0=%.0fHz+harmonics: winner f0Hz=%.3f partials=%d score=%.3f conf=%.3f | %d peaks detected\n",
                sr, f0, ctx.f0Hz, ctx.supportingPartials, ctx.f0Score, ctx.f0Confidence, az.debugNumPeaks());
            for (auto& c : az.debugF0Candidates())
                if (c.active) std::printf("      candidateHz=%8.3f supportingPartials=%d aggregateEvidence=%.3f\n", c.centerHz, c.matches, c.evidence);
        }
    }

    // ---------------- 10. Regression dump: Case B @ 44.1kHz full peak/candidate list ----------------
    std::printf("\n=== 10. REGRESSION DUMP: 80Hz+harmonics+135Hz-non-harmonic @ 44.1kHz, full peak list ===\n");
    {
        double sr = 44100.0;
        int n = (int) (sr * 1.5);
        auto sig = genHarmonicSeries(sr, n, 80.0, 0.3f, 6);
        {
            juce::Random rng(1);
            double bwHz = 135.0 / 8.0;
            for (int k = 0; k < 9; ++k)
            {
                double t = (double) k / 8.0 - 0.5;
                double f = 135.0 + t * bwHz;
                double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
                for (int i = 0; i < n; ++i) { sig[(size_t) i] += (float) std::sin(ph) * (0.5f / 3.0f); ph += inc; }
            }
        }
        LowFrequencyHarmonicAnalyzer az; az.prepare(sr);
        const int block = 512;
        for (int i = 0; i < n; i += block) az.pushSamples(sig.data() + i, juce::jmin(block, n - i));
        double decBinHz = az.analysisBinHz();
        std::printf("  decBinHz=%.4f numPeaks=%d\n", decBinHz, az.debugNumPeaks());
        std::printf("  -- raw magnitude peaks near expected harmonics (magDb) --\n");
        for (double expected : { 80.0, 135.0, 160.0, 240.0, 320.0, 400.0, 480.0 })
        {
            int b = (int) std::round(expected / decBinHz);
            std::printf("    near %.0fHz (bin %d): magDb[b-1..b+1]=%.2f/%.2f/%.2f  promDb[b-1..b+1]=%.2f/%.2f/%.2f\n",
                expected, b, az.debugMagDb()[(size_t) (b-1)], az.debugMagDb()[(size_t) b], az.debugMagDb()[(size_t) (b+1)],
                az.debugProminence()[(size_t) (b-1)], az.debugProminence()[(size_t) b], az.debugProminence()[(size_t) (b+1)]);
        }
        std::printf("  -- F0 candidates --\n");
        for (auto& c : az.debugF0Candidates())
            if (c.active) std::printf("    candidateHz=%8.3f supportingPartials=%d aggregateEvidence=%.3f\n", c.centerHz, c.matches, c.evidence);
        auto ctx = az.currentContext();
        std::printf("  winner: f0Hz=%.3f partials=%d score=%.3f conf=%.3f\n", ctx.f0Hz, ctx.supportingPartials, ctx.f0Score, ctx.f0Confidence);
    }

    std::printf("\nDiagnosis complete -- see stage-by-stage tables above to identify where the bias enters.\n");
    return 0;
}
