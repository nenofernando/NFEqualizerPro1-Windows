// V2-A5C: core/context geometry calibration sweep for Method C
// (O1_RobustSideSlope), targeting the 8-12% leakage-bias found in the
// V2-A5B precision comparison. Investigates ONLY geometry (core exclusion
// width vs context block width) -- no sort/nth_element/scanning-by-radius/
// scheduler/candidate-refinement, and no post-hoc multiplier correction.
// Method A is not re-swept here (approved as reference/fallback only).

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/SpectralProminenceEngineV2.h"
#include <algorithm>

using NM = SpectralProminenceEngineV5::NarrowMethod;

//==============================================================================
// Same corrected generator as V2A5B_PrecisionComparison.cpp.
struct TiltSpec { const char* name; double dbPerOctave; double curveDb; };
static const TiltSpec kTilts[] = { { "flat", 0.0, 0.0 }, { "-3dB/oct", -3.0, 0.0 } };
static constexpr double kFloorHz = 30.0, kRefHz = 1000.0, kRefDb = -30.0;
static double analyticBaselineDb(double hz, const TiltSpec& t)
{
    double hzC = juce::jmax(kFloorHz, hz);
    double oct = std::log2(hzC / kRefHz);
    double db = kRefDb + t.dbPerOctave * oct;
    if (t.curveDb != 0.0) { double lo = std::log2(20.0/kRefHz), hi = std::log2(22000.0/kRefHz); double u = juce::jlimit(0.0,1.0,(oct-lo)/(hi-lo)); db += t.curveDb*(u-0.5)*(u-0.5)*4.0; }
    return db;
}
static std::vector<float> genBaseline(int bins, double sr, int fftSize, const TiltSpec& t)
{
    std::vector<float> v((size_t) bins);
    for (int i = 0; i < bins; ++i) { double hz = (double) i * sr / fftSize; v[(size_t) i] = (float) analyticBaselineDb(hz, t); }
    return v;
}
struct InjectInfo { int bin; double hz; double widthHz; };
static InjectInfo injectLogGaussian(std::vector<float>& magDb, double sr, int fftSize, double targetHz, double amplitudeDb, double q)
{
    int bins = (int) magDb.size();
    int centerBin = juce::jlimit(1, bins - 1, (int) std::round(targetHz * fftSize / sr));
    double centerHz = juce::jmax(1.0, (double) centerBin * sr / fftSize);
    double widthOct = (1.0 / q) / std::log(2.0);
    double centerLog = std::log2(centerHz);
    for (int i = 0; i < bins; ++i) { double hz = juce::jmax(1.0, (double) i * sr / fftSize); double d = (std::log2(hz) - centerLog) / juce::jmax(1e-6, widthOct); magDb[(size_t) i] += (float) (amplitudeDb * std::exp(-0.5 * d * d)); }
    return { centerBin, centerHz, centerHz / q };
}
static double median(std::vector<double> v) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); size_t n=v.size(); return n%2 ? v[n/2] : 0.5*(v[n/2-1]+v[n/2]); }
static double percentile(std::vector<double> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); double idx=p/100.0*(double)(v.size()-1); size_t lo=(size_t)idx; size_t hi=juce::jmin(v.size()-1,lo+1); double f=idx-(double)lo; return v[lo]+(v[hi]-v[lo])*f; }
static bool isResolutionLimited(double widthHz, double sr, int fftSize) { return widthHz < (sr / fftSize); }

//==============================================================================
struct Candidate { const char* name; double coreOct, ctxOct; };
static const Candidate kCandidates[] = {
    { "current(0.04/0.90)", 0.04, 0.90 },
    { "0.08/0.90",          0.08, 0.90 },
    { "0.12/0.90",          0.12, 0.90 },
    { "0.18/0.90",          0.18, 0.90 },
    { "0.25/0.90",          0.25, 0.90 },
    { "0.12/1.10",          0.12, 1.10 },
    { "0.18/1.20",          0.18, 1.20 },
};

