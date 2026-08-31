// NF Resonance -- V2-A4 region budget sweep on worst-case content only.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV4.h"

static const double sr192 = 192000.0;
static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin) { return bin * sr192 / fftSize; }

int main()
{
    auto genPink=[&](){ std::vector<float> m((size_t)bins); juce::Random rng(1);
        for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i)); m[(size_t)i]=(float)(-40.0-3.0*std::log2(hz/1000.0))+(rng.nextFloat()-0.5f)*6.0f; } return m; };
    auto genGuitar=[&](){ std::vector<float> m((size_t)bins,-60.0f); juce::Random rng(3); double f0=220.0;
        for(int h=1;h<=15;++h){ double f=f0*h; if(f>15000)break; double envDb=-8.0-6.0*std::log2(f/f0);
            for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(envDb-80.0*std::abs(d))); } }
        for(int i=0;i<bins;++i) m[(size_t)i]+=(rng.nextFloat()-0.5f)*3.0f; return m; };
    auto genDrums=[&](){ std::vector<float> m((size_t)bins); juce::Random rng(4);
        for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i)); double base=hz<200?-15.0:-30.0-4.0*std::log2(hz/1000.0); m[(size_t)i]=(float)base+(rng.nextFloat()-0.5f)*8.0f; } return m; };
    auto genDenseMix=[&](){ auto v=genGuitar(); auto d=genDrums(); std::vector<float> m((size_t)bins);
        for(int i=0;i<bins;++i){ double sum=std::pow(10.0,v[(size_t)i]/10.0)+std::pow(10.0,d[(size_t)i]/10.0); m[(size_t)i]=(float)(10.0*std::log10(sum)); } return m; };

    struct Content{ const char* name; std::function<std::vector<float>()> gen; };
    Content contents[] = { {"pink noise",genPink},{"guitar",genGuitar},{"drums",genDrums},{"dense mix",genDenseMix} };
    int budgets[] = { 16, 24, 32, 48, 64, 96 };
    const double hopUs = 1000000.0*512.0/sr192;

    std::cout << "hop budget @192kHz = " << hopUs << " us. Target P99(2ch) < 667us.\n\n";
    std::printf("%-14s | %6s | %10s | %10s | %10s | %14s\n","content","budget","mean(2ch)","P99(2ch)","max(2ch)","%hopP99");
    for (auto& c : contents)
    {
        auto frame = c.gen();
        for (int budget : budgets)
        {
            SpectralProminenceEngineV4 eng; eng.prepare(bins, sr192, fftSize, 36, 72);
            eng.setNarrowScaleParams(0.04, 0.9);
            eng.setNarrowCandidateThresholdDb(2.5, 3.0);
            eng.setValleySplitThresholdDb(3.0);
            eng.setSmallRegionWidthBins(3);
            eng.setRegionBudget(budget);
            std::vector<float> prom;
            eng.computeProminence(frame, 4.0f, prom);
            std::vector<double> times;
            for (int i=0;i<300;++i)
            {
                auto t0=std::chrono::high_resolution_clock::now();
                eng.computeProminence(frame,4.0f,prom);
                auto t1=std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
            }
            std::sort(times.begin(),times.end());
            double mean=0; for(double t:times) mean+=t; mean/=times.size();
            double p99=times[(size_t)(times.size()*0.99)], mx=times.back();
            std::printf("%-14s | %6d | %10.2f | %10.2f | %10.2f | %13.2f%%\n", c.name, budget, mean, p99, mx, p99/hopUs*100.0);
        }
        std::cout << "\n";
    }
    return 0;
}
