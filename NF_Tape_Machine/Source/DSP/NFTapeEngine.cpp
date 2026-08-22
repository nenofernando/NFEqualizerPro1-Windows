#include "NFTapeEngine.h"

namespace NF
{

namespace
{
    constexpr float baseDelayMs = 10.0f;
    constexpr float maxModMs = 6.0f;
    constexpr float smoothingSeconds = 0.02f;
}

void TapeEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxDelaySamples = (int) std::ceil(sampleRate * 0.05) + 8;

    channels.clear();

    const int numChannels = (int) juce::jmax<juce::uint32>(1, spec.numChannels);

    juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* state = new ChannelState();

        state->hpf.prepare(monoSpec);
        state->headBump.prepare(monoSpec);
        state->hfLoss.prepare(monoSpec);
        state->reproShelf.prepare(monoSpec);
        state->eqLowShelf.prepare(monoSpec);
        state->eqHighShelf.prepare(monoSpec);
        state->outputLpf.prepare(monoSpec);
        state->dcBlocker.prepare(monoSpec);
        state->hissLpf.prepare(monoSpec);
        state->hissHpf.prepare(monoSpec);

        state->dcBlocker.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 5.0f);
        state->hissLpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 7500.0f);
        state->hissHpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 200.0f);

        state->delayBuffer.assign((size_t) maxDelaySamples, 0.0f);
        state->noiseRandom.setSeedRandomly();
        state->dropoutRandom.setSeedRandomly();

        updateFilters(*state);

        channels.add(state);
    }

    inputGainSmoothed.reset(sampleRate, smoothingSeconds);
    outputGainSmoothed.reset(sampleRate, smoothingSeconds);
    mixSmoothed.reset(sampleRate, smoothingSeconds);
    driveSmoothed.reset(sampleRate, smoothingSeconds);
    biasSmoothed.reset(sampleRate, smoothingSeconds);
    wowDepthSmoothed.reset(sampleRate, smoothingSeconds);
    noiseLevelSmoothed.reset(sampleRate, smoothingSeconds);

    reset();
}

void TapeEngine::reset()
{
    for (auto* state : channels)
    {
        state->hpf.reset();
        state->headBump.reset();
        state->hfLoss.reset();
        state->reproShelf.reset();
        state->eqLowShelf.reset();
        state->eqHighShelf.reset();
        state->outputLpf.reset();
        state->dcBlocker.reset();
        state->hissLpf.reset();
        state->hissHpf.reset();

        std::fill(state->delayBuffer.begin(), state->delayBuffer.end(), 0.0f);
        state->delayWriteIndex = 0;
        state->wowPhase = 0.0f;
        state->flutterPhase = 0.0f;
        state->dropoutEnvelope = 1.0f;
        state->dropoutTargetEnvelope = 1.0f;
        state->dropoutHoldSamples = 0;
        state->envelopeFollower = -60.0f;
    }

    outputLevelDbL.store(-60.0f);
    outputLevelDbR.store(-60.0f);
}

void TapeEngine::updateFilters(ChannelState& state)
{
    const auto typeProfile = getTapeTypeProfile(params.tapeType);
    const auto speedProfile = getTapeSpeedProfile(params.tapeSpeed);

    state.hpf.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, juce::jmax(10.0f, params.hpfHz));

    const float totalBumpDb = speedProfile.headBumpDb + typeProfile.headBumpDb;
    state.headBump.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, speedProfile.headBumpFreq, 0.9f,
        juce::Decibels::decibelsToGain(totalBumpDb));

    const float ageDamping = 1.0f + (params.tapeAgePct / 100.0f) * 0.7f;
    const float hfCutoff = juce::jlimit(1200.0f, (float) sampleRate * 0.45f,
        speedProfile.hfRolloffHz / (typeProfile.hfDamping * ageDamping));
    state.hfLoss.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, hfCutoff, 0.707f);

    const float speedWeight = (params.tapeSpeed == TapeSpeed::IPS7_5) ? 1.0f
                              : (params.tapeSpeed == TapeSpeed::IPS15) ? 0.6f : 0.2f;
    const float reproDb = (params.reproHead == ReproHead::NAB ? 1.5f : -1.0f) * speedWeight;
    state.reproShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, 100.0f, 0.707f, juce::Decibels::decibelsToGain(reproDb));

    state.eqLowShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, 150.0f, 0.707f, juce::Decibels::decibelsToGain(params.eqLfDb));

    state.eqHighShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, 6000.0f, 0.707f, juce::Decibels::decibelsToGain(params.eqHfDb));

    state.outputLpf.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jmax(500.0f, params.lpfHz), 0.707f);
}

