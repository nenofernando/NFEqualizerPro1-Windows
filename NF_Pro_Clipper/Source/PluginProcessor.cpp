#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================
// CONSTRUCTOR
// ============================================================

NFProClipperAudioProcessor::
NFProClipperAudioProcessor()

    : AudioProcessor (
        BusesProperties()

        .withInput (
            "Input",
            juce::AudioChannelSet::stereo(),
            true)

        .withOutput (
            "Output",
            juce::AudioChannelSet::stereo(),
            true)),

      apvts (
          *this,
          nullptr,
          "NF_PRO_CLIPPER_STATE",
          createParameterLayout())
{
}

// ============================================================
// PARAMETERS
// ============================================================

juce::AudioProcessorValueTreeState::ParameterLayout
NFProClipperAudioProcessor::createParameterLayout()
{
    std::vector<
        std::unique_ptr<
            juce::RangedAudioParameter>>
        params;

    // INPUT
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "input",

                "Input",

                juce::NormalisableRange<float>(
                    -24.0f,
                    24.0f,
                    0.01f),

                0.0f));

    // DRIVE
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "drive",

                "Drive",

                juce::NormalisableRange<float>(
                    0.0f,
                    24.0f,
                    0.01f),

                6.0f));

    // OUTPUT
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "output",

                "Output",

                juce::NormalisableRange<float>(
                    -24.0f,
                    24.0f,
                    0.01f),

                0.0f));

    // CEILING
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "ceiling",

                "Ceiling",

                juce::NormalisableRange<float>(
                    -12.0f,
                    0.0f,
                    0.01f),

                -0.3f));

    // KNEE
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "knee",

                "Knee",

                juce::NormalisableRange<float>(
                    0.0f,
                    12.0f,
                    0.01f),

                6.0f));

    // MIX
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "mix",

                "Mix",

                juce::NormalisableRange<float>(
                    0.0f,
                    100.0f,
                    0.1f),

                100.0f));

    // TONE
    params.push_back (
        std::make_unique<
            juce::AudioParameterFloat>(

                "tone",

                "Tone",

                juce::NormalisableRange<float>(
                    -100.0f,
                    100.0f,
                    0.1f),

                0.0f));

    // CLIP MODE
    params.push_back (
        std::make_unique<
            juce::AudioParameterChoice>(

                "clipMode",

                "Clip Mode",

                juce::StringArray {
                    "Soft",
                    "Medium",
                    "Hard"
                },

                0));

    // OVERSAMPLING
    params.push_back (
        std::make_unique<
            juce::AudioParameterChoice>(

                "oversampling",

                "Oversampling",

                juce::StringArray {
                    "1x",
                    "2x",
                    "4x",
                    "8x",
                    "16x"
                },

                2));

    // MONITOR
    params.push_back (
        std::make_unique<
            juce::AudioParameterChoice>(

                "monitor",

                "Monitor",

                juce::StringArray {
                    "IN",
                    "OUT",
                    "CLIP"
                },

                1));

    // BYPASS
    params.push_back (
        std::make_unique<
            juce::AudioParameterBool>(

                "bypass",

                "Bypass",

                false));

    return {
        params.begin(),
        params.end()
    };
}

// ============================================================
// PREPARE
// ============================================================

