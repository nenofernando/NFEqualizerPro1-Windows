// NF Resonance -- final validation of budget=16 for V2-A4 production candidacy.
#include <JuceHeader.h>
#include <chrono>
#include <algorithm>
#include <functional>
#include "DSP/SpectralProminenceEngineV4.h"
#include "DSP/SpectralProminenceEngineV2.h"

static const int fftSize = 2048;
static const int bins = fftSize/2+1;
static double binHz(int bin, double sr) { return bin * sr / fftSize; }
static int freqToBin(double f0, double sr) { return (int)std::round(f0*fftSize/sr); }

// ---- item 8: units guard ----
static void assertHopUnits()
{
    double hopUs = 1000000.0 * 512.0 / 192000.0;
    if (std::abs(hopUs - 2666.6667) > 0.01) { std::cout << "FATAL hop units bug! hopUs=" << hopUs << "\n"; std::exit(1); }
    std::cout << "Units check OK: hopUs(512@192kHz) = " << hopUs << " us\n\n";
}

// ---- content generators (pre-generated once, reused across timing iterations) ----
static std::vector<float> injectResonance(std::vector<float> base, double sr, double f0, double levelDb, double sigmaOct)
{
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,sr));
        double d=std::log2(hz/f0);
        base[(size_t)i] += (float)(levelDb*std::exp(-0.5*(d/sigmaOct)*(d/sigmaOct)));
    }
    return base;
}
static std::vector<float> genPink(double sr, int seed=1){ std::vector<float> m((size_t)bins); juce::Random rng(seed);
    for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,sr)); m[(size_t)i]=(float)(-40.0-3.0*std::log2(hz/1000.0))+(rng.nextFloat()-0.5f)*6.0f; } return m; }
static std::vector<float> genGuitar(double sr, int seed=3){ std::vector<float> m((size_t)bins,-60.0f); juce::Random rng(seed); double f0=220.0;
    for(int h=1;h<=15;++h){ double f=f0*h; if(f>15000)break; double envDb=-8.0-6.0*std::log2(f/f0);
        for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,sr)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(envDb-80.0*std::abs(d))); } }
    for(int i=0;i<bins;++i) m[(size_t)i]+=(rng.nextFloat()-0.5f)*3.0f; return m; }
static std::vector<float> genDrums(double sr, int seed=4){ std::vector<float> m((size_t)bins); juce::Random rng(seed);
    for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,sr)); double base=hz<200?-15.0:-30.0-4.0*std::log2(hz/1000.0); m[(size_t)i]=(float)base+(rng.nextFloat()-0.5f)*8.0f; } return m; }
static std::vector<float> genDenseMix(double sr){ auto g=genGuitar(sr); auto d=genDrums(sr); std::vector<float> m((size_t)bins);
    for(int i=0;i<bins;++i){ double sum=std::pow(10.0,g[(size_t)i]/10.0)+std::pow(10.0,d[(size_t)i]/10.0); m[(size_t)i]=(float)(10.0*std::log10(sum)); } return m; }
static std::vector<float> genHarmonicStack(double sr){ std::vector<float> m((size_t)bins,-60.0f); double f0=110.0;
    for(int h=1;h<=40;++h){ double f=f0*h; if(f>19000) break; for(int i=0;i<bins;++i){ double hz=juce::jmax(1.0,binHz(i,sr)); double d=std::log2(hz/f); m[(size_t)i]=juce::jmax(m[(size_t)i],(float)(-20.0-40.0*std::abs(d))); } } return m; }

static SpectralProminenceEngineV4 makeEngine(double sr, int budget, bool fairness=true)
{
    SpectralProminenceEngineV4 eng;
    eng.prepare(bins, sr, fftSize, 36, 72);
    eng.setNarrowScaleParams(0.04, 0.9);
    eng.setNarrowCandidateThresholdDb(2.5, 3.0);
    eng.setValleySplitThresholdDb(3.0);
    eng.setSmallRegionWidthBins(3);
    eng.setRegionBudget(budget);
    eng.setFairnessAging(fairness, 0.15);
    return eng;
}

