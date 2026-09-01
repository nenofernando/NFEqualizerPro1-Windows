// PHYSICAL B, Passo 2 (musical material) + Passo 3 (V2 prominence-only,
// same material) + Passo 4 (known controls) + Passo 5 (bass-focused A/B).
//
// Benchmark parameters: sample rate = 48000 Hz, FFT size = 2048,
// bin spacing = 48000/2048 = 23.4375 Hz/bin. V1 runs through the real
// NFResonanceAudioProcessor (its own internal STFT/hop, unrelated to this
// file's manual analysis STFT). V2 prominence-only uses an independent
// Hann-windowed 2048/512-hop STFT built in this file purely for spectral
// analysis -- it does not reconstruct audio and is not part of the plugin.
//
// Wording note: V1's prominence error growing with frequency (see the
// synthetic sweep report) is observed in that benchmark, not fitted --
// reported here as "error increases strongly with frequency in this
// benchmark", not "linearly", since no regression was run to justify that.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DSP/SpectralProminenceEngineV5.h"
#include <algorithm>
#include <map>

using juce::AudioBuffer;
static const double sr = 48000.0;
static const int fftSize = 2048, hop = 512;
static const int bins = fftSize / 2 + 1;
static const double binHz = sr / (double) fftSize;

// ---------------- Signal generators ----------------
static void addTone(AudioBuffer<float>& b, double freq, float amp, double phaseOffset = 0.0)
{
    int n = b.getNumSamples();
    double ph = phaseOffset, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (int i = 0; i < n; ++i) { float s = (float) std::sin(ph) * amp; for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, s); ph += inc; }
}
static void addNoise(AudioBuffer<float>& b, float amp, int seed)
{
    juce::Random rng(seed);
    for (int i = 0; i < b.getNumSamples(); ++i) { float x = (rng.nextFloat() * 2.0f - 1.0f) * amp; for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, x); }
}
static AudioBuffer<float> makeSilence(int n) { AudioBuffer<float> b(2, n); b.clear(); return b; }

static double bufRms(const AudioBuffer<float>& b) { double s = 0; long n = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) { double v = b.getSample(c, i); s += v * v; ++n; } return n ? std::sqrt(s / (double) n) : 0.0; }
static void normalizeRms(AudioBuffer<float>& b, float targetRms)
{
    double cur = bufRms(b);
    if (cur < 1e-12) return;
    float g = (float) (targetRms / cur);
    for (int c = 0; c < b.getNumChannels(); ++c) b.applyGain(c, 0, b.getNumSamples(), g);
}