void NFProClipperAudioProcessor::prepareToPlay (
    double sampleRate,
    int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;

    // --------------------------------------------------------
    // BUFFERS PRÉ-ALOCADOS
    //
    // Todos os buffers de escopo usados dentro de processBlock
    // são dimensionados aqui, para que processBlock nunca
    // precise alocar memória.
    // --------------------------------------------------------

    dryBuffer.setSize (
        2,
        samplesPerBlock,
        false,
        false,
        true);

    monitorInputBuffer.setSize (
        2,
        samplesPerBlock,
        false,
        false,
        true);

    preClipBuffer.setSize (
        2,
        samplesPerBlock,
        false,
        false,
        true);

    clipDeltaBuffer.setSize (
        2,
        samplesPerBlock,
        false,
        false,
        true);

    crossfadeBuffer.setSize (
        2,
        samplesPerBlock,
        false,
        false,
        true);

    ceilingPerSample.assign (
        (size_t) samplesPerBlock,
        0.0f);

    kneePerSample.assign (
        (size_t) samplesPerBlock,
        0.0f);

    tonePerSample.assign (
        (size_t) samplesPerBlock,
        0.0f);

    // --------------------------------------------------------
    // OVERSAMPLERS
    //
    // Todos os 5 fatores são construídos e inicializados aqui.
    // processBlock apenas escolhe qual instância usar, nunca
    // reconstrói nenhuma delas.
    //
    // O número de canais usado para construir cada Oversampling
    // precisa bater exatamente com o número de canais que
    // processBlock de fato vai passar (1 para hosts mono, 2
    // para estéreo); caso contrário o filtro interno de
    // compensação de latência fracionária (DelayLine) dispara
    // asserts de mismatch de canais (visto em testes mono do
    // auval).
    // --------------------------------------------------------

    int channelsForOversampling =
        juce::jlimit (
            1,
            2,
            getTotalNumOutputChannels());

    for (int factor = 0;
         factor < 5;
         ++factor)
    {
        oversamplers[(size_t) factor] =
            std::make_unique<
                juce::dsp::Oversampling<float>>(

                    (size_t) channelsForOversampling,

                    factor,

                    juce::dsp::Oversampling<float>::
                        filterHalfBandPolyphaseIIR,

                    true,

                    true);

        oversamplers[
            (size_t) factor]
            ->initProcessing (
                (size_t) samplesPerBlock);

        oversamplers[
            (size_t) factor]
            ->reset();
    }

    // --------------------------------------------------------
    // ESTADO DO OVERSAMPLING / CROSSFADE
    // --------------------------------------------------------

    activeOversamplingIndex =
        juce::jlimit (
            0,
            4,

            (int)
            apvts
                .getRawParameterValue (
                    "oversampling")
                ->load());

    previousOversamplingIndexForCrossfade =
        activeOversamplingIndex;

    crossfadeSamplesTotal =
        juce::jmax (
            1,

            (int) std::round (
                sampleRate * 0.015));

    crossfadeSamplesRemaining = 0;

    setLatencySamples (
        (int) std::round (
            oversamplers[
                (size_t) activeOversamplingIndex]
                ->getLatencyInSamples()));

    // --------------------------------------------------------
    // SMOOTHING
    // --------------------------------------------------------

    constexpr double smoothingTime =
        0.020;

    inputGainSmooth.reset (
        sampleRate,
        smoothingTime);

    driveGainSmooth.reset (
        sampleRate,
        smoothingTime);

    outputGainSmooth.reset (
        sampleRate,
        smoothingTime);

    ceilingSmooth.reset (
        sampleRate,
        smoothingTime);

    mixSmooth.reset (
        sampleRate,
        smoothingTime);

    toneSmooth.reset (
        sampleRate,
        smoothingTime);

    kneeSmooth.reset (
        sampleRate,
        smoothingTime);

    inputGainSmooth.setCurrentAndTargetValue (
        1.0f);

    driveGainSmooth.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            6.0f));

    outputGainSmooth.setCurrentAndTargetValue (
        1.0f);

    ceilingSmooth.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            -0.3f));

    mixSmooth.setCurrentAndTargetValue (
        1.0f);

    toneSmooth.setCurrentAndTargetValue (
        0.0f);

    kneeSmooth.setCurrentAndTargetValue (
        6.0f);
}

// ============================================================

void NFProClipperAudioProcessor::
releaseResources()
{
}

// ============================================================
// BUS
// ============================================================

bool NFProClipperAudioProcessor::
isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
    auto output =
        layouts.getMainOutputChannelSet();

    if (output
        != juce::AudioChannelSet::mono()

        &&

        output
        != juce::AudioChannelSet::stereo())
    {
        return false;
    }

    return
        layouts.getMainInputChannelSet()
        ==
        output;
}

// ============================================================
// UTILITIES
// ============================================================

float NFProClipperAudioProcessor::
linearToDb (
    float linear) noexcept
{
    return
        juce::Decibels::gainToDecibels (
            juce::jmax (
                linear,
                1.0e-9f),

            -100.0f);
}

// ============================================================

