#include "StressorEngine.h"

namespace NF
{

namespace
{
    constexpr float kThresholdDb = -18.0f; // fixed internal reference the INPUT knob drives against
    constexpr float kSoftKneeWidthDb = 6.0f;
    constexpr float kNukeKneeWidthDb = 1.0f;
    constexpr float kLevelSmoothMs = 0.3f; // fast rectifier smoothing, not the musical envelope

    float calcOnePoleCoeff(double timeMs, double sr)
    {
        if (timeMs <= 0.0)
            return 0.0f;
        return static_cast<float>(std::exp(-1.0 / (0.001 * timeMs * sr)));
    }

    // Knob 0..10 -> ms, nonlinear so the fast end (FET-like, near-instant attack)
    // gets more turning resolution, matching the feel of the real front panel.
    float mapAttackMs(float amount)
    {
        const float t = juce::jlimit(0.0f, 1.0f, amount / 10.0f);
        const float shaped = std::pow(1.0f - t, 2.0f);
        return juce::jmap(shaped, 0.0f, 1.0f, 0.15f, 40.0f);
    }

    float mapReleaseMs(float amount)
    {
        const float t = juce::jlimit(0.0f, 1.0f, amount / 10.0f);
        const float shaped = std::pow(t, 1.5f);
        return juce::jmap(shaped, 0.0f, 1.0f, 50.0f, 4000.0f);
    }

    // Drive-compensated tanh waveshaper: stays unity-gain at x = 1 regardless
    // of drive/asymmetry, so engaging a character switch colours the sound
    // without an overall level jump.
    float driveShape(float x, float drive, float asymmetry)
    {
        const float biased = drive * x + asymmetry;
        const float shaped = std::tanh(biased) - std::tanh(asymmetry);
        const float norm = std::tanh(drive + asymmetry) - std::tanh(asymmetry);
        return norm > 1.0e-6f ? shaped / norm : x;
    }
}

void StressorEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    channels.clear();
    for (juce::uint32 ch = 0; ch < spec.numChannels; ++ch)
    {
        auto* state = channels.add(new ChannelState());
        *state->scHpf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 150.0f);
        state->scHpf.prepare(spec);
    }

    dryBuffer.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);

    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    reset();
}

void StressorEngine::reset()
{
    for (auto* state : channels)
    {
        state->scHpf.reset();
        state->levelSmoothed = 0.0f;
        state->grDb = 0.0f;
        state->releaseMemory = 0.0f;
    }

    inputGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(params.inputDb));
    outputGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(params.outputDb));
    mixSmoothed.setCurrentAndTargetValue(params.mixPct / 100.0f);
    gainReductionDb.store(0.0f);
}

float StressorEngine::computeGainReductionDb(float levelDb, float ratio, bool nukeEngaged, bool bigNuke) const
{
    const float overDb = levelDb - kThresholdDb;

    // NUKE overrides the ratio entirely with an effectively infinite ratio
    // and a razor-thin knee — a true brick-wall limiter stage, stacked on
    // top of whatever RATIO/knee is already selected rather than replacing
    // the UI's ratio choice.
    constexpr float kBrickWallRatio = 500.0f;
    constexpr float kBrickWallKneeDb = 0.3f;
    const float effectiveRatio = bigNuke ? kBrickWallRatio : ratio;
    const float kneeWidth = bigNuke ? kBrickWallKneeDb : (nukeEngaged ? kNukeKneeWidthDb : kSoftKneeWidthDb);

    float reduction;
    if (2.0f * overDb < -kneeWidth)
        reduction = 0.0f;
    else if (2.0f * std::abs(overDb) <= kneeWidth)
        reduction = ((1.0f / effectiveRatio - 1.0f) * std::pow(overDb + kneeWidth * 0.5f, 2.0f)) / (2.0f * kneeWidth);
    else
        reduction = overDb * (1.0f / effectiveRatio - 1.0f);

    float grDb = -reduction; // reduction is <= 0 (a gain change); track the magnitude
    if (bigNuke)
        grDb *= 1.35f; // extra redline bite, harder than the mild 10:1/20:1 nuke stage
    else if (nukeEngaged)
        grDb *= 1.15f;

    return juce::jmax(0.0f, grDb);
}

