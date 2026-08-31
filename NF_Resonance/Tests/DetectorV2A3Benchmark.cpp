// NF Resonance -- V2-A3 scale-specific engine validation.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV3.h"
#include "DSP/SpectralProminenceEngineV2.h" // for full-P25-all-scales / hybrid comparison

static const double sr = 48000.0;
static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin, double s=sr) { return bin * s / fftSize; }
static int freqToBin(double f0, double s=sr) { return (int)std::round(f0*fftSize/s); }

static std::vector<float> makeFrame(double f0, double levelDb, double sigmaOct, double baseSlope=-3.0, double s=sr)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,s));
        double d=std::log2(hz/f0);
        double bump = levelDb*std::exp(-0.5*(d/sigmaOct)*(d/sigmaOct));
        m[(size_t)i]=(float)(-40.0 + baseSlope*std::log2(hz/1000.0) + bump);
    }
    return m;
}
static std::vector<float> makeSlopeChangeFrame(double s=sr)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,s));
        double v = hz<500 ? (-40.0 - 3.0*std::log2(hz/1000.0)) : (-40.0 + 8.0*std::log2(hz/500.0)/std::log2(20000.0/500.0));
        m[(size_t)i]=(float)v;
    }
    return m;
}
static std::vector<float> makeBroadHumpFrame(double centerHz, double levelDb, double s=sr)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,s));
        double d=std::log2(hz/centerHz);
        double bump = levelDb*std::exp(-0.5*(d/1.0)*(d/1.0)); // 1-octave-ish broad hump
        m[(size_t)i]=(float)(-40.0 - 3.0*std::log2(hz/1000.0) + bump);
    }
    return m;
}

static SpectralProminenceEngineV3 makeEngine(SpectralProminenceEngineV3::BroadMethod bm, SpectralProminenceEngineV3::MediumMethod mm)
{
    SpectralProminenceEngineV3 eng;
    eng.prepare(bins, sr, fftSize, 36, 72);
    eng.setBroadMethod(bm); eng.setMediumMethod(mm);
    eng.setNarrowCandidateThresholdDb(2.5, 3.0);
    eng.setNarrowRefinementBudget(128);
    return eng;
}

