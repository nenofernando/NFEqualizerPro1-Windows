// NF Resonance -- V2-A4 region-based NARROW refinement validation.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV4.h"
#include "DSP/SpectralProminenceEngineV2.h" // full-bin P25 reference

static const double sr = 48000.0;
static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin, double s=sr) { return bin * s / fftSize; }
static int freqToBin(double f0, double s=sr) { return (int)std::round(f0*fftSize/s); }

// --- Item 14: units guard, asserted once at startup ---
static void assertHopUnits()
{
    double hopUs = 1000000.0 * 512.0 / 192000.0;
    jassert(std::abs(hopUs - 2666.6667) < 0.01);
    if (std::abs(hopUs - 2666.6667) > 0.01) { std::cout << "FATAL: hop unit calculation is wrong! hopUs=" << hopUs << "\n"; std::exit(1); }
    std::cout << "Units check OK: hopUs(512@192kHz) = " << hopUs << " us\n\n";
}

static SpectralProminenceEngineV4 makeEngine(double thresholdDb=2.5, double marginDb=3.0, double valleyDb=3.0, int smallRegionW=3, int budget=-1, bool cacheOn=false)
{
    SpectralProminenceEngineV4 eng;
    eng.prepare(bins, sr, fftSize, 36, 72);
    eng.setNarrowScaleParams(0.04, 0.9);
    eng.setNarrowCandidateThresholdDb(thresholdDb, marginDb);
    eng.setValleySplitThresholdDb(valleyDb);
    eng.setSmallRegionWidthBins(smallRegionW);
    eng.setRegionBudget(budget);
    eng.setTemporalCache(cacheOn);
    return eng;
}

static std::vector<float> makeFrame(double f0, double levelDb, double sigmaOct, double s=sr)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,s));
        double d=std::log2(hz/f0);
        double bump = levelDb*std::exp(-0.5*(d/sigmaOct)*(d/sigmaOct));
        m[(size_t)i]=(float)(-40.0 - 3.0*std::log2(hz/1000.0) + bump);
    }
    return m;
}
static std::vector<float> makeTwoPeaks(double f1, double f2, double levelDb, double sigmaOct, double s=sr)
{
    std::vector<float> m((size_t)bins);
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,s));
        double d1=std::log2(hz/f1), d2=std::log2(hz/f2);
        double bump = levelDb*std::exp(-0.5*(d1/sigmaOct)*(d1/sigmaOct)) + levelDb*std::exp(-0.5*(d2/sigmaOct)*(d2/sigmaOct));
        m[(size_t)i]=(float)(-40.0 - 3.0*std::log2(hz/1000.0) + bump);
    }
    return m;
}

