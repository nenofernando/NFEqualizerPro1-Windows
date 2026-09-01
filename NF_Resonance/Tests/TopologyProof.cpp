#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/LowFrequencyHarmonicAnalyzer.h"
#include <vector>
#include <cstdio>
#include <cmath>

static std::vector<float> genSilence(int n) { return std::vector<float>((size_t) n, 0.0f); }
static void addTone(std::vector<float>& b, double sr, double freq, float amp) { double ph=0.0, inc=juce::MathConstants<double>::twoPi*freq/sr; for (auto& s : b) { s += (float)std::sin(ph)*amp; ph+=inc; } }
static std::vector<float> genHarmonicSeries(double sr, int n, double f0, float amp, int numH, float rolloffDb=3.0f) { auto b=genSilence(n); for(int h=1;h<=numH;++h) addTone(b,sr,f0*h,amp*(float)juce::Decibels::decibelsToGain(-rolloffDb*(h-1))); return b; }
static void addBurst(std::vector<float>& b, double sr, double freqHz, float amp, double Q, int seed)
{
    juce::Random rng(seed); double bwHz=freqHz/Q; int n=(int)b.size();
    for(int k=0;k<9;++k){ double t=(double)k/8.0-0.5, f=freqHz+t*bwHz; double ph=rng.nextDouble()*juce::MathConstants<double>::twoPi, inc=juce::MathConstants<double>::twoPi*f/sr;
        for(int i=0;i<n;++i){ b[(size_t)i]+=(float)std::sin(ph)*(amp/3.0f); ph+=inc; } }
}
static float crossoverWeight(float hz, float lowHz, float highHz) { if (hz<=lowHz) return 1.0f; if (hz>=highHz) return 0.0f; float t=(hz-lowHz)/(highHz-lowHz); return 1.0f-(t*t*(3.0f-2.0f*t)); }
static float resAdvW(double mainBinHz, double auxBinHz) { if (mainBinHz<=0.0) return 0.0f; return (float) juce::jlimit(0.0, 1.0, 1.0 - auxBinHz/mainBinHz); }

template <typename Arr>
static void listLocalMaxima(const char* label, const Arr& arr, double binHz, float floorDb, int maxBin)
{
    std::printf("  %s local maxima (floor=%.1fdB): ", label, floorDb);
    for (int b = 1; b < juce::jmin((int) arr.size() - 1, maxBin); ++b)
        if (arr[(size_t) b] > arr[(size_t) (b - 1)] && arr[(size_t) b] >= arr[(size_t) (b + 1)] && arr[(size_t) b] > floorDb)
            std::printf("[bin%d=%.1fHz,%.1fdB] ", b, b * binHz, arr[(size_t) b]);
    std::printf("\n");
}

int main()
{
    for (double sr : { 96000.0, 192000.0 })
    {
        std::printf("======== sr=%.0f ========\n", sr);
        for (auto pr : std::vector<std::pair<double,double>>{ {80,135}, {120,170} })
        {
            double f0 = pr.first, resHz = pr.second;
            int n = (int)(sr*0.8);
            auto sig = genHarmonicSeries(sr, n, f0, 0.3f, 6);
            addBurst(sig, sr, resHz, 0.5f, 8.0, 1);

            SpectralProminenceEngineV5 prom; prom.prepare(2048/2+1, sr, 2048);
            prom.setNarrowMethod(SpectralProminenceEngineV5::NarrowMethod::O1_RobustSideSlope);
            LowFrequencyHarmonicAnalyzer aux; aux.prepare(sr);
            juce::dsp::FFT fft(11);
            std::vector<float> window((size_t)2048);
            for(int i=0;i<2048;++i) window[(size_t)i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/2047.0f);
            std::vector<float> scratch((size_t)4096), magDb((size_t)(2048/2+1)), promOut((size_t)(2048/2+1)), blended((size_t)(2048/2+1));
            double mainBinHz = sr/2048.0;

            for (int i = 0; i+2048 <= n; i += 512)
            {
                for(int k=0;k<2048;++k) scratch[(size_t)k]=sig[(size_t)(i+k)]*window[(size_t)k];
                std::fill(scratch.begin()+2048, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform(scratch.data());
                int bins=2048/2+1;
                for(int b=0;b<bins;++b){ float re=scratch[(size_t)(2*b)], im=(b==0||b==bins-1)?0.0f:scratch[(size_t)(2*b+1)];
                    magDb[(size_t)b]=juce::Decibels::gainToDecibels(std::sqrt(re*re+im*im)/2048.0f+1e-12f,-120.0f); }
                prom.computeProminence(magDb, 4.0f, promOut);
                aux.pushSamples(sig.data()+i, 512);
            }
            // recompute the OLD bin-by-bin blend for comparison
            double auxBinHz = aux.analysisBinHz();
            float resAdv = resAdvW(mainBinHz, auxBinHz);
            blended = promOut;
            int blendLimit = juce::jmin((int)promOut.size()-2, (int)std::ceil(800.0/mainBinHz)+2);
            if (resAdv > 1e-4f)
                for (int b = 1; b <= blendLimit; ++b)
                {
                    float fw = crossoverWeight((float)(b*mainBinHz), 300.0f, 800.0f);
                    if (fw <= 1e-4f) continue;
                    float est, vrel, frel;
                    float auxDb = aux.auxProminenceFor((float)(b*mainBinHz), &est, &vrel, &frel);
                    float w = juce::jlimit(0.0f,1.0f, fw*resAdv*vrel); // simplified old-style weight for illustration
                    blended[(size_t)b] = w*auxDb + (1.0f-w)*promOut[(size_t)b];
                }

            std::printf(" -- f0=%.0f resonance=%.0f --\n", f0, resHz);
            listLocalMaxima("MAIN (raw, unblended)", promOut, mainBinHz, 2.0f, blendLimit+3);
            listLocalMaxima("OLD BLENDED (bin-by-bin)", blended, mainBinHz, 2.0f, blendLimit+3);
            listLocalMaxima("AUX (own domain)", aux.debugProminence(), auxBinHz, 2.0f, (int)(800.0/auxBinHz)+3);
        }
    }
    return 0;
}