int main()
{
    //======================================================================
    std::cout << "==================== 12. BROAD ESTIMATOR COMPARISON (tilt/hump/dip/slope-change) ====================\n";
    {
        using BM = SpectralProminenceEngineV3::BroadMethod;
        struct M{ const char* name; BM m; };
        M methods[] = { {"UnweightedMean",BM::UnweightedMean}, {"WinsorizedMean",BM::WinsorizedMean},
                        {"ZScoreTrimmedMean",BM::ZScoreTrimmedMean}, {"RobustLinearRegression",BM::RobustLinearRegression} };
        std::cout << "\n-- tilt tracking: pure baseline (no resonance), does BROAD stay near 0 despite slope? --\n";
        std::printf("%-24s | %10s | %10s | %10s\n","method","-3dB/oct","+3dB/oct","slope-change");
        for (auto& mm : methods)
        {
            auto eng = makeEngine(mm.m, SpectralProminenceEngineV3::MediumMethod::CheapMean);
            std::vector<float> prom;
            auto f1=makeFrame(1000,0.0,0.1,-3.0); eng.computeProminence(f1,0.0f,prom); double e1=prom[(size_t)freqToBin(1000)];
            auto f2=makeFrame(1000,0.0,0.1,3.0);  eng.computeProminence(f2,0.0f,prom); double e2=prom[(size_t)freqToBin(1000)];
            auto f3=makeSlopeChangeFrame();       eng.computeProminence(f3,0.0f,prom); double e3=prom[(size_t)freqToBin(1000)];
            std::printf("%-24s | %10.4f | %10.4f | %10.4f\n", mm.name, e1, e2, e3);
        }
        std::cout << "\n-- broad hump/dip detection (should DETECT these, unlike tilt) --\n";
        std::printf("%-24s | %10s | %10s | %10s | %10s\n","method","hump+2dB","hump+4dB","hump+8dB","dip-4dB");
        for (auto& mm : methods)
        {
            auto eng = makeEngine(mm.m, SpectralProminenceEngineV3::MediumMethod::CheapMean);
            std::vector<float> prom;
            auto f1=makeBroadHumpFrame(1000,2.0); eng.computeProminence(f1,0.0f,prom); double e1=prom[(size_t)freqToBin(1000)];
            auto f2=makeBroadHumpFrame(1000,4.0); eng.computeProminence(f2,0.0f,prom); double e2=prom[(size_t)freqToBin(1000)];
            auto f3=makeBroadHumpFrame(1000,8.0); eng.computeProminence(f3,0.0f,prom); double e3=prom[(size_t)freqToBin(1000)];
            auto f4=makeBroadHumpFrame(1000,-4.0); eng.computeProminence(f4,0.0f,prom); double e4=prom[(size_t)freqToBin(1000)];
            std::printf("%-24s | %10.4f | %10.4f | %10.4f | %10.4f\n", mm.name, e1, e2, e3, e4);
        }
        // CPU
        std::cout << "\n-- CPU (grid-only cost, PPO=36, us/frame 1ch) --\n";
        for (auto& mm : methods)
        {
            auto eng = makeEngine(mm.m, SpectralProminenceEngineV3::MediumMethod::CheapMean);
            auto f = makeFrame(1000,4.0,0.3,-3.0);
            std::vector<float> prom;
            eng.computeProminence(f,4.0f,prom);
            const int iters=300;
            auto t0=std::chrono::high_resolution_clock::now();
            for(int i=0;i<iters;++i) eng.computeProminence(f,4.0f,prom);
            auto t1=std::chrono::high_resolution_clock::now();
            std::cout << "  " << mm.name << ": " << (std::chrono::duration<double,std::micro>(t1-t0).count()/iters) << " us/frame\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== MEDIUM: CheapMean(grid) vs Percentile(full-bin) ====================\n";
    {
        double freqs[] = {120,1000,10000};
        double levels[] = {2,4,8,12};
        auto engCheap = makeEngine(SpectralProminenceEngineV3::BroadMethod::RobustLinearRegression, SpectralProminenceEngineV3::MediumMethod::CheapMean);
        auto engP25   = makeEngine(SpectralProminenceEngineV3::BroadMethod::RobustLinearRegression, SpectralProminenceEngineV3::MediumMethod::Percentile);
        std::vector<float> pc, pp;
        double sumC=0,sumP=0; int n=0;
        for (double f0:freqs) for (double lvl:levels)
        {
            auto frame = makeFrame(f0, lvl, 0.12); // "medium" width resonance
            engCheap.computeProminence(frame, 5.0f, pc);
            engP25.computeProminence(frame, 5.0f, pp);
            int bin=freqToBin(f0);
            sumC += std::abs(pc[(size_t)bin]-lvl); sumP += std::abs(pp[(size_t)bin]-lvl); ++n;
        }
        std::cout << "MAE MEDIUM=CheapMean(grid72): " << (sumC/n) << " dB\n";
        std::cout << "MAE MEDIUM=Percentile(full-bin): " << (sumP/n) << " dB\n";
        auto f=makeFrame(1000,8.0,0.12);
        const int iters=200; std::vector<float> pr;
        auto t0=std::chrono::high_resolution_clock::now(); for(int i=0;i<iters;++i) engCheap.computeProminence(f,5.0f,pr); auto t1=std::chrono::high_resolution_clock::now();
        std::cout << "CPU CheapMean(grid): " << (std::chrono::duration<double,std::micro>(t1-t0).count()/iters) << " us/frame\n";
        t0=std::chrono::high_resolution_clock::now(); for(int i=0;i<iters;++i) engP25.computeProminence(f,5.0f,pr); t1=std::chrono::high_resolution_clock::now();
        std::cout << "CPU Percentile(full-bin): " << (std::chrono::duration<double,std::micro>(t1-t0).count()/iters) << " us/frame\n";
    }

    //======================================================================
    std::cout << "\n==================== 13. NARROW ACCURACY (V2-A3 vs P25 full reference) ====================\n";
    {
        double freqs[] = {250,1000,4000,10000,16000};
        double levels[] = {2,4,8,12};
        auto eng = makeEngine(SpectralProminenceEngineV3::BroadMethod::RobustLinearRegression, SpectralProminenceEngineV3::MediumMethod::CheapMean);
        SpectralProminenceEngineV2 ref; ref.prepare(bins, sr, fftSize, 48);
        std::vector<float> pv3, pref;
        double sumV3=0,sumRef=0; int n=0;
        std::printf("%8s | %6s | %10s | %10s | %10s\n","freq","level","true","V2-A3-est","fullP25-est");
        for (double f0:freqs) for (double lvl:levels)
        {
            auto frame = makeFrame(f0, lvl, 0.035); // narrow
            eng.computeProminence(frame, 10.0f, pv3);
            ref.computeProminenceFullBinReference(frame, 10.0f, pref);
            int bin=freqToBin(f0);
            sumV3 += std::abs(pv3[(size_t)bin]-lvl); sumRef += std::abs(pref[(size_t)bin]-lvl); ++n;
            if (lvl==2.0 || lvl==8.0)
                std::printf("%8.0f | %6.1f | %10.3f | %10.3f | %10.3f\n", f0, lvl, lvl, pv3[(size_t)bin], pref[(size_t)bin]);
        }
        std::cout << "MAE V2-A3 (bounded budget, sharpness=10): " << (sumV3/n) << " dB\n";
        std::cout << "MAE full P25 reference: " << (sumRef/n) << " dB\n";

        std::cout << "\n-- narrow-between-grid-points (grid affects BROAD/MEDIUM only; NARROW is full-bin) --\n";
        std::printf("%8s | %10s | %10s | %10s | %14s\n","freq","offset%%","true","V2A3-est","abs-err(dB)");
        for (double f0 : {1000.0, 4000.0, 10000.0, 16000.0})
        {
            double gridSpacingHz = f0*(std::pow(2.0,1.0/36.0)-1.0); // even at coarse BROAD grid spacing
            for (double offsetFrac : {0.0, 0.5, 1.0})
            {
                double testF = f0 + offsetFrac*gridSpacingHz;
                auto frame = makeFrame(testF, 8.0, 0.02);
                eng.computeProminence(frame, 10.0f, pv3);
                int bin = freqToBin(testF);
                std::printf("%8.0f | %10.0f | %10.3f | %10.3f | %14.3f\n", f0, offsetFrac*100, 8.0, pv3[(size_t)bin], std::abs(pv3[(size_t)bin]-8.0));
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== 14. SHARPNESS RETEST (V2-A3, scale-specific) ====================\n";
    {
        auto eng = makeEngine(SpectralProminenceEngineV3::BroadMethod::RobustLinearRegression, SpectralProminenceEngineV3::MediumMethod::CheapMean);
        std::vector<float> prom;
        std::cout << "\n-- broad hump +6dB @1kHz (should dominate at Sharpness=0) --\n";
        auto fB = makeBroadHumpFrame(1000, 6.0);
        for (float sh : {0.0f,5.0f,10.0f}) { eng.computeProminence(fB,sh,prom); std::cout << "  Sharpness="<<sh<<": "<<prom[(size_t)freqToBin(1000)]<<"dB\n"; }
        std::cout << "\n-- narrow resonance +8dB @1kHz sigma=0.02oct (should dominate at Sharpness=10) --\n";
        auto fN = makeFrame(1000, 8.0, 0.02);
        for (float sh : {0.0f,5.0f,10.0f}) { eng.computeProminence(fN,sh,prom); std::cout << "  Sharpness="<<sh<<": "<<prom[(size_t)freqToBin(1000)]<<"dB\n"; }
    }

    //======================================================================
    std::cout << "\n==================== 15/16. CPU WORST-CASE (mean/P95/P99/max, 2ch@192kHz, budget=128) ====================\n";
    {
        auto genSilence=[&](double){ return std::vector<float>((size_t)bins,-100.0f); };
        auto genPink=[&](double s){ std::vector<float> m((size_t)bins); juce::Random rng(1);
            for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); m[(size_t)i]=(float)(-40.0-3.0*std::log2(hz/1000.0))+(rng.nextFloat()-0.5f)*6.0f; } return m; };
        auto genVoice=[&](double s){ std::vector<float> m((size_t)bins,-60.0f); juce::Random rng(2); double f0=180.0;
            for(int h=1;h<=25;++h){ double f=f0*h; if(f>18000)break; double envDb=-10.0-8.0*std::log2(f/f0);
                for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(envDb-60.0*std::abs(d))); } }
            for(int i=0;i<bins;++i) m[(size_t)i]+=(rng.nextFloat()-0.5f)*2.0f; return m; };
        auto genGuitar=[&](double s){ std::vector<float> m((size_t)bins,-60.0f); juce::Random rng(3); double f0=220.0;
            for(int h=1;h<=15;++h){ double f=f0*h; if(f>15000)break; double envDb=-8.0-6.0*std::log2(f/f0);
                for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(envDb-80.0*std::abs(d))); } }
            for(int i=0;i<bins;++i) m[(size_t)i]+=(rng.nextFloat()-0.5f)*3.0f; return m; };
        auto genDrums=[&](double s){ std::vector<float> m((size_t)bins); juce::Random rng(4);
            for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double base=hz<200?-15.0:-30.0-4.0*std::log2(hz/1000.0); m[(size_t)i]=(float)base+(rng.nextFloat()-0.5f)*8.0f; } return m; };
        auto genDenseMix=[&](double s){ auto v=genVoice(s); auto g=genGuitar(s); auto d=genDrums(s); std::vector<float> m((size_t)bins);
            for(int i=0;i<bins;++i){ double sum=std::pow(10.0,v[(size_t)i]/10.0)+std::pow(10.0,g[(size_t)i]/10.0)+std::pow(10.0,d[(size_t)i]/10.0); m[(size_t)i]=(float)(10.0*std::log10(sum)); } return m; };
        auto genAdvHarmonic=[&](double s){ std::vector<float> m((size_t)bins,-60.0f); double f0=110.0;
            for(int h=1;h<=40;++h){ double f=f0*h; if(f>19000) break; for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-20.0-40.0*std::abs(d))); } } return m; };
        auto genAdvMany=[&](double s){ std::vector<float> m((size_t)bins,-40.0f); double freqsAdv[]={60,120,250,500,1000,1500,2000,3000,4000,5000,7000,9000,11000,14000,17000};
            for(double f:freqsAdv) for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-40.0+10.0*std::exp(-0.5*(d/0.03)*(d/0.03)))); } return m; };

        struct Content{ const char* name; std::function<std::vector<float>(double)> gen; };
        Content contents[] = {
            {"silence",genSilence},{"pink noise",genPink},{"vocal",genVoice},{"guitar",genGuitar},
            {"drums",genDrums},{"dense mix",genDenseMix},{"adversarial harmonic stack",genAdvHarmonic},{"adversarial many resonances",genAdvMany},
        };
        const double hopUs = 1000000.0*512.0/192000.0;
        std::cout << "hop budget @192kHz = " << hopUs << " us. Target: P99(2ch) < 667us, ideal 400-530us.\n\n";
        std::printf("%-30s | %10s | %10s | %10s | %10s | %14s | %12s\n","content","mean(2ch)","P95(2ch)","P99(2ch)","max(2ch)","%hopP99","narrowRefined");
        for (auto& c : contents)
        {
            SpectralProminenceEngineV3 e2; e2.prepare(bins, 192000.0, fftSize, 36, 72);
            e2.setBroadMethod(SpectralProminenceEngineV3::BroadMethod::UnweightedMean);
            e2.setMediumMethod(SpectralProminenceEngineV3::MediumMethod::CheapMean);
            e2.setNarrowCandidateThresholdDb(2.5,3.0); e2.setNarrowRefinementBudget(128);
            auto frame = c.gen(192000.0);
            std::vector<float> prom;
            e2.computeProminence(frame,4.0f,prom); // warm
            std::vector<double> times;
            for (int i=0;i<300;++i)
            {
                auto t0=std::chrono::high_resolution_clock::now();
                e2.computeProminence(frame,4.0f,prom);
                auto t1=std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
            }
            std::sort(times.begin(),times.end());
            double mean=0; for(double t:times) mean+=t; mean/=times.size();
            double p95=times[(size_t)(times.size()*0.95)], p99=times[(size_t)(times.size()*0.99)], mx=times.back();
            std::printf("%-30s | %10.2f | %10.2f | %10.2f | %10.2f | %13.2f%% | %12d\n",
                c.name, mean, p95, p99, mx, p99/hopUs*100.0, e2.lastNarrowRefinedCount());
        }
    }

    return 0;
}
