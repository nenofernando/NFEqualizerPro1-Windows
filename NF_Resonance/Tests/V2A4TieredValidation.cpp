// NF Resonance -- V2-A4 tiered scheduler validation: separates candidate
// detection (A) from scheduling/fairness (B) from refinement accuracy (C),
// using injection frequencies deliberately chosen to avoid the base
// material's own harmonics.
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

static void assertHopUnits()
{
    double hopUs = 1000000.0 * 512.0 / 192000.0;
    if (std::abs(hopUs - 2666.6667) > 0.01) { std::cout << "FATAL hop units bug!\n"; std::exit(1); }
    std::cout << "Units check OK: hopUs(512@192kHz) = " << hopUs << " us. cpuPercent = 100*detectorUs/hopUs.\n\n";
}

// Injection frequencies deliberately chosen NOT to coincide with guitar (220Hz,
// harmonics to 3300Hz) or the 40-harmonic stack (110Hz, harmonics to 4400Hz).
static const double SAFE_FREQS[] = { 400.0, 1350.0, 5500.0, 9500.0, 13500.0, 17500.0 };

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

// Records local baseline BEFORE injection, then injects relative to it.
static std::vector<float> injectRelativeToLocalBaseline(std::vector<float> base, double sr, double f0, double levelDb, double sigmaOct, double* outLocalBaseline=nullptr)
{
    int bin = freqToBin(f0, sr);
    // local baseline = mean of a modest window around the target bin, excluding the immediate center
    double sum=0; int n=0;
    for (int i=juce::jmax(0,bin-20); i<=juce::jmin(bins-1,bin+20); ++i) if (std::abs(i-bin)>3) { sum+=base[(size_t)i]; ++n; }
    double localBaseline = n>0 ? sum/n : base[(size_t)bin];
    if (outLocalBaseline) *outLocalBaseline = localBaseline;
    for (int i=0;i<bins;++i)
    {
        double hz=juce::jmax(1.0,binHz(i,sr));
        double d=std::log2(hz/f0);
        base[(size_t)i] += (float)(levelDb*std::exp(-0.5*(d/sigmaOct)*(d/sigmaOct)));
    }
    return base;
}

static SpectralProminenceEngineV4 makeTiered(double sr, int t1, int t2, int t3)
{
    SpectralProminenceEngineV4 eng;
    eng.prepare(bins, sr, fftSize, 36, 72);
    eng.setNarrowScaleParams(0.04, 0.9);
    eng.setNarrowCandidateThresholdDb(2.5, 3.0);
    eng.setValleySplitThresholdDb(3.0);
    eng.setSmallRegionWidthBins(3);
    eng.setTieredScheduling(true, t1, t2, t3);
    return eng;
}