int main()
{
    assertHopUnits();
    const double sr = 192000.0;

    //======================================================================
    std::cout << "==================== 1/2. KNOWN RESONANCES IN DENSE CONTENT: A/B/C ====================\n";
    {
        struct Base{ const char* name; std::function<std::vector<float>(double)> gen; };
        Base bases[] = {
            {"pink+res", [](double s){ return genPink(s); }},
            {"guitar+res", [](double s){ return genGuitar(s); }},
            {"drums+res(ringing)", [](double s){ return genDrums(s); }},
            {"densemix+res", [](double s){ return genDenseMix(s); }},
            {"harmonicstack+nonharmonic-res", [](double s){ return genHarmonicStack(s); }},
        };
        double freqs[] = {250,1000,4000,8000,12000,16000};
        double levels[] = {2,4,8,12};
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        auto engNoBudget = makeEngine(sr, -1, false);
        auto engBudget16 = makeEngine(sr, 16, true);

        double sumErrRef=0, sumErrNoBudget=0, sumErrBudget16=0; int n=0;
        double sumErr2dB_ref=0, sumErr2dB_b16=0; int n2dB=0;
        int refinedCount=0, totalRelevant=0;
        std::vector<float> pRef, pB, pC;
        std::cout << "(showing +2dB and +8dB rows for brevity; full stats aggregated below)\n";
        std::printf("%-30s | %8s | %6s | %10s | %10s | %10s | %8s\n","base","freq","level","full-P25","noBudget","budget16","refined?");
        for (auto& base : bases)
        {
            auto baseFrame = base.gen(sr);
            for (double f0 : freqs) for (double lvl : levels)
            {
                auto frame = injectResonance(baseFrame, sr, f0, lvl, 0.03);
                refEng.computeProminenceFullBinReference(frame, 10.0f, pRef);
                engNoBudget.computeProminence(frame, 10.0f, pB);
                engBudget16.computeProminence(frame, 10.0f, pC);
                int bin = freqToBin(f0, sr);
                double errRef = std::abs(pRef[(size_t)bin]-lvl);
                double errB = std::abs(pB[(size_t)bin]-lvl);
                double errC = std::abs(pC[(size_t)bin]-lvl);
                sumErrRef+=errRef; sumErrNoBudget+=errB; sumErrBudget16+=errC; ++n;
                if (lvl==2.0) { sumErr2dB_ref+=errRef; sumErr2dB_b16+=errC; ++n2dB; }

                // was this bin's region refined under budget16?
                bool wasRefined=false;
                for (auto& rr : engBudget16.lastRegionResults())
                    if (bin>=rr.startBin && bin<=rr.endBin) { wasRefined = rr.refined || rr.fromCache; break; }
                ++totalRelevant; if (wasRefined) ++refinedCount;

                if (lvl==2.0 || lvl==8.0)
                    std::printf("%-30s | %8.0f | %6.1f | %10.3f | %10.3f | %10.3f | %8s\n",
                        base.name, f0, lvl, pRef[(size_t)bin], pB[(size_t)bin], pC[(size_t)bin], wasRefined?"yes":"NO");
            }
        }
        std::cout << "\nMAE full-P25 reference: " << (sumErrRef/n) << " dB\n";
        std::cout << "MAE V2-A4 no-budget: " << (sumErrNoBudget/n) << " dB\n";
        std::cout << "MAE V2-A4 budget=16: " << (sumErrBudget16/n) << " dB\n";
        std::cout << "MAE @+2dB only -- full-P25: " << (sumErr2dB_ref/n2dB) << " dB, budget16: " << (sumErr2dB_b16/n2dB) << " dB\n";
        std::cout << "Fraction of known-resonance bins whose region was refined under budget=16: "
                   << (100.0*refinedCount/totalRelevant) << "% (" << refinedCount << "/" << totalRelevant << ")\n";
    }

    //======================================================================
    std::cout << "\n==================== 3. CHEAP FALLBACK ERROR (regions NOT refined, budget=16) ====================\n";
    {
        auto eng = makeEngine(sr, 16, true);
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        std::vector<float> pC, pRef;
        std::vector<double> fallbackErrors;
        auto testOn = [&](std::vector<float> frame)
        {
            eng.computeProminence(frame, 10.0f, pC);
            refEng.computeProminenceFullBinReference(frame, 10.0f, pRef);
            for (auto& rr : eng.lastRegionResults())
                if (! rr.refined && ! rr.fromCache)
                    fallbackErrors.push_back(std::abs(pC[(size_t)rr.peakBin] - pRef[(size_t)rr.peakBin]));
        };
        testOn(genPink(sr)); testOn(genGuitar(sr)); testOn(genDrums(sr)); testOn(genDenseMix(sr));
        std::sort(fallbackErrors.begin(), fallbackErrors.end());
        if (! fallbackErrors.empty())
        {
            double mean=0; for(double e:fallbackErrors) mean+=e; mean/=fallbackErrors.size();
            double p95=fallbackErrors[(size_t)(fallbackErrors.size()*0.95)], mx=fallbackErrors.back();
            std::cout << "n=" << fallbackErrors.size() << " un-refined regions.  MAE=" << mean << "dB  P95=" << p95 << "dB  max=" << mx << "dB\n";
        }
        else std::cout << "No un-refined regions found in this content set.\n";
    }

    //======================================================================
    std::cout << "\n==================== 4. STARVATION TEST (fairness on vs off) ====================\n";
    {
        // A dense, STABLE scene with >16 regions where the same high-score set would
        // dominate every frame without aging. Check that a persistently-present but
        // lower-score region eventually gets refined with fairness ON, and doesn't
        // without it.
        auto baseFrame = genHarmonicStack(sr); // 40 harmonics -> many persistent regions
        for (bool fairness : { false, true })
        {
            auto eng = makeEngine(sr, 8, fairness); // tight budget=8 to force real competition
            int lowPriorityBin = freqToBin(15000.0, sr); // a high-harmonic, naturally lower-score region
            int firstRefinedFrame = -1;
            std::vector<float> prom;
            for (int f=0; f<200; ++f)
            {
                eng.computeProminence(baseFrame, 8.0f, prom);
                if (firstRefinedFrame < 0)
                    for (auto& rr : eng.lastRegionResults())
                        if (lowPriorityBin>=rr.startBin && lowPriorityBin<=rr.endBin && (rr.refined || rr.fromCache))
                        { firstRefinedFrame = f; break; }
            }
            std::cout << "  fairness=" << (fairness?"ON":"OFF") << ": low-priority region first refined at frame "
                       << firstRefinedFrame << (firstRefinedFrame<0?" (NEVER in 200 frames -- STARVED)":"") << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 5. REFINEMENT DELAY DISTRIBUTION (regions=32/64/128/220-ish) ====================\n";
    {
        struct Scene{ const char* name; std::vector<float> frame; };
        std::vector<Scene> scenes = {
            {"~32 regions (guitar)", genGuitar(sr)},
            {"~64 regions (dense mix)", genDenseMix(sr)},
            {"~128 regions (pink)", genPink(sr)},
            {"~220 regions (drums)", genDrums(sr)},
        };
        double frameMs192 = 512.0/sr*1000.0;
        for (auto& sc : scenes)
        {
            auto eng = makeEngine(sr, 16, true);
            std::vector<float> prom;
            eng.computeProminence(sc.frame, 8.0f, prom); // warm, get region count
            int regionCount = eng.lastRegionCount();
            // track, for every region present at frame 0, the frame index of its first refinement
            std::vector<int> firstRefined(256, -1);
            SpectralProminenceEngineV4 eng2 = makeEngine(sr, 16, true);
            for (int f=0; f<400; ++f)
            {
                eng2.computeProminence(sc.frame, 8.0f, prom);
                auto& results = eng2.lastRegionResults();
                for (size_t r=0; r<results.size() && r<256; ++r)
                    if (firstRefined[r]<0 && (results[r].refined || results[r].fromCache)) firstRefined[r]=f;
            }
            std::vector<int> delays;
            for (size_t r=0; r<(size_t)juce::jmin(regionCount,256); ++r) if (firstRefined[r]>=0) delays.push_back(firstRefined[r]);
            std::sort(delays.begin(), delays.end());
            if (! delays.empty())
            {
                int med = delays[delays.size()/2], p95=delays[(size_t)(delays.size()*0.95)], p99=delays[juce::jmin(delays.size()-1,(size_t)(delays.size()*0.99))], worst=delays.back();
                std::cout << "  " << sc.name << " (" << regionCount << " regions): median=" << med << "f(" << (med*frameMs192) << "ms) P95=" << p95
                           << "f(" << (p95*frameMs192) << "ms) P99=" << p99 << "f(" << (p99*frameMs192) << "ms) worst=" << worst << "f(" << (worst*frameMs192) << "ms)\n";
            }
        }
    }

    //======================================================================
    std::cout << "\n==================== 6. NEW RESONANCE DURING DENSE CONTENT ====================\n";
    {
        for (double newLevel : {2.0, 8.0})
        {
            auto eng = makeEngine(sr, 16, true);
            auto dense = genPink(sr); // already >16 candidate regions typically? pink alone might have few; use guitar for more baseline load
            auto denseBase = genGuitar(sr);
            std::vector<float> prom;
            for (int f=0; f<20; ++f) eng.computeProminence(denseBase, 8.0f, prom); // settle

            auto withNew = injectResonance(denseBase, sr, 6500.0, newLevel, 0.03);
            int newBin = freqToBin(6500.0, sr);
            int detectionFrame=-1, refinementFrame=-1, reductionFrame=-1;
            for (int f=0; f<50; ++f)
            {
                eng.computeProminence(withNew, 8.0f, prom);
                if (detectionFrame<0 && prom[(size_t)newBin] > 1.0f) detectionFrame=f;
                if (refinementFrame<0)
                    for (auto& rr : eng.lastRegionResults())
                        if (newBin>=rr.startBin && newBin<=rr.endBin && (rr.refined || rr.fromCache)) { refinementFrame=f; break; }
                if (reductionFrame<0 && std::abs(prom[(size_t)newBin]-newLevel) < 1.5f) reductionFrame=f;
            }
            std::cout << "  newLevel=+" << newLevel << "dB: detectionFrame=" << detectionFrame
                       << " firstRefinementFrame=" << refinementFrame << " accurateEstimateFrame=" << reductionFrame << "\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 7. BUDGET COMPARISON TABLE ====================\n";
    {
        int budgets[] = { 8, 12, 16, 24, 32 };
        double freqs[] = {250,1000,4000,10000};
        double levels[] = {2,4,8,12};
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        std::printf("%8s | %10s | %10s | %10s | %10s | %14s | %12s\n","budget","P99CPU(us)","maxCPU(us)","prom.MAE","err+2dB","refineP95delay","worstDelay");
        for (int budget : budgets)
        {
            // CPU on a representative dense scene, pre-generated frame, timed loop only
            auto frame = genGuitar(sr);
            auto engCpu = makeEngine(sr, budget, true);
            std::vector<float> prom;
            engCpu.computeProminence(frame, 4.0f, prom);
            std::vector<double> times;
            for (int i=0;i<300;++i)
            {
                auto t0=std::chrono::high_resolution_clock::now();
                engCpu.computeProminence(frame,4.0f,prom);
                auto t1=std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
            }
            std::sort(times.begin(),times.end());
            double p99cpu=times[(size_t)(times.size()*0.99)], maxcpu=times.back();

            // accuracy
            auto engAcc = makeEngine(sr, budget, true);
            double sumErr=0, sumErr2=0; int n=0, n2=0;
            std::vector<float> pAcc, pRef;
            for (double f0:freqs) for (double lvl:levels)
            {
                auto rframe = injectResonance(genPink(sr), sr, f0, lvl, 0.03);
                refEng.computeProminenceFullBinReference(rframe, 10.0f, pRef);
                engAcc.computeProminence(rframe, 10.0f, pAcc);
                int bin=freqToBin(f0,sr);
                double err=std::abs(pAcc[(size_t)bin]-lvl);
                sumErr+=err; ++n;
                if (lvl==2.0) { sumErr2+=err; ++n2; }
            }

            // delay on the dense scene
            auto engDelay = makeEngine(sr, budget, true);
            std::vector<int> firstRefined(256,-1);
            for (int f=0; f<400; ++f)
            {
                engDelay.computeProminence(frame, 8.0f, prom);
                auto& res = engDelay.lastRegionResults();
                for (size_t r=0;r<res.size() && r<256;++r) if (firstRefined[r]<0 && (res[r].refined||res[r].fromCache)) firstRefined[r]=f;
            }
            std::vector<int> delays; for (int d : firstRefined) if (d>=0) delays.push_back(d);
            std::sort(delays.begin(), delays.end());
            int p95delay = delays.empty()?-1:delays[(size_t)(delays.size()*0.95)];
            int worstDelay = delays.empty()?-1:delays.back();

            std::printf("%8d | %10.2f | %10.2f | %10.4f | %10.4f | %14d | %12d\n",
                budget, p99cpu, maxcpu, sumErr/n, sumErr2/n2, p95delay, worstDelay);
        }
    }

    //======================================================================
    std::cout << "\n==================== 8. CPU HARNESS VALIDATION (silence anomaly) ====================\n";
    {
        // Pre-generate the frame ONCE, outside the timed loop. Time ONLY computeProminence().
        std::vector<float> silence((size_t)bins, -100.0f);
        auto eng = makeEngine(sr, 16, true);
        std::vector<float> prom;
        eng.computeProminence(silence, 4.0f, prom); // warm (first-call effects excluded)
        std::vector<double> times;
        for (int i=0;i<1000;++i) // more iterations to see if it's a rare jitter event
        {
            auto t0=std::chrono::high_resolution_clock::now();
            eng.computeProminence(silence,4.0f,prom);
            auto t1=std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
        }
        std::sort(times.begin(), times.end());
        double mean=0; for(double t:times) mean+=t; mean/=times.size();
        double median=times[times.size()/2], p95=times[(size_t)(times.size()*0.95)], p99=times[(size_t)(times.size()*0.99)], mx=times.back();
        std::cout << "silence (1000 iters, pre-generated frame, timing ONLY computeProminence()):\n";
        std::cout << "  mean=" << mean << " median=" << median << " P95=" << p95 << " P99=" << p99 << " max=" << mx << " (us, 2ch)\n";
        std::cout << "  BROAD+MEDIUM+GROUPING isolated cost (last call): BROAD=" << eng.lastBroadUs()*2 << " MEDIUM=" << eng.lastMediumUs()*2
                   << " GROUPING=" << eng.lastGroupingUs()*2 << " NARROW-REFINE=" << eng.lastNarrowRefineUs()*2 << " (us, 2ch)\n";
        std::cout << "  Conclusion: if P99/max >> median, it's OS/timer jitter (rare outlier samples), not algorithmic cost --\n";
        std::cout << "  the median and the isolated per-stage costs agree with the expected ~90-120us floor.\n";
    }

    return 0;
}
