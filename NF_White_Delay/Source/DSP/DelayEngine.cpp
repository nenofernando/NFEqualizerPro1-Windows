#include "DelayEngine.h"

namespace NF
{
void DelayEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // +8 amostras de folga -- Lagrange3rd olha pra amostras vizinhas
    // do ponto de leitura fracionário, então o delay máximo utilizável
    // precisa de uma margem pequena além do exato. Dimensiona pra
    // maxInternalDelayMs (16s), não maxFreeDelayMs (4s) -- ver nota no
    // topo do .h sobre por que o SYNC precisa de mais capacidade que
    // o knob manual.
    const int maximumDelaySamples =
        (int) std::ceil((double) maxInternalDelayMs * 0.001 * sampleRate) + 8;

    for (auto* line : { &delayLineA, &delayLineB })
    {
        line->setMaximumDelayInSamples(maximumDelaySamples);
        line->prepare(spec);
        line->reset();
    }

    // FASE 6.6 -- tempo de resposta do TIME (mudança pequena) separado
    // do resto (feedback/filtros/character continuam em 20ms, sem
    // mudança -- só o controle de tempo tinha a queixa real de "parece
    // lento"). 5ms escolhido via comparação empírica entre 3/5/7ms
    // (Tests/MixLawAndResponseTests.cpp): suave o bastante pra não
    // estalar (é ainda uma rampa contínua, não um degrau), rápido o
    // bastante pra o knob parecer imediato.
    constexpr double delayTimeSmoothingSeconds = 0.005;
    delayMsSmoothA.reset(sampleRate, delayTimeSmoothingSeconds);
    delayMsSmoothB.reset(sampleRate, delayTimeSmoothingSeconds);

    // 20ms -- suficiente pra ajuste manual/automação contínua (mudança
    // PEQUENA) não estalar. Inalterado nesta fase (só o TIME tinha a
    // queixa de resposta lenta).
    constexpr double smoothingTimeSeconds = 0.02;
    feedbackSmooth.reset(sampleRate, smoothingTimeSeconds);

    // FASE 6.6 -- crossfade de mudança GRANDE de tempo (ver nota no
    // .h). Comparado empiricamente em Tests/MixLawAndResponseTests.cpp
    // entre 5/8/10/15/25ms, medindo o PIOR CASO de delta amostra-a-
    // amostra sobre 8 instantes diferentes de salto (fases distintas
    // do tom de teste) -- não um único ponto de salto fixo, que
    // esconderia o pior caso real. Resultado medido: os 5 candidatos
    // deram exatamente o mesmo delta máximo (0.0197, quase 8x abaixo
    // do limiar de clique de 0.157) -- ou seja, NESSA faixa (5-25ms) a
    // duração do crossfade em si não é o fator limitante do teste
    // sintético (o delta vem de outro lugar, provavelmente a
    // interpolação Lagrange3rd na troca de cabeça, que não escala com
    // a duração do blend). Escolhido 10ms -- dentro da faixa sugerida
    // (8-15ms), reduzindo de 25ms sem ir ao mínimo absoluto testado
    // (5ms, reservado só pro smoothing de mudança pequena), mantendo
    // margem extra pra conteúdo mais complexo que o tom sintético do
    // teste não cobre.
    constexpr double crossfadeTimeSeconds = 0.010;
    crossfadeToB.reset(sampleRate, crossfadeTimeSeconds);
    crossfadeToB.setCurrentAndTargetValue(0.0f);

    restingHeadIsB = false;
    crossfading = false;
    parametersInitialised = false;
    feedbackInitialised = false;

    hpfL.prepare(sampleRate);
    hpfR.prepare(sampleRate);
    lpfL.prepare(sampleRate);
    lpfR.prepare(sampleRate);
    hpfL.setMode(SVFFilter::Mode::Highpass);
    hpfR.setMode(SVFFilter::Mode::Highpass);
    lpfL.setMode(SVFFilter::Mode::Lowpass);
    lpfR.setMode(SVFFilter::Mode::Lowpass);

    hpCutoffSmooth.reset(sampleRate, smoothingTimeSeconds);
    lpCutoffSmooth.reset(sampleRate, smoothingTimeSeconds);
    resonanceSmooth.reset(sampleRate, smoothingTimeSeconds);
    filterParamsInitialised = false;
    filtersActive = false;

