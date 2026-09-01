// PHYSICAL C2.3c: frame-by-frame cross-source diagnosis for 80/100/120Hz @
// 96/192kHz, to find exactly which term makes aux win at 100Hz@96kHz when
// main was already better there.

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz = freqHz / Q; int n = (int) b.size();
    for (int k = 0; k < 9; ++k) { double t = (double) k / 8.0 - 0.5, f = freqHz + t * bwHz;
        double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * f / sr;
        for (int i = 0; i < n; ++i) { b[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; } }
}
static float parabolicDelta(float l, float c, float r) { float denom = l - 2.0f * c + r; if (std::abs(denom) < 1.0e-6f) return 0.0f; return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom); }
static float crossoverWeight(float hz, float lowHz, float highHz) { if (hz <= lowHz) return 1.0f; if (hz >= highHz) return 0.0f; float t = (hz - lowHz) / (highHz - lowHz); return 1.0f - (t * t * (3.0f - 2.0f * t)); }
static float resolutionAdvantageWeight(double mainBinHz, double auxBinHz) { if (mainBinHz <= 0.0) return 0.0f; double ratio = auxBinHz / mainBinHz; return (float) juce::jlimit(0.0, 1.0, 1.0 - ratio); }

static float mainLowBinReliability(int bin, const std::vector<float>& magDb, const std::vector<float>& promOut, double binHz)
{
    int n = (int) magDb.size();
    if (bin < 1 || bin >= n - 1) return 0.5f;
    float magL = magDb[(size_t) (bin - 1)], magC = magDb[(size_t) bin], magR = magDb[(size_t) (bin + 1)];
    float promL = promOut[(size_t) (bin - 1)], promC = promOut[(size_t) bin], promR = promOut[(size_t) (bin + 1)];
    auto bestOf3 = [](float l, float c, float r) { if (c >= l && c >= r) return 0; return (l > r) ? -1 : 1; };
    int magBest = bestOf3(magL, magC, magR), promBest = bestOf3(promL, promC, promR);
    float orderAgreement = (magBest == promBest) ? 1.0f : 0.0f;
    float magAsym = (magR - magL) / juce::jmax(1.0f, std::abs(magR) + std::abs(magL));
    float promAsym = (promR - promL) / juce::jmax(1.0f, std::abs(promR) + std::abs(promL));
    float asymDivergence = juce::jlimit(0.0f, 1.0f, std::abs(magAsym - promAsym));
    float freqHz = (float) (bin * binHz);
    float relRes = juce::jlimit(0.0f, 1.0f, 1.0f - (float) (binHz / juce::jmax(1.0, (double) freqHz)));
    return juce::jlimit(0.0f, 1.0f, relRes * orderAgreement * (1.0f - asymDivergence));
}

// C2.3c: physical consistency -- does THIS source's own prominence-peak
// location agree with THIS source's own raw-magnitude-peak location? Uses
// only information available within that source (no ground truth).
static float physicalConsistency(int mainPromPeakBin, int mainMagPeakBin, double binHz)
{
    float distBins = std::abs((float) (mainPromPeakBin - mainMagPeakBin));
    float distCents = distBins * 1200.0f * (float) std::log2(1.0 + binHz / juce::jmax(1.0, (double) (mainMagPeakBin * binHz) + 1.0));
    // Consistent if the two peaks (prominence-domain vs magnitude-domain) land on the SAME bin or an immediately adjacent one; degrade smoothly beyond that.
    return juce::jlimit(0.0f, 1.0f, 1.0f - distCents / 400.0f);
}

