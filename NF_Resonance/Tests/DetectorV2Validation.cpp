// NF Resonance -- Detector benchmark harness (baseline against the current
// V1 detector, ahead of the V2 redesign). Offline investigation only.
#include <JuceHeader.h>
#include <map>
#include "PluginProcessor.h"
#include "DSP/SpectralEngine.h"

using juce::AudioBuffer;

static void setParam(NFResonanceAudioProcessor& proc, const char* id, float value)
{
    if (auto* p = proc.state().getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}
static void runThrough(NFResonanceAudioProcessor& proc, AudioBuffer<float>& buf, int blockSize)
{
    juce::MidiBuffer midi; const int total = buf.getNumSamples();
    for (int pos=0; pos<total; pos+=blockSize)
    {
        int n = juce::jmin(blockSize, total-pos);
        AudioBuffer<float> block(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, n);
        proc.processBlock(block, midi);
    }
}

// Paul Kellett economy pink noise filter.
static AudioBuffer<float> genPinkNoise(int channels, int numSamples, float amp, int seed)
{
    AudioBuffer<float> b(channels, numSamples);
    juce::Random rng(seed);
    for (int c=0;c<channels;++c)
    {
        float b0=0,b1=0,b2=0;
        for (int i=0;i<numSamples;++i)
        {
            float white = rng.nextFloat()*2.0f-1.0f;
            b0 = 0.99765f*b0 + white*0.0990460f;
            b1 = 0.96300f*b1 + white*0.2965164f;
            b2 = 0.57000f*b2 + white*1.0526913f;
            float pink = b0+b1+b2+white*0.1848f;
            b.setSample(c,i,pink*amp*0.25f);
        }
    }
    return b;
}

// Apply a peaking-EQ boost (JUCE biquad) to shape a resonance onto pink noise.
static void applyResonance(AudioBuffer<float>& buf, double sr, double freq, double q, double gainDb)
{
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, (float)freq, (float)q, juce::Decibels::decibelsToGain((float)gainDb));
    for (int c=0;c<buf.getNumChannels();++c)
    {
        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = coeffs;
        juce::dsp::AudioBlock<float> block(buf);
        auto chanBlock = block.getSingleChannelBlock((size_t)c);
        juce::dsp::ProcessContextReplacing<float> ctx(chanBlock);
        filter.prepare({ sr, (juce::uint32)buf.getNumSamples(), 1 });
        filter.process(ctx);
    }
}

static double toDb(double lin){ return 20.0*std::log10(juce::jmax(lin,1e-15)); }