float NFProClipperAudioProcessor::
calculatePeak (
    const juce::AudioBuffer<float>& b,
    int channel) noexcept
{
    if (channel >=
        b.getNumChannels())
    {
        return 0.0f;
    }

    float peak = 0.0f;

    auto* data =
        b.getReadPointer (
            channel);

    for (int i = 0;
         i < b.getNumSamples();
         ++i)
    {
        peak =
            juce::jmax (
                peak,
                std::abs (
                    data[i]));
    }

    return peak;
}

// ============================================================
// TONE
//
// V1 simples.
// Pode ser substituído posteriormente por tilt EQ real.
// ============================================================

float NFProClipperAudioProcessor::
processTone (
    float sample,
    float toneValue) const noexcept
{
    float amount =
        juce::jlimit (
            -1.0f,
            1.0f,
            toneValue / 100.0f);

    if (std::abs (amount)
        < 0.0001f)
    {
        return sample;
    }

    float shaped =
        std::tanh (
            sample *
            (1.0f
             +
             std::abs (amount)
             * 0.30f));

    float mix =
        std::abs (amount)
        * 0.18f;

    float result =
        juce::jmap (
            mix,
            sample,
            shaped);

    if (amount < 0.0f)
        result *= 0.96f;

    return result;
}

// ============================================================
// CLIPPER
//
// Soft  -> tanh puro, satura suavemente e nunca alcança o teto,
//          o que preserva harmônicos mais musicais em baixo drive.
// Medium-> mistura hard/soft ponderada por o quão longe a amostra
//          está do teto (smoothstep), em vez de uma média fixa
//          50/50 - dá uma transição mais natural entre os dois
//          comportamentos conforme o nível de overshoot.
// Hard  -> brickwall real (jlimit) - o teto NUNCA é ultrapassado.
// ============================================================

float NFProClipperAudioProcessor::
processClipperSample (
    float x,
    float ceiling,
    float kneeDb,
    int mode) const noexcept
{
    ceiling =
        juce::jmax (
            ceiling,
            0.00001f);

    // --------------------------------------------------------
    // HARD CLIP
    // --------------------------------------------------------

    float hard =
        juce::jlimit (
            -ceiling,
            ceiling,
            x);

    // --------------------------------------------------------
    // SOFT CLIP
    // --------------------------------------------------------

    float soft =
        ceiling
        *
        std::tanh (
            x / ceiling);

    float clipped = hard;

    if (mode == SoftClip)
    {
        clipped = soft;
    }
    else if (mode == MediumClip)
    {
        float overshoot =
            juce::jlimit (
                0.0f,
                1.0f,

                (std::abs (x) - ceiling)
                /
                juce::jmax (
                    ceiling,
                    0.00001f));

        // quanto mais a amostra ultrapassa o teto, mais o
        // caráter tende para hard clip; perto do teto,
        // predomina o soft clip.
        float blend =
            overshoot
            *
            overshoot
            *
            (3.0f - 2.0f * overshoot);

        clipped =
            juce::jmap (
                blend,
                soft,
                hard);
    }

    // --------------------------------------------------------
    // KNEE
    // --------------------------------------------------------

    if (kneeDb <= 0.001f)
        return clipped;

    float kneeStart =
        ceiling
        *
        juce::Decibels::
            decibelsToGain (
                -kneeDb);

    float absX =
        std::abs (x);

    if (absX <= kneeStart)
        return x;

    if (absX >= ceiling)
        return clipped;

    float progress =
        (absX - kneeStart)
        /
        juce::jmax (
            ceiling - kneeStart,
            0.000001f);

    progress =
        juce::jlimit (
            0.0f,
            1.0f,
            progress);

    // smoothstep
    float smooth =
        progress
        *
        progress
        *
        (3.0f
         -
         2.0f
         *
         progress);

    return
        juce::jmap (
            smooth,
            x,
            clipped);
}

// ============================================================
// TRANSFER CURVE (para o gráfico da UI)
//
// Usa exatamente os mesmos parâmetros e a mesma função de
// clipping do processBlock, então o gráfico nunca fica
// dessincronizado do áudio, inclusive o knee.
// ============================================================

