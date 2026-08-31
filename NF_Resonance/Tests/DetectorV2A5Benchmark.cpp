// NF Resonance -- V2-A5 deterministic fast prominence engine validation.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV5.h"
#include "DSP/SpectralProminenceEngineV2.h" // P25 offline oracle

static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin, double sr) { return bin * sr / fftSize; }
static int freqToBin(double f0, double sr) { return (int)std::round(f0*fftSize/sr); }

static void assertHopUnits()
{
    double hopUs = 1000000.0 * 512.0 / 192000.0;
    if (std::abs(hopUs - 2666.6667) > 0.01) { std::cout << "FATAL hop units bug!\n"; std::exit(1); }
    std::cout << "Units OK: hopUs(512@192kHz)=" << hopUs << "us\n\n";
}

static std::vector<float> makeSynthetic(double sr, double f0, double levelDb, double sigmaOct, double slopeDbOct=-3.0)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,sr));
        double d=std::log2(hz/f0);
        double bump = levelDb*std::exp(-0.5*(d/sigmaOct)*(d/sigmaOct));
        m[(size_t)i]=(float)(-40.0 + slopeDbOct*std::log2(hz/1000.0) + bump);
    }
    return m;
}

static SpectralProminenceEngineV5 makeEngine(double sr, SpectralProminenceEngineV5::NarrowMethod m)
{
    SpectralProminenceEngineV5 eng;
    eng.prepare(bins, sr, fftSize, 36, 72);
    eng.setNarrowScaleParams(0.04, 0.9);
    eng.setNarrowMethod(m);
    return eng;
}
static const char* methodName(SpectralProminenceEngineV5::NarrowMethod m)
{
    using M = SpectralProminenceEngineV5::NarrowMethod;
    switch(m){ case M::SidebandMean:return "SidebandMean"; case M::SidebandMin:return "SidebandMin";
        case M::WinsorizedSideband:return "WinsorizedSideband"; default:return "MeanOfGroupMeans"; }
}