float TapeEngine::readModulatedDelay(ChannelState& state, float input, float depthPct)
{
    const auto speedProfile = getTapeSpeedProfile(params.tapeSpeed);

    const float wowFreq = juce::jmax(0.05f, params.wowRateHz);
    const float flutterFreq = wowFreq * 6.0f + 4.0f;

    state.wowPhase += wowFreq / (float) sampleRate;
    if (state.wowPhase >= 1.0f) state.wowPhase -= 1.0f;

    state.flutterPhase += flutterFreq / (float) sampleRate;
    if (state.flutterPhase >= 1.0f) state.flutterPhase -= 1.0f;

    const float modMs =
        (0.7f * std::sin(juce::MathConstants<float>::twoPi * state.wowPhase) +
         0.3f * std::sin(juce::MathConstants<float>::twoPi * state.flutterPhase)) *
        maxModMs * (depthPct / 100.0f) * speedProfile.wowFlutterScale;

    const float delaySamples =
        juce::jlimit(1.0f, (float) maxDelaySamples - 2.0f,
                     (baseDelayMs + modMs) * 0.001f * (float) sampleRate);

    state.delayBuffer[(size_t) state.delayWriteIndex] = input;

    float readPos = (float) state.delayWriteIndex - delaySamples;
    while (readPos < 0.0f)
        readPos += (float) maxDelaySamples;

    const int index0 = (int) readPos % maxDelaySamples;
    const int index1 = (index0 + 1) % maxDelaySamples;
    const float frac = readPos - std::floor(readPos);

    const float out = state.delayBuffer[(size_t) index0] * (1.0f - frac) +
                       state.delayBuffer[(size_t) index1] * frac;

    state.delayWriteIndex = (state.delayWriteIndex + 1) % maxDelaySamples;

    return out;
}

float TapeEngine::shapeSaturation(float x, float driveKnob, float biasKnob,
                                  const TapeTypeProfile& profile) const
{
    const float driveGain = 1.0f + (driveKnob / 10.0f) * 4.0f * profile.driveScale;

    const float centred = (biasKnob - 5.0f) / 5.0f;
    const float biasOffset = params.biasCalEnabled
        ? centred * 0.05f
        : centred * 0.12f + 0.06f;

    const float driven = x * driveGain;
    float shaped = std::tanh(driven + biasOffset) - std::tanh(biasOffset);

    shaped += profile.evenHarmonic * 0.12f * driven * driven * (x >= 0.0f ? 1.0f : -1.0f);

    return shaped / std::sqrt(juce::jmax(1.0f, driveGain));
}

void TapeEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels.size());
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0)
        return;

    for (auto* state : channels)
        updateFilters(*state);

    inputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(params.inputDb));
    outputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(params.outputDb));
    mixSmoothed.setTargetValue(params.mixPct / 100.0f);
    driveSmoothed.setTargetValue(params.drive);
    biasSmoothed.setTargetValue(params.bias);
    wowDepthSmoothed.setTargetValue(params.wowFlutterEnabled ? params.wowDepthPct : 0.0f);
    noiseLevelSmoothed.setTargetValue(params.noiseEnabled ? params.noiseAmount : 0.0f);

    const auto typeProfile = getTapeTypeProfile(params.tapeType);
    const auto speedProfile = getTapeSpeedProfile(params.tapeSpeed);

    const float dropoutRate = params.dropoutAmount / 10.0f;
    const float dropoutAgeBoost = 1.0f + (params.tapeAgePct / 100.0f) * 2.5f;
    const float dropoutTriggerProb = (0.0000006f + dropoutRate * 0.000006f) * dropoutAgeBoost;

    const float noiseAgeBoost = 1.0f + (params.tapeAgePct / 100.0f) * 1.2f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float inputGain = inputGainSmoothed.getNextValue();
        const float outputGain = outputGainSmoothed.getNextValue();
        const float mixAmt = mixSmoothed.getNextValue();
        const float driveKnob = driveSmoothed.getNextValue();
        const float biasKnob = biasSmoothed.getNextValue();
        const float wowDepth = wowDepthSmoothed.getNextValue();
        const float noiseLevel = noiseLevelSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = *channels[(size_t) ch];

            float x = buffer.getSample(ch, n) * inputGain;
            x = state.hpf.processSample(x);
            const float dry = x;

            float wet = readModulatedDelay(state, x, wowDepth);

            if (params.satEnabled)
                wet = shapeSaturation(wet, driveKnob, biasKnob, typeProfile);

            wet = state.headBump.processSample(wet);
            wet = state.hfLoss.processSample(wet);
            wet = state.reproShelf.processSample(wet);
            wet = state.eqLowShelf.processSample(wet);
            wet = state.eqHighShelf.processSample(wet);

            if (noiseLevel > 0.0001f)
            {
                float noise = state.noiseRandom.nextFloat() * 2.0f - 1.0f;
                noise = state.hissHpf.processSample(noise);
                noise = state.hissLpf.processSample(noise);

                const float amp = (noiseLevel / 10.0f) * 0.02f *
                                   typeProfile.noiseTint * speedProfile.noiseFloorScale * noiseAgeBoost;
                wet += noise * amp;
            }

            if (params.dropoutsEnabled)
            {
                if (state.dropoutHoldSamples > 0)
                {
                    --state.dropoutHoldSamples;
                }
                else if (state.dropoutRandom.nextFloat() < dropoutTriggerProb)
                {
                    state.dropoutTargetEnvelope = juce::jmap(state.dropoutRandom.nextFloat(), 0.05f, 0.5f);
                    state.dropoutHoldSamples =
                        (int) (sampleRate * juce::jmap(state.dropoutRandom.nextFloat(), 0.005f, 0.05f));
                }
                else
                {
                    state.dropoutTargetEnvelope = 1.0f;
                }

                state.dropoutEnvelope += (state.dropoutTargetEnvelope - state.dropoutEnvelope) * 0.05f;
                wet *= state.dropoutEnvelope;
            }
            else
            {
                state.dropoutEnvelope = 1.0f;
            }

            float mixed = dry * (1.0f - mixAmt) + wet * mixAmt;
            mixed = state.outputLpf.processSample(mixed);
            mixed = state.dcBlocker.processSample(mixed);
            mixed *= outputGain;

            buffer.setSample(ch, n, mixed);

            const float targetDb = juce::Decibels::gainToDecibels(std::abs(mixed), -100.0f);
            const float coeff = targetDb > state.envelopeFollower ? 0.35f : 0.02f;
            state.envelopeFollower += (targetDb - state.envelopeFollower) * coeff;
        }
    }

    if (channels.size() > 0)
        outputLevelDbL.store(channels[0]->envelopeFollower);

    if (channels.size() > 1)
        outputLevelDbR.store(channels[1]->envelopeFollower);
    else if (channels.size() > 0)
        outputLevelDbR.store(channels[0]->envelopeFollower);
}

void TapeEngine::processBypassed(juce::AudioBuffer<float>& buffer)
{
    juce::ignoreUnused(buffer);
}

} // namespace NF