// Continuous, deterministic noise generators (item 1 fix -- replaces the
// old discrete-tones approximation). Same seeded juce::Random per channel
// so the result is exactly reproducible run to run.
static AudioBuffer<float> genWhiteNoise(int n, float targetRms, int seed)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    for (int i = 0; i < n; ++i) { float x = rng.nextFloat() * 2.0f - 1.0f; for (int c = 0; c < 2; ++c) b.setSample(c, i, x); }
    normalizeRms(b, targetRms);
    return b;
}
// Paul Kellett's economy pink noise filter -- standard, deterministic given
// a seeded white-noise source, -3dB/octave (1/f amplitude, 1/f^2 power).
static AudioBuffer<float> genPinkNoise(int n, float targetRms, int seed)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (int i = 0; i < n; ++i)
    {
        float white = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        pink *= 0.11f;
        for (int c = 0; c < 2; ++c) b.setSample(c, i, pink);
    }
    normalizeRms(b, targetRms);
    return b;
}
// Brown-like/steep-tilted: simple leaky-integrator low-pass on white noise
// (-6dB/octave, 1/f^2 amplitude), DC-blocked so it doesn't just ramp away.
static AudioBuffer<float> genBrownNoise(int n, float targetRms, int seed)
{
    AudioBuffer<float> b(2, n);
    juce::Random rng(seed);
    float acc = 0, dcPrev = 0, dcState = 0;
    const float leak = 0.985f; // integrator leak, keeps it bounded
    for (int i = 0; i < n; ++i)
    {
        float white = rng.nextFloat() * 2.0f - 1.0f;
        acc = leak * acc + white;
        float x = acc;
        float dcBlocked = x - dcPrev + 0.995f * dcState; // one-pole DC blocker
        dcPrev = x; dcState = dcBlocked;
        for (int c = 0; c < 2; ++c) b.setSample(c, i, dcBlocked);
    }
    normalizeRms(b, targetRms);
    return b;
}
// Narrowband resonance burst for controlled A/B injection: a handful of
// deterministic tones spread within [freq*(1-1/(2Q)), freq*(1+1/(2Q))],
// approximating a resonance of the given Q at a KNOWN, chosen frequency
// (never exactly on a harmonic of the underlying musical content, per the
// corrected bass A/B test in item 2).
static AudioBuffer<float> genResonanceBurst(int n, double freqHz, float ampLinear, double Q, int seed)
{
    auto b = makeSilence(n);
    juce::Random rng(seed);
    double bwHz = freqHz / Q;
    const int nTones = 9;
    for (int k = 0; k < nTones; ++k)
    {
        double t = (double) k / (double) (nTones - 1) - 0.5; // -0.5..0.5
        double f = freqHz + t * bwHz;
        addTone(b, f, ampLinear / (float) std::sqrt((double) nTones), rng.nextDouble() * juce::MathConstants<double>::twoPi);
    }
    return b;
}
static AudioBuffer<float> genHarmonicSeries(int n, double fundamental, float amp, int numHarmonics, float rolloffDbPerHarm = 3.0f)
{
    auto b = makeSilence(n);
    for (int h = 1; h <= numHarmonics; ++h)
    {
        float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDbPerHarm * (h - 1));
        addTone(b, fundamental * h, a);
    }
    return b;
}
// Same, but one specific harmonic (1-indexed) gets an extra dB boost --
// H2 in the harmonic protection test (Passo 3).
static AudioBuffer<float> genHarmonicSeriesBoosted(int n, double fundamental, float amp, int numHarmonics, int boostHarmonic, float boostDb, float rolloffDbPerHarm = 3.0f)
{
    auto b = makeSilence(n);
    for (int h = 1; h <= numHarmonics; ++h)
    {
        float a = amp * (float) juce::Decibels::decibelsToGain(-rolloffDbPerHarm * (h - 1));
        if (h == boostHarmonic) a *= (float) juce::Decibels::decibelsToGain(boostDb);
        addTone(b, fundamental * h, a);
    }
    return b;
}
static AudioBuffer<float> genTransientBroadband(int n, int seed)
{
    auto b = makeSilence(n);
    juce::Random rng(seed);
    // A handful of short broadband clicks/transients spread through the buffer.
    int numClicks = juce::jmax(1, n / (int) (sr * 0.4));
    for (int c = 0; c < numClicks; ++c)
    {
        int pos = rng.nextInt(juce::jmax(1, n - 200));
        for (int i = 0; i < 200 && pos + i < n; ++i)
        {
            float env = std::exp(-((float) i) / 20.0f);
            float x = (rng.nextFloat() * 2.0f - 1.0f) * env * 0.8f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch) b.addSample(ch, pos + i, x);
        }
    }
    return b;
}
static AudioBuffer<float> genKick(int n)
{
    auto b = makeSilence(n);
    juce::Random rng(3);
    double periodSamples = sr * 0.8;
    for (int hit = 0; hit * periodSamples < n; ++hit)
    {
        int start = (int) (hit * periodSamples);
        for (int i = 0; i < (int) (sr * 0.3) && start + i < n; ++i)
        {
            double t = i / sr;
            double freq = 120.0 * std::exp(-t * 18.0) + 45.0; // pitch envelope typical of a kick
            float env = (float) std::exp(-t * 9.0);
            float s = (float) std::sin(juce::MathConstants<double>::twoPi * freq * t) * env * 0.9f;
            float click = (i < 30) ? (rng.nextFloat() * 2.0f - 1.0f) * env * 0.3f : 0.0f;
            for (int c = 0; c < b.getNumChannels(); ++c) { b.addSample(c, start + i, s); b.addSample(c, start + i, click); }
        }
    }
    return b;
}
static AudioBuffer<float> genBass(int n)
{
    auto b = makeSilence(n);
    double notes[] = { 55.0, 55.0, 73.42, 65.41 }; // A1, A1, D2, C2 -- simple bassline
    double noteLen = sr * 0.5;
    for (int i = 0; i < n; ++i)
    {
        int noteIdx = (int) (i / noteLen) % 4;
        double f = notes[noteIdx];
        double t = std::fmod((double) i, noteLen) / sr;
        float env = (float) juce::jmin(1.0, t * 30.0) * (float) std::exp(-t * 0.6);
        for (int h = 1; h <= 6; ++h)
        {
            float a = env * 0.25f * (float) juce::Decibels::decibelsToGain(-3.0 * (h - 1));
            float s = a * (float) std::sin(juce::MathConstants<double>::twoPi * f * h * i / sr);
            for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, s);
        }
    }
    return b;
}
static AudioBuffer<float> genVocal(int n, int seed)
{
    auto b = makeSilence(n);
    double fundamental = 130.0; // male voice-ish
    juce::Random rng(seed);
    double vibratoDepth = 3.0, vibratoRate = 5.5;
    for (int i = 0; i < n; ++i)
    {
        double t = i / sr;
        double f0 = fundamental + vibratoDepth * std::sin(juce::MathConstants<double>::twoPi * vibratoRate * t);
        float env = 0.5f + 0.1f * (float) std::sin(juce::MathConstants<double>::twoPi * 0.3 * t);
        for (int h = 1; h <= 14; ++h)
        {
            double hf = f0 * h;
            // Formant-ish emphasis around 700Hz and 1200Hz (very rough male-voice approximation)
            double formantGain = std::exp(-0.5 * std::pow((std::log2(hf) - std::log2(700.0)) / 0.6, 2.0)) * 0.6
                                + std::exp(-0.5 * std::pow((std::log2(hf) - std::log2(1200.0)) / 0.5, 2.0)) * 0.4
                                + 0.15;
            float a = env * 0.35f * (float) juce::Decibels::decibelsToGain(-2.5 * (h - 1)) * (float) formantGain;
            float s = a * (float) std::sin(juce::MathConstants<double>::twoPi * hf * t);
            for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, s);
        }
    }
    addNoise(b, 0.01f, seed + 1); // breath noise
    return b;
}
static AudioBuffer<float> genGuitar(int n)
{
    auto b = makeSilence(n);
    double notes[] = { 196.0, 246.94, 293.66, 220.0 }; // G3 B3 D4 A3 -ish, simple chord tones
    double noteLen = sr * 0.6;
    for (int i = 0; i < n; ++i)
    {
        int noteIdx = (int) (i / noteLen) % 4;
        double f = notes[noteIdx];
        double t = std::fmod((double) i, noteLen) / sr;
        float env = (float) std::exp(-t * 3.5); // plucked decay
        for (int h = 1; h <= 10; ++h)
        {
            float a = env * 0.3f * (float) juce::Decibels::decibelsToGain(-4.0 * (h - 1));
            float s = a * (float) std::sin(juce::MathConstants<double>::twoPi * f * h * i / sr);
            for (int c = 0; c < b.getNumChannels(); ++c) b.addSample(c, i, s);
        }
    }
    return b;
}
static AudioBuffer<float> mixBuffers(std::initializer_list<AudioBuffer<float>*> list, int n)
{
    AudioBuffer<float> out(2, n); out.clear();
    for (auto* b : list) for (int c = 0; c < 2; ++c) out.addFrom(c, 0, *b, c, 0, n);
    return out;
}