int main()
{
    assertHopUnits();
    const double sr = 192000.0;

    //======================================================================
    std::cout << "==================== 5/7. NARROW METHOD COMPARISON vs P25 ORACLE (pure synthetic) ====================\n";
    {
        SpectralProminenceEngineV5::NarrowMethod methods[] = {
            SpectralProminenceEngineV5::NarrowMethod::SidebandMean,
            SpectralProminenceEngineV5::NarrowMethod::SidebandMin,
            SpectralProminenceEngineV5::NarrowMethod::WinsorizedSideband,
            SpectralProminenceEngineV5::NarrowMethod::MeanOfGroupMeans,
        };
        double freqs[] = {120,250,1000,4000,10000,16000};
        double levels[] = {2,4,8,12,18};
        struct QDef{ const char* label; double sigmaOct; };
        QDef widths[] = { {"wide",0.45},{"medium",0.12},{"narrow",0.035},{"xnarrow",0.01} };

        std::printf("%-18s | %10s | %10s | %10s | %12s\n","method","MAE(dB)","P95err(dB)","maxErr(dB)","bias(dB)");
        for (auto m : methods)
        {
            auto eng = makeEngine(sr, m);
            std::vector<double> errs; double sumBias=0; int n=0;
            std::vector<float> prom;
            for (double f0:freqs) for (double lvl:levels) for (auto& w:widths)
            {
                auto frame = makeSynthetic(sr, f0, lvl, w.sigmaOct);
                eng.computeProminence(frame, 5.0f, prom); // sharpness=5, balanced blend for this comparison
                int bin=freqToBin(f0,sr);
                double est = prom[(size_t)bin];
                errs.push_back(std::abs(est-lvl));
                sumBias += (est-lvl); ++n;
            }
            std::sort(errs.begin(),errs.end());
            double mae=0; for(double e:errs) mae+=e; mae/=errs.size();
            double p95=errs[(size_t)(errs.size()*0.95)], mx=errs.back();
            std::printf("%-18s | %10.4f | %10.4f | %10.4f | %12.4f\n", methodName(m), mae, p95, mx, sumBias/n);
        }
    }

    //======================================================================
    std::cout << "\n==================== 11. FREQUENCY CONSISTENCY (+2dB, winner method) ====================\n";
    SpectralProminenceEngineV5::NarrowMethod winner = SpectralProminenceEngineV5::NarrowMethod::WinsorizedSideband; // provisional; see printed MAE above to confirm
    {
        auto eng = makeEngine(sr, winner);
        double freqs[] = {120,250,1000,4000,10000,16000};
        std::vector<float> prom;
        std::printf("%8s | %10s | %10s | %12s\n","freq","true","estimated","note");
        for (double f0 : freqs)
        {
            auto frame = makeSynthetic(sr, f0, 2.0, 0.12); // medium width
            eng.computeProminence(frame, 5.0f, prom);
            int bin = freqToBin(f0,sr);
            auto geo = eng.scaleGeometryAt(bin, SpectralProminenceEngineV5::Narrow);
            std::printf("%8.0f | %10.3f | %10.3f | %12s\n", f0, 2.0, prom[(size_t)bin], geo.contextResolutionLimited?"RESOLUTION LIMITED":"");
        }
    }

    //======================================================================
    std::cout << "\n==================== 10. MONOTONIC RESPONSE ====================\n";
    {
        auto eng = makeEngine(sr, winner);
        double freqs[] = {120,250,1000,4000,10000,16000};
        double levels[] = {2,4,8,12,18};
        std::vector<float> prom;
        for (double f0 : freqs)
        {
            std::cout << "  " << f0 << "Hz: ";
            bool monotonic = true; double prev=-1e9;
            for (double lvl : levels)
            {
                auto frame = makeSynthetic(sr, f0, lvl, 0.12);
                eng.computeProminence(frame, 5.0f, prom);
                double est = prom[(size_t)freqToBin(f0,sr)];
                std::cout << "score(+" << lvl << ")=" << est << " ";
                if (est <= prev) monotonic = false;
                prev = est;
            }
            std::cout << (monotonic?"[MONOTONIC OK]":"[NOT MONOTONIC]") << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 12. SHARPNESS DIRECTION CHECK ====================\n";
    {
        auto eng = makeEngine(sr, winner);
        std::vector<float> prom;
        std::cout << "broad hump +6dB @1kHz (expect DECREASING score as sharpness rises):\n";
        auto fB = makeSynthetic(sr, 1000, 6.0, 1.0); // broad
        for (float sh : {0.0f,5.0f,10.0f}) { eng.computeProminence(fB,sh,prom); std::cout << "  Sharpness="<<sh<<": "<<prom[(size_t)freqToBin(1000,sr)]<<"dB\n"; }
        std::cout << "narrow resonance +8dB @1kHz (expect INCREASING/stable-high score as sharpness rises):\n";
        auto fN = makeSynthetic(sr, 1000, 8.0, 0.02); // narrow
        for (float sh : {0.0f,5.0f,10.0f}) { eng.computeProminence(fN,sh,prom); std::cout << "  Sharpness="<<sh<<": "<<prom[(size_t)freqToBin(1000,sr)]<<"dB\n"; }
    }

    //======================================================================
    std::cout << "\n==================== 8. PAIRED DENSE-MATERIAL TEST (base vs base+injection, delta) ====================\n";
    {
        auto genGuitar=[&](double s, int seed=3){ std::vector<float> m((size_t)bins,-60.0f); juce::Random rng(seed); double f0=220.0;
            for(int h=1;h<=15;++h){ double f=f0*h; if(f>15000)break; double envDb=-8.0-6.0*std::log2(f/f0);
                for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(envDb-80.0*std::abs(d))); } }
            for(int i=0;i<bins;++i) m[(size_t)i]+=(rng.nextFloat()-0.5f)*3.0f; return m; };
        auto eng = makeEngine(sr, winner);
        double testFreqs[] = {350, 1350, 5500, 9500, 13500};
        std::vector<float> promBase, promInj;
        std::printf("%8s | %14s | %14s | %10s\n","freq","score(base)","score(base+inj)","delta");
        for (double f0 : testFreqs)
        {
            auto base = genGuitar(sr);
            auto withInj = base;
            for (int i=0;i<bins;++i)
            {
                double hz=juce::jmax(1.0,binHz(i,sr)); double d=std::log2(hz/f0);
                withInj[(size_t)i] += (float)(6.0*std::exp(-0.5*(d/0.03)*(d/0.03)));
            }
            eng.computeProminence(base, 5.0f, promBase);
            eng.computeProminence(withInj, 5.0f, promInj);
            int bin = freqToBin(f0,sr);
            std::printf("%8.0f | %14.3f | %14.3f | %10.3f\n", f0, promBase[(size_t)bin], promInj[(size_t)bin], promInj[(size_t)bin]-promBase[(size_t)bin]);
        }
        std::cout << "(true injected level = 6dB; delta should approach that, independent of the base material's own structure)\n";
    }

    //======================================================================
    std::cout << "\n==================== 13. CPU DETERMINISM ACROSS CONTENT (192kHz stereo) ====================\n";
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
        auto genAdversarial=[&](double s){ std::vector<float> m((size_t)bins,-40.0f); double freqsAdv[]={60,120,250,500,1000,1500,2000,3000,4000,5000,7000,9000,11000,14000,17000};
            for(double f:freqsAdv) for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-40.0+10.0*std::exp(-0.5*(d/0.03)*(d/0.03)))); } return m; };

        struct Content{ const char* name; std::function<std::vector<float>(double)> gen; };
        Content contents[] = { {"silence",genSilence},{"pink noise",genPink},{"voice",genVoice},{"guitar",genGuitar},{"drums",genDrums},{"dense mix",genDenseMix},{"adversarial",genAdversarial} };
        const double hopUs = 1000000.0*512.0/sr;
        std::printf("%-14s | %10s | %10s | %10s | %10s | %10s | %10s\n","content","mean(2ch)","median(2ch)","P95(2ch)","P99(2ch)","max(2ch)","%hopP99");
        for (auto& c : contents)
        {
            auto eng = makeEngine(sr, winner);
            auto frame = c.gen(sr);
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
            double med=times[times.size()/2], p95=times[(size_t)(times.size()*0.95)], p99=times[(size_t)(times.size()*0.99)], mx=times.back();
            std::printf("%-14s | %10.2f | %10.2f | %10.2f | %10.2f | %10.2f | %9.2f%%\n", c.name, mean, med, p95, p99, mx, p99/hopUs*100.0);
        }
    }

    //======================================================================
    std::cout << "\n==================== 16. FINAL COMPARISON: V1 vs V2-A3/4 vs V2-A5 vs P25 oracle ====================\n";
    {
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48); // full-bin P25, oracle
        auto eng = makeEngine(sr, winner);
        double freqs[] = {120,1000,10000};
        double levels[] = {2,4,8,12};
        double sumV5=0, sumRef=0; int n=0;
        std::vector<float> pV5, pRef;
        for (double f0:freqs) for (double lvl:levels)
        {
            auto frame = makeSynthetic(sr, f0, lvl, 0.12);
            eng.computeProminence(frame, 8.0f, pV5);
            refEng.computeProminenceFullBinReference(frame, 8.0f, pRef);
            int bin=freqToBin(f0,sr);
            sumV5 += std::abs(pV5[(size_t)bin]-lvl); sumRef += std::abs(pRef[(size_t)bin]-lvl); ++n;
        }
        std::cout << "V2-A5 (deterministic, no P25 realtime) MAE: " << (sumV5/n) << " dB\n";
        std::cout << "P25 oracle (offline reference) MAE: " << (sumRef/n) << " dB\n";
        std::cout << "(V1's equivalent 120Hz+2dB error was 9.78dB estimated vs true 2dB -- see checkpoint history)\n";
    }

    return 0;
}