struct MainProm
{
    SpectralProminenceEngineV5 prom;
    juce::dsp::FFT fft{ 11 };
    std::array<float, 2048> window{};
    std::array<float, 4096> scratch{};
    std::vector<float> magDb, promOut;
    void prepare(double sr) { prom.prepare(2048 / 2 + 1, sr, 2048); prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
        for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f);
        magDb.assign(2048 / 2 + 1, -120.0f); promOut.assign(2048 / 2 + 1, 0.0f); }
    void processFrame(const float* samples)
    {
        for (int k = 0; k < 2048; ++k) scratch[(size_t) k] = samples[k] * window[(size_t) k];
        std::fill(scratch.begin() + 2048, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins = 2048 / 2 + 1;
        for (int b = 0; b < bins; ++b)
        {
            float re = scratch[(size_t) (2 * b)], im = (b == 0 || b == bins - 1) ? 0.0f : scratch[(size_t) (2 * b + 1)];
            magDb[(size_t) b] = juce::Decibels::gainToDecibels(std::sqrt(re * re + im * im) / 2048.0f + 1e-12f, -120.0f);
        }
        prom.computeProminence(magDb, 4.0f, promOut);
    }
};

int main()
{
    const float crossLow = 300.0f, crossHigh = 800.0f;
    for (double sr : { 96000.0, 192000.0 })
    {
        std::printf("======== SR=%.0fHz ========\n", sr);
        for (double f : { 80.0, 100.0, 120.0 })
        {
            int n = (int) (sr * 0.5);
            auto sig = genSilence(n);
            addBurst(sig, sr, f, (float) juce::Decibels::decibelsToGain(4.0f) * 0.4f, 6.0, 5); // amp=+4dB, width=medium

            MainProm mp; mp.prepare(sr);
            LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
            double hostBinHz = sr / 2048.0;
            int approxBin = juce::jlimit(1, 2048 / 2 - 2, (int) std::round(f / hostBinHz));

            const int hop = 512;
            int frameToPrint = -1;
            int frame = 0;
            for (int i = 0; i + 2048 <= n; i += hop)
            {
                mp.processFrame(sig.data() + i);
                aux.pushSamples(sig.data() + i, hop);
                if (frame == 20) frameToPrint = frame; // steady state, matches earlier diagnostics
                if (frame == frameToPrint)
                {
                    // main raw magnitude peak (search window)
                    int mainMagBin = approxBin; float mainMagBest = -999;
                    for (int b = juce::jmax(1, approxBin - 2); b <= juce::jmin(2048 / 2 - 2, approxBin + 2); ++b) if (mp.magDb[(size_t) b] > mainMagBest) { mainMagBest = mp.magDb[(size_t) b]; mainMagBin = b; }
                    float mL = mp.magDb[(size_t) (mainMagBin - 1)], mC = mp.magDb[(size_t) mainMagBin], mR = mp.magDb[(size_t) (mainMagBin + 1)];
                    float mainMagSubHz = (float) ((mainMagBin + parabolicDelta(mL, mC, mR)) * hostBinHz);

                    // main prominence peak (search window)
                    int mainPromBin = approxBin; float mainPromBest = -999;
                    for (int b = juce::jmax(1, approxBin - 2); b <= juce::jmin(2048 / 2 - 2, approxBin + 2); ++b) if (mp.promOut[(size_t) b] > mainPromBest) { mainPromBest = mp.promOut[(size_t) b]; mainPromBin = b; }
                    float pL = mp.promOut[(size_t) (mainPromBin - 1)], pC = mp.promOut[(size_t) mainPromBin], pR = mp.promOut[(size_t) (mainPromBin + 1)];
                    float mainPromSubHz = (float) ((mainPromBin + parabolicDelta(pL, pC, pR)) * hostBinHz);

                    float mainLowBinRel = mainLowBinReliability(mainPromBin, mp.magDb, mp.promOut, hostBinHz);
                    float mainPhysCons = physicalConsistency(mainPromBin, mainMagBin, hostBinHz);

                    float auxEst = 0, auxValRel = 0, auxFreqRel = 0;
                    float auxPromDb = aux.auxProminenceFor((float) f, &auxEst, &auxValRel, &auxFreqRel);

                    // aux raw magnitude peak (independent lookup, same window logic on aux's own decimated arrays via debug accessors)
                    double auxBinHz = aux.analysisBinHz();
                    int auxApproxBin = juce::jlimit(1, (int) aux.debugMagDb().size() - 2, (int) std::round(f / auxBinHz));
                    int auxMagBin = auxApproxBin; float auxMagBest = -999;
                    for (int b = juce::jmax(1, auxApproxBin - 2); b <= juce::jmin((int) aux.debugMagDb().size() - 2, auxApproxBin + 2); ++b) if (aux.debugMagDb()[(size_t) b] > auxMagBest) { auxMagBest = aux.debugMagDb()[(size_t) b]; auxMagBin = b; }
                    float aML = aux.debugMagDb()[(size_t) (auxMagBin - 1)], aMC = aux.debugMagDb()[(size_t) auxMagBin], aMR = aux.debugMagDb()[(size_t) (auxMagBin + 1)];
                    float auxMagSubHz = (float) ((auxMagBin + parabolicDelta(aML, aMC, aMR)) * auxBinHz);

                    int auxPromBin = auxApproxBin; float auxPromBest = -999;
                    for (int b = juce::jmax(1, auxApproxBin - 2); b <= juce::jmin((int) aux.debugProminence().size() - 2, auxApproxBin + 2); ++b) if (aux.debugProminence()[(size_t) b] > auxPromBest) { auxPromBest = aux.debugProminence()[(size_t) b]; auxPromBin = b; }
                    float auxPhysCons = physicalConsistency(auxPromBin, auxMagBin, auxBinHz);

                    float freqWeight = crossoverWeight((float) f, crossLow, crossHigh);
                    float resAdv = resolutionAdvantageWeight(hostBinHz, auxBinHz);
                    float needForAux = 1.0f - mainLowBinRel;
                    float oldAuxWeight = juce::jlimit(0.0f, 1.0f, freqWeight * resAdv * auxValRel * needForAux);
                    // C2.3c candidate revision: gate by BOTH sources' physical consistency, not just main's reliability.
                    float auxAuthority = freqWeight * resAdv * auxValRel * auxPhysCons * needForAux;
                    float mainAuthority = mainLowBinRel * mainPhysCons;
                    float newAuxWeight = (auxAuthority + mainAuthority > 1.0e-6f) ? juce::jlimit(0.0f, 1.0f, auxAuthority / (auxAuthority + mainAuthority)) : 0.0f;

                    std::printf(" -- f=%.0fHz --\n", f);
                    std::printf("    host: rawMagPeak=%.1fHz(bin%d) subHz=%.1fHz | mainPromPeak=%.1fHz(bin%d) subHz=%.1fHz\n", mainMagBin * hostBinHz, mainMagBin, mainMagSubHz, mainPromBin * hostBinHz, mainPromBin, mainPromSubHz);
                    std::printf("    mainLowBinReliability=%.3f  mainPhysicalConsistency=%.3f  mainAuthority=%.3f\n", mainLowBinRel, mainPhysCons, mainAuthority);
                    std::printf("    aux:  rawMagPeak=%.1fHz(bin%d) subHz=%.1fHz | auxPromPeak(bin)=%d  auxEstHz=%.1fHz  auxPromDb=%.2f\n", auxMagBin * auxBinHz, auxMagBin, auxMagSubHz, auxPromBin, auxEst, auxPromDb);
                    std::printf("    auxValueReliability=%.3f  auxFreqReliability=%.3f  auxPhysicalConsistency=%.3f\n", auxValRel, auxFreqRel, auxPhysCons);
                    std::printf("    resolutionAdvantage=%.3f  frequencyWeight=%.3f  needForAux=%.3f\n", resAdv, freqWeight, needForAux);
                    std::printf("    OLD auxWeight=%.3f  NEW (consistency-gated) auxWeight=%.3f  auxAuthority=%.3f\n", oldAuxWeight, newAuxWeight, auxAuthority);
                }
                ++frame;
            }
        }
    }
    return 0;
}
