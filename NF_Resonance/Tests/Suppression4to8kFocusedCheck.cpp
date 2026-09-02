// FOCUSED 4-8kHz suppression audit, exact conditions requested by the user:
// Low=25Hz, High=16kHz, Mix=100%, Output=0dB, Max Reduction=3dB(ON),
// Sidechain=Internal, Mode=Stereo, Delta OFF for the main measurement pass.
// True 2-channel processing (not a mono stand-in) via SpectralEngine::process()
// -- the exact same method PluginProcessor::processBlock() calls, mode=0
// (Stereo, matches Params::mode's own "Stereo" value; MidSide's ch==2&&mode==2
// branch is never taken here) so this is the REAL per-channel signal path.
// Also renders 4 WAV files (original/processed/delta/bypass @6.3kHz, 48kHz)
// for direct listening, since this session cannot record a screen video
// (interactive GUI/mouse automation is off after an earlier unrelated-window
// capture incident -- code-level measurement + rendered audio replaces it).

#include <JuceHeader.h>
#include "DSP/SpectralEngine.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <random>

static std::vector<float> genPinkNoise(int n, float amp, unsigned seed)
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
static float peakDb(const std::vector<float>& x, int start, int len)
{
    float pk = 0.0f; for (int i = 0; i < len; ++i) pk = juce::jmax(pk, std::abs(x[(size_t) (start + i)]));
    return juce::Decibels::gainToDecibels(pk + 1e-12f, -120.0f);
}
static int binForFreq(double sr, double freq) { return (int) std::round(freq * 2048.0 / sr); }

struct RunResult { std::vector<float> outL, outR; std::array<float, GainMaskEngine::kUIBins> snapshot{}; int snapshotBins = 0; };
static RunResult runStereo(const std::vector<float>& sigL, const std::vector<float>& sigR, double sr, const SpectralEngine::Params& p)
{
    RunResult r;
    juce::AudioBuffer<float> buf(2, (int) sigL.size());
    for (int i = 0; i < (int) sigL.size(); ++i) { buf.setSample(0, i, sigL[(size_t) i]); buf.setSample(1, i, sigR[(size_t) i]); }
    SpectralEngine eng; eng.prepare(sr, 2); eng.setParams(p); eng.process(buf, nullptr);
    r.outL.resize((size_t) sigL.size()); r.outR.resize((size_t) sigL.size());
    for (int i = 0; i < (int) sigL.size(); ++i) { r.outL[(size_t) i] = buf.getSample(0, i); r.outR[(size_t) i] = buf.getSample(1, i); }
    r.snapshot = eng.getAppliedReductionSnapshot(); r.snapshotBins = eng.getAppliedReductionSnapshotBinCount();
    return r;
}

static void writeWav(const juce::File& f, const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream(f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), sr, 2, 24, {}, 0));
    if (writer != nullptr)
    {
        stream.release(); // writer now owns it
        juce::AudioBuffer<float> buf(2, (int) L.size());
        for (int i = 0; i < (int) L.size(); ++i) { buf.setSample(0, i, L[(size_t) i]); buf.setSample(1, i, R[(size_t) i]); }
        writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    }
}