// ---------------- V1 analysis (real AudioProcessor) ----------------
struct BandStats { double meanAbsReduction = 0; double pctBinsActive = 0; int n = 0; };
struct V1Result
{
    double inputRms, outputRms, rmsLossDb, peakIn, peakOut, deltaRms, deltaOverOriginal;
    double maxReduction, meanActiveReduction, pctBinsActive;
    int regionCount; double meanRegionWidthHz, widestRegionHz;
    std::map<juce::String, BandStats> bands;
};
static double rms(const AudioBuffer<float>& b) { double s = 0; long n = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) { double v = b.getSample(c, i); s += v * v; ++n; } return n ? std::sqrt(s / (double) n) : 0.0; }
static double peakAbs(const AudioBuffer<float>& b) { double p = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) p = juce::jmax(p, (double) std::abs(b.getSample(c, i))); return p; }

static const std::pair<const char*, std::pair<double, double>> kBands[] = {
    { "20-100Hz", { 20, 100 } }, { "100-300Hz", { 100, 300 } }, { "300Hz-1kHz", { 300, 1000 } },
    { "1-4kHz", { 1000, 4000 } }, { "4-10kHz", { 4000, 10000 } }, { "10-20kHz", { 10000, 20000 } }
};

static V1Result analyzeV1(const AudioBuffer<float>& input)
{
    NFResonanceAudioProcessor proc;
    proc.state().getParameter("depth")->setValueNotifyingHost(proc.state().getParameter("depth")->convertTo0to1(5.0f));
    proc.state().getParameter("sharpness")->setValueNotifyingHost(proc.state().getParameter("sharpness")->convertTo0to1(4.0f));
    proc.state().getParameter("selectivity")->setValueNotifyingHost(proc.state().getParameter("selectivity")->convertTo0to1(5.0f));
    proc.state().getParameter("mix")->setValueNotifyingHost(1.0f);
    const int blockSize = 512;
    proc.prepareToPlay(sr, blockSize);
    AudioBuffer<float> wet(input);
    juce::MidiBuffer midi;

    // Aggregate reduction stats across frames.
    double sumAbsActive = 0; long nActive = 0; double maxRed = 0; long totalBins = 0;
    int regionSum = 0, regionCountFrames = 0; double widestHz = 0; double regionWidthSumHz = 0; int regionWidthN = 0;
    std::map<juce::String, double> bandSumAbs; std::map<juce::String, long> bandActiveCount; std::map<juce::String, long> bandTotalCount;
    for (auto& kb : kBands) { bandSumAbs[kb.first] = 0; bandActiveCount[kb.first] = 0; bandTotalCount[kb.first] = 0; }

    for (int pos = 0; pos < wet.getNumSamples(); pos += blockSize)
    {
        int len = juce::jmin(blockSize, wet.getNumSamples() - pos);
        AudioBuffer<float> blk(wet.getArrayOfWritePointers(), wet.getNumChannels(), pos, len);
        proc.processBlock(blk, midi);
        auto red = proc.engine().getLastReduction();
        if ((int) red.size() != bins) continue;
        int regionsThisFrame = 0; bool inRegion = false; int regionStart = 0;
        for (int b = 0; b < bins; ++b)
        {
            double hz = b * binHz;
            double a = std::abs(red[(size_t) b]);
            totalBins++;
            if (a > 0.5) { sumAbsActive += a; nActive++; maxRed = juce::jmax(maxRed, a); }
            if (a > 0.5 && ! inRegion) { inRegion = true; regionStart = b; }
            if ((a <= 0.5 || b == bins - 1) && inRegion) { inRegion = false; int w = b - regionStart; double wHz = w * binHz; regionWidthSumHz += wHz; regionWidthN++; widestHz = juce::jmax(widestHz, wHz); regionsThisFrame++; }
            for (auto& kb : kBands)
                if (hz >= kb.second.first && hz < kb.second.second)
                {
                    bandSumAbs[kb.first] += a; bandTotalCount[kb.first]++;
                    if (a > 0.5) bandActiveCount[kb.first]++;
                }
        }
        regionSum += regionsThisFrame; regionCountFrames++;
    }

    // Delta signal RMS (delta=true isolates what's being removed).
    NFResonanceAudioProcessor procDelta;
    procDelta.state().getParameter("depth")->setValueNotifyingHost(procDelta.state().getParameter("depth")->convertTo0to1(5.0f));
    procDelta.state().getParameter("sharpness")->setValueNotifyingHost(procDelta.state().getParameter("sharpness")->convertTo0to1(4.0f));
    procDelta.state().getParameter("selectivity")->setValueNotifyingHost(procDelta.state().getParameter("selectivity")->convertTo0to1(5.0f));
    procDelta.state().getParameter("mix")->setValueNotifyingHost(1.0f);
    procDelta.state().getParameter("delta")->setValueNotifyingHost(1.0f);
    procDelta.prepareToPlay(sr, blockSize);
    AudioBuffer<float> deltaBuf(input);
    for (int pos = 0; pos < deltaBuf.getNumSamples(); pos += blockSize)
    {
        int len = juce::jmin(blockSize, deltaBuf.getNumSamples() - pos);
        AudioBuffer<float> blk(deltaBuf.getArrayOfWritePointers(), deltaBuf.getNumChannels(), pos, len);
        procDelta.processBlock(blk, midi);
    }

    V1Result r{};
    r.inputRms = rms(input); r.outputRms = rms(wet);
    r.rmsLossDb = 20.0 * std::log10(juce::jmax(1e-12, r.outputRms) / juce::jmax(1e-12, r.inputRms));
    r.peakIn = peakAbs(input); r.peakOut = peakAbs(wet);
    r.deltaRms = rms(deltaBuf);
    r.deltaOverOriginal = r.inputRms > 1e-9 ? r.deltaRms / r.inputRms : 0.0;
    r.maxReduction = maxRed; r.meanActiveReduction = nActive ? sumAbsActive / (double) nActive : 0.0;
    r.pctBinsActive = totalBins ? 100.0 * (double) nActive / (double) totalBins : 0.0;
    r.regionCount = regionCountFrames ? (int) std::round((double) regionSum / (double) regionCountFrames) : 0;
    r.meanRegionWidthHz = regionWidthN ? regionWidthSumHz / regionWidthN : 0.0;
    r.widestRegionHz = widestHz;
    for (auto& kb : kBands)
    {
        BandStats bs; bs.n = (int) bandTotalCount[kb.first];
        bs.meanAbsReduction = bandTotalCount[kb.first] ? bandSumAbs[kb.first] / (double) bandTotalCount[kb.first] : 0.0;
        bs.pctBinsActive = bandTotalCount[kb.first] ? 100.0 * (double) bandActiveCount[kb.first] / (double) bandTotalCount[kb.first] : 0.0;
        r.bands[kb.first] = bs;
    }
    return r;
}