// Compute a simple local-mean prominence at a given bin, matching V1's own
// method (box-car mean, radius derived the same way V1 does at default
// Sharpness), so "input prominence" is measured consistently with what the
// detector itself would see.
static float computeProminenceAtBin(const std::vector<float>& magDb, int bin, int radius)
{
    int n=(int)magDb.size();
    int a=juce::jmax(0,bin-radius), b=juce::jmin(n-1,bin+radius);
    double sum=0; for (int i=a;i<=b;++i) sum+=magDb[(size_t)i];
    double mean=sum/(b-a+1);
    return (float)(magDb[(size_t)bin]-mean);
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const int fftSize = 2048;

    double freqs[] = { 120, 250, 500, 1000, 2500, 4000, 7000, 10000, 14000 };
    double levels[] = { 2, 4, 8, 12, 18 };
    struct QDef{ const char* label; double q; };
    QDef qs[] = { {"wide",0.7}, {"medium",2.0}, {"narrow",6.0}, {"xnarrow",15.0} };

    juce::File csvFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nf_resonance_detector_v1_benchmark.csv");
    juce::FileOutputStream os(csvFile); os.setPosition(0); os.truncate();
    os << "frequency,level_dB,q_label,q_value,input_prominence_dB,detected_prominence_dB,applied_reduction_dB,affected_bandwidth_Hz,attack_ms,release_ms\n";

    std::cout << "Running " << (9*5*4) << " combinations (9 freq x 5 level x 4 Q)...\n";
    std::cout << "Full results: " << csvFile.getFullPathName() << "\n\n";

    // Representative summary accumulators
    struct Agg{ double sumReduction=0; int n=0; };
    std::map<double,Agg> byFreq;
    std::map<double,Agg> byLevel;
    std::map<juce::String,Agg> byQ;

    int radiusAtDefaultSharpness = juce::jlimit(2,48,(int)std::round(34.0-4.0*2.8)); // Sharpness default=4

    for (double freq : freqs)
    {
        for (double level : levels)
        {
            for (auto& q : qs)
            {
                // Reference (no resonance) spectrum
                auto refNoise = genPinkNoise(2, (int)(sr*3.0), 0.2f, 42);
                auto refProc = std::make_unique<NFResonanceAudioProcessor>();
                refProc->prepareToPlay(sr, blockSize);
                AudioBuffer<float> refOut; refOut.makeCopyOf(refNoise);
                runThrough(*refProc, refOut, blockSize);
                auto refSpectrum = refProc->engine().getLastSpectrum();

                // Test signal: pink noise + resonance
                auto testNoise = genPinkNoise(2, (int)(sr*3.0), 0.2f, 42);
                applyResonance(testNoise, sr, freq, q.q, level);
                auto proc = std::make_unique<NFResonanceAudioProcessor>();
                proc->prepareToPlay(sr, blockSize);
                AudioBuffer<float> out; out.makeCopyOf(testNoise);
                runThrough(*proc, out, blockSize);
                auto testSpectrum = proc->engine().getLastSpectrum();
                auto reduction = proc->engine().getLastReduction();

                int bin = (int)std::round(freq * fftSize / sr);
                bin = juce::jlimit(0, (int)testSpectrum.size()-1, bin);

                float inputProminence = (bin < (int)testSpectrum.size() && bin < (int)refSpectrum.size())
                    ? (testSpectrum[(size_t)bin] - refSpectrum[(size_t)bin]) : 0.0f;
                float detectedProminence = computeProminenceAtBin(testSpectrum, bin, radiusAtDefaultSharpness);
                float appliedReduction = (bin < (int)reduction.size()) ? -reduction[(size_t)bin] : 0.0f;

                // affected bandwidth: how far reduction stays > 50% of peak, around bin
                float peakRed = appliedReduction;
                int lo=bin, hiB=bin;
                while (lo>0 && bin<(int)reduction.size() && -reduction[(size_t)lo] > peakRed*0.5f) --lo;
                while (hiB<(int)reduction.size()-1 && -reduction[(size_t)hiB] > peakRed*0.5f) ++hiB;
                double bwHz = (hiB-lo) * sr / fftSize;

                // Attack/release: measure via a separate run where the resonance turns on at t=1s and off at t=2s
                double attackMs=-1, releaseMs=-1;
                {
                    auto dynNoise = genPinkNoise(2, (int)(sr*3.0), 0.2f, 43);
                    int onSample = (int)(sr*1.0), offSample = (int)(sr*2.0);
                    AudioBuffer<float> resOnly(2, dynNoise.getNumSamples()); resOnly.clear();
                    {
                        AudioBuffer<float> tmp; tmp.makeCopyOf(dynNoise);
                        applyResonance(tmp, sr, freq, q.q, level);
                        for (int i=onSample;i<offSample && i<tmp.getNumSamples();++i)
                            for (int c=0;c<2;++c) resOnly.setSample(c,i, tmp.getSample(c,i)-dynNoise.getSample(c,i));
                    }
                    AudioBuffer<float> dynIn; dynIn.makeCopyOf(dynNoise);
                    for (int i=0;i<dynIn.getNumSamples();++i) for (int c=0;c<2;++c)
                        dynIn.setSample(c,i, dynIn.getSample(c,i)+resOnly.getSample(c,i));

                    auto dynProc = std::make_unique<NFResonanceAudioProcessor>();
                    dynProc->prepareToPlay(sr, blockSize);
                    // Process in hop-sized chunks, sampling reduction at bin after each frame
                    juce::MidiBuffer midi;
                    std::vector<float> redTrace;
                    int lat = dynProc->getLatencySamples();
                    for (int pos=0; pos<dynIn.getNumSamples(); pos+=blockSize)
                    {
                        int n=juce::jmin(blockSize,dynIn.getNumSamples()-pos);
                        AudioBuffer<float> block(dynIn.getArrayOfWritePointers(),2,pos,n);
                        dynProc->processBlock(block, midi);
                        auto r = dynProc->engine().getLastReduction();
                        if (bin<(int)r.size()) redTrace.push_back(-r[(size_t)bin]);
                    }
                    // find sample index (in blocks) crossing 90% of final reduction after onSample+lat, and 10% after offSample+lat
                    float finalRed = redTrace.empty()?0.0f:redTrace.back();
                    int onBlockIdx = (onSample+lat)/blockSize;
                    int offBlockIdx = (offSample+lat)/blockSize;
                    float maxDuring=0;
                    for (int i=onBlockIdx;i<juce::jmin((int)redTrace.size(),offBlockIdx);++i) maxDuring=juce::jmax(maxDuring,redTrace[(size_t)i]);
                    for (int i=onBlockIdx;i<juce::jmin((int)redTrace.size(),offBlockIdx);++i)
                        if (redTrace[(size_t)i] >= maxDuring*0.9f) { attackMs = (i-onBlockIdx)*blockSize/sr*1000.0; break; }
                    for (int i=offBlockIdx;i<(int)redTrace.size();++i)
                        if (redTrace[(size_t)i] <= maxDuring*0.1f) { releaseMs = (i-offBlockIdx)*blockSize/sr*1000.0; break; }
                }

                os << freq << "," << level << "," << q.label << "," << q.q << ","
                   << inputProminence << "," << detectedProminence << "," << appliedReduction << ","
                   << bwHz << "," << attackMs << "," << releaseMs << "\n";

                byFreq[freq].sumReduction += appliedReduction; byFreq[freq].n++;
                byLevel[level].sumReduction += appliedReduction; byLevel[level].n++;
                byQ[q.label].sumReduction += appliedReduction; byQ[q.label].n++;
            }
        }
    }
    os.flush();

    std::cout << "==================== SUMMARY: mean applied reduction by frequency (all levels/Q) ====================\n";
    for (double f : freqs) std::cout << "  " << f << " Hz: " << (byFreq[f].sumReduction/byFreq[f].n) << " dB avg\n";

    std::cout << "\n==================== SUMMARY: mean applied reduction by injected level (all freq/Q) ====================\n";
    for (double l : levels) std::cout << "  +" << l << " dB injected: " << (byLevel[l].sumReduction/byLevel[l].n) << " dB avg reduction\n";

    std::cout << "\n==================== SUMMARY: mean applied reduction by Q ====================\n";
    for (auto& q : qs) std::cout << "  " << q.label << " (Q=" << q.q << "): " << (byQ[q.label].sumReduction/byQ[q.label].n) << " dB avg reduction\n";

    std::cout << "\nDone. Full 180-row CSV at: " << csvFile.getFullPathName() << "\n";
    return 0;
}
