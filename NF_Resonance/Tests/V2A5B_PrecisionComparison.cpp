// V2-A5B precision comparison: Method A (O1_LeftRightInterp) and Method C
// (O1_RobustSideSlope) against the P25 offline oracle (SpectralProminenceEngineV2
// computeProminenceFullBinReference), on a corrected synthetic generator.
// CPU is NOT measured here (that was V2A5CpuIsolationBenchmark.cpp, at
// 192kHz/2048/512). This harness is precision-only, evaluated primarily at
// 48kHz and secondarily at 44.1/96/192kHz. No production files touched.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/SpectralProminenceEngineV2.h"
#include <algorithm>

using NM = SpectralProminenceEngineV5::NarrowMethod;

//==============================================================================
// Corrected synthetic generator (item: "corrija definitivamente o gerador de
// tilt"). DC handled via an explicit frequency floor -- log() is NEVER
// evaluated at hz=0, and nothing approaches +-inf. baseline(hz) below the
// floor is pinned to baseline(floor), not extrapolated, so bin 0 never
// carries an artificial spike. "Curved" tilt is a bounded parabola in
// log-frequency, not a runaway slope.
struct TiltSpec { const char* name; double dbPerOctave; double curveDb; };
static const TiltSpec kTilts[] = {
    { "flat",         0.0, 0.0 },
    { "-3dB/oct",    -3.0, 0.0 },
    { "+3dB/oct",    +3.0, 0.0 },
    { "curved",      -1.5, 6.0 },
};
static constexpr double kFloorHz = 30.0;   // frequency floor -- nothing below this is ever extrapolated
static constexpr double kRefHz   = 1000.0;
static constexpr double kRefDb   = -30.0;

static double analyticBaselineDb(double hz, const TiltSpec& t)
{
    double hzC = juce::jmax(kFloorHz, hz);
    double oct = std::log2(hzC / kRefHz);
    double db = kRefDb + t.dbPerOctave * oct;
    if (t.curveDb != 0.0)
    {
        // bounded parabola over the 20Hz..22kHz log-span, peak deviation curveDb at the ends
        double lo = std::log2(20.0 / kRefHz), hi = std::log2(22000.0 / kRefHz);
        double u = juce::jlimit(0.0, 1.0, (oct - lo) / (hi - lo));
        db += t.curveDb * (u - 0.5) * (u - 0.5) * 4.0;
    }
    return db;
}
static std::vector<float> genBaseline(int bins, double sr, int fftSize, const TiltSpec& t)
{
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i)
    {
        double hz = (double) i * sr / fftSize; // bin 0 => hz=0, goes through the floor in analyticBaselineDb, no log(0)
        v[(size_t) i] = (float) analyticBaselineDb(hz, t);
    }
    return v;
}
// Injects a log-Gaussian peak of `amplitudeDb` at the bin nearest `centerHz`,
// snapped so the discretely-added amount at that bin is exactly amplitudeDb
// (no off-bin discretization error in the "true prominence" ground truth).
// Returns {actualCenterBin, actualCenterHz, widthHz(FWHM approx)}.
struct InjectInfo { int bin; double hz; double widthHz; };
static InjectInfo injectLogGaussian(std::vector<float>& magDb, double sr, int fftSize, double targetHz, double amplitudeDb, double q)
{
    int bins = (int) magDb.size();
    int centerBin = juce::jlimit(1, bins - 1, (int) std::round(targetHz * fftSize / sr));
    double centerHz = juce::jmax(1.0, (double) centerBin * sr / fftSize);
    double widthOct = (1.0 / q) / std::log(2.0); // Q -> octave sigma-ish width, small-angle approx
    double centerLog = std::log2(centerHz);
    for (int i = 0; i < bins; ++i)
    {
        double hz = juce::jmax(1.0, (double) i * sr / fftSize);
        double d = (std::log2(hz) - centerLog) / juce::jmax(1e-6, widthOct);
        magDb[(size_t) i] += (float) (amplitudeDb * std::exp(-0.5 * d * d));
    }
    return { centerBin, centerHz, centerHz / q };
}