// ---------------- V2 prominence-only analysis (manual STFT) ----------------
struct V2Result
{
    double p50, p75, p90, p95, p99;
    double meanCandidateRegionsPerFrame, meanRegionWidthHz, widestRegionHz;
    std::map<juce::String, double> bandP90;
    // For peak-picking (item 4): per-bin time-averaged prominence and the
    // fraction of frames each bin exceeded the candidate threshold (used as
    // "temporal duration" of that bin's own elevated prominence).
    std::vector<double> avgProminencePerBin;
    std::vector<double> candidateFractionPerBin;
};
static V2Result analyzeV2(const AudioBuffer<float>& input, SpectralProminenceEngineV5& eng)
{
    juce::dsp::FFT fft(11); // 2^11 = 2048
    std::vector<float> window((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
    std::vector<float> fftBuf((size_t) fftSize * 2, 0.0f);
    std::vector<float> magDb((size_t) bins, -120.0f);
    std::vector<float> prom((size_t) bins, 0.0f);
    std::vector<float> allProm;
    std::map<juce::String, std::vector<float>> bandProm;
    for (auto& kb : kBands) bandProm[kb.first] = {};

    int n = input.getNumSamples();
    int regionSum = 0, frameCount = 0; double widestHz = 0, regionWidthSum = 0; int regionWidthN = 0;
    const double candidateThresholdDb = 3.0;
    std::vector<double> sumPromPerBin((size_t) bins, 0.0);
    std::vector<int> candidateFramesPerBin((size_t) bins, 0);

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
        eng.computeProminence(magDb, 4.0f, prom);
        int regionsThisFrame = 0; bool inRegion = false; int regionStart = 0;
        for (int b = 0; b < bins; ++b)
        {
            allProm.push_back(prom[(size_t) b]);
            double hz = b * binHz;
            for (auto& kb : kBands) if (hz >= kb.second.first && hz < kb.second.second) bandProm[kb.first].push_back(prom[(size_t) b]);
            sumPromPerBin[(size_t) b] += prom[(size_t) b];
            if (prom[(size_t) b] > candidateThresholdDb) candidateFramesPerBin[(size_t) b]++;
            if (prom[(size_t) b] > candidateThresholdDb && ! inRegion) { inRegion = true; regionStart = b; }
            if ((prom[(size_t) b] <= candidateThresholdDb || b == bins - 1) && inRegion) { inRegion = false; int w = b - regionStart; double wHz = w * binHz; regionWidthSum += wHz; regionWidthN++; widestHz = juce::jmax(widestHz, wHz); regionsThisFrame++; }
        }
        regionSum += regionsThisFrame; frameCount++;
    }

    auto percentile = [](std::vector<float> v, double p) { if (v.empty()) return 0.0; std::sort(v.begin(), v.end()); size_t idx = (size_t) juce::jlimit(0.0, (double) v.size() - 1, p * (v.size() - 1)); return (double) v[idx]; };
    V2Result r{};
    r.p50 = percentile(allProm, 0.50); r.p75 = percentile(allProm, 0.75); r.p90 = percentile(allProm, 0.90);
    r.p95 = percentile(allProm, 0.95); r.p99 = percentile(allProm, 0.99);
    r.meanCandidateRegionsPerFrame = frameCount ? (double) regionSum / (double) frameCount : 0.0;
    r.meanRegionWidthHz = regionWidthN ? regionWidthSum / regionWidthN : 0.0;
    r.widestRegionHz = widestHz;
    for (auto& kb : kBands) r.bandP90[kb.first] = percentile(bandProm[kb.first], 0.90);
    r.avgProminencePerBin.resize((size_t) bins);
    r.candidateFractionPerBin.resize((size_t) bins);
    for (int b = 0; b < bins; ++b)
    {
        r.avgProminencePerBin[(size_t) b] = frameCount ? sumPromPerBin[(size_t) b] / frameCount : 0.0;
        r.candidateFractionPerBin[(size_t) b] = frameCount ? 100.0 * candidateFramesPerBin[(size_t) b] / frameCount : 0.0;
    }
    return r;
}

// Peak-picking (item 4): finds the top-K local maxima of TIME-AVERAGED
// prominence, each with its own width (bins around it still above
// peak-3dB), temporal duration (% of frames that bin itself was a
// candidate), and distance to the nearest harmonic of fundamentalHz (0 =
// not applicable / no known fundamental for this signal). Explicitly NOT a
// classification -- a high value here means "measured prominent relative
// to local spectral context", nothing about whether that's musically
// desirable or not (that judgement doesn't exist until confidence/
// persistence/harmonic-protection are built).
// Direct readout at a KNOWN frequency (e.g. an injected resonance) --
// necessary because a weak-relative-to-context injection can legitimately
// fail to crack a top-K ranking dominated by strong harmonics, which would
// otherwise look like "the resonance wasn't detected" when it may simply
// still show real, above-baseline prominence at that exact bin.
static void printPromAt(const V2Result& r, double freqHz, const char* label)
{
    int bin = (int) std::round(freqHz / binHz);
    bin = juce::jlimit(0, bins - 1, bin);
    std::printf("    prominence AT %s (%.1fHz, bin %d): avg=%.2fdB  candidateFrames=%.1f%%\n",
        label, freqHz, bin, r.avgProminencePerBin[(size_t) bin], r.candidateFractionPerBin[(size_t) bin]);
}
static void printTopPeaks(const V2Result& r, double fundamentalHz, int topK = 5)
{
    struct Peak { int bin; double heightDb; };
    std::vector<Peak> peaks;
    for (int b = 1; b < bins - 1; ++b)
        if (r.avgProminencePerBin[(size_t) b] > r.avgProminencePerBin[(size_t) (b - 1)]
            && r.avgProminencePerBin[(size_t) b] >= r.avgProminencePerBin[(size_t) (b + 1)]
            && r.avgProminencePerBin[(size_t) b] > 1.0)
            peaks.push_back({ b, r.avgProminencePerBin[(size_t) b] });
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& z) { return a.heightDb > z.heightDb; });
    if ((int) peaks.size() > topK) peaks.resize((size_t) topK);
    std::printf("  top persistent peaks (avg prominence, not a defect judgement):\n");
    if (peaks.empty()) { std::printf("    (none above 1dB average)\n"); return; }
    for (auto& pk : peaks)
    {
        double hz = pk.bin * binHz;
        int lo = pk.bin, hi = pk.bin;
        double half = pk.heightDb - 3.0;
        while (lo > 0 && r.avgProminencePerBin[(size_t) (lo - 1)] > half) --lo;
        while (hi < bins - 1 && r.avgProminencePerBin[(size_t) (hi + 1)] > half) ++hi;
        double widthHz = (hi - lo + 1) * binHz;
        double durationPct = r.candidateFractionPerBin[(size_t) pk.bin];
        juce::String harmDist = "n/a";
        if (fundamentalHz > 1.0)
        {
            double nearestMult = std::round(hz / fundamentalHz);
            nearestMult = juce::jmax(1.0, nearestMult);
            double nearestHarmHz = nearestMult * fundamentalHz;
            harmDist = juce::String(hz - nearestHarmHz, 1) + "Hz from H" + juce::String((int) nearestMult);
        }
        std::printf("    %8.1fHz  avgProm=%6.2fdB  width=%7.1fHz  duration=%5.1f%%  distToHarmonic=%s\n",
            hz, pk.heightDb, widthHz, durationPct, harmDist.toRawUTF8());
    }
}