float NFProClipperAudioProcessor::
computeTransferCurveSample (
    float x) const noexcept
{
    float ceilingDb =
        apvts
            .getRawParameterValue (
                "ceiling")
            ->load();

    float ceiling =
        juce::Decibels::
            decibelsToGain (
                ceilingDb);

    float kneeDb =
        apvts
            .getRawParameterValue (
                "knee")
            ->load();

    int mode =
        (int)
        apvts
            .getRawParameterValue (
                "clipMode")
            ->load();

    float clipped =
        processClipperSample (
            x,
            ceiling,
            kneeDb,
            mode);

    return
        juce::jlimit (
            -ceiling,
            ceiling,
            clipped);
}

// ============================================================
// RENDER THROUGH ONE OVERSAMPLING CHAIN
//
// Faz upsample -> clip + tone amostra a amostra -> downsample,
// usando a instância de oversampler já construída em
// prepareToPlay (nenhuma reconstrução ou alocação aqui).
// ============================================================

void NFProClipperAudioProcessor::
renderThroughClipper (
    juce::AudioBuffer<float>& ioBuffer,
    int oversamplingIndex,
    const float* ceilingPerBaseSample,
    const float* kneePerBaseSample,
    const float* toneValuePerBaseSample,
    int clipMode,
    int numBaseSamples,
    float& maxBeforeOut,
    float& maxAfterOut,
    float& clippingAmountOut) noexcept
{
    auto& oversampler =
        *oversamplers[
            (size_t) oversamplingIndex];

    juce::dsp::AudioBlock<float>
        baseBlock (
            ioBuffer);

    auto oversampledBlock =
        oversampler
            .processSamplesUp (
                baseBlock);

    size_t factor =
        oversampler
            .getOversamplingFactor();

    float maxBefore = 0.0f;
    float maxAfter = 0.0f;
    float clippingAmount = 0.0f;

    size_t osSamples =
        oversampledBlock
            .getNumSamples();

    size_t osChannels =
        juce::jmin (
            (size_t) ioBuffer.getNumChannels(),

            oversampledBlock
                .getNumChannels());

    for (size_t sample = 0;
         sample < osSamples;
         ++sample)
    {
        size_t baseIndex =
            juce::jmin (
                sample / juce::jmax ((size_t) 1, factor),

                (size_t) juce::jmax (0, numBaseSamples - 1));

        float ceiling =
            ceilingPerBaseSample[baseIndex];

        float kneeDb =
            kneePerBaseSample[baseIndex];

        float toneValue =
            toneValuePerBaseSample[baseIndex];

        for (size_t ch = 0;
             ch < osChannels;
             ++ch)
        {
            float x =
                oversampledBlock
                    .getSample (
                        (int) ch,
                        (int) sample);

            maxBefore =
                juce::jmax (
                    maxBefore,
                    std::abs (x));

            float clipped =
                processClipperSample (
                    x,
                    ceiling,
                    kneeDb,
                    clipMode);

            clipped =
                processTone (
                    clipped,
                    toneValue);

            // segurança de true-peak: independente do modo ou
            // do tone shaping, o sinal nunca deve ultrapassar
            // o teto configurado.
            clipped =
                juce::jlimit (
                    -ceiling,
                    ceiling,
                    clipped);

            maxAfter =
                juce::jmax (
                    maxAfter,
                    std::abs (
                        clipped));

            if (std::abs (x)
                >
                ceiling)
            {
                float difference =
                    std::abs (x)
                    -
                    ceiling;

                float amount =
                    difference
                    /
                    juce::jmax (
                        ceiling,
                        0.00001f);

                clippingAmount =
                    juce::jmax (
                        clippingAmount,

                        juce::jlimit (
                            0.0f,
                            1.0f,
                            amount));
            }

            oversampledBlock
                .setSample (
                    (int) ch,
                    (int) sample,
                    clipped);
        }
    }

    oversampler
        .processSamplesDown (
            baseBlock);

    maxBeforeOut = maxBefore;
    maxAfterOut = maxAfter;
    clippingAmountOut = clippingAmount;
}

// ============================================================
// PROCESS BLOCK
// ============================================================

