#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/ConfidenceEngine.h"
#include <cstdio>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp)
{
    double ph = 0.0, inc = juce::MathConstants<double>::twoPi * freq / sr;
    for (auto& s : b) { s += (float) std::sin(ph) * amp; ph += inc; }
}

int main()
{
    double sr = 48000.0; int fftSize = 2048, hop = 512;
    int n = (int) (sr * 1.0);
    auto sig = genSilence(n);
    double f0 = 110.0;
    for (int h = 1; h <= 12; ++h) addTone(sig, sr, f0 * h, 0.3f * (float) juce::Decibels::decibelsToGain(-2.0f * (h - 1)));

    SpectralProminenceEngineV5 prom; prom.prepare(fftSize / 2 + 1, sr, fftSize);
    prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
    ConfidenceEngine conf; conf.prepare(sr, fftSize, hop);

    juce::dsp::FFT fft(11);
    std::array<float, 2048> window{};
    for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f);
    std::array<float, 4096> scratch{};
    std::vector<float> magDb((size_t)(fftSize/2+1)), promOut((size_t)(fftSize/2+1));

    for (int i = 0; i + fftSize <= n; i += hop)
    {
        for (int k = 0; k < fftSize; ++k) scratch[(size_t)k] = sig[(size_t)(i+k)] * window[(size_t)k];
        std::fill(scratch.begin()+fftSize, scratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform(scratch.data());
        int bins = fftSize/2+1;
        for (int b = 0; b < bins; ++b) { float re = scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)]; magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/(float)fftSize+1e-12f,-120.0f); }
        prom.computeProminence(magDb, 4.0f, promOut);
        conf.process(promOut, nullptr);
    }

    int w = conf.lastF0WinnerIndex();
    std::printf("f0WinnerIndex=%d\n", w);
    if (w >= 0)
    {
        auto& c = conf.lastF0Candidates()[(size_t) w];
        std::printf("winner centerHz=%.2f matches=%d evidence=%.3f\n", c.centerHz, c.matches, c.evidence);
    }
    for (auto& r : conf.regions())
        if (r.active) std::printf("region freq=%.1f harmLike=%.3f\n", r.centerHz, r.harmonicLikelihood);
    return 0;
}