static double toDb(double lin) { return 20.0 * std::log10(juce::jmax(lin, 1e-12)); }
static double median(std::vector<double> v) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); size_t n = v.size(); return n % 2 ? v[n/2] : 0.5*(v[n/2-1]+v[n/2]); }
static double percentile(std::vector<double> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); double idx = p/100.0*(double)(v.size()-1); size_t lo=(size_t)idx; size_t hi=juce::jmin(v.size()-1,lo+1); double f=idx-(double)lo; return v[lo]+(v[hi]-v[lo])*f; }

//==============================================================================
struct Engines
{
    SpectralProminenceEngineV5 a, c;
    SpectralProminenceEngineV2 oracle;
    int bins; double sr; int fftSize;
    void prepare(int binsIn, double srIn, int fftIn)
    {
        bins = binsIn; sr = srIn; fftSize = fftIn;
        a.prepare(bins, sr, fftSize); a.setNarrowMethod(NM::O1_LeftRightInterp);
        c.prepare(bins, sr, fftSize); c.setNarrowMethod(NM::O1_RobustSideSlope);
        oracle.prepare(bins, sr, fftSize);
    }
    // sharpness=10 isolates pure NARROW (wN=1,wM=wB=0) for A/C -- what we're
    // actually comparing. Oracle mirrors that with the same sharpness.
    float queryA(const std::vector<float>& magDb, int bin, float sharpness = 10.0f)      { std::vector<float> out; a.computeProminence(magDb, sharpness, out); return out[(size_t) bin]; }
    float queryC(const std::vector<float>& magDb, int bin, float sharpness = 10.0f)      { std::vector<float> out; c.computeProminence(magDb, sharpness, out); return out[(size_t) bin]; }
    float queryOracle(const std::vector<float>& magDb, int bin, float sharpness = 10.0f) { std::vector<float> out; oracle.computeProminenceFullBinReference(magDb, sharpness, out); return out[(size_t) bin]; }
};

static bool isResolutionLimited(double widthHz, double sr, int fftSize)
{
    double binHz = sr / fftSize;
    return widthHz < binHz;
}

//==============================================================================
struct ErrorSample { double trueDb, oracleV, aV, cV; double errA, errC, errOracle; double freqHz; bool resLimited; };

static void printErrStats(const char* label, std::vector<double>& absErr, std::vector<double>& signedErr)
{
    double mae = 0; for (double e : absErr) mae += e; mae /= juce::jmax((size_t) 1, absErr.size());
    double bias = 0; for (double e : signedErr) bias += e; bias /= juce::jmax((size_t) 1, signedErr.size());
    std::printf("    %-8s MAE=%.3f  medianAE=%.3f  P95=%.3f  max=%.3f  bias=%+.3f  (n=%zu)\n",
                label, mae, median(absErr), percentile(absErr, 95), absErr.empty() ? 0.0 : *std::max_element(absErr.begin(), absErr.end()), bias, absErr.size());
}

