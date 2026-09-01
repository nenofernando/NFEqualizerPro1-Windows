// PHYSICAL B, Passos 2+3: official V1 prominence/reduction baseline, then
// the SAME synthetic stimuli run through SpectralProminenceEngineV5 (A5C
// geometry + O1_RobustSideSlope) in PROMINENCE-ONLY mode (no gain applied).
// This is a controlled, reproducible SPECTRAL-DOMAIN benchmark: rather than
// synthesizing audio and running a real FFT (which adds STFT-window/leakage
// noise on top of what we're trying to measure), we build the magDb array
// directly -- a flat noise floor plus a Gaussian bump of known peak height
// (true prominence) and known width (from Q), which is exactly the
// spectral shape both V1's mean-subtraction and V2's core/context robust
// estimators operate on. RESOLUTION LIMITED is flagged whenever the
// injected width is narrower than the FFT's own bin resolution can
// faithfully represent.

#include <JuceHeader.h>
#include "DSP/ResonanceDetector.h"
#include "DSP/SpectralProminenceEngineV5.h"
#include <map>

static const double sr = 48000.0;
static const int fftSize = 2048;
static const int bins = fftSize / 2 + 1;
static const double binHz = sr / (double) fftSize;

// Builds a synthetic magDb spectrum: flat floor + one Gaussian bump.
// ampDb = true peak prominence above the floor. Q = freq / bandwidthHz
// (bandwidth = FWHM). Returns the bin index of the peak and whether the
// injected width is resolution-limited by this FFT's bin spacing.
static std::vector<float> makeSpectrum(double freqHz, double ampDb, double Q, int& peakBin, bool& resolutionLimited)
{
    std::vector<float> magDb((size_t) bins, -40.0f);
    peakBin = (int) std::round(freqHz / binHz);
    double bandwidthHz = freqHz / Q;
    double sigmaHz = bandwidthHz / 2.3548; // FWHM -> Gaussian sigma
    double sigmaBins = sigmaHz / binHz;
    resolutionLimited = sigmaBins < 1.0; // narrower than ~1 bin can't be faithfully represented
    double sigmaBinsClamped = juce::jmax(0.6, sigmaBins); // still inject SOMETHING even when resolution-limited, just flag it
    for (int b = 0; b < bins; ++b)
    {
        double d = (b - peakBin) / sigmaBinsClamped;
        double bump = ampDb * std::exp(-0.5 * d * d);
        magDb[(size_t) b] = (float) (-40.0 + bump);
    }
    return magDb;
}

// V1's OWN internal prominence formula, replicated exactly (matches
// ResonanceDetector::compute()'s private computation) so we can inspect
// prominence directly, independent of threshold/depth/reduction math.
static double v1EstimatedProminence(const std::vector<float>& magDb, int bin, float sharpness)
{
    const int radius = juce::jlimit(2, 48, (int) std::round(34.0f - sharpness * 2.8f));
    int a = juce::jmax(0, bin - radius), b = juce::jmin(bins - 1, bin + radius);
    double sum = 0; for (int i = a; i <= b; ++i) sum += magDb[(size_t) i];
    double mean = sum / (double) (b - a + 1);
    return magDb[(size_t) bin] - mean;
}

// Real V1 reduction at the peak bin + affected bandwidth (contiguous bins
// with |reductionDb| > 0.5dB around the peak), run to steady state.
struct V1ReductionResult { double reductionDb; double affectedBandwidthHz; int freqBiasBins; };
static V1ReductionResult v1Reduction(const std::vector<float>& magDb, int peakBin)
{
    ResonanceDetector det; det.prepare(bins, sr, fftSize); det.reset();
    float freq[ResonanceDetector::kMaxBands]{}, sens[ResonanceDetector::kMaxBands]{}, width[ResonanceDetector::kMaxBands]{}, focus[ResonanceDetector::kMaxBands]{};
    int shape[ResonanceDetector::kMaxBands]{}; bool active[ResonanceDetector::kMaxBands]{};
    std::vector<float> reduction;
    for (int i = 0; i < 60; ++i) // enough frames for attack to converge
        det.compute(magDb, reduction, 5.0f, 4.0f, 5.0f, 5.0f, 30.0f, 20.0f, 20000.0f, 0.0f, freq, sens, width, shape, focus, active, 1.0f);
    double atPeak = reduction[(size_t) peakBin];
    int lo = peakBin, hi = peakBin;
    while (lo > 0 && std::abs(reduction[(size_t) (lo - 1)]) > 0.5f) --lo;
    while (hi < bins - 1 && std::abs(reduction[(size_t) (hi + 1)]) > 0.5f) ++hi;
    double bwHz = (hi - lo + 1) * binHz;
    int argmax = peakBin; double best = -1e9;
    for (int b = juce::jmax(0, peakBin - 20); b <= juce::jmin(bins - 1, peakBin + 20); ++b)
        if (std::abs(reduction[(size_t) b]) > best) { best = std::abs(reduction[(size_t) b]); argmax = b; }
    return { atPeak, bwHz, argmax - peakBin };
}