int main()
{
    assertHopUnits();

    //======================================================================
    std::cout << "==================== 5. REGION REDUCTION RATIO (no budget cap) ====================\n";
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
        auto genHarmonicStack=[&](double s){ std::vector<float> m((size_t)bins,-60.0f); double f0=110.0;
            for(int h=1;h<=40;++h){ double f=f0*h; if(f>19000) break; for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-20.0-40.0*std::abs(d))); } } return m; };
        auto genManyRes=[&](double s){ std::vector<float> m((size_t)bins,-40.0f); double freqsAdv[]={60,120,250,500,1000,1500,2000,3000,4000,5000,7000,9000,11000,14000,17000};
            for(double f:freqsAdv) for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-40.0+10.0*std::exp(-0.5*(d/0.03)*(d/0.03)))); } return m; };

        struct Content{ const char* name; std::function<std::vector<float>(double)> gen; };
        Content contents[] = {
            {"silence",genSilence},{"pink noise",genPink},{"voice",genVoice},{"guitar",genGuitar},
            {"drums",genDrums},{"dense mix",genDenseMix},{"harmonic stack",genHarmonicStack},{"many resonances",genManyRes},
        };
        std::printf("%-18s | %14s | %14s | %14s\n","content","candidate bins","regions","reduction ratio");
        auto eng = makeEngine();
        std::vector<float> prom;
        for (auto& c : contents)
        {
            auto frame = c.gen(sr);
            eng.computeProminence(frame, 4.0f, prom);
            int cb = eng.lastCandidateBinCount(), rg = eng.lastRegionCount();
            std::printf("%-18s | %14d | %14d | %13.1fx\n", c.name, cb, rg, rg>0 ? (double)cb/rg : 0.0);
        }
    }

    //======================================================================
    std::cout << "\n==================== 3. TWO-PEAK SEPARATION TEST ====================\n";
    {
        auto eng = makeEngine();
        std::vector<float> prom;
        double freqs[] = {1000, 4000, 10000};
        std::cout << "separations tested in bins: 1,2,3,5 and in octaves: 1/24, 1/12\n\n";
        std::printf("%8s | %14s | %10s | %10s\n","freq","separation","regions","expect");
        for (double f0 : freqs)
        {
            double hzPerBin = sr/fftSize;
            for (int sepBins : {1,2,3,5})
            {
                double f2 = f0 + sepBins*hzPerBin;
                auto frame = makeTwoPeaks(f0, f2, 8.0, 0.01);
                eng.computeProminence(frame, 8.0f, prom);
                std::printf("%8.0f | %11d bins | %10d | %10s\n", f0, sepBins, eng.lastRegionCount(), sepBins<=2?"1 (likely)":"1-2");
            }
            for (double sepOct : {1.0/24.0, 1.0/12.0})
            {
                double f2 = f0*std::pow(2.0,sepOct);
                auto frame = makeTwoPeaks(f0, f2, 8.0, 0.01);
                eng.computeProminence(frame, 8.0f, prom);
                std::printf("%8.0f | %8.4f oct | %10d | %10s\n", f0, sepOct, eng.lastRegionCount(), "2 (likely)");
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== 12. ACCURACY: A full-P25, B peak-only, C 3-point, D+budget, vs ref ====================\n";
    {
        double freqs[] = {250,1000,4000,10000,16000};
        double levels[] = {2,4,8,12};
        struct QDef{ const char* label; double sigmaOct; };
        QDef widths[] = { {"narrow",0.035}, {"xnarrow",0.01} };

        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        auto engPeakOnly = makeEngine(2.5,3.0,3.0, 999, -1); // smallRegionWidth huge -> always peak-only
        auto eng3Point    = makeEngine(2.5,3.0,3.0, 0, -1);  // smallRegionWidth 0 -> always 3-point (if width>0)
        auto engBudget32  = makeEngine(2.5,3.0,3.0, 3, 32);

        std::vector<float> pRef, pB, pC, pD;
        double sumRef=0,sumB=0,sumC=0,sumD=0; int n=0;
        for (double f0:freqs) for (double lvl:levels) for (auto& w:widths)
        {
            auto frame = makeFrame(f0, lvl, w.sigmaOct);
            refEng.computeProminenceFullBinReference(frame, 10.0f, pRef);
            engPeakOnly.computeProminence(frame, 10.0f, pB);
            eng3Point.computeProminence(frame, 10.0f, pC);
            engBudget32.computeProminence(frame, 10.0f, pD);
            int bin=freqToBin(f0);
            sumRef+=std::abs(pRef[(size_t)bin]-lvl); sumB+=std::abs(pB[(size_t)bin]-lvl);
            sumC+=std::abs(pC[(size_t)bin]-lvl); sumD+=std::abs(pD[(size_t)bin]-lvl); ++n;
        }
        std::cout << "MAE A (full-bin P25 reference): " << (sumRef/n) << " dB\n";
        std::cout << "MAE B (region, peak-only):        " << (sumB/n) << " dB\n";
        std::cout << "MAE C (region, 3-point):           " << (sumC/n) << " dB\n";
        std::cout << "MAE D (region, budget=32):          " << (sumD/n) << " dB\n";
    }

    //======================================================================
    std::cout << "\n==================== 6/13. CPU WITHOUT CAP, staged breakdown (2ch@192kHz) ====================\n";
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
        auto genHarmonicStack=[&](double s){ std::vector<float> m((size_t)bins,-60.0f); double f0=110.0;
            for(int h=1;h<=40;++h){ double f=f0*h; if(f>19000) break; for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-20.0-40.0*std::abs(d))); } } return m; };
        auto genManyRes=[&](double s){ std::vector<float> m((size_t)bins,-40.0f); double freqsAdv[]={60,120,250,500,1000,1500,2000,3000,4000,5000,7000,9000,11000,14000,17000};
            for(double f:freqsAdv) for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,s)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-40.0+10.0*std::exp(-0.5*(d/0.03)*(d/0.03)))); } return m; };

        struct Content{ const char* name; std::function<std::vector<float>(double)> gen; };
        Content contents[] = {
            {"silence",genSilence},{"pink noise",genPink},{"voice",genVoice},{"guitar",genGuitar},
            {"drums",genDrums},{"dense mix",genDenseMix},{"harmonic stack",genHarmonicStack},{"many resonances",genManyRes},
        };
        const double hopUs = 1000000.0*512.0/192000.0;
        std::cout << "hop budget @192kHz = " << hopUs << " us. Target P99(2ch) < 667us, ideal 400-530us.\n";
        std::cout << "NO region budget cap in this test -- natural cost after grouping only.\n\n";
        std::printf("%-16s | %10s | %10s | %10s | %10s | %14s | %8s | %8s\n","content","mean(2ch)","P95(2ch)","P99(2ch)","max(2ch)","%hopP99","regions","refined");
        for (auto& c : contents)
        {
            SpectralProminenceEngineV4 eng; eng.prepare(bins, 192000.0, fftSize, 36, 72);
            eng.setNarrowScaleParams(0.04, 0.9);
            eng.setNarrowCandidateThresholdDb(2.5, 3.0);
            eng.setValleySplitThresholdDb(3.0);
            eng.setSmallRegionWidthBins(3);
            eng.setRegionBudget(-1); // unlimited, per item 6
            auto frame = c.gen(192000.0);
            std::vector<float> prom;
            eng.computeProminence(frame, 4.0f, prom); // warm
            std::vector<double> times; std::vector<double> broadT, mediumT, groupT, refineT;
            for (int i=0;i<300;++i)
            {
                auto t0=std::chrono::high_resolution_clock::now();
                eng.computeProminence(frame,4.0f,prom);
                auto t1=std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
                broadT.push_back(eng.lastBroadUs()*2.0); mediumT.push_back(eng.lastMediumUs()*2.0);
                groupT.push_back(eng.lastGroupingUs()*2.0); refineT.push_back(eng.lastNarrowRefineUs()*2.0);
            }
            std::sort(times.begin(),times.end());
            double mean=0; for(double t:times) mean+=t; mean/=times.size();
            double p95=times[(size_t)(times.size()*0.95)], p99=times[(size_t)(times.size()*0.99)], mx=times.back();
            std::printf("%-16s | %10.2f | %10.2f | %10.2f | %10.2f | %13.2f%% | %8d | %8d\n",
                c.name, mean, p95, p99, mx, p99/hopUs*100.0, eng.lastRegionCount(), eng.lastRegionsRefinedCount());
            double avgB=0,avgM=0,avgG=0,avgR=0; for(size_t i=0;i<times.size();++i){avgB+=broadT[i];avgM+=mediumT[i];avgG+=groupT[i];avgR+=refineT[i];}
            avgB/=times.size();avgM/=times.size();avgG/=times.size();avgR/=times.size();
            std::printf("    stage avg(2ch): BROAD=%.2fus MEDIUM=%.2fus GROUPING=%.2fus NARROW-REFINE=%.2fus\n", avgB,avgM,avgG,avgR);
        }
    }

    //======================================================================
    std::cout << "\n==================== 10. TEMPORAL CACHE: worst-case detection latency ====================\n";
    {
        auto eng = makeEngine(2.5,3.0,3.0,3,-1,true); // cache enabled
        // steady tone for a while, then a NEW resonance appears -- measure frames until refined
        std::vector<float> prom;
        auto steady = makeFrame(1000, 8.0, 0.02);
        for (int f=0; f<10; ++f) eng.computeProminence(steady, 8.0f, prom); // settle cache

        auto withNew = makeTwoPeaks(1000, 5000, 8.0, 0.02); // new resonance at 5kHz appears
        int newBin = freqToBin(5000.0);
        int framesToRefine = -1;
        for (int f=0; f<20; ++f)
        {
            eng.computeProminence(withNew, 8.0f, prom);
            // heuristic: refined if estimate is close to the true level (8dB)
            if (std::abs(prom[(size_t)newBin] - 8.0f) < 1.0f) { framesToRefine = f; break; }
        }
        double frameMs = 512.0/sr*1000.0;
        std::cout << "New resonance detected/refined after " << framesToRefine << " frames (" << (framesToRefine*frameMs) << " ms)\n";
        std::cout << "(frame 0 = the very first frame in which the new resonance is present in the input)\n";
    }

    return 0;
}
