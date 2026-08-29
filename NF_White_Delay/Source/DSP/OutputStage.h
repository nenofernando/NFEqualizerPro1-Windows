#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace NF
{
// ============================================================
// FASE 6 -- Dry/Wet + Output + Bypass. Extraído do PluginProcessor
// pra ficar testável isolado (mesmo princípio já seguido em todo o
// resto do motor -- ver DelayEngine.h/SVFFilter.h/etc).
//
// MIX (Seção 3, revisado na FASE 6.6 após teste real em host):
// EQUAL-POWER, não mais linear -- decisão revertida deliberadamente.
// A escolha original (linear, wetGain=mix/dryGain=1-mix) partia do
// raciocínio teórico de que delays curtos correlacionados com o dry
// poderiam somar amplitude com equal-power e soar mais alto no meio
// da faixa -- mas o teste real na voz mostrou o problema oposto e bem
// mais perceptível: a lei linear faz o dry cair rápido demais só de
// abrir uma fração de wet (mix=0.35, por exemplo, já reduz o dry pra
// 65% da amplitude ORIGINAL, e como dry+wet raramente somam em fase
// perfeita, a energia percebida cai visivelmente, dando a sensação de
// "a voz perdeu volume"). Equal-power garante que a POTÊNCIA
// combinada (não a amplitude) fique constante ao longo do fader --
// dryGain=cos(mix*π/2), wetGain=sin(mix*π/2) -- então mix=0% continua
// dry puro, mix=100% continua wet puro, mas o meio da faixa não tem
// mais aquele mergulho de nível perceptível. Ver Tests/
// MixLawAndResponseTests.cpp pra a comparação de RMS entre bypass/
// mix 0%/20%/35% que motivou essa escolha.
//
// BYPASS (Seções 5-7): dois ramps independentes de mesma duração
// (~15ms, dentro dos 5-20ms pedidos):
//   - bypassInputGain: escala o que entra no DelayEngine, indo a 0
//     suavemente. O motor em si continua rodando (chamado todo bloco,
//     nunca pulado) -- só para de RECEBER sinal novo, deixando
//     qualquer tail/feedback já existente decair sozinho, sem
//     resetar nenhum estado (LFO, filtros, character, delay memory,
//     duck detector).
//   - bypassOutputMix: crossfada a SAÍDA final entre o sinal
//     processado (dry/wet + output já aplicados) e o dry intocado.
//     Em bypass total (bypassOutputMix==0), a saída é dry EXATO --
//     Output/Dry-Wet/Character/etc não têm efeito nenhum nesse
//     estado, por construção (não é um "if bypass return" abrupto,
//     é o próprio resultado da mistura quando o peso do processado é
//     zero).
// ============================================================

class OutputStage
{
public:
    void prepare(double sampleRateIn, int /*maxBlockSize*/) noexcept
    {
        sampleRate = sampleRateIn;

        constexpr double mixOutputSmoothingSeconds = 0.02;   // 20ms -- mesmo padrão do resto do motor
        constexpr double bypassCrossfadeSeconds = 0.015;     // 15ms -- dentro de 5-20ms (Seção 5)

        mixSmooth.reset(sampleRate, mixOutputSmoothingSeconds);
        outputGainSmooth.reset(sampleRate, mixOutputSmoothingSeconds);
        bypassInputGainSmooth.reset(sampleRate, bypassCrossfadeSeconds);
        bypassOutputMixSmooth.reset(sampleRate, bypassCrossfadeSeconds);

        initialised = false;
    }

    void reset() noexcept
    {
        // De propósito, NÃO reseta os SmoothedValue -- só limpa o
        // estado que não faz sentido persistir entre prepare()s. Os
        // ganhos em si (mix/output/bypass) devem continuar de onde
        // estavam; quem os re-alveja é setMix()/setOutputGainDb()/
        // setBypassed(), chamados de novo no próximo bloco.
    }

    // Um método só, não três setters separados -- os quatro
    // SmoothedValue precisam decidir "é a primeira chamada, faço snap"
    // ou "não é, re-alveja" JUNTOS e atomicamente. Setters separados
    // com uma flag "initialised" compartilhada teriam um risco real de
    // ordem de chamada: se um setter viesse depois de outro que já
    // tivesse virado a flag pra true, esse primeiro faria um snap
    // (correto) mas o SEGUNDO já veria a flag true e re-alvejaria
    // (rampa espúria a partir do valor padrão do SmoothedValue, não do
    // valor real). Chamar todos os quatro alvos de uma vez elimina
    // esse risco por construção -- ver o mesmo padrão já usado em
    // DelayEngine::setDelayTimeMs().
    void updateParameters(float mix0to1, float outputGainDb, bool bypassed) noexcept
    {
        const float mixClamped = juce::jlimit(0.0f, 1.0f, mix0to1);
        const float outGain = juce::Decibels::decibelsToGain(outputGainDb);
        const float bypassTarget = bypassed ? 0.0f : 1.0f;

        if (! initialised)
        {
            mixSmooth.setCurrentAndTargetValue(mixClamped);
            outputGainSmooth.setCurrentAndTargetValue(outGain);
            bypassInputGainSmooth.setCurrentAndTargetValue(bypassTarget);
            bypassOutputMixSmooth.setCurrentAndTargetValue(bypassTarget);
            initialised = true;
        }
        else
        {
            mixSmooth.setTargetValue(mixClamped);
            outputGainSmooth.setTargetValue(outGain);
            bypassInputGainSmooth.setTargetValue(bypassTarget);
            bypassOutputMixSmooth.setTargetValue(bypassTarget);
        }
    }

    // Escala IN-PLACE o que vai entrar no DelayEngine -- chamar ANTES
    // de delayEngine.process(). Some suavemente pra 0 em bypass.
    void applyBypassInputGain(juce::AudioBuffer<float>& delayInput, int numSamples) noexcept
    {
        bypassInputGainSmooth.applyGain(delayInput, numSamples);
    }

    // dry: capturado ANTES de qualquer processamento (limpo). wet: já
    // processado pelo DelayEngine (que já recebeu o dry escalado por
    // applyBypassInputGain). Escreve o resultado final em output --
    // pode ser o mesmo buffer que dry/wet (processa amostra a amostra,
    // lendo antes de escrever cada uma).
    void mixAndOutput(const juce::AudioBuffer<float>& dry, const juce::AudioBuffer<float>& wet,
                      juce::AudioBuffer<float>& output, int numSamples) noexcept
    {
        const int numChannels = output.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float mixValue = mixSmooth.getNextValue();
            const float angle = mixValue * juce::MathConstants<float>::halfPi;
            const float dryGain = std::cos(angle);
            const float wetGain = std::sin(angle);
            const float outGain = outputGainSmooth.getNextValue();
            const float bypassMix = bypassOutputMixSmooth.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float drySample = dry.getSample(ch, i);
                const float wetSample = wet.getSample(ch, i);
                const float processed = (drySample * dryGain + wetSample * wetGain) * outGain;

                output.setSample(ch, i, drySample * (1.0f - bypassMix) + processed * bypassMix);
            }
        }
    }

private:
    double sampleRate = 44100.0;
    bool initialised = false;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassInputGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassOutputMixSmooth;
};
}