int main()
{
    std::printf("================================================================\n");
    std::printf("PHYSICAL B -- V1 official baseline vs V2-A5C prominence-only\n");
    std::printf("(Method C = O1_RobustSideSlope, geometry core=0.18oct ctx=1.20oct)\n");
    std::printf("================================================================\n\n");

    SpectralProminenceEngineV5 v2;
    v2.prepare(bins, sr, fftSize);
    v2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    jassert(v2.activeNarrowMethod() == SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);

    const double freqs[] = { 120, 250, 500, 1000, 2000, 4000, 8000, 12000 };
    const double amps[] = { 2, 4, 8, 12 };
    const double Qs[] = { 3, 6, 12, 24 };

    std::printf("%-8s %-4s %-4s %-4s | %8s %8s %8s %8s | %9s %9s %8s | %8s %9s\n",
        "Freq", "Amp", "Q", "RES?", "trueProm", "V1 est", "V1 err", "V2 est", "V2 err", "V1 redDb", "biasBin", "affBW Hz", "V2biasBin");
    std::printf("--------------------------------------------------------------------------------------------------------------------------\n");

    // Accumulators for the low-vs-high frequency consistency summary.
    struct Acc { double sumAbsErrV1 = 0, sumAbsErrV2 = 0; int n = 0; };
    std::map<double, Acc> byFreqV1, byFreqV2;

    for (double freq : freqs)
        for (double amp : amps)
            for (double Q : Qs)
            {
                int peakBin; bool resLimited;
                auto magDb = makeSpectrum(freq, amp, Q, peakBin, resLimited);

                double v1est = v1EstimatedProminence(magDb, peakBin, 4.0f);
                double v1err = v1est - amp;

                std::vector<float> v2prom((size_t) bins, 0.0f);
                v2.computeProminence(magDb, 4.0f, v2prom);
                double v2est = v2prom[(size_t) peakBin];
                double v2err = v2est - amp;

                auto redRes = v1Reduction(magDb, peakBin);

                int v2ArgMax = peakBin; double v2Best = -1e9;
                for (int b = juce::jmax(0, peakBin - 20); b <= juce::jmin(bins - 1, peakBin + 20); ++b)
                    if (v2prom[(size_t) b] > v2Best) { v2Best = v2prom[(size_t) b]; v2ArgMax = b; }

                std::printf("%-8.0f %-4.0f %-4.0f %-4s | %8.2f %8.2f %8.2f %8.2f | %9.2f %9.2f %8d | %8.1f %9d\n",
                    freq, amp, Q, resLimited ? "LIM" : "ok",
                    amp, v1est, v1err, v2est, v2err, redRes.reductionDb, redRes.freqBiasBins, redRes.affectedBandwidthHz, v2ArgMax - peakBin);

                byFreqV1[freq].sumAbsErrV1 += std::abs(v1err); byFreqV1[freq].n++;
                byFreqV2[freq].sumAbsErrV2 += std::abs(v2err); byFreqV2[freq].n++;
            }

    std::printf("\n-- Per-frequency mean |prominence error| (averaged over all Amp x Q) --\n");
    std::printf("%-8s %12s %12s %12s\n", "Freq", "V1 MAE", "V2-A5C MAE", "improvement");
    for (double freq : freqs)
    {
        double v1mae = byFreqV1[freq].sumAbsErrV1 / byFreqV1[freq].n;
        double v2mae = byFreqV2[freq].sumAbsErrV2 / byFreqV2[freq].n;
        double improvementPct = v1mae > 1e-9 ? 100.0 * (v1mae - v2mae) / v1mae : 0.0;
        std::printf("%-8.0f %12.3f %12.3f %11.1f%%\n", freq, v1mae, v2mae, improvementPct);
    }

    std::printf("\n================================================================\n");
    std::printf("Benchmark complete. No production defaults changed by this run.\n");
    std::printf("================================================================\n");
    return 0;
}