void NFProClipperAudioProcessor::
processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals
        noDenormals;

    int channels =
        juce::jmin (
            2,
            buffer.getNumChannels());

    int numSamples =
        buffer.getNumSamples();

    if (channels <= 0
        ||
        numSamples <= 0)
    {
        return;
    }

    // --------------------------------------------------------
    // INPUT METERS
    // --------------------------------------------------------

    for (int ch = 0;
         ch < channels;
         ++ch)
    {
        float peak =
            calculatePeak (
                buffer,
                ch);

        inputPeakDb[
            (size_t) ch]
            .store (
                linearToDb (
                    peak));
    }

    // --------------------------------------------------------
    // GUARDA INPUT ORIGINAL
    // --------------------------------------------------------

    monitorInputBuffer
        .makeCopyOf (
            buffer,
            true);

    dryBuffer
        .makeCopyOf (
            buffer,
            true);

    // --------------------------------------------------------
    // BYPASS
    // --------------------------------------------------------

    bool bypassed =
        apvts
            .getRawParameterValue (
                "bypass")
            ->load()
        >
        0.5f;

    if (bypassed)
    {
        for (int ch = 0;
             ch < channels;
             ++ch)
        {
            outputPeakDb[
                (size_t) ch]
                .store (
                    inputPeakDb[
                        (size_t) ch]
                        .load());
        }

        reductionDb.store (0.0f);
        clipAmount.store (0.0f);

        return;
    }

    // --------------------------------------------------------
    // PARAMETERS
    // --------------------------------------------------------

    float inputDb =
        apvts
            .getRawParameterValue (
                "input")
            ->load();

    float driveDb =
        apvts
            .getRawParameterValue (
                "drive")
            ->load();

    float outputDb =
        apvts
            .getRawParameterValue (
                "output")
            ->load();

    float ceilingDb =
        apvts
            .getRawParameterValue (
                "ceiling")
            ->load();

    float kneeDb =
        apvts
            .getRawParameterValue (
                "knee")
            ->load();

    float mixPercent =
        apvts
            .getRawParameterValue (
                "mix")
            ->load();

    float tone =
        apvts
            .getRawParameterValue (
                "tone")
            ->load();

    int clipMode =
        (int)
        apvts
            .getRawParameterValue (
                "clipMode")
            ->load();

    int requestedOversamplingIndex =
        juce::jlimit (
            0,
            4,

            (int)
            apvts
                .getRawParameterValue (
                    "oversampling")
                ->load());

    int monitorMode =
        juce::jlimit (
            0,
            2,

            (int)
            apvts
                .getRawParameterValue (
                    "monitor")
                ->load());

    // --------------------------------------------------------
    // TROCA DE OVERSAMPLING -> DISPARA CROSSFADE
    //
    // Nunca reconstrói nenhum Oversampling aqui: apenas
    // seleciona qual instância pré-construída usar, e cruza
    // suavemente entre a instância antiga e a nova por
    // crossfadeSamplesTotal amostras para eliminar cliques.
    // --------------------------------------------------------

    if (requestedOversamplingIndex
        !=
        activeOversamplingIndex)
    {
        previousOversamplingIndexForCrossfade =
            activeOversamplingIndex;

        activeOversamplingIndex =
            requestedOversamplingIndex;

        crossfadeSamplesRemaining =
            crossfadeSamplesTotal;
    }

    bool crossfading =
        crossfadeSamplesRemaining > 0;

    // --------------------------------------------------------
    // SMOOTH TARGETS
    // --------------------------------------------------------

    inputGainSmooth
        .setTargetValue (
            juce::Decibels::
                decibelsToGain (
                    inputDb));

    driveGainSmooth
        .setTargetValue (
            juce::Decibels::
                decibelsToGain (
                    driveDb));

    outputGainSmooth
        .setTargetValue (
            juce::Decibels::
                decibelsToGain (
                    outputDb));

    ceilingSmooth
        .setTargetValue (
            juce::Decibels::
                decibelsToGain (
                    ceilingDb));

    mixSmooth
        .setTargetValue (
            juce::jlimit (
                0.0f,
                1.0f,
                mixPercent / 100.0f));

    toneSmooth
        .setTargetValue (
            tone);

    kneeSmooth
        .setTargetValue (
            kneeDb);

    // --------------------------------------------------------
    // INPUT + DRIVE + smoothing amostra-a-amostra de
    // ceiling / knee / tone (taxa base, antes do oversampling)
    // --------------------------------------------------------

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        float inputGain =
            inputGainSmooth
                .getNextValue();

        float driveGain =
            driveGainSmooth
                .getNextValue();

        float totalGain =
            inputGain
            *
            driveGain;

        for (int ch = 0;
             ch < channels;
             ++ch)
        {
            buffer
                .getWritePointer (
                    ch)[sample]
                *=
                totalGain;
        }

        ceilingPerSample[
            (size_t) sample] =
            ceilingSmooth
                .getNextValue();

        kneePerSample[
            (size_t) sample] =
            kneeSmooth
                .getNextValue();

        tonePerSample[
            (size_t) sample] =
            toneSmooth
                .getNextValue();
    }

    // --------------------------------------------------------
    // SNAPSHOT PRÉ-CLIP (para o monitor CLIP)
    // --------------------------------------------------------

    preClipBuffer
        .makeCopyOf (
            buffer,
            true);

    // --------------------------------------------------------
    // OVERSAMPLING + CLIPPER
    // --------------------------------------------------------

    float maxBefore = 0.0f;
    float maxAfter = 0.0f;
    float clippingAmount = 0.0f;

    if (crossfading)
    {
        crossfadeBuffer
            .makeCopyOf (
                buffer,
                true);

        float oldMaxBefore = 0.0f;
        float oldMaxAfter = 0.0f;
        float oldClipAmount = 0.0f;

        renderThroughClipper (
            crossfadeBuffer,
            previousOversamplingIndexForCrossfade,
            ceilingPerSample.data(),
            kneePerSample.data(),
            tonePerSample.data(),
            clipMode,
            numSamples,
            oldMaxBefore,
            oldMaxAfter,
            oldClipAmount);
    }

    renderThroughClipper (
        buffer,
        activeOversamplingIndex,
        ceilingPerSample.data(),
        kneePerSample.data(),
        tonePerSample.data(),
        clipMode,
        numSamples,
        maxBefore,
        maxAfter,
        clippingAmount);

    if (crossfading)
    {
        for (int sample = 0;
             sample < numSamples;
             ++sample)
        {
            float newWeight = 1.0f;

            if (crossfadeSamplesRemaining
                > 0)
            {
                newWeight =
                    1.0f
                    -

                    (float)
                    crossfadeSamplesRemaining
                    /
                    (float)
                    crossfadeSamplesTotal;

                --crossfadeSamplesRemaining;
            }

            float oldWeight =
                1.0f - newWeight;

            for (int ch = 0;
                 ch < channels;
                 ++ch)
            {
                auto* out =
                    buffer
                        .getWritePointer (
                            ch);

                auto* old =
                    crossfadeBuffer
                        .getReadPointer (
                            ch);

                out[sample]
                    =
                    out[sample] * newWeight
                    +
                    old[sample] * oldWeight;
            }
        }
    }

    // --------------------------------------------------------
    // DELTA DE CLIPPING (para o monitor CLIP)
    //
    // A diferença é calculada entre o sinal logo antes do
    // clipper (pós input/drive) e logo depois (pós clip+tone),
    // ambos na taxa base - ou seja, exatamente o material
    // removido/alterado pelo clipper, sem interferência do
    // Mix ou do ganho de saída.
    // --------------------------------------------------------

    for (int ch = 0;
         ch < channels;
         ++ch)
    {
        auto* pre =
            preClipBuffer
                .getReadPointer (
                    ch);

        auto* post =
            buffer
                .getReadPointer (
                    ch);

        auto* delta =
            clipDeltaBuffer
                .getWritePointer (
                    ch);

        for (int sample = 0;
             sample < numSamples;
             ++sample)
        {
            delta[sample]
                =
                pre[sample]
                -
                post[sample];
        }
    }

    // --------------------------------------------------------
    // OUTPUT GAIN
    //
    // Aplicado igualmente ao sinal processado e ao delta de
    // clipping, para que o monitor CLIP fique no mesmo
    // patamar de ganho do OUT.
    // --------------------------------------------------------

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        float outputGain =
            outputGainSmooth
                .getNextValue();

        for (int ch = 0;
             ch < channels;
             ++ch)
        {
            buffer
                .getWritePointer (
                    ch)[sample]
                *=
                outputGain;

            clipDeltaBuffer
                .getWritePointer (
                    ch)[sample]
                *=
                outputGain;
        }
    }

    // --------------------------------------------------------
    // MIX
    // --------------------------------------------------------

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        float wet =
            mixSmooth
                .getNextValue();

        float dry =
            1.0f - wet;

        for (int ch = 0;
             ch < channels;
             ++ch)
        {
            float processed =
                buffer
                    .getReadPointer (
                        ch)[sample];

            float original =
                dryBuffer
                    .getReadPointer (
                        ch)[sample];

            buffer
                .getWritePointer (
                    ch)[sample]
                =
                processed * wet
                +
                original * dry;
        }
    }

    // --------------------------------------------------------
    // MONITOR
    //
    // 0 = IN   -> sinal de entrada, intocado
    // 1 = OUT  -> saída normal (processada + mix + ganho)
    // 2 = CLIP -> apenas o material removido pelo clipper
    // --------------------------------------------------------

    if (monitorMode == 0)
    {
        buffer.makeCopyOf (
            monitorInputBuffer,
            true);
    }
    else if (monitorMode == 2)
    {
        buffer.makeCopyOf (
            clipDeltaBuffer,
            true);
    }

    // --------------------------------------------------------
    // METERS OUTPUT
    // --------------------------------------------------------

    for (int ch = 0;
         ch < channels;
         ++ch)
    {
        float peak =
            calculatePeak (
                buffer,
                ch);

        outputPeakDb[
            (size_t) ch]
            .store (
                linearToDb (
                    peak));
    }

    // --------------------------------------------------------
    // REDUCTION
    // --------------------------------------------------------

    float beforeDb =
        linearToDb (
            juce::jmax (
                maxBefore,
                0.0000001f));

    float afterDb =
        linearToDb (
            juce::jmax (
                maxAfter,
                0.0000001f));

    float reduction =
        juce::jmax (
            0.0f,
            beforeDb - afterDb);

    reductionDb.store (
        juce::jlimit (
            0.0f,
            24.0f,
            reduction));

    clipAmount.store (
        clippingAmount);
}