//==============================================================================
int main()
{
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    std::printf("================================================================\n");
    std::printf("V2-A5B precision comparison: A (O1_LeftRightInterp) vs C (O1_RobustSideSlope) vs P25 oracle\n");
    std::printf("Primary SR: 48000 Hz. Secondary: 44100 / 96000 / 192000 Hz. FFT=%d.\n", fftSize);
    std::printf("================================================================\n\n");

    struct SrSpec { double sr; const char* label; bool detailed; };
    std::vector<SrSpec> srs = { { 48000.0, "48kHz (PRIMARY)", true }, { 44100.0, "44.1kHz", false }, { 96000.0, "96kHz", false }, { 192000.0, "192kHz", false } };

    const double dbLevels[] = { 2, 4, 8, 12, 18 };
    const double freqs[] = { 120, 250, 1000, 4000, 10000, 16000 };
    const double q = 12.0; // moderately narrow test resonance (realistic, not artificially thin)

    for (auto& srSpec : srs)
    {
        double sr = srSpec.sr;
        double binHz = sr / fftSize;
        std::printf("\n================ SAMPLE RATE: %s  (%.4f Hz/bin) ================\n", srSpec.label, binHz);

        Engines eng; eng.prepare(bins, sr, fftSize);

        // ---- Baseline verification (numeric, BEFORE any injection) ----
        std::printf("-- Baseline verification (estimator readback vs analytic baseline, no injection) --\n");
        for (auto& t : kTilts)
        {
            auto base = genBaseline(bins, sr, fftSize, t);
            std::vector<float> outA; eng.a.computeProminence(base, 10.0f, outA);
            double worstDev = 0;
            for (int testBin : { (int) std::round(120.0*fftSize/sr), (int) std::round(1000.0*fftSize/sr), (int) std::round(8000.0*fftSize/sr) })
            {
                if (testBin < 1 || testBin >= bins) continue;
                worstDev = juce::jmax(worstDev, std::abs((double) outA[(size_t) testBin])); // prominence of a flat/tilted baseline (no injection) should read ~0
            }
            std::printf("    %-10s max |prominence| on un-injected baseline at test bins: %.4f dB (expect ~0)\n", t.name, worstDev);
        }

        if (!srSpec.detailed)
        {
            std::printf("-- (secondary SR: condensed run, flat + curved tilts only) --\n");
        }

        // ---- Group 1: synthetic exact baseline, dB x freq sweep ----
        std::vector<ErrorSample> allSamples;
        for (auto& t : kTilts)
        {
            if (!srSpec.detailed && juce::String(t.name) != "flat" && juce::String(t.name) != "curved") continue;
            auto base0 = genBaseline(bins, sr, fftSize, t);
            std::printf("\n-- Tilt: %s --\n", t.name);
            if (srSpec.detailed)
                std::printf("  %-6s %-6s | %-8s %-8s %-8s %-8s | %-8s %-8s %-6s\n", "freq", "dB", "true", "oracle", "A", "C", "errA", "errC", "RESLIM");
            for (double f : freqs)
            {
                for (double db : dbLevels)
                {
                    auto frame = base0;
                    auto info = injectLogGaussian(frame, sr, fftSize, f, db, q);
                    bool resLim = isResolutionLimited(info.widthHz, sr, fftSize);
                    double trueP = db; // by construction: exact amount added at info.bin
                    double oracleV = eng.queryOracle(frame, info.bin);
                    double aV = eng.queryA(frame, info.bin);
                    double cV = eng.queryC(frame, info.bin);
                    ErrorSample s{ trueP, oracleV, aV, cV, aV - trueP, cV - trueP, oracleV - trueP, f, resLim };
                    allSamples.push_back(s);
                    if (srSpec.detailed)
                        std::printf("  %-6.0f %-6.0f | %-8.3f %-8.3f %-8.3f %-8.3f | %+-8.3f %+-8.3f %-6s\n",
                                    f, db, trueP, oracleV, aV, cV, s.errA, s.errC, resLim ? "YES" : "no");
                }
            }
        }

        // ---- Aggregate error stats (this SR, all tilts/freqs/dB pooled) ----
        std::vector<double> absA, absC, absOracle, signA, signC, signOracle;
        std::vector<double> absA_noLim, absC_noLim; // excluding resolution-limited points
        for (auto& s : allSamples)
        {
            absA.push_back(std::abs(s.errA)); absC.push_back(std::abs(s.errC)); absOracle.push_back(std::abs(s.errOracle));
            signA.push_back(s.errA); signC.push_back(s.errC); signOracle.push_back(s.errOracle);
            if (!s.resLimited) { absA_noLim.push_back(std::abs(s.errA)); absC_noLim.push_back(std::abs(s.errC)); }
        }
        std::printf("\n-- Aggregate error, %s (n=%zu, all tilts/freqs/dB pooled) --\n", srSpec.label, allSamples.size());
        printErrStats("A", absA, signA);
        printErrStats("C", absC, signC);
        printErrStats("P25oracle", absOracle, signOracle);
        std::printf("  (excluding RESOLUTION LIMITED points, n=%zu)\n", absA_noLim.size());
        printErrStats("A-noLim", absA_noLim, signA);
        printErrStats("C-noLim", absC_noLim, signC);

        // ---- Frequency consistency: mean error grouped by frequency ----
        std::printf("  Frequency consistency (mean signed error per freq, dB):\n");
        for (double f : freqs)
        {
            std::vector<double> ea, ec; bool anyLim = false;
            for (auto& s : allSamples) if (s.freqHz == f) { ea.push_back(s.errA); ec.push_back(s.errC); anyLim |= s.resLimited; }
            if (ea.empty()) continue;
            double ma = 0, mc = 0; for (double x : ea) ma += x; ma /= (double) ea.size(); for (double x : ec) mc += x; mc /= (double) ec.size();
            std::printf("    %6.0f Hz: A=%+.3f  C=%+.3f  %s\n", f, ma, mc, anyLim ? "(some RESOLUTION LIMITED)" : "");
        }

        // ---- Monotonicity: score(+2)<score(+4)<...<score(+18) per (tilt,freq) ----
        int monoOkA = 0, monoOkC = 0, monoTotal = 0;
        juce::StringArray violationsA, violationsC;
        for (auto& t : kTilts)
        {
            if (!srSpec.detailed && juce::String(t.name) != "flat" && juce::String(t.name) != "curved") continue;
            auto base0 = genBaseline(bins, sr, fftSize, t);
            for (double f : freqs)
            {
                std::vector<double> scoresA, scoresC;
                for (double db : dbLevels)
                {
                    auto frame = base0;
                    auto info = injectLogGaussian(frame, sr, fftSize, f, db, q);
                    scoresA.push_back(eng.queryA(frame, info.bin));
                    scoresC.push_back(eng.queryC(frame, info.bin));
                }
                ++monoTotal;
                bool okA = true, okC = true;
                for (size_t i = 1; i < scoresA.size(); ++i) { if (scoresA[i] <= scoresA[i-1]) okA = false; if (scoresC[i] <= scoresC[i-1]) okC = false; }
                if (okA) ++monoOkA; else violationsA.add(juce::String(t.name) + "@" + juce::String(f) + "Hz");
                if (okC) ++monoOkC; else violationsC.add(juce::String(t.name) + "@" + juce::String(f) + "Hz");
            }
        }
        std::printf("  Monotonicity: A %d/%d strictly increasing (+2<+4<+8<+12<+18)%s\n", monoOkA, monoTotal,
                    violationsA.isEmpty() ? "" : ("  VIOLATIONS: " + violationsA.joinIntoString(", ")).toRawUTF8());
        std::printf("  Monotonicity: C %d/%d strictly increasing%s\n", monoOkC, monoTotal,
                    violationsC.isEmpty() ? "" : ("  VIOLATIONS: " + violationsC.joinIntoString(", ")).toRawUTF8());
    }

    //==========================================================================
    // ---- Group 2: paired dense-content delta test (48kHz only) ----
    std::printf("\n\n================ GROUP 2: PAIRED DENSE-CONTENT DELTA TEST (48kHz) ================\n");
    {
        const double sr = 48000.0;
        Engines eng; eng.prepare(bins, sr, fftSize);

        // Dense-ish realistic content: harmonic stack (vocal-like f0=180Hz) + 3
        // formant bumps -- same class of content used in the DAW-preview
        // diagnostics, deliberately NOT a clean synthetic baseline.
        std::vector<float> denseBase((size_t) bins, -90.0f);
        double f0 = 180.0;
        for (int h = 1; h <= 60; ++h) { double hz = f0 * h; if (hz >= sr * 0.45) break; int b = (int) std::round(hz * fftSize / sr); if (b < bins) denseBase[(size_t) b] = (float) (-6.0 - 10.0 * std::log2((double) h + 1)); }
        for (int i = 0; i < bins; ++i) { double hz = juce::jmax(1.0, (double) i * sr / fftSize); double formant = -50.0 - 6.0*std::log2(hz/1000.0+1.0) + 8.0*std::exp(-0.5*std::pow((std::log2(hz)-std::log2(700.0))/0.3,2.0)) + 5.0*std::exp(-0.5*std::pow((std::log2(hz)-std::log2(1200.0))/0.3,2.0)); denseBase[(size_t) i] = juce::jmax(denseBase[(size_t) i], (float) formant); }

        // Candidate frequencies -- for each, examine the LOCAL baseline first
        // and reject positions already sitting on a strong pre-existing peak
        // (a harmonic), so the delta measurement isn't contaminated.
        std::vector<double> candidateFreqs = { 310, 620, 890, 1450, 1830, 2650, 3200, 5100, 6700, 9400 };
        std::vector<double> acceptedFreqs;
        std::printf("-- Candidate screening (reject positions contaminated by a strong pre-existing peak) --\n");
        for (double f : candidateFreqs)
        {
            int b = (int) std::round(f * fftSize / sr);
            int radius = 6;
            float localMax = -999, centerVal = denseBase[(size_t) b];
            for (int k = -radius; k <= radius; ++k) { int bb = juce::jlimit(0, bins - 1, b + k); if (k != 0) localMax = juce::jmax(localMax, denseBase[(size_t) bb]); }
            bool contaminated = (centerVal - localMax) > -3.0; // center already prominent vs its own neighborhood
            std::printf("    %6.0f Hz: centerVal=%.1fdB localMaxNbr=%.1fdB %s\n", f, centerVal, localMax, contaminated ? "REJECTED (contaminated)" : "accepted");
            if (! contaminated) acceptedFreqs.push_back(f);
        }

        std::printf("\n-- Delta prominence = estimate(base+injected) - estimate(base), accepted freqs only --\n");
        std::printf("  %-6s %-6s | %-8s %-8s | %-8s %-8s\n", "freq", "dB", "deltaA", "deltaC", "errA", "errC");
        std::vector<double> absA, absC, signA, signC;
        for (double f : acceptedFreqs)
        {
            for (double db : dbLevels)
            {
                auto injected = denseBase;
                auto info = injectLogGaussian(injected, sr, fftSize, f, db, q);
                float baseA = eng.queryA(denseBase, info.bin), baseC = eng.queryC(denseBase, info.bin);
                float injA = eng.queryA(injected, info.bin), injC = eng.queryC(injected, info.bin);
                double deltaA = injA - baseA, deltaC = injC - baseC;
                double eA = deltaA - db, eC = deltaC - db;
                absA.push_back(std::abs(eA)); absC.push_back(std::abs(eC)); signA.push_back(eA); signC.push_back(eC);
                std::printf("  %-6.0f %-6.0f | %-8.3f %-8.3f | %+-8.3f %+-8.3f\n", f, db, deltaA, deltaC, eA, eC);
            }
        }
        std::printf("\n-- Group 2 aggregate (n=%zu) --\n", absA.size());
        printErrStats("A", absA, signA);
        printErrStats("C", absC, signC);
    }

    //==========================================================================
    // ---- Sharpness behaviour: broad resonance should gain weight at low
    // Sharpness, narrow resonance at high Sharpness (48kHz). ----
    std::printf("\n\n================ SHARPNESS BEHAVIOUR (48kHz) ================\n");
    {
        const double sr = 48000.0;
        Engines eng; eng.prepare(bins, sr, fftSize);
        TiltSpec flat{ "flat", 0.0, 0.0 };
        auto base0 = genBaseline(bins, sr, fftSize, flat);

        auto narrowFrame = base0; auto narrowInfo = injectLogGaussian(narrowFrame, sr, fftSize, 2000.0, 10.0, 25.0);  // Q=25: narrow
        auto broadFrame  = base0; auto broadInfo  = injectLogGaussian(broadFrame,  sr, fftSize, 2000.0, 10.0, 1.5);   // Q=1.5: broad

        std::printf("  %-10s | %-16s | %-16s\n", "sharpness", "narrow score A/C", "broad score A/C");
        for (float sh : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
        {
            std::vector<float> outAn, outCn, outAb, outCb;
            eng.a.computeProminence(narrowFrame, sh, outAn); eng.c.computeProminence(narrowFrame, sh, outCn);
            eng.a.computeProminence(broadFrame, sh, outAb);  eng.c.computeProminence(broadFrame, sh, outCb);
            std::printf("  %-10.1f | %6.3f / %6.3f | %6.3f / %6.3f\n", sh,
                        outAn[(size_t) narrowInfo.bin], outCn[(size_t) narrowInfo.bin],
                        outAb[(size_t) broadInfo.bin], outCb[(size_t) broadInfo.bin]);
        }
        std::printf("  Expectation: narrow-score should RISE with sharpness; broad-score should FALL (or rise much less).\n");
    }

    std::printf("\n================================================================\n");
    std::printf("Precision comparison complete. No production/DSP files modified.\n");
    std::printf("================================================================\n");
    return 0;
}
