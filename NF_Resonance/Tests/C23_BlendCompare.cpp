// PHYSICAL C2.3 item 4/6/8/9: effectiveProminence blend, gated by BOTH
// crossover frequency AND aux reliability AND decimationFactor (aux only
// gets real structural advantage where the host is decimated -- 96/192kHz
// -- so its weight is hard-gated to 0 at decimation=1x regardless of
// reliability, per explicit direction: don't let a single reliability
// reading risk regressing 44.1/48kHz to fix 192kHz).

#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }

static float crossoverWeight(float hz, float lowHz, float highHz)
{
    if (hz <= lowHz) return 1.0f;
    if (hz >= highHz) return 0.0f;
    float t = (hz - lowHz) / (highHz - lowHz);
    return 1.0f - (t * t * (3.0f - 2.0f * t));
}

// PHYSICAL C2.3 (revised per explicit direction): auxWeight = frequencyWeight
// x resolutionAdvantageWeight x auxProminenceReliability. No hard switch by
// sample rate or decimationFactor -- resolutionAdvantageWeight is DERIVED
// from the actual bin-width ratio (auxBinWidthHz / mainBinWidthHz), so it
// naturally evaluates to ~0 at 44.1/48kHz (aux runs 1:1 there, same bin
// width as main -- no structural advantage to hand it authority) and rises
// continuously with decimation depth at 96/192kHz, with no per-SR branch
// and no discontinuity -- robust to any future intermediate rate
// (88.2/176.4kHz) for free, since it's the same formula either way.
static float resolutionAdvantageWeight(double mainBinHz, double auxBinHz)
{
    if (mainBinHz <= 0.0) return 0.0f;
    double ratio = auxBinHz / mainBinHz; // <1 = aux resolves finer than main
    return (float) juce::jlimit(0.0, 1.0, 1.0 - ratio);
}

