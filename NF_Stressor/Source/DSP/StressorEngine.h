#pragma once
#include <JuceHeader.h>

namespace NF
{

// Ratio choices as shown on the front panel: 1:1 (bypass) through 6:1 are
// classic soft-knee compression; 10:1 and 20:1 engage the extra NUKE/redline
// stage (harder knee + a touch more drive into the character stage).
inline constexpr std::array<float, 7> kRatioTable { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f, 20.0f };
inline constexpr int kNukeStartIndex = 5; // 10:1 and above
inline constexpr int kOptoRatioIndex = 5; // 10:1 only — real hardware's "Opto" ratio position;
                                          // selecting it swaps in a program-dependent detector,
                                          // independent of the ATTACK/RELEASE knob settings

// One call per processed block; the engine smooths internally so parameter
// changes never zipper/click even though this only lands once per block.
struct StressorParameters
{
    float inputDb = 0.0f;      // -20..20, drives the signal into a fixed internal
                               // reference — there is no manual threshold, exactly
                               // like the hardware this emulates.
    float attackAmount = 5.0f;  // 0..10
    float releaseAmount = 5.0f; // 0..10
    int ratioIndex = 3;         // index into kRatioTable, default 4:1

    bool hpEnabled = false;    // DETECTOR column, top switch: sidechain highpass
    bool linkEnabled = true;   // DETECTOR column, bottom switch: stereo-linked detection

    bool dist2Enabled = false; // AUDIO column, top switch: FET-style character
    bool dist3Enabled = false; // AUDIO column, bottom switch: opto-style character
                               // (both on together = coloured "British"/Nuke character)

    float mixPct = 100.0f;     // 0..100
    float outputDb = 0.0f;     // -20..20

    bool bypass = false;

    // Dedicated NUKE button next to MIX — independent of RATIO. Engages a
    // true brick-wall limiter stage (effectively infinite ratio, razor-thin
    // knee, near-instant catch) layered on top of whatever RATIO/character
    // is already dialled in, with extra harmonic bite for that "redlined"
    // edge — a genuinely more extreme mode, not just a shortcut to 20:1.
    bool nukeMode = false;
};

// Faithful-in-spirit emulation of a classic opto/FET "Distressor"-style
// channel compressor, stripped to the controls of the vertical-strip front
// panel: sidechain HPF -> envelope detector with a real discrete OPTO mode
// (ATTACK all the way up / RELEASE all the way down engages a slower,
// program-dependent opto-cell response instead of the fixed FET time,
// exactly like the hardware's end-of-travel detents) -> soft/hard-knee ratio
// gain computer with an extra NUKE stage at 10:1/20:1 -> FET/opto character
// shaping -> dry/wet mix -> output trim.
class StressorEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(const StressorParameters& newParams) { params = newParams; }

    void process(juce::AudioBuffer<float>& buffer);

    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> scHpf;
        float levelSmoothed = 0.0f; // linear, fast one-pole rectifier smoothing
        float grDb = 0.0f;
        float releaseMemory = 0.0f; // slow average of recent GR; drives the OPTO
                                    // release's "the harder it's been hit, the
                                    // longer it takes to let go" behaviour
    };

    float detectAndFollow(ChannelState& state, float absLevel, float ratio, bool nukeEngaged, bool bigNuke, bool optoEngaged,
                          float attackAmount, float releaseAmount);
    float computeGainReductionDb(float levelDb, float ratio, bool nukeEngaged, bool bigNuke) const;
    float shapeCharacter(float x) const;
    float shapeCharacterWithNuke(float x) const;

    double sampleRate = 44100.0;

    juce::OwnedArray<ChannelState> channels;
    juce::AudioBuffer<float> dryBuffer;

    StressorParameters params;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    std::atomic<float> gainReductionDb { 0.0f };
};

} // namespace NF