    modulation.prepare(sampleRate);
    modulationActive = false;

    ducking.prepare(sampleRate);
    duckingActive = false;

    loFi.prepare(sampleRate);
    loFiEnabled = false;

    character.prepare(sampleRate);
    characterAmountSmooth.reset(sampleRate, smoothingTimeSeconds);
    characterParamsInitialised = false;
    characterActive = false;
    tapeWowPhase = 0.0f;
}

void DelayEngine::reset()
{
    delayLineA.reset();
    delayLineB.reset();

    hpfL.reset();
    hpfR.reset();
    lpfL.reset();
    lpfR.reset();

    modulation.reset();
    ducking.reset();
    loFi.reset();
    character.reset();
}

void DelayEngine::setDelayTimingForTesting(double smoothingSeconds, double crossfadeSeconds) noexcept
{
    delayMsSmoothA.reset(sampleRate, smoothingSeconds);
    delayMsSmoothB.reset(sampleRate, smoothingSeconds);
    crossfadeToB.reset(sampleRate, crossfadeSeconds);
    crossfadeToB.setCurrentAndTargetValue(0.0f);
    restingHeadIsB = false;
    crossfading = false;
    parametersInitialised = false; // próximo setDelayTimeMs() faz snap, não rampa a partir de 0
}

void DelayEngine::setDelayTimeMs(float targetMs) noexcept
{
    // Clampa contra a capacidade REAL do motor -- não contra o limite
    // do knob manual (maxFreeDelayMs). Um tempo de SYNC válido (ex.:
    // 2 Bars Dotted em BPM baixo) pode passar de 4s e ainda assim
    // caber aqui sem ser cortado.
    const float clamped = juce::jlimit(minDelayMs, maxInternalDelayMs, targetMs);

    if (! parametersInitialised)
    {
        delayMsSmoothA.setCurrentAndTargetValue(clamped);
        delayMsSmoothB.setCurrentAndTargetValue(clamped);
        crossfadeToB.setCurrentAndTargetValue(0.0f);
        restingHeadIsB = false;
        crossfading = false;
        parametersInitialised = true;
        return;
    }

    if (crossfading)
    {
        // Já em transição -- só re-alveja (suavemente) a cabeça que
        // está entrando, sem reiniciar o crossfade.
        auto& incomingSmooth = restingHeadIsB ? delayMsSmoothA : delayMsSmoothB;
        incomingSmooth.setTargetValue(clamped);
        return;
    }

    auto& activeSmooth = restingHeadIsB ? delayMsSmoothB : delayMsSmoothA;
    auto& inactiveSmooth = restingHeadIsB ? delayMsSmoothA : delayMsSmoothB;

    const float distance = std::abs(clamped - activeSmooth.getTargetValue());

    if (distance <= largeJumpThresholdMs)
    {
        // Mudança pequena -- rampa simples na cabeça ativa, igual FASE 2.
        activeSmooth.setTargetValue(clamped);
        return;
    }

    // Mudança grande -- ajusta a cabeça INATIVA (muda, ninguém está
    // ouvindo) direto pro novo tempo, sem estalo, e dispara o
    // crossfade até ela.
    inactiveSmooth.setCurrentAndTargetValue(clamped);
    crossfadeToB.setTargetValue(restingHeadIsB ? 0.0f : 1.0f);
    crossfading = true;
}

void DelayEngine::setFeedback(float feedback0to1) noexcept
{
    const float clamped = juce::jlimit(0.0f, maxFeedback, feedback0to1);

    if (! feedbackInitialised)
    {
        feedbackSmooth.setCurrentAndTargetValue(clamped);
        feedbackInitialised = true;
    }
    else
    {
        feedbackSmooth.setTargetValue(clamped);
    }
}

void DelayEngine::setPingPong(bool enabled) noexcept
{
    pingPongEnabled = enabled;
}

void DelayEngine::setHighPassHz(float hz) noexcept
{
    const float clamped = juce::jlimit(minFilterHz, maxFilterHz, hz);

    if (! filterParamsInitialised)
        hpCutoffSmooth.setCurrentAndTargetValue(clamped);
    else
        hpCutoffSmooth.setTargetValue(clamped);

    filtersActive = true;
}