// Blend domain: dB. Both mainDb and auxDb are ALREADY the same kind of
// quantity -- SpectralProminenceEngineV5's own dB-domain prominence output,
// just computed on two different sample streams (host-rate vs decimated).
// The rest of the detector (ConfidenceEngine's prominenceEvidence mapping,
// the admission floors) already treats prominence as a linear-in-dB scale,
// so a weighted arithmetic mean IN dB is the domain-coherent choice here --
// converting to linear power first would impose an unjustified assumption
// about what "prominence dB" represents physically (it is a relative,
// baseline-subtracted score, not a literal power ratio).
static float effectiveProminence(float mainDb, float queryHz, const LowFrequencyHarmonicAnalyzer& aux, double mainBinHz, float crossoverLowHz, float crossoverHighHz, float* outAuxWeight = nullptr)
{
    float auxEst = 0, auxRel = 0;
    float auxDb = aux.auxProminenceFor(queryHz, &auxEst, &auxRel);
    float freqWeight = crossoverWeight(queryHz, crossoverLowHz, crossoverHighHz);
    float resAdv = resolutionAdvantageWeight(mainBinHz, aux.analysisBinHz());
    float auxWeight = freqWeight * resAdv * auxRel;
    if (outAuxWeight) *outAuxWeight = auxWeight;
    return auxWeight * auxDb + (1.0f - auxWeight) * mainDb;
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
static float parabolicDelta(float l, float c, float r) { float denom = l - 2.0f * c + r; if (std::abs(denom) < 1.0e-6f) return 0.0f; return juce::jlimit(-0.5f, 0.5f, 0.5f * (l - r) / denom); }

int main()
{
    const double freqs[] = { 80, 100, 110, 115, 120, 125, 130, 150, 200, 250, 300, 400, 500, 800 };
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const float ampDb = 4.0f;
    const double Q = 10.0;
    const float crossLow = 300.0f, crossHigh = 800.0f; // provisional, per item 4 default proposal

    std::printf("=== C2.3 EFFECTIVE PROMINENCE (blended, decimation-gated) vs main-only ===\n");
    int total = 0, mainWrong = 0, blendWrong = 0;
    int total44_48 = 0, mainWrong44_48 = 0, blendWrong44_48 = 0;
    for (double sr : rates)
    {
        std::printf(" -- Sample rate %.0f Hz --\n", sr);
        for (double f : freqs)
        {
            int n = (int) (sr * 0.6);
            auto sig = genSilence(n);
            {
                juce::Random rng(5); double bwHz = f / Q;
                for (int k = 0; k < 9; ++k)
                {
                    double t = (double) k / 8.0 - 0.5, ff = f + t * bwHz;
                    double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi, inc = juce::MathConstants<double>::twoPi * ff / sr;
                    float amp = (float) juce::Decibels::decibelsToGain(ampDb) * 0.4f;
                    for (int i = 0; i < n; ++i) { sig[(size_t) i] += (float) std::sin(ph) * (amp / 3.0f); ph += inc; }
                }
            }
            MainProm mp; mp.prepare(sr);
            LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
            const int hop = 512;
            double hostBinHz = sr / 2048.0;
            float rawMagPeakHz = 0, rawMagPeakDb = -999; int rawBin = -1;
            float mainPeakHz = 0, mainPeakDb = -999; int mainBin = -1;
            float blendPeakHz = 0, blendPeakDb = -999; float lastAuxWeight = 0;
            for (int i = 0; i + 2048 <= n; i += hop)
            {
                mp.processFrame(sig.data() + i);
                aux.pushSamples(sig.data() + i, hop);
                int approxBin = juce::jlimit(1, 2048 / 2 - 2, (int) std::round(f / hostBinHz));
                for (int b = juce::jmax(1, approxBin - 2); b <= juce::jmin(2048 / 2 - 2, approxBin + 2); ++b)
                {
                    if (mp.magDb[(size_t) b] > rawMagPeakDb) { rawMagPeakDb = mp.magDb[(size_t) b]; rawBin = b; }
                    if (mp.promOut[(size_t) b] > mainPeakDb) { mainPeakDb = mp.promOut[(size_t) b]; mainBin = b; }
                    float aw = 0;
                    float blendedDb = effectiveProminence(mp.promOut[(size_t) b], (float) (b * hostBinHz), aux, hostBinHz, crossLow, crossHigh, &aw);
                    if (blendedDb > blendPeakDb) { blendPeakDb = blendedDb; blendPeakHz = (float) (b * hostBinHz); lastAuxWeight = aw; }
                }
            }
            if (rawBin >= 0) { float l = mp.magDb[(size_t) (rawBin - 1)], c = mp.magDb[(size_t) rawBin], r = mp.magDb[(size_t) (rawBin + 1)]; rawMagPeakHz = (float) ((rawBin + parabolicDelta(l, c, r)) * hostBinHz); }
            if (mainBin >= 0) { float l = mp.promOut[(size_t) (mainBin - 1)], c = mp.promOut[(size_t) mainBin], r = mp.promOut[(size_t) (mainBin + 1)]; mainPeakHz = (float) ((mainBin + parabolicDelta(l, c, r)) * hostBinHz); }
            // blendPeakHz here is just the argmax BIN's raw Hz (not sub-bin refined) -- for a fair location comparison, refine around blendPeak's bin using magnitude, same as main/raw.
            int blendBin = (int) std::round(blendPeakHz / hostBinHz);
            float bl = mp.magDb[(size_t) juce::jmax(0, blendBin - 1)], bc = mp.magDb[(size_t) juce::jlimit(0, (int) mp.magDb.size() - 1, blendBin)], br = mp.magDb[(size_t) juce::jmin((int) mp.magDb.size() - 1, blendBin + 1)];
            float blendRefinedHz = (float) ((blendBin + parabolicDelta(bl, bc, br)) * hostBinHz);

            float mainErrCents = rawMagPeakHz > 0 && mainPeakHz > 0 ? 1200.0f * std::log2(mainPeakHz / rawMagPeakHz) : 9999.0f;
            float blendErrCents = rawMagPeakHz > 0 && blendRefinedHz > 0 ? 1200.0f * std::log2(blendRefinedHz / rawMagPeakHz) : 9999.0f;
            bool mainOk = std::abs(mainErrCents) < 150.0f;
            bool blendOk = std::abs(blendErrCents) < 150.0f;
            ++total; if (! mainOk) ++mainWrong; if (! blendOk) ++blendWrong;
            bool is4448 = (sr == 44100.0 || sr == 48000.0);
            if (is4448) { ++total44_48; if (! mainOk) ++mainWrong44_48; if (! blendOk) ++blendWrong44_48; }

            std::printf("  f=%6.1fHz: rawMag=%7.1fHz | main=%7.1fHz err=%+6.1fc %-7s | blend=%7.1fHz err=%+6.1fc %-7s auxWeight=%.2f\n",
                f, rawMagPeakHz, mainPeakHz, mainErrCents, mainOk ? "OK" : "WRONG", blendRefinedHz, blendErrCents, blendOk ? "OK" : "WRONG", lastAuxWeight);
        }
    }
    std::printf("\n=== SUMMARY ===\n");
    std::printf("  ALL: total=%d main WRONG=%d (%.1f%%) blend WRONG=%d (%.1f%%)\n", total, mainWrong, 100.0*mainWrong/total, blendWrong, 100.0*blendWrong/total);
    std::printf("  44.1/48kHz ONLY (must NOT regress): total=%d main WRONG=%d (%.1f%%) blend WRONG=%d (%.1f%%)\n",
        total44_48, mainWrong44_48, 100.0*mainWrong44_48/total44_48, blendWrong44_48, 100.0*blendWrong44_48/total44_48);
    return 0;
}
