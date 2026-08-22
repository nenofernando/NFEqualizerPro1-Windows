#pragma once
#include <JuceHeader.h>

namespace NF
{

enum class TapeType { GP9 = 0, T456 = 1, T499 = 2, T250 = 3 };
enum class TapeSpeed { IPS7_5 = 0, IPS15 = 1, IPS30 = 2 };
enum class ReproHead { NAB = 0, IEC = 1 };

// Clean-room character profile per tape "type" — not a model of any real
// stock, just a distinct saturation/noise/head-bump flavour per choice.
struct TapeTypeProfile
{
    float driveScale;      // multiplies the DRIVE knob's effective gain
    float evenHarmonic;    // amount of 2nd-harmonic colour mixed into the shaper
    float headBumpDb;      // extra head-bump gain on top of the speed's base bump
    float hfDamping;       // extra HF roll-off multiplier (1 = neutral)
    float noiseTint;       // relative hiss level for this stock
};

inline TapeTypeProfile getTapeTypeProfile(TapeType type)
{
    switch (type)
    {
        case TapeType::GP9:  return { 1.00f, 0.20f, 0.0f, 1.00f, 0.70f }; // modern, clean, extended
        case TapeType::T456: return { 1.15f, 0.45f, 1.4f, 1.15f, 1.00f }; // classic warm workhorse
        case TapeType::T499: return { 0.95f, 0.15f, 0.4f, 0.90f, 0.55f }; // bright, tight, low noise
        case TapeType::T250: return { 1.35f, 0.60f, 2.6f, 1.55f, 1.60f }; // vintage, lo-fi, gritty
    }
    return {};
}

struct TapeSpeedProfile
{
    float headBumpFreq;
    float headBumpDb;
    float hfRolloffHz;
    float wowFlutterScale;
    float noiseFloorScale;
};

inline TapeSpeedProfile getTapeSpeedProfile(TapeSpeed speed)
{
    switch (speed)
    {
        case TapeSpeed::IPS7_5: return { 65.0f, 3.2f, 9000.0f,  1.7f, 1.5f };
        case TapeSpeed::IPS15:  return { 52.0f, 1.6f, 15000.0f, 1.0f, 1.0f };
        case TapeSpeed::IPS30:  return { 42.0f, 0.6f, 19500.0f, 0.5f, 0.65f };
    }
    return {};
}

// One call per processed block; the engine smooths internally so parameter
// changes never zipper/click even though this only lands once per block.
struct TapeParameters
{
    float inputDb = 0.0f;
    float hpfHz = 20.0f;

    TapeType tapeType = TapeType::GP9;

    float drive = 4.5f;         // 0..10
    bool satEnabled = true;

    float bias = 5.0f;          // 0..10, 5 = centred
    bool biasCalEnabled = true;

    float wowRateHz = 0.6f;     // 0.1..10
    float wowDepthPct = 8.0f;   // 0..100
    bool wowFlutterEnabled = true;

    float noiseAmount = 3.0f;   // 0..10
    bool noiseEnabled = false;

    float eqLfDb = 0.0f;        // -12..12
    float eqHfDb = 0.0f;        // -12..12

    float outputDb = 0.0f;
    float lpfHz = 20000.0f;

    TapeSpeed tapeSpeed = TapeSpeed::IPS15;
    ReproHead reproHead = ReproHead::NAB;

    float tapeAgePct = 12.0f;   // 0..100, new -> worn

    float dropoutAmount = 2.0f; // 0..10
    bool dropoutsEnabled = false;

    float mixPct = 100.0f;      // 0..100
};

// Stereo (or mono) analog-tape emulator: input trim + HPF, wow/flutter
// (modulated delay line), tape saturation with a bias-offset asymmetry
// trick, head-bump resonance, HF self-erasure roll-off, repro-head EQ
// trim, user LF/HF trim, hiss, random dropouts, dry/wet mix, output LPF
// and trim. Everything is block-rate parameter smoothing over per-sample
// DSP so automation and knob moves stay click-free.
class TapeEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(const TapeParameters& newParams) { params = newParams; }

    void process(juce::AudioBuffer<float>& buffer);
    void processBypassed(juce::AudioBuffer<float>& buffer);

    float getOutputLevelL() const { return outputLevelDbL.load(); }
    float getOutputLevelR() const { return outputLevelDbR.load(); }

private:
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> hpf;
        juce::dsp::IIR::Filter<float> headBump;
        juce::dsp::IIR::Filter<float> hfLoss;
        juce::dsp::IIR::Filter<float> reproShelf;
        juce::dsp::IIR::Filter<float> eqLowShelf;
        juce::dsp::IIR::Filter<float> eqHighShelf;
        juce::dsp::IIR::Filter<float> outputLpf;
        juce::dsp::IIR::Filter<float> dcBlocker;
        juce::dsp::IIR::Filter<float> hissLpf;
        juce::dsp::IIR::Filter<float> hissHpf;

        std::vector<float> delayBuffer;
        int delayWriteIndex = 0;

        float wowPhase = 0.0f;
        float flutterPhase = 0.0f;

        juce::Random noiseRandom;

        float dropoutEnvelope = 1.0f;
        float dropoutTargetEnvelope = 1.0f;
        int dropoutHoldSamples = 0;
        juce::Random dropoutRandom;

        float envelopeFollower = -60.0f;
    };

    void updateFilters(ChannelState& state);
    float readModulatedDelay(ChannelState& state, float input, float depthPct);
    float shapeSaturation(float x, float driveKnob, float biasKnob,
                          const TapeTypeProfile& profile) const;

    double sampleRate = 44100.0;
    int maxDelaySamples = 0;

    juce::OwnedArray<ChannelState> channels;

    TapeParameters params;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> biasSmoothed;
    juce::SmoothedValue<float> wowDepthSmoothed;
    juce::SmoothedValue<float> noiseLevelSmoothed;

    std::atomic<float> outputLevelDbL { -60.0f };
    std::atomic<float> outputLevelDbR { -60.0f };
};

} // namespace NF