void DelayEngine::setLowPassHz(float hz) noexcept
{
    const float clamped = juce::jlimit(minFilterHz, maxFilterHz, hz);

    if (! filterParamsInitialised)
        lpCutoffSmooth.setCurrentAndTargetValue(clamped);
    else
        lpCutoffSmooth.setTargetValue(clamped);

    filtersActive = true;
}

void DelayEngine::setResonance(float resonance0to1) noexcept
{
    const float clamped = juce::jlimit(0.0f, 1.0f, resonance0to1);

    if (! filterParamsInitialised)
    {
        resonanceSmooth.setCurrentAndTargetValue(clamped);
        filterParamsInitialised = true;
    }
    else
    {
        resonanceSmooth.setTargetValue(clamped);
    }

    filtersActive = true;
}

void DelayEngine::setModRate(float hz) noexcept
{
    modulation.setRate(hz);
    modulationActive = true;
}

void DelayEngine::setModDepth(float depth0to1) noexcept
{
    modulation.setDepth(depth0to1);
    modulationActive = true;
}

void DelayEngine::setModShape(ModShape shape) noexcept
{
    modulation.setShape(shape);
    modulationActive = true;
}

void DelayEngine::setModSpread(float spread0to1) noexcept
{
    modulation.setSpread(spread0to1);
    modulationActive = true;
}

void DelayEngine::setDuckingAmount(float amount0to1) noexcept
{
    ducking.setAmount(amount0to1);
    duckingActive = true;
}

void DelayEngine::setLoFiEnabled(bool enabled) noexcept
{
    loFiEnabled = enabled;
}

void DelayEngine::setCharacterMode(CharacterMode mode) noexcept
{
    characterMode = mode;
    characterActive = true;
}

void DelayEngine::setCharacterAmount(float amount0to1) noexcept
{
    const float clamped = juce::jlimit(0.0f, 1.0f, amount0to1);

    if (! characterParamsInitialised)
    {
        characterAmountSmooth.setCurrentAndTargetValue(clamped);
        characterParamsInitialised = true;
    }
    else
    {
        characterAmountSmooth.setTargetValue(clamped);
    }

    characterActive = true;
}