static void printV1(const char* name, const V1Result& r)
{
    std::printf("\n[V1] %s\n", name);
    std::printf("  input RMS=%.5f  output RMS=%.5f  RMS loss=%.2fdB  peakIn=%.4f  peakOut=%.4f\n", r.inputRms, r.outputRms, r.rmsLossDb, r.peakIn, r.peakOut);
    std::printf("  Delta RMS=%.5f  Delta/original=%.4f\n", r.deltaRms, r.deltaOverOriginal);
    std::printf("  max reduction=%.2fdB  mean active reduction=%.2fdB  %%bins active=%.2f%%\n", r.maxReduction, r.meanActiveReduction, r.pctBinsActive);
    std::printf("  regions/frame=%d  mean region width=%.1fHz  widest region=%.1fHz\n", r.regionCount, r.meanRegionWidthHz, r.widestRegionHz);
    std::printf("  per-band mean|reduction| / %%active:\n");
    for (auto& kb : kBands) { const auto& bs = r.bands.at(kb.first); std::printf("    %-12s %.3fdB  %.2f%%\n", kb.first, bs.meanAbsReduction, bs.pctBinsActive); }
}
static void printV2(const char* name, const V2Result& r)
{
    std::printf("[V2-A5C prominence-only] %s\n", name);
    std::printf("  P50=%.2f P75=%.2f P90=%.2f P95=%.2f P99=%.2f dB\n", r.p50, r.p75, r.p90, r.p95, r.p99);
    std::printf("  candidate regions/frame(>3dB)=%.2f  mean width=%.1fHz  widest=%.1fHz\n", r.meanCandidateRegionsPerFrame, r.meanRegionWidthHz, r.widestRegionHz);
    std::printf("  per-band P90:\n");
    for (auto& kb : kBands) std::printf("    %-12s %.2fdB\n", kb.first, r.bandP90.at(kb.first));
}