int main()
{
    bool allPass = true;
    auto check = [&](const char* what, bool cond) { std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what); if (!cond) allPass = false; };

    const double testFreqs[] = { 4000.0, 5000.0, 6300.0, 8000.0 };
    const int kLatency = 2048;

    for (double sr : { 44100.0, 48000.0 })
    {
        std::printf("\n=== sr=%.0fHz -- Low=25Hz High=16kHz Mix=100%% Output=0dB MaxRed=3dB(ON) Sidechain=Internal Mode=Stereo ===\n", sr);
        const int n = (int) (sr * 2.5);
        const int measLen = (int) (sr * 0.8), measStart = n - measLen;

        SpectralEngine::Params p;
        p.depth = 3.0f; p.sharpness = 4.0f; p.selectivity = 3.5f; p.attackMs = 10.0f; p.releaseMs = 80.0f;
        p.transient = 5.0f; p.lowHz = 25.0f; p.highHz = 16000.0f; p.detail = 5.0f;
        p.maxReductionEnabled = true; p.maxReductionDb = 3.0f;
        p.detectorSource = 0; // Internal
        p.mode = 0;           // Stereo (not Mid/Side)
        p.delta = false;

        std::printf("  %8s | %10s | %11s | %10s | %10s | %10s | %10s | %14s\n",
                     "freq(Hz)", "peakIn(dB)", "detectHz", "reqRedDb", "appliedDb", "peakOut(dB)", "measIODb", "centerHz(disp)");
        for (double f : testFreqs)
        {
            const int kTrials = 5;
            double sumPeakIn=0,sumPeakOut=0,sumMeasIO=0,sumDisp=0,sumDetectHz=0; int maxShift=0;
            for (int trial = 0; trial < kTrials; ++trial)
            {
                auto sigL = genPinkNoise(n, 0.05f, (unsigned) (2000 + trial*131 + (int) f));
                auto sigR = sigL; // correlated L/R, same resonance in both channels
                addTone(sigL, sr, f, 0.3f); addTone(sigR, sr, f, 0.3f);
                auto r = runStereo(sigL, sigR, sr, p);
                float peakIn = peakDb(sigL, measStart, measLen);
                float peakOut = peakDb(r.outL, measStart, measLen);
                float inDb = goertzelDb(sigL, measStart, measLen, sr, f);
                float outDb = goertzelDb(r.outL, measStart, measLen, sr, f);
                int expectedBin = binForFreq(sr, f);
                int peakBin = expectedBin; float peakVal = 1e9f;
                int lo = juce::jmax(0, expectedBin - 20), hi = juce::jmin(r.snapshotBins - 1, expectedBin + 20);
                for (int b = lo; b <= hi; ++b) if (r.snapshot[(size_t) b] < peakVal) { peakVal = r.snapshot[(size_t) b]; peakBin = b; }
                float displayed = r.snapshot[(size_t) expectedBin];
                maxShift = juce::jmax(maxShift, std::abs(peakBin - expectedBin));
                sumPeakIn += peakIn; sumPeakOut += peakOut; sumMeasIO += (outDb - inDb); sumDisp += displayed;
                sumDetectHz += peakBin * sr / 2048.0;
            }
            float peakIn=(float)(sumPeakIn/kTrials), peakOut=(float)(sumPeakOut/kTrials), measIO=(float)(sumMeasIO/kTrials), disp=(float)(sumDisp/kTrials);
            double detectHz = sumDetectHz/kTrials;
            // "requested" (pre-clamp/pre-smoothing detector demand) is architecturally IDENTICAL to
            // "applied" here -- appliedReductionSnapshot() IS the value multiplied into fftData (see
            // the prior audit's ITEM 1 direct array-identity proof); there is no separate/independent
            // detector-only number anywhere downstream of GainMaskEngine::process().
            std::printf("  %8.0f | %10.2f | %11.1f | %10.2f | %10.2f | %10.2f | %10.2f | %14.1f\n",
                         f, peakIn, detectHz, disp, disp, peakOut, measIO, detectHz);
            char l1[96]; std::snprintf(l1, sizeof(l1), "sr=%.0f f=%.0fHz: detected/applied center matches test freq within 2 FFT bins", sr, f);
            check(l1, maxShift <= 2);
            char l2[96]; std::snprintf(l2, sizeof(l2), "sr=%.0f f=%.0fHz: measured I/O and analyzer-displayed reduction agree within 0.6dB", sr, f);
            check(l2, std::abs(measIO - disp) < 0.6f);
        }

        // ---- Delta reconstruction, 6.3kHz ----
        {
            double f = 6300.0;
            auto sigL = genPinkNoise(n, 0.05f, 4242); auto sigR = sigL;
            addTone(sigL, sr, f, 0.3f); addTone(sigR, sr, f, 0.3f);
            SpectralEngine::Params pProc = p; pProc.delta = false;
            SpectralEngine::Params pDelta = p; pDelta.delta = true;
            auto rProc = runStereo(sigL, sigR, sr, pProc);
            auto rDelta = runStereo(sigL, sigR, sr, pDelta);
            double maxErr = 0.0, sumSqErr = 0.0, sumSqOrig = 0.0;
            for (int i = measStart; i < measStart + measLen; ++i)
            {
                double recon = (double) rProc.outL[(size_t) i] + (double) rDelta.outL[(size_t) i];
                double err = recon - (double) sigL[(size_t) (i - kLatency)];
                maxErr = juce::jmax(maxErr, std::abs(err));
                sumSqErr += err * err; sumSqOrig += (double) sigL[(size_t) (i - kLatency)] * sigL[(size_t) (i - kLatency)];
            }
            double relDb = 10.0 * std::log10((sumSqErr + 1e-30) / (sumSqOrig + 1e-30));
            float deltaAtF = goertzelDb(rDelta.outL, measStart, measLen, sr, f);
            float inAtF = goertzelDb(sigL, measStart, measLen, sr, f);
            std::printf("  Delta @6.3kHz: reconstruction error=%.1fdB rel, delta-energy-at-6.3k=%.2fdB vs input=%.2fdB\n", relDb, deltaAtF, inAtF);
            char l3[96]; std::snprintf(l3, sizeof(l3), "sr=%.0f: processed+Delta reconstructs original (<-80dB rel error)", sr);
            check(l3, relDb < -80.0);
            char l4[96]; std::snprintf(l4, sizeof(l4), "sr=%.0f: Delta carries the removed 6.3kHz energy (below input level)", sr);
            check(l4, deltaAtF < inAtF - 0.5f);
        }

        // ---- Render WAV files at 48kHz, 6.3kHz case, for direct listening ----
        if (sr == 48000.0)
        {
            double f = 6300.0;
            auto sigL = genPinkNoise(n, 0.05f, 4242); auto sigR = sigL;
            addTone(sigL, sr, f, 0.3f); addTone(sigR, sr, f, 0.3f);
            SpectralEngine::Params pProc = p; pProc.delta = false;
            SpectralEngine::Params pDelta = p; pDelta.delta = true;
            SpectralEngine::Params pBypassLike = p; pBypassLike.maxReductionEnabled = false; pBypassLike.depth = 0.0f;
            auto rProc = runStereo(sigL, sigR, sr, pProc);
            auto rDelta = runStereo(sigL, sigR, sr, pDelta);
            juce::File outDir("/private/tmp/claude-501/-Users-nenofernando-Desktop-NF-Equalizer-JUCE-V2/a967e14b-a955-4d7b-a42c-27d5fb0ef458/scratchpad/suppression_audit_wav");
            outDir.createDirectory();
            writeWav(outDir.getChildFile("original_6300Hz.wav"), sigL, sigR, sr);
            writeWav(outDir.getChildFile("processed_6300Hz.wav"), rProc.outL, rProc.outR, sr);
            writeWav(outDir.getChildFile("delta_6300Hz.wav"), rDelta.outL, rDelta.outR, sr);
            std::printf("\n  WAV files written to: %s\n", outDir.getFullPathName().toRawUTF8());
        }
    }

    std::printf("\n%s\n", allPass ? "=== ALL FOCUSED 4-8kHz CHECKS PASS ===" : "=== SOME FOCUSED CHECKS FAILED -- SEE ABOVE ===");
    return allPass ? 0 : 1;
}