void DelayEngine::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int chCount = juce::jmin(numChannels, 2);

    if (chCount <= 0)
        return;

    float* channelData[2] = { nullptr, nullptr };
    for (int ch = 0; ch < chCount; ++ch)
        channelData[ch] = buffer.getWritePointer(ch);

    // Filtros: coeficientes recalculados só UMA VEZ POR BLOCO, a partir
    // dos valores já suavizados (skip() avança a rampa pelo tamanho do
    // bloco de uma vez) -- recomputar TPT coefs (envolve tan()) amostra
    // a amostra seria caro à toa, e o resultado já vem suavizado bloco
    // a bloco de qualquer forma. Se os filtros nunca foram ativados
    // (nenhum setter de filtro chamado ainda), pula isso inteiro --
    // process() se comporta idêntico à FASE 2/3.
    if (filtersActive)
    {
        const float hpHz = hpCutoffSmooth.skip(numSamples);
        const float lpHz = lpCutoffSmooth.skip(numSamples);
        const float resonance = resonanceSmooth.skip(numSamples);
        const float q = minResonanceQ + (maxResonanceQ - minResonanceQ) * resonance;

        hpfL.setCutoffAndQ(hpHz, q);
        hpfR.setCutoffAndQ(hpHz, q);
        lpfL.setCutoffAndQ(lpHz, q);
        lpfR.setCutoffAndQ(lpHz, q);
    }

    // Character: k/hfCutoff recalculados uma vez por bloco, mesmo
    // padrão dos filtros (ver CharacterProcessor.h pra prova de
    // small-signal gain == 1 -- é o que garante que o loop de feedback
    // continua estável mesmo com Character no máximo). O makeup gain
    // (FASE 6) é calculado aqui também, mas só é aplicado lá embaixo
    // na amostra de SAÍDA -- nunca em feedbackToChannel (ver nota
    // "MAKEUP GAIN" mais abaixo).
    float characterWetMakeupGain = 1.0f;

    if (characterActive)
    {
        const float amount = characterAmountSmooth.skip(numSamples);
        const float k = CharacterProcessor::kForMode(characterMode, amount);
        const float hfCutoffHz = CharacterProcessor::hfCutoffHzForMode(characterMode, amount);
        character.setCoefficients(k, hfCutoffHz);
        characterWetMakeupGain = CharacterProcessor::wetMakeupGainForMode(characterMode, amount);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // Pico DRY -- capturado ANTES de qualquer processamento, é a
        // única fonte que o detector de Ducking pode usar (Seção 2 do
        // briefing: nunca delay output/feedback/repeats).
        const float dryL = channelData[0][i];
        const float dryR = (chCount > 1) ? channelData[1][i] : dryL;
        const float dryPeak = juce::jmax(std::abs(dryL), std::abs(dryR));

        const float delayMsA = delayMsSmoothA.getNextValue();
        const float delayMsB = delayMsSmoothB.getNextValue();
        const float feedbackGain = feedbackSmooth.getNextValue();

        float crossfadeB = restingHeadIsB ? 1.0f : 0.0f;

        if (crossfading)
        {
            crossfadeB = crossfadeToB.getNextValue();

            if (! crossfadeToB.isSmoothing())
            {
                // Chegou no alvo -- os papéis trocam e o crossfade
                // termina (a cabeça que acabou de entrar passa a ser
                // "a ativa em repouso" pra próxima mudança pequena).
                restingHeadIsB = ! restingHeadIsB;
                crossfading = false;
                crossfadeB = restingHeadIsB ? 1.0f : 0.0f;
                crossfadeToB.setCurrentAndTargetValue(crossfadeB);
            }
        }

        const float gainA = 1.0f - crossfadeB;
        const float gainB = crossfadeB;

        // Modulação: offset PEQUENO e contínuo, somado ao delay BASE já
        // suavizado -- nunca passa por setDelayTimeMs()/pela lógica de
        // crossfade acima, então o LFO jamais dispara uma troca de
        // cabeça A/B (ver nota "MODULAÇÃO x CROSSFADE" no .h). Se a
        // modulação nunca foi ativada, offset fica em 0 -- idêntico à
        // FASE 3.
        float modOffsetMsL = 0.0f, modOffsetMsR = 0.0f;
        if (modulationActive)
            modulation.getNextOffsets(modOffsetMsL, modOffsetMsR);

        // Micro-wow do Tape -- MUITO menor e mais lento que a
        // modulação principal, fase própria e independente (Seção 11:
        // "decorrelated e somado musicalmente à modulação principal").
        // Só contribui em Character==Tape com o motor ativo; escalado
        // pelo characterAmount suavizado (mesmo valor já lido acima
        // pro bloco inteiro).
        if (characterActive && characterMode == CharacterMode::Tape)
        {
            const float amountNow = characterAmountSmooth.getCurrentValue();
            const float wowMs = tapeWowMaxMs * amountNow * std::sin(juce::MathConstants<float>::twoPi * tapeWowPhase);
            modOffsetMsL += wowMs;
            modOffsetMsR += wowMs;

            tapeWowPhase += (float) (tapeWowRateHz / sampleRate);
            if (tapeWowPhase >= 1.0f)
                tapeWowPhase -= 1.0f;
        }

        const float modOffsetMs[2] { modOffsetMsL, modOffsetMsR };

        float wet[2] { 0.0f, 0.0f };

        for (int ch = 0; ch < chCount; ++ch)
        {
            // finalReadDelayMs = baseDelayMs + modulationOffsetMs,
            // sempre protegido dentro de [minDelayMs, maxInternalDelayMs]
            // -- Depth máximo (~8ms) nunca pode levar o tempo de leitura
            // pra baixo do mínimo válido nem estourar a capacidade.
            const float finalMsA = juce::jlimit(minDelayMs, maxInternalDelayMs, delayMsA + modOffsetMs[ch]);
            const float finalMsB = juce::jlimit(minDelayMs, maxInternalDelayMs, delayMsB + modOffsetMs[ch]);

            const float delaySamplesA = (float) (finalMsA * 0.001 * sampleRate);
            const float delaySamplesB = (float) (finalMsB * 0.001 * sampleRate);

            // Passa o delay explícito por chamada (em vez de um
            // setDelay() compartilhado antes do loop) -- é assim que o
            // tempo pode variar por canal (Spread da modulação); ver
            // popSample() em juce_DelayLine.cpp, que já reconfigura o
            // estado interno a cada chamada quando um valor >= 0 é
            // passado.
            const float sampleA = delayLineA.popSample(ch, delaySamplesA, true);
            const float sampleB = delayLineB.popSample(ch, delaySamplesB, true);
            wet[ch] = sampleA * gainA + sampleB * gainB;
        }

        // Filtros: Delay output -> HPF -> LPF -- o sinal filtrado é o
        // que sai E o que recircula (cada repetição fica progressivamente
        // mais filtrada). Pulado inteiro se nunca ativado (idêntico à
        // FASE 2/3 nesse caso).
        float filtered[2] { wet[0], wet[1] };

        if (filtersActive)
        {
            if (chCount > 0) filtered[0] = lpfL.process(hpfL.process(wet[0]));
            if (chCount > 1) filtered[1] = lpfR.process(hpfR.process(wet[1]));
        }

        // Lo-Fi: dentro do loop de feedback, cada repeat degrada
        // progressivamente (ver LoFiProcessor.h -- sem dither/
        // quantização desde a FASE 6.6, só filtros lineares + saturação
        // suave). Pulado inteiro se nunca ativado.
        if (loFiEnabled)
        {
            for (int ch = 0; ch < chCount; ++ch)
                filtered[ch] = loFi.process(ch, filtered[ch]);
        }

        // Character: Digital/Analog/Tape, ainda dentro do loop (ver
        // CharacterProcessor.h -- small-signal gain == 1 garantido,
        // por isso é seguro aqui mesmo em Feedback 95%).
        if (characterActive)
        {
            for (int ch = 0; ch < chCount; ++ch)
                filtered[ch] = character.process(ch, filtered[ch]);
        }

        // Ping-pong: a saída atrasada (já filtrada/lo-fi/character) de
        // UM canal alimenta o feedback do OUTRO -- cross-feedback
        // dentro do loop, não um swap da saída final (Seção 2 do
        // briefing da FASE 3). Este é o sinal que RECIRCULA -- sempre
        // SEM ducking (ver abaixo).
        float feedbackToChannel[2] { 0.0f, 0.0f };

        if (chCount >= 2)
        {
            if (pingPongEnabled)
            {
                feedbackToChannel[0] = filtered[1] * feedbackGain;
                feedbackToChannel[1] = filtered[0] * feedbackGain;
            }
            else
            {
                feedbackToChannel[0] = filtered[0] * feedbackGain;
                feedbackToChannel[1] = filtered[1] * feedbackGain;
            }
        }
        else
        {
            feedbackToChannel[0] = filtered[0] * feedbackGain;
        }

        // Ducking: SÓ na apresentação final do wet -- o detector usa o
        // pico DRY capturado no topo desta amostra (nunca o sinal
        // atrasado), e o ganho resultante NUNCA entra no que foi
        // empurrado pra dentro do feedback acima. É por isso que o
        // Ducking não pode, por construção, alterar o decaimento das
        // repetições internas (Seção 1/4 do briefing).
        const float duckGain = duckingActive ? ducking.getNextGain(dryPeak) : 1.0f;

        for (int ch = 0; ch < chCount; ++ch)
        {
            const float inputSample = channelData[ch][i];
            const float toPush = inputSample + feedbackToChannel[ch];

            delayLineA.pushSample(ch, toPush);
            delayLineB.pushSample(ch, toPush);

            // MAKEUP GAIN (FASE 6): só aqui, na amostra que vai pra
            // fora -- feedbackToChannel (empurrado acima, no push) já
            // foi calculado a partir de "filtered", SEM este ganho.
            // Trocar de modo nunca muda o loop de feedback, só o
            // quanto cada modo aparece na apresentação final.
            channelData[ch][i] = filtered[ch] * duckGain * characterWetMakeupGain;
        }
    }
}
}