int main()
{
    std::printf("================================================================\n");
    std::printf("PHYSICAL B, Passo 2+3+4+5 -- musical material + controls, V1 vs V2-A5C\n");
    std::printf("sample rate=%.0fHz  FFT size=%d  bin spacing=%.4fHz/bin\n", sr, fftSize, binHz);
    std::printf("================================================================\n");

    SpectralProminenceEngineV5 v2;
    v2.prepare(bins, sr, fftSize);
    v2.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    jassert(v2.activeNarrowMethod() == SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);

    const int n = (int) (sr * 5.0); // 5s per signal

    struct Case { const char* name; AudioBuffer<float> buf; };
    std::vector<Case> cases;
    cases.push_back({ "Vocal", genVocal(n, 1) });
    cases.push_back({ "Bass", genBass(n) });
    cases.push_back({ "Kick", genKick(n) });
    cases.push_back({ "Guitar", genGuitar(n) });
    { auto v = genVocal(n, 2), ba = genBass(n), k = genKick(n), g = genGuitar(n);
      cases.push_back({ "Dense full mix", mixBuffers({ &v, &ba, &k, &g }, n) }); }

    // Item 1 fix: continuous, deterministic PRNG-based noise (white/pink/
    // brown), no discrete-tone approximation, RMS-normalized, fixed seeds.
    cases.push_back({ "CONTROL: white noise (continuous, no resonance)", genWhiteNoise(n, 0.2f, 100) });
    cases.push_back({ "CONTROL: pink noise (continuous, no resonance)", genPinkNoise(n, 0.2f, 101) });
    cases.push_back({ "CONTROL: brown-like/steep-tilted (continuous, no resonance)", genBrownNoise(n, 0.2f, 102) });
    cases.push_back({ "CONTROL: clean harmonic series (110Hz x16)", genHarmonicSeries(n, 110.0, 0.4f, 16) });
    { auto h = genHarmonicSeries(n, 110.0, 0.4f, 16);
      auto res = genResonanceBurst(n, 2800.0, 0.35f, 12.0, 200);
      cases.push_back({ "CONTROL: harmonic series + known non-harmonic resonance @2.8kHz Q12", mixBuffers({ &h, &res }, n) }); }
    cases.push_back({ "CONTROL: broadband transients (clicks)", genTransientBroadband(n, 5) });
    cases.push_back({ "CONTROL: bass fundamental+harmonics, no resonance", genHarmonicSeries(n, 60.0, 0.4f, 10) });

    for (auto& c : cases)
    {
        std::printf("\n================================================================\n%s\n================================================================\n", c.name);
        printV1(c.name, analyzeV1(c.buf));
        auto v2r = analyzeV2(c.buf, v2);
        printV2(c.name, v2r);
        printTopPeaks(v2r, 0.0); // no single fundamental applicable to these mixed/noise cases
    }

    // Passo 2 (item 5) fix: re-run Bass and Guitar in isolation to confirm
    // the earlier finding doesn't depend on the (now-replaced) noise
    // generator -- neither Bass nor Guitar ever used it, so this is a
    // reproducibility check, not expected to change.
    std::printf("\n================================================================\n");
    std::printf("Reconfirmation -- Bass and Guitar isolated re-run (unaffected by the\n");
    std::printf("noise-generator fix, included for end-to-end reproducibility)\n");
    std::printf("================================================================\n");
    printV1("Bass (re-run)", analyzeV1(genBass(n)));
    printV1("Guitar (re-run)", analyzeV1(genGuitar(n)));

    // Passo 2 fix (item 2): bass A/B test, resonance injected at a KNOWN
    // NON-HARMONIC frequency (never exactly on a partial), at two different
    // fundamentals, per the corrected spec.
    std::printf("\n================================================================\n");
    std::printf("Passo 5 (corrected) -- Bass A/B with NON-HARMONIC injected resonance\n");
    std::printf("================================================================\n");
    struct BassAB { double fundamental; double resonanceHz; };
    for (auto& ab : { BassAB{ 80.0, 135.0 }, BassAB{ 120.0, 170.0 } })
    {
        auto sceneA = genHarmonicSeries(n, ab.fundamental, 0.4f, 8); // natural harmonic series only
        auto res = genResonanceBurst(n, ab.resonanceHz, 0.35f, 10.0, 300);
        auto sceneB = mixBuffers({ &sceneA, &res }, n);

        auto v2a = analyzeV2(sceneA, v2);
        auto v2b = analyzeV2(sceneB, v2);
        std::printf("\n-- fundamental=%.0fHz, injected resonance=%.0fHz (non-harmonic) --\n", ab.fundamental, ab.resonanceHz);
        std::printf("Scenario A (natural harmonic series only):\n");
        printTopPeaks(v2a, ab.fundamental);
        printPromAt(v2a, ab.resonanceHz, "resonance freq (absent in A)");
        std::printf("Scenario B (same series + resonance @%.0fHz):\n", ab.resonanceHz);
        printTopPeaks(v2b, ab.fundamental);
        printPromAt(v2b, ab.resonanceHz, "injected resonance");
    }

    // Passo 3: H1 (clean harmonic series) / H2 (one partial boosted) / H3
    // (same series + non-harmonic resonance) -- comparing prominence maps
    // only, no reduction decision, groundwork for future Harmonic Protection.
    std::printf("\n================================================================\n");
    std::printf("Passo 3 -- H1/H2/H3 harmonic prominence comparison\n");
    std::printf("================================================================\n");
    const double hFund = 110.0;
    auto H1 = genHarmonicSeries(n, hFund, 0.4f, 12);
    auto H2 = genHarmonicSeriesBoosted(n, hFund, 0.4f, 12, 4, 8.0f); // 4th partial (440Hz) boosted +8dB
    AudioBuffer<float> H3;
    { auto h = genHarmonicSeries(n, hFund, 0.4f, 12);
      auto res = genResonanceBurst(n, 505.0, 0.3f, 15.0, 400); // between H4(440) and H5(550), non-harmonic
      H3 = mixBuffers({ &h, &res }, n); }

    auto v2H1 = analyzeV2(H1, v2), v2H2 = analyzeV2(H2, v2), v2H3 = analyzeV2(H3, v2);
    std::printf("\nH1 -- clean harmonic series (fundamental=%.0fHz):\n", hFund);
    printTopPeaks(v2H1, hFund);
    printPromAt(v2H1, hFund * 4.0, "partial #4 (440Hz, unboosted here)");
    std::printf("\nH2 -- same series, partial #4 (440Hz) boosted +8dB:\n");
    printTopPeaks(v2H2, hFund);
    printPromAt(v2H2, hFund * 4.0, "partial #4 (440Hz, +8dB boosted)");
    std::printf("\nH3 -- same series + non-harmonic resonance @505Hz:\n");
    printTopPeaks(v2H3, hFund);
    printPromAt(v2H1, 505.0, "505Hz in H1 (no injection, baseline)");
    printPromAt(v2H3, 505.0, "505Hz in H3 (injected resonance)");

    std::printf("\n================================================================\n");
    std::printf("Report complete. No production defaults changed by this run.\n");
    std::printf("No gain-reduction mask was implemented for V2 -- prominence-only,\n");
    std::printf("as instructed. Awaiting review before any prominence->reduction work.\n");
    std::printf("================================================================\n");
    return 0;
}
