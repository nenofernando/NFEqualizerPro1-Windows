// NF Resonance -- V2-A2 hybrid candidate-based engine validation.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV2.h"

static const double sr = 48000.0;
static const int fftSize = 2048;
static const int bins = fftSize/2+1;

static double binHz(int bin, double s=sr) { return bin * s / fftSize; }

static std::vector<float> makeFrame(double f0, double levelDb, double sigmaOct, double s=sr)
{
    std::vector<float> magDb((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz = juce::jmax(1.0, binHz(i,s));
        double octDist = std::log2(hz/f0);
        double bump = levelDb * std::exp(-0.5*(octDist/sigmaOct)*(octDist/sigmaOct));
        double baseline = -40.0 - 3.0*std::log2(hz/1000.0); // pink-like
        magDb[(size_t)i] = (float)(baseline + bump);
    }
    return magDb;
}
static int freqToBin(double f0, double s=sr) { return (int)std::round(f0*fftSize/s); }

int main()
{
    //======================================================================
    std::cout << "==================== 2. ADVERSARIAL: narrow resonance between grid points ====================\n";
    std::cout << "(inserting a narrow resonance at freq+offset, sweeping offset between adjacent grid points)\n\n";
    {
        double testFreqs[] = { 1000, 4000, 10000, 16000 };
        int ppoValues[] = { 36, 48, 72 };
        for (int ppo : ppoValues)
        {
            std::cout << "\n-- " << ppo << " PPO --\n";
            std::printf("%8s | %10s | %10s | %10s | %14s\n","freq","offset%%","true","hybrid-est","abs-err(dB)");
            for (double f0 : testFreqs)
            {
                SpectralProminenceEngineV2 eng;
                eng.prepare(bins, sr, fftSize, ppo);
                eng.setCandidateThresholdDb(2.5, 3.0);
                // grid spacing in Hz near f0
                double gridSpacingOct = 1.0/ppo;
                double gridSpacingHz = f0*(std::pow(2.0,gridSpacingOct)-1.0);
                std::vector<float> prom;
                for (double offsetFrac : {0.0, 0.25, 0.5, 0.75, 1.0})
                {
                    double testF = f0 + offsetFrac*gridSpacingHz;
                    auto frame = makeFrame(testF, 8.0, 0.02); // genuinely narrow, sigma=0.02oct
                    eng.computeProminenceHybrid(frame, 8.0f, prom);
                    int bin = freqToBin(testF);
                    double est = prom[(size_t)bin];
                    std::printf("%8.0f | %10.0f | %10.3f | %10.3f | %14.3f\n", f0, offsetFrac*100, 8.0, est, std::abs(est-8.0));
                }
            }
        }

        std::cout << "\n-- FULL-BIN REFERENCE (no grid) for comparison --\n";
        std::printf("%8s | %10s | %10s | %10s | %14s\n","freq","offset%%","true","fullbin-est","abs-err(dB)");
        for (double f0 : testFreqs)
        {
            SpectralProminenceEngineV2 eng; eng.prepare(bins, sr, fftSize, 48);
            std::vector<float> prom;
            for (double offsetFrac : {0.0, 0.5, 1.0})
            {
                double gridSpacingHz = f0*(std::pow(2.0,1.0/48.0)-1.0);
                double testF = f0 + offsetFrac*gridSpacingHz;
                auto frame = makeFrame(testF, 8.0, 0.02);
                eng.computeProminenceFullBinReference(frame, 8.0f, prom);
                int bin = freqToBin(testF);
                double est = prom[(size_t)bin];
                std::printf("%8.0f | %10.0f | %10.3f | %10.3f | %14.3f\n", f0, offsetFrac*100, 8.0, est, std::abs(est-8.0));
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== 5. SHARPNESS RETEST (new core/context design) ====================\n";
    {
        struct QDef{ const char* label; double sigmaOct; };
        QDef widths[] = { {"wide",0.45}, {"medium",0.12}, {"narrow",0.035}, {"xnarrow",0.01} };
        SpectralProminenceEngineV2 eng; eng.prepare(bins, sr, fftSize, 48);
        eng.setCandidateThresholdDb(2.5, 3.0);
        std::vector<float> prom;
        for (auto& w : widths)
        {
            std::cout << "\n-- width=" << w.label << " (sigma=" << w.sigmaOct << "oct), 1kHz +8dB --\n";
            auto frame = makeFrame(1000.0, 8.0, w.sigmaOct);
            int bin = freqToBin(1000.0);
            for (float sharp : {0.0f,2.5f,5.0f,7.5f,10.0f})
            {
                eng.computeProminenceHybrid(frame, sharp, prom);
                std::cout << "  Sharpness=" << sharp << ": estimated=" << prom[(size_t)bin] << "dB\n";
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== 6. HYBRID vs FULL-BIN REFERENCE accuracy (P25) ====================\n";
    {
        double freqs[] = { 120, 1000, 10000 };
        double levels[] = { 2, 4, 8, 12, 18 };
        struct QDef{ const char* label; double sigmaOct; };
        QDef widths[] = { {"wide",0.45}, {"medium",0.12}, {"narrow",0.035}, {"xnarrow",0.01} };
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        SpectralProminenceEngineV2 hybEng; hybEng.prepare(bins, sr, fftSize, 48); hybEng.setCandidateThresholdDb(2.5, 3.0);
        double sumRef=0, sumHyb=0, maxDiffHybVsRef=0; int n=0;
        std::vector<float> promRef, promHyb;
        for (double f0 : freqs) for (double level : levels) for (auto& w : widths)
        {
            auto frame = makeFrame(f0, level, w.sigmaOct);
            refEng.computeProminenceFullBinReference(frame, 8.0f, promRef);
            hybEng.computeProminenceHybrid(frame, 8.0f, promHyb);
            int bin = freqToBin(f0);
            double errRef = std::abs(promRef[(size_t)bin]-level);
            double errHyb = std::abs(promHyb[(size_t)bin]-level);
            double diff = std::abs(promHyb[(size_t)bin]-promRef[(size_t)bin]);
            sumRef+=errRef; sumHyb+=errHyb; maxDiffHybVsRef=juce::jmax(maxDiffHybVsRef,diff); ++n;
        }
        std::cout << "MAE full-bin reference: " << (sumRef/n) << " dB\n";
        std::cout << "MAE hybrid (grid+refine): " << (sumHyb/n) << " dB\n";
        std::cout << "Max |hybrid - reference| across all combos: " << maxDiffHybVsRef << " dB\n";
    }

    //======================================================================
    std::cout << "\n==================== 7. FAST APPROX (prefix-sum mean only) vs P25 ====================\n";
    {
        double freqs[] = { 120, 1000, 10000 };
        double levels[] = { 2, 4, 8, 12, 18 };
        struct QDef{ const char* label; double sigmaOct; };
        QDef widths[] = { {"wide",0.45}, {"medium",0.12}, {"narrow",0.035}, {"xnarrow",0.01} };
        SpectralProminenceEngineV2 eng; eng.prepare(bins, sr, fftSize, 48);
        double sumFast=0, sumRef=0; int n=0;
        std::vector<float> promFast, promRef;
        for (double f0 : freqs) for (double level : levels) for (auto& w : widths)
        {
            auto frame = makeFrame(f0, level, w.sigmaOct);
            eng.computeProminenceFastApprox(frame, 8.0f, promFast);
            eng.computeProminenceFullBinReference(frame, 8.0f, promRef);
            int bin = freqToBin(f0);
            sumFast += std::abs(promFast[(size_t)bin]-level);
            sumRef += std::abs(promRef[(size_t)bin]-level);
            ++n;
        }
        std::cout << "MAE fast-approx (mean, non-robust): " << (sumFast/n) << " dB\n";
        std::cout << "MAE P25 full-bin reference: " << (sumRef/n) << " dB\n";

        // CPU comparison
        auto frame = makeFrame(1000.0, 8.0, 0.12);
        std::vector<float> prom;
        const int iters = 200;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i=0;i<iters;++i) eng.computeProminenceFastApprox(frame, 4.0f, prom);
        auto t1 = std::chrono::high_resolution_clock::now();
        double usFast = std::chrono::duration<double,std::micro>(t1-t0).count()/iters;
        t0 = std::chrono::high_resolution_clock::now();
        for (int i=0;i<iters;++i) eng.computeProminenceFullBinReference(frame, 4.0f, prom);
        t1 = std::chrono::high_resolution_clock::now();
        double usRef = std::chrono::duration<double,std::micro>(t1-t0).count()/iters;
        std::cout << "CPU fast-approx: " << usFast << " us/frame(1ch).  CPU P25 full-bin: " << usRef << " us/frame(1ch).\n";
    }

    //======================================================================
    std::cout << "\n==================== 8/9/12. CPU: mean/median/P95/P99/max across scenarios & sample rates ====================\n";
    {
        auto genSilence = [&](int n)->std::vector<float>{ return std::vector<float>((size_t)n, -100.0f); };
        auto genPinkFrame = [&](double s)->std::vector<float>{
            std::vector<float> m((size_t)bins);
            juce::Random rng(7);
            for (int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); m[(size_t)i]=(float)(-40.0-3.0*std::log2(hz/1000.0)+ (rng.nextFloat()-0.5f)*6.0f); }
            return m;
        };
        auto genAdversarialDense = [&](double s)->std::vector<float>{
            // Many resonances at once (worst case for candidate count)
            std::vector<float> m((size_t)bins, -40.0f);
            double freqsAdv[] = {60,120,250,500,1000,1500,2000,3000,4000,5000,7000,9000,11000,14000,17000};
            for (double f : freqsAdv)
            {
                for (int i=0;i<bins;++i)
                {
                    double hz=juce::jmax(1.0,binHz(i,s));
                    double d=std::log2(hz/f);
                    m[(size_t)i]=juce::jmax(m[(size_t)i], (float)(-40.0+10.0*std::exp(-0.5*(d/0.03)*(d/0.03))));
                }
            }
            return m;
        };

        double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        struct Scenario{ const char* name; std::function<std::vector<float>(double)> gen; };
        Scenario scenarios[] = {
            {"silence", [&](double s){ (void)s; return genSilence(bins); }},
            {"pink noise", genPinkFrame},
            {"adversarial (15 resonances)", genAdversarialDense},
        };

        std::printf("%-30s | %10s | %10s | %10s | %10s | %10s | %10s | %14s\n",
            "scenario","sampleRate","mean(us)","median(us)","P95(us)","P99(us)","max(us)","%hopP99(2ch)");
        for (auto& scen : scenarios)
        {
            for (double testSr : rates)
            {
                SpectralProminenceEngineV2 eng; eng.prepare(bins, testSr, fftSize, 48);
                eng.setCandidateThresholdDb(2.5, 3.0);
                auto frame = scen.gen(testSr);
                std::vector<float> prom;
                const int iters = 300;
                std::vector<double> times; times.reserve(iters);
                for (int i=0;i<iters;++i)
                {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    eng.computeProminenceHybrid(frame, 4.0f, prom);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
                }
                std::sort(times.begin(), times.end());
                double mean=0; for(double t:times) mean+=t; mean/=times.size();
                double median = times[times.size()/2];
                double p95 = times[(size_t)(times.size()*0.95)];
                double p99 = times[(size_t)(times.size()*0.99)];
                double maxT = times.back();
                double hopUs = 1000.0*512.0/testSr;
                double pctP99_2ch = (p99*2.0)/hopUs*100.0;
                std::printf("%-30s | %10.0f | %10.2f | %10.2f | %10.2f | %10.2f | %10.2f | %14.2f\n",
                    scen.name, testSr, mean, median, p95, p99, maxT, pctP99_2ch);
            }
        }
        std::cout << "\nGoal: <25% hop budget at P99, preferably <15-20%. See recommendation in written report.\n";
    }

    return 0;
}