int main()
{
    const double sr = 48000.0; // primary SR per the stated 48kHz target
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    const double qs[] = { 3, 6, 12, 24, 48 };
    const double dbLevels[] = { 2, 4, 8, 12, 18 };
    const double freqs[] = { 250, 1000, 4000, 10000, 16000 };

    std::printf("================================================================\n");
    std::printf("V2-A5C geometry sweep -- Method C (O1_RobustSideSlope), 48kHz, FFT=2048\n");
    std::printf("Ground truth = analytic baseline + known injected amplitude (NOT called P25 oracle).\n");
    std::printf("P25 = offline reference only, for comparison, not ground truth.\n");
    std::printf("================================================================\n\n");

    SpectralProminenceEngineV2 p25; p25.prepare(bins, sr, fftSize);

    for (auto& cand : kCandidates)
    {
        SpectralProminenceEngineV5 eng;
        eng.prepare(bins, sr, fftSize);
        eng.setNarrowMethod(NM::O1_RobustSideSlope);
        eng.setNarrowScaleParams(cand.coreOct, cand.ctxOct);

        std::printf("---- Candidate: %s ----\n", cand.name);
        std::vector<double> absErr, signErr;
        std::vector<double> absErrByQ[5], signErrByQ[5];
        int monoOk = 0, monoTotal = 0;

        for (auto& t : kTilts)
        {
            auto base0 = genBaseline(bins, sr, fftSize, t);
            for (double f : freqs)
            {
                for (size_t qi = 0; qi < 5; ++qi)
                {
                    double q = qs[qi];
                    std::vector<double> scores;
                    for (double db : dbLevels)
                    {
                        auto frame = base0;
                        auto info = injectLogGaussian(frame, sr, fftSize, f, db, q);
                        std::vector<float> out; eng.computeProminence(frame, 10.0f, out);
                        double est = out[(size_t) info.bin];
                        double err = est - db;
                        absErr.push_back(std::abs(err)); signErr.push_back(err);
                        absErrByQ[qi].push_back(std::abs(err)); signErrByQ[qi].push_back(err);
                        scores.push_back(est);
                    }
                    ++monoTotal;
                    bool ok = true;
                    for (size_t i = 1; i < scores.size(); ++i) if (scores[i] <= scores[i-1]) ok = false;
                    if (ok) ++monoOk;
                }
            }
        }
        double mae = 0; for (double e : absErr) mae += e; mae /= (double) absErr.size();
        double bias = 0; for (double e : signErr) bias += e; bias /= (double) signErr.size();
        std::printf("  overall: MAE=%.3f  bias=%+.3f  medianAE=%.3f  P95=%.3f  monotonic=%d/%d\n",
                    mae, bias, median(absErr), percentile(absErr, 95), monoOk, monoTotal);
        for (size_t qi = 0; qi < 5; ++qi)
        {
            double maeQ = 0; for (double e : absErrByQ[qi]) maeQ += e; maeQ /= (double) absErrByQ[qi].size();
            double biasQ = 0; for (double e : signErrByQ[qi]) biasQ += e; biasQ /= (double) signErrByQ[qi].size();
            std::printf("    Q=%-4.0f MAE=%.3f bias=%+.3f\n", qs[qi], maeQ, biasQ);
        }
        std::printf("\n");
    }

    // ---- Resolution-limited annotation (Q=48 at 250Hz/1kHz is the tightest case at 48kHz) ----
    std::printf("---- RESOLUTION LIMITED check (48kHz, bin=%.4fHz) ----\n", sr / fftSize);
    for (double f : freqs)
        for (double q : qs)
        {
            double widthHz = f / q;
            std::printf("  freq=%-6.0f Q=%-4.0f widthHz=%-8.2f %s\n", f, q, widthHz, isResolutionLimited(widthHz, sr, fftSize) ? "RESOLUTION LIMITED" : "ok");
        }

    std::printf("\n================================================================\n");
    std::printf("Sweep complete. No production defaults changed by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