float StressorEngine::detectAndFollow(ChannelState& state, float absLevel, float ratio, bool nukeEngaged, bool bigNuke, bool optoEngaged,
                                      float attackAmount, float releaseAmount)
{
    const float levelCoeff = calcOnePoleCoeff(kLevelSmoothMs, sampleRate);
    state.levelSmoothed += (absLevel - state.levelSmoothed) * (1.0f - levelCoeff);

    const float levelDb = juce::Decibels::gainToDecibels(state.levelSmoothed, -100.0f);
    const float grTargetDb = computeGainReductionDb(levelDb, ratio, nukeEngaged, bigNuke);

    // OPTO mode (RATIO = 10:1, the hardware's dedicated "Opto" position):
    // swaps in a program-dependent detector entirely, ignoring the ATTACK/
    // RELEASE knobs — bigger transients get a slower, gentler attack instead
    // of slamming instantly, and release is driven by a slow (~1.5 s) memory
    // of recent gain reduction, so it takes longer to let go the harder it's
    // been hit. Any other ratio uses the plain fixed FET attack/release time
    // set by the knobs.
    float attackMsUsed;
    if (optoEngaged)
    {
        attackMsUsed = juce::jmap(juce::jlimit(0.0f, 24.0f, grTargetDb), 0.0f, 24.0f, 8.0f, 60.0f);
    }
    else
    {
        attackMsUsed = mapAttackMs(attackAmount);
    }
    // A brick-wall limiter can't leak transients while it waits on the
    // ATTACK knob — NUKE forces a near-instant catch regardless of that
    // knob's setting (or of OPTO mode).
    if (bigNuke)
        attackMsUsed = juce::jmin(attackMsUsed, 0.3f);
    const float attackCoeff = calcOnePoleCoeff(attackMsUsed, sampleRate);

    float releaseMsUsed;
    if (optoEngaged)
    {
        const float memoryCoeff = calcOnePoleCoeff(1500.0, sampleRate);
        state.releaseMemory += (grTargetDb - state.releaseMemory) * (1.0f - memoryCoeff);
        releaseMsUsed = juce::jmap(juce::jlimit(0.0f, 20.0f, state.releaseMemory), 0.0f, 20.0f, 300.0f, 3000.0f);
    }
    else
    {
        // Keep the memory primed with the live GR while OPTO isn't engaged,
        // so re-entering 10:1 later starts from a sensible value instead of
        // whatever was left over from the last time it was active.
        state.releaseMemory = grTargetDb;
        releaseMsUsed = mapReleaseMs(releaseAmount);
    }
    const float releaseCoeff = calcOnePoleCoeff(releaseMsUsed, sampleRate);

    const float coeff = grTargetDb > state.grDb ? attackCoeff : releaseCoeff;
    state.grDb = state.grDb * coeff + grTargetDb * (1.0f - coeff);

    return state.grDb;
}

float StressorEngine::shapeCharacter(float x) const
{
    if (!params.dist2Enabled && !params.dist3Enabled)
        return driveShape(x, 1.5f, 0.0f); // clean

    if (params.dist2Enabled && !params.dist3Enabled)
        return driveShape(x, 3.0f, 0.35f); // FET-ish, asymmetric

    if (!params.dist2Enabled && params.dist3Enabled)
        return driveShape(x, 4.0f, 0.0f); // opto-ish, smoother but more driven

    // both -> coloured "British"/Nuke character: cascade both curves
    const float y = driveShape(x, 3.0f, 0.35f);
    return driveShape(y, 2.0f, -0.25f);
}

float StressorEngine::shapeCharacterWithNuke(float x) const
{
    float y = shapeCharacter(x);
    if (params.nukeMode)
        y = driveShape(y, 1.8f, 0.0f); // extra grit/bite on top, whatever the character switches are doing
    return y;
}

void StressorEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels.size());

    if (numChannels == 0 || numSamples == 0)
        return;

    if (params.bypass)
    {
        gainReductionDb.store(0.0f);
        return;
    }

    inputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(params.inputDb));
    outputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(params.outputDb));
    mixSmoothed.setTargetValue(params.mixPct / 100.0f);

    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    const float ratio = kRatioTable[(size_t) juce::jlimit(0, (int) kRatioTable.size() - 1, params.ratioIndex)];
    const bool nukeEngaged = params.ratioIndex >= kNukeStartIndex;
    const bool bigNuke = params.nukeMode;
    // OPTO is only really engaged while ATTACK/RELEASE are still sitting at
    // their panel "OPTO" detent (10 / 0) — the knobs stay fully draggable,
    // so moving either one away leaves OPTO (and the panel's single LED,
    // driven by the same condition — see isOptoActive() in the editor)
    // even though RATIO itself can stay at 10:1.
    const bool optoEngaged = params.ratioIndex == kOptoRatioIndex
                            && params.attackAmount >= 9.95f
                            && params.releaseAmount <= 0.05f;
    const float attackAmount = params.attackAmount;
    const float releaseAmount = params.releaseAmount;

    const bool linked = params.linkEnabled && numChannels > 1;
    float maxGrDb = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputGain = inputGainSmoothed.getNextValue();
        const float outputGain = outputGainSmoothed.getNextValue();
        const float mixAmount = mixSmoothed.getNextValue();

        float scSamples[8] = {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float driven = buffer.getSample(ch, i) * inputGain;
            scSamples[ch] = params.hpEnabled ? channels[ch]->scHpf.processSample(driven) : driven;
        }

        float linkedGrDb = 0.0f;
        if (linked)
        {
            float linkedLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                linkedLevel = juce::jmax(linkedLevel, std::abs(scSamples[ch]));

            linkedGrDb = detectAndFollow(*channels[0], linkedLevel, ratio, nukeEngaged, bigNuke, optoEngaged,
                                        attackAmount, releaseAmount);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float chGrDb = linked ? linkedGrDb
                                        : detectAndFollow(*channels[ch], std::abs(scSamples[ch]), ratio, nukeEngaged, bigNuke, optoEngaged,
                                                          attackAmount, releaseAmount);

            maxGrDb = juce::jmax(maxGrDb, chGrDb);

            const float driven = buffer.getSample(ch, i) * inputGain;
            const float compressed = driven * juce::Decibels::decibelsToGain(-chGrDb);
            const float wet = shapeCharacterWithNuke(compressed);
            const float dry = dryBuffer.getSample(ch, i);

            const float mixed = dry * (1.0f - mixAmount) + wet * mixAmount;
            buffer.setSample(ch, i, mixed * outputGain);
        }
    }

    gainReductionDb.store(maxGrDb);
}

} // namespace NF
