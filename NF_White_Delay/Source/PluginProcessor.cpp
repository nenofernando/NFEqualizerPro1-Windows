#include "PluginProcessor.h"
#include "PluginEditor.h"

NFWhiteDelayAudioProcessor::NFWhiteDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NF_WHITE_DELAY_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
NFWhiteDelayAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam = juce::AudioParameterBool;
    using ChoiceParam = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Faixa log-ish pra Hz -- move devagar nos graves, rápido nos agudos,
    // como qualquer knob de frequência decente.
    auto hzRange = [](float minHz, float maxHz)
    {
        return juce::NormalisableRange<float>(minHz, maxHz, 0.01f, 0.3f);
    };

    // --------------------------------------------------------
    // TIME / SYNC / FEEDBACK / PING-PONG
    // --------------------------------------------------------

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::delayTimeMs, "Delay Time",
        juce::NormalisableRange<float>(1.0f, 4000.0f, 0.01f, 0.3f), 300.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<BoolParam>(
        ParamIDs::syncEnabled, "Sync", true));

    params.push_back(std::make_unique<ChoiceParam>(
        ParamIDs::syncDivision, "Sync Division",
        juce::StringArray { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" },
        3));

    params.push_back(std::make_unique<ChoiceParam>(
        ParamIDs::syncModifier, "Sync Modifier",
        juce::StringArray { "Straight", "Dotted", "Triplet" },
        0));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::feedback, "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<BoolParam>(
        ParamIDs::pingPong, "Ping Pong", false));

    // --------------------------------------------------------
    // MODULATION
    // --------------------------------------------------------

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::modRate, "Mod Rate",
        juce::NormalisableRange<float>(0.05f, 10.0f, 0.001f, 0.35f), 0.6f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::modDepth, "Mod Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f), 3.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<ChoiceParam>(
        ParamIDs::modShape, "Mod Shape",
        juce::StringArray { "Sine", "Triangle", "Soft Random" },
        0));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::modSpread, "Mod Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // --------------------------------------------------------
    // FILTERS
    // --------------------------------------------------------

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::highPass, "High Pass", hzRange(20.0f, 20000.0f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::lowPass, "Low Pass", hzRange(20.0f, 20000.0f), 16000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::resonance, "Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // --------------------------------------------------------
    // DUCKING / LO-FI / CHARACTER
    // --------------------------------------------------------

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::duckingAmount, "Ducking",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<BoolParam>(
        ParamIDs::loFiEnabled, "Lo-Fi", false));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::characterAmount, "Character",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<ChoiceParam>(
        ParamIDs::delayMode, "Delay Mode",
        juce::StringArray { "Digital", "Analog", "Tape" },
        0));

    // --------------------------------------------------------
    // DRY/WET / OUTPUT / BYPASS
    // --------------------------------------------------------

    // FASE 8 -- default de fábrica revisado pra uso como insert
    // (pedido explícito do usuário, 2026-08-26): 25%, não mais 100%.
    // Todos os outros defaults desta lista permanecem intocados
    // (Feedback=35%, Sync=ON, Division=1/8, Modifier=Straight,
    // Ducking=0%, Lo-Fi=OFF, Mode=Digital, Character=0%, Output=0dB).
    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::dryWet, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f), 25.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<FloatParam>(
        ParamIDs::outputGain, "Output",
        juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<BoolParam>(
        ParamIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

void NFWhiteDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;
    preparedBlockSize = juce::jmax(1, samplesPerBlock);

    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax(1, getTotalNumOutputChannels())
    };

    delayEngine.prepare(spec);

    // Scratch buffers da FASE 6 -- dimensionados UMA VEZ aqui, nunca
    // redimensionados dentro de processBlock()/processChunk() (Seção 2
    // do briefing). Se o host mandar um bloco maior que
    // preparedBlockSize, processBlock() processa em chunks dessa
    // capacidade em vez de realocar -- ver processBlock().
    const int numChannels = juce::jmax(1, getTotalNumOutputChannels());
    dryBuffer.setSize(numChannels, preparedBlockSize, false, false, true);
    delayInputBuffer.setSize(numChannels, preparedBlockSize, false, false, true);

    outputStage.prepare(sampleRate, preparedBlockSize);
}

void NFWhiteDelayAudioProcessor::releaseResources()
{
    delayEngine.reset();
    outputStage.reset();
}

bool NFWhiteDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == output;
}

void NFWhiteDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalInput = getTotalNumInputChannels();
    const int totalOutput = getTotalNumOutputChannels();

    // Zera qualquer canal de saída sem entrada correspondente -- host
    // pode pedir mais canais de saída do que de entrada.
    for (int channel = totalInput; channel < totalOutput; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    // FASE 6, Seção 2: se o host mandar um bloco maior que
    // preparedBlockSize (dryBuffer/delayInputBuffer só têm essa
    // capacidade), processa em CHUNKS em vez de redimensionar --
    // AudioBuffer(float* const*, numChannels, startSample, numSamples)
    // é só uma VIEW sobre os dados já existentes (não copia, não
    // aloca), então isso não custa nada extra no caso comum onde o
    // bloco já cabe inteiro num chunk só.
    const int totalSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    int samplesDone = 0;

    while (samplesDone < totalSamples)
    {
        const int chunkSamples = juce::jmin(preparedBlockSize, totalSamples - samplesDone);

        juce::AudioBuffer<float> chunk(buffer.getArrayOfWritePointers(), numChannels, samplesDone, chunkSamples);
        processChunk(chunk);

        samplesDone += chunkSamples;
    }
}