int main()
{
    assertHopUnits();
    const double sr = 192000.0;
    double frameMs = 512.0/sr*1000.0;

    //======================================================================
    std::cout << "==================== A. CANDIDATE DETECTION ISOLATION (+2dB, safe freq) ====================\n";
    {
        double f0 = SAFE_FREQS[1]; // 1350 Hz
        double localBaseline;
        auto frame = injectRelativeToLocalBaseline(genGuitar(sr), sr, f0, 2.0, 0.03, &localBaseline);
        int bin = freqToBin(f0, sr);

        // A: full P25 (no candidate stage at all)
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        std::vector<float> pRef; refEng.computeProminenceFullBinReference(frame, 10.0f, pRef);

        // D: full V2-A4 pipeline
        auto engFull = makeTiered(sr, 8, 4, 4);
        std::vector<float> pFull; engFull.computeProminence(frame, 10.0f, pFull);

        // B/C: cheap estimate + candidate flag directly from lastRegionResults
        bool isCandidate=false; float cheapAtBin=0; int rank=-1; bool refined=false; int tier=0;
        for (auto& rr : engFull.lastRegionResults())
            if (bin>=rr.startBin && bin<=rr.endBin) { isCandidate=true; rank=rr.rank; refined=rr.refined||rr.fromCache; tier=rr.tier; break; }

        std::cout << "local baseline near " << f0 << "Hz = " << localBaseline << " dB\n";
        std::cout << "A (full P25 reference) prominence: " << pRef[(size_t)bin] << " dB (true=2.0)\n";
        std::cout << "B (is this bin a NARROW candidate at all?): " << (isCandidate?"YES":"NO") << "\n";
        std::cout << "C (region rank/tier if candidate): rank=" << rank << " tier=" << tier << " refined=" << (refined?"yes":"no") << "\n";
        std::cout << "D (full V2-A4 pipeline output): " << pFull[(size_t)bin] << " dB\n";
        std::cout << (isCandidate ? "Diagnosis: candidate stage DOES find it -- any remaining gap is scheduling/refinement, not detection.\n"
                                   : "Diagnosis: candidate stage FAILS to flag it -- problem is in the cheap-pass threshold, not scheduling.\n");
    }

    //======================================================================
    std::cout << "\n==================== B. +8dB PIPELINE TRACE (safe freq, 5 frames) ====================\n";
    {
        double f0 = SAFE_FREQS[2]; // 5500 Hz
        auto frame = injectRelativeToLocalBaseline(genGuitar(sr), sr, f0, 8.0, 0.03);
        int bin = freqToBin(f0, sr);
        auto eng = makeTiered(sr, 8, 4, 4);
        std::vector<float> prom;
        std::printf("%6s | %10s | %10s | %8s | %6s\n","frame","blended","rank","refined","tier");
        for (int f=0; f<5; ++f)
        {
            eng.computeProminence(frame, 10.0f, prom);
            int rank=-1; bool refined=false; int tier=0;
            for (auto& rr : eng.lastRegionResults()) if (bin>=rr.startBin && bin<=rr.endBin) { rank=rr.rank; refined=rr.refined||rr.fromCache; tier=rr.tier; break; }
            std::printf("%6d | %10.3f | %10d | %8s | %6d\n", f, prom[(size_t)bin], rank, refined?"yes":"no", tier);
        }
        std::cout << "(true=8.0dB; if blended value stays wrong across frames despite refined=yes, the bug is in\n"
                     " region interpolation/context, not scheduling -- since content and refinement status are stable.)\n";
    }

    //======================================================================
    std::cout << "\n==================== 3. CORRECTED STARVATION TEST (genuine mid-priority candidate) ====================\n";
    {
        // Base with MANY simultaneous candidates of varying strength: harmonic stack
        // (many strong regions) + one deliberately medium-strength extra resonance.
        double midFreq = SAFE_FREQS[3]; // 9500 Hz -- clear of the 40-harmonic stack (max 4400Hz)
        auto frame = injectRelativeToLocalBaseline(genHarmonicStack(sr), sr, midFreq, 3.5, 0.03); // moderate, mid-priority
        int midBin = freqToBin(midFreq, sr);

        for (auto& cfg : { std::make_pair("OFF (no fairness, no tiers)", 0), std::make_pair("TIERED(8,4,4)", 1) })
        {
            auto eng = cfg.second ? makeTiered(sr, 8, 4, 4) : SpectralProminenceEngineV4{};
            if (! cfg.second)
            {
                eng.prepare(bins, sr, fftSize, 36, 72);
                eng.setNarrowScaleParams(0.04, 0.9);
                eng.setNarrowCandidateThresholdDb(2.5, 3.0);
                eng.setValleySplitThresholdDb(3.0);
                eng.setSmallRegionWidthBins(3);
                eng.setRegionBudget(16);
                eng.setFairnessAging(false);
            }
            std::vector<float> prom;
            eng.computeProminence(frame, 8.0f, prom); // frame 0: confirm candidate + get initial rank
            bool isCandidate=false; int initialRank=-1;
            for (auto& rr : eng.lastRegionResults()) if (midBin>=rr.startBin && midBin<=rr.endBin) { isCandidate=true; initialRank=rr.rank; break; }

            int refinements=0, maxGap=0, gap=0, firstRefined=-1;
            for (int f=1; f<500; ++f)
            {
                eng.computeProminence(frame, 8.0f, prom);
                bool served=false;
                for (auto& rr : eng.lastRegionResults()) if (midBin>=rr.startBin && midBin<=rr.endBin) { served = rr.refined||rr.fromCache; break; }
                if (served) { if (firstRefined<0) firstRefined=f; ++refinements; maxGap=juce::jmax(maxGap,gap); gap=0; } else ++gap;
            }
            maxGap = juce::jmax(maxGap, gap);
            std::cout << "  " << cfg.first << ": candidate=" << (isCandidate?"YES":"NO") << " initialRank=" << initialRank
                       << " firstRefinedFrame=" << firstRefined << " totalRefinements(500f)=" << refinements
                       << " maxConsecutiveFramesWithoutRefinement=" << maxGap << " (" << (maxGap*frameMs) << "ms)\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 8. DEADLINE GUARANTEE CHECK (tier3 slots vs region count) ====================\n";
    {
        auto frame = genDrums(sr); // ~220 regions, worst case
        for (int t3 : {2,4,8,12})
        {
            auto eng = makeTiered(sr, 8, 4, t3);
            std::vector<float> prom;
            eng.computeProminence(frame, 4.0f, prom);
            int regionCount = eng.lastRegionCount();
            int predictedWorstCase = (t3>0) ? (int)std::ceil((double)regionCount/t3) : -1;
            int observedMaxAge=0;
            for (int f=0; f<juce::jmax(50,predictedWorstCase+10); ++f) { eng.computeProminence(frame,4.0f,prom); observedMaxAge=juce::jmax(observedMaxAge, eng.lastMaxAgeAtSelection()); }
            std::cout << "  tier3Slots=" << t3 << ": regions=" << regionCount << " predicted worst-case wait<=" << predictedWorstCase
                       << " frames (" << (predictedWorstCase*frameMs) << "ms), observed max age seen=" << observedMaxAge << " frames\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 10. NEW RESONANCE IN DENSE CONTENT (staged timing, safe freq) ====================\n";
    {
        auto denseBase = genDenseMix(sr);
        for (double lvl : {2.0, 4.0, 8.0})
        {
            auto eng = makeTiered(sr, 8, 4, 4);
            std::vector<float> prom;
            for (int f=0; f<20; ++f) eng.computeProminence(denseBase, 8.0f, prom); // settle baseline regions

            double f0 = SAFE_FREQS[4]; // 13500 Hz
            auto withNew = injectRelativeToLocalBaseline(denseBase, sr, f0, lvl, 0.03);
            int newBin = freqToBin(f0, sr);
            int tCandidate=-1, tSchedule=-1, tRefine=-1;
            for (int f=0; f<30; ++f)
            {
                eng.computeProminence(withNew, 8.0f, prom);
                bool isCand=false, wasScheduled=false, wasRefined=false;
                for (auto& rr : eng.lastRegionResults())
                    if (newBin>=rr.startBin && newBin<=rr.endBin) { isCand=true; wasScheduled = rr.rank>=0; wasRefined = rr.refined||rr.fromCache; break; }
                if (tCandidate<0 && isCand) tCandidate=f;
                if (tSchedule<0 && wasScheduled) tSchedule=f;
                if (tRefine<0 && wasRefined) tRefine=f;
            }
            std::cout << "  +" << lvl << "dB: Tcandidate=" << tCandidate << " Tschedule=" << tSchedule << " Trefine=" << tRefine
                       << "  (frames; " << frameMs << "ms/frame @192kHz)\n";
        }
    }

    //======================================================================
    std::cout << "\n==================== 11. CPU BY SUBSYSTEM (192kHz, dense scene) ====================\n";
    {
        auto frame = genDrums(sr);
        auto eng = makeTiered(sr, 8, 4, 4);
        std::vector<float> prom;
        eng.computeProminence(frame, 4.0f, prom); // warm
        const int iters=300;
        std::vector<double> totalT, broadT, mediumT, groupT, refineT;
        for (int i=0;i<iters;++i)
        {
            auto t0=std::chrono::high_resolution_clock::now();
            eng.computeProminence(frame,4.0f,prom);
            auto t1=std::chrono::high_resolution_clock::now();
            totalT.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0);
            broadT.push_back(eng.lastBroadUs()*2.0); mediumT.push_back(eng.lastMediumUs()*2.0);
            groupT.push_back(eng.lastGroupingUs()*2.0); refineT.push_back(eng.lastNarrowRefineUs()*2.0);
        }
        std::sort(totalT.begin(),totalT.end());
        double hopUs=1000000.0*512.0/sr;
        double p99total=totalT[(size_t)(totalT.size()*0.99)];
        double avgB=0,avgM=0,avgG=0,avgR=0; for(size_t i=0;i<totalT.size();++i){avgB+=broadT[i];avgM+=mediumT[i];avgG+=groupT[i];avgR+=refineT[i];}
        avgB/=totalT.size();avgM/=totalT.size();avgG/=totalT.size();avgR/=totalT.size();
        std::cout << "  P99 total=" << p99total << "us (" << (100.0*p99total/hopUs) << "% of hop)\n";
        std::cout << "  avg BROAD=" << avgB << "us  MEDIUM=" << avgM << "us  GROUPING(incl.sort/tier-select)=" << avgG
                   << "us  NARROW-REFINE(incl.P25+cache)=" << avgR << "us\n";
        std::cout << "  NOTE: grouping timer currently also includes region formation only; tier-selection sort cost\n";
        std::cout << "  is counted inside NARROW-REFINE's window in this build (measured before the P25 loop starts).\n";
    }

    //======================================================================
    std::cout << "\n==================== 12. BUDGET/TIER COMPARISON (after scheduler fix) ====================\n";
    {
        struct Cfg{ const char* name; int t1,t2,t3; };
        Cfg configs[] = {
            {"8/4/4 (total16)", 8,4,4}, {"12/6/6 (total24)",12,6,6}, {"16/8/8 (total32)",16,8,8},
            {"4/2/10 (fairness-heavy)",4,2,10}, {"12/2/2 (urgent-heavy)",12,2,2},
        };
        auto denseBase = genDenseMix(sr);
        SpectralProminenceEngineV2 refEng; refEng.prepare(bins, sr, fftSize, 48);
        double hopUs=1000000.0*512.0/sr;
        std::printf("%-24s | %10s | %10s | %10s | %14s | %14s\n","config","P99CPU%","prom.MAE","+2dBerr","maxAge(frames)","maxAge(ms)");
        for (auto& cfg : configs)
        {
            auto eng = makeTiered(sr, cfg.t1, cfg.t2, cfg.t3);
            std::vector<float> prom;
            eng.computeProminence(denseBase, 4.0f, prom);
            std::vector<double> times;
            for (int i=0;i<300;++i){ auto t0=std::chrono::high_resolution_clock::now(); eng.computeProminence(denseBase,4.0f,prom); auto t1=std::chrono::high_resolution_clock::now(); times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count()*2.0); }
            std::sort(times.begin(),times.end());
            double p99=times[(size_t)(times.size()*0.99)];

            auto engAcc = makeTiered(sr, cfg.t1, cfg.t2, cfg.t3);
            double sumErr=0, sumErr2=0; int n=0,n2=0;
            std::vector<float> pAcc, pRef;
            for (double f0 : SAFE_FREQS) for (double lvl : {2.0,4.0,8.0,12.0})
            {
                auto rframe = injectRelativeToLocalBaseline(genGuitar(sr), sr, f0, lvl, 0.03);
                refEng.computeProminenceFullBinReference(rframe, 10.0f, pRef);
                engAcc.computeProminence(rframe, 10.0f, pAcc);
                int bin=freqToBin(f0,sr);
                double err=std::abs(pAcc[(size_t)bin]-lvl); sumErr+=err; ++n;
                if (lvl==2.0) { sumErr2+=err; ++n2; }
            }

            auto engAge = makeTiered(sr, cfg.t1, cfg.t2, cfg.t3);
            int maxAge=0;
            for (int f=0; f<300; ++f) { engAge.computeProminence(genDrums(sr), 4.0f, prom); maxAge=juce::jmax(maxAge, engAge.lastMaxAgeAtSelection()); }

            std::printf("%-24s | %9.2f%% | %10.4f | %10.4f | %14d | %14.2f\n",
                cfg.name, 100.0*p99/hopUs, sumErr/n, sumErr2/n2, maxAge, maxAge*frameMs);
        }
    }

    return 0;
}