// ============================================================
// METERS ACCESS
// ============================================================

float NFProClipperAudioProcessor::
getInputPeakDb (
    int channel) const noexcept
{
    return
        inputPeakDb[
            (size_t)
            juce::jlimit (
                0,
                1,
                channel)]
            .load();
}

float NFProClipperAudioProcessor::
getOutputPeakDb (
    int channel) const noexcept
{
    return
        outputPeakDb[
            (size_t)
            juce::jlimit (
                0,
                1,
                channel)]
            .load();
}

// ============================================================
// PRESETS
// ============================================================

juce::File
NFProClipperAudioProcessor::
getPresetsDirectory()
{
    auto dir =
        juce::File::
            getSpecialLocation (
                juce::File::
                    userApplicationDataDirectory)

            .getChildFile (
                "Neno Fernando Audio Tools")

            .getChildFile (
                "NF Pro Clipper")

            .getChildFile (
                "Presets");

    if (! dir.isDirectory())
    {
        dir.createDirectory();
    }

    return dir;
}

// ============================================================
// STATE
// ============================================================

void NFProClipperAudioProcessor::
getStateInformation (
    juce::MemoryBlock& destData)
{
    auto state =
        apvts.copyState();

    if (auto xml =
        state.createXml())
    {
        copyXmlToBinary (
            *xml,
            destData);
    }
}

// ============================================================

void NFProClipperAudioProcessor::
setStateInformation (
    const void* data,
    int sizeInBytes)
{
    auto xml =
        getXmlFromBinary (
            data,
            sizeInBytes);

    if (xml != nullptr)
    {
        if (xml->hasTagName (
                apvts.state.getType()))
        {
            apvts.replaceState (
                juce::ValueTree::
                    fromXml (
                        *xml));
        }
    }
}

// ============================================================
// EDITOR
// ============================================================

juce::AudioProcessorEditor*
NFProClipperAudioProcessor::
createEditor()
{
    return
        new NFProClipperAudioProcessorEditor (
            *this);
}

// ============================================================
// FACTORY
// ============================================================

juce::AudioProcessor*
JUCE_CALLTYPE
createPluginFilter()
{
    return
        new NFProClipperAudioProcessor();
}