void NFWhiteDelayAudioProcessor::processChunk(juce::AudioBuffer<float>& chunk)
{
    const int numSamples = chunk.getNumSamples();
    const int numChannels = chunk.getNumChannels();

    // ------------------------------------------------------------
    // 1) Captura o dry ABSOLUTAMENTE limpo -- antes de qualquer outra
    //    coisa tocar em "chunk". Nunca passa por Delay/Filters/Lo-Fi/
    //    Character/Ducking (Seção 1 do briefing da FASE 6).
    // ------------------------------------------------------------
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, chunk, ch, 0, numSamples);

    // ------------------------------------------------------------
    // Parâmetros de mix/output/bypass -- lidos uma vez por chunk (ver
    // OutputStage.h pra por que os quatro SmoothedValue precisam ser
    // atualizados juntos, não em setters separados).
    // ------------------------------------------------------------
    const bool bypassRequested = apvts.getRawParameterValue(ParamIDs::bypass)->load() > 0.5f;
    const float mixTarget = apvts.getRawParameterValue(ParamIDs::dryWet)->load() * 0.01f;
    const float outputGainDb = apvts.getRawParameterValue(ParamIDs::outputGain)->load();

    outputStage.updateParameters(mixTarget, outputGainDb, bypassRequested);

    // ------------------------------------------------------------
    // 2) Prepara a entrada do DelayEngine -- uma cópia do dry, escalada
    //    pelo ganho de bypass. O motor NUNCA para de rodar (Seção 7:
    //    LFO/filtros/character/delay memory/duck detector continuam
    //    coerentes) -- só para de RECEBER sinal novo quando bypassado,
    //    deixando o tail/feedback existente decair sozinho (Seção 6).
    // ------------------------------------------------------------
    for (int ch = 0; ch < numChannels; ++ch)
        delayInputBuffer.copyFrom(ch, 0, chunk, ch, 0, numSamples);

    // View de delayInputBuffer limitada a numSamples -- delayInputBuffer
    // em si tem capacidade preparedBlockSize (pode ser maior que
    // numSamples se este chunk for o último, menor, de um bloco do
    // host que não dividiu igual); sem isso, DelayEngine::process()
    // processaria além do que é real, avançando o estado interno com
    // amostras indevidas. Construtor de view -- não copia, não aloca.
    juce::AudioBuffer<float> delayInputView(delayInputBuffer.getArrayOfWritePointers(), numChannels, 0, numSamples);

    outputStage.applyBypassInputGain(delayInputView, numSamples);

    // FASE 3 + FASE 4 + FASE 5: host sync + feedback + ping-pong +
    // filtros + modulação + ducking + lo-fi + character.
    const bool syncEnabled = apvts.getRawParameterValue(ParamIDs::syncEnabled)->load() > 0.5f;

    float targetDelayMs = apvts.getRawParameterValue(ParamIDs::delayTimeMs)->load();

    // Nunca assume que o host informa BPM/transporte/assinatura de
    // tempo -- tudo aqui é opcional e cai num fallback seguro
    // (NF::sanitiseBpm, 4/4) sem nunca escrever nada de volta no host.
    // Cálculo é só aritmética simples -- sem alocação, conversão de
    // string, mutex ou log.
    //
    // Lido SEMPRE (não só quando syncEnabled) e publicado em
    // lastKnownHostBpm -- é a ÚNICA leitura seguro de getPlayHead()
    // neste plugin (dentro do audio thread). O editor NUNCA chama
    // getPlayHead() por conta própria (ver nota em PluginProcessor.h
    // sobre o crash real capturado no LUNA/AU).
    double hostBpm = -1.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;

            if (auto timeSig = position->getTimeSignature())
            {
                timeSigNumerator = timeSig->numerator;
                timeSigDenominator = timeSig->denominator;
            }
        }
    }

    lastKnownHostBpm.store(hostBpm, std::memory_order_relaxed);

    if (syncEnabled)
    {
        const auto division = (NF::SyncDivision) (int) apvts.getRawParameterValue(ParamIDs::syncDivision)->load();
        const auto modifier = (NF::SyncModifier) (int) apvts.getRawParameterValue(ParamIDs::syncModifier)->load();

        targetDelayMs = (float) NF::syncDivisionMs(hostBpm, division, modifier,
                                                   timeSigNumerator, timeSigDenominator);
    }

    const float feedback = apvts.getRawParameterValue(ParamIDs::feedback)->load() * 0.01f;
    const bool pingPong = apvts.getRawParameterValue(ParamIDs::pingPong)->load() > 0.5f;

    delayEngine.setDelayTimeMs(targetDelayMs);
    delayEngine.setFeedback(feedback);
    delayEngine.setPingPong(pingPong);

    delayEngine.setHighPassHz(apvts.getRawParameterValue(ParamIDs::highPass)->load());
    delayEngine.setLowPassHz(apvts.getRawParameterValue(ParamIDs::lowPass)->load());
    delayEngine.setResonance(apvts.getRawParameterValue(ParamIDs::resonance)->load() * 0.01f);

    delayEngine.setModRate(apvts.getRawParameterValue(ParamIDs::modRate)->load());
    delayEngine.setModDepth(apvts.getRawParameterValue(ParamIDs::modDepth)->load() * 0.01f);
    delayEngine.setModShape((NF::ModShape) (int) apvts.getRawParameterValue(ParamIDs::modShape)->load());
    delayEngine.setModSpread(apvts.getRawParameterValue(ParamIDs::modSpread)->load() * 0.01f);

    delayEngine.setDuckingAmount(apvts.getRawParameterValue(ParamIDs::duckingAmount)->load() * 0.01f);
    delayEngine.setLoFiEnabled(apvts.getRawParameterValue(ParamIDs::loFiEnabled)->load() > 0.5f);
    delayEngine.setCharacterMode((NF::CharacterMode) (int) apvts.getRawParameterValue(ParamIDs::delayMode)->load());
    delayEngine.setCharacterAmount(apvts.getRawParameterValue(ParamIDs::characterAmount)->load() * 0.01f);

    delayEngine.process(delayInputView);   // delayInputBuffer (via a view) agora contém o WET

    // ------------------------------------------------------------
    // FASE 7.4 -- envelope simples do WET pra alimentar a animação do
    // display (ver PluginProcessor.h, wetActivity). Só MEDE o que já
    // está em delayInputBuffer (não escreve nada nele, não afeta o
    // sinal) -- pico absoluto do bloco, com smoothing attack rápido
    // (~8ms, reage rápido quando o delay entra) e release lento
    // (~450ms, "quando o tail morrer: animação desacelera" com um
    // decaimento musical, não um corte seco).
    // ------------------------------------------------------------
    {
        float blockPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto range = delayInputView.findMinMax(ch, 0, numSamples);
            blockPeak = juce::jmax(blockPeak, std::abs(range.getStart()), std::abs(range.getEnd()));
        }

        const float attackCoeff = 1.0f - std::exp(-1.0f / (0.008f * (float) currentSampleRate));
        const float releaseCoeff = 1.0f - std::exp(-1.0f / (0.45f * (float) currentSampleRate));
        const float coeff = (blockPeak > wetEnvelopeState) ? attackCoeff : releaseCoeff;

        // Aproximação por bloco (um passo de smoothing por chunk, não
        // por amostra) -- suficiente pra uma animação de UI a 30fps,
        // custo desprezível no audio thread.
        wetEnvelopeState += (blockPeak - wetEnvelopeState) * juce::jmin(1.0f, coeff * (float) numSamples);
        wetActivity.store(wetEnvelopeState, std::memory_order_relaxed);
    }

    // ------------------------------------------------------------
    // 3) Dry/Wet (linear, Seção 3) + Output (Seção 4) + Bypass
    //    (Seção 5) -- ver OutputStage.h. Bypass completo: saída == dry
    //    exato, independente de Dry/Wet/Output/Ducking/Character/etc
    //    (Seção 5/10 -- Output nunca continua amplificando o sinal
    //    bypassado).
    // ------------------------------------------------------------
    outputStage.mixAndOutput(dryBuffer, delayInputBuffer, chunk, numSamples);
}

void NFWhiteDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute("stateVersion", kCurrentStateVersion);
        copyXmlToBinary(*xml, destData);
    }
}

void NFWhiteDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            // Ausência do atributo (state salvo antes do versionamento
            // existir) é tratada como versão 0 -- migrateState() decide
            // o que fazer, em vez de simplesmente assumir compatibilidade.
            const int loadedVersion = xml->getIntAttribute("stateVersion", 0);

            migrateState(*xml, loadedVersion);

            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }
    }
}

void NFWhiteDelayAudioProcessor::migrateState(juce::XmlElement& stateXml, int loadedVersion)
{
    juce::ignoreUnused(stateXml);

    // Só existe a v1 até agora -- nada pra migrar. Quando a v2 chegar
    // (ex.: um ID de parâmetro mudou de significado, ou um novo
    // parâmetro precisa de um default diferente do "não existia"),
    // a conversão entra aqui, condicionada em loadedVersion.
    juce::ignoreUnused(loadedVersion);
}

juce::AudioProcessorEditor* NFWhiteDelayAudioProcessor::createEditor()
{
    return new NFWhiteDelayAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NFWhiteDelayAudioProcessor();
}
