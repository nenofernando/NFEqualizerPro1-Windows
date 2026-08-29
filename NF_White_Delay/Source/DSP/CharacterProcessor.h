#pragma once
#include <cmath>
#include <algorithm>

namespace NF
{
enum class CharacterMode { Digital = 0, Analog, Tape };

// ============================================================
// FASE 5 -- saturação + amaciamento de agudos por modo (Digital/
// Analog/Tape). Zero dependência de JUCE (mesmo motivo do
// HostSync/SVFFilter). O micro-wow do Tape NÃO mora aqui -- ele
// precisa somar no offset de modulação do DelayEngine (a mesma
// leitura fracionária), então fica lá (ver DelayEngine.h/.cpp).
//
// PROVA DE SMALL-SIGNAL GAIN <= 1 (Seção 10 do briefing -- crítico
// pra estabilidade do loop de feedback com Character dentro dele):
//
// A forma ingênua y = tanh(drive*x)/tanh(drive) normaliza pelo valor
// da função EM x=1 (pico de referência), não pela sua INCLINAÇÃO em
// x=0. Como tanh(u) < u pra todo u>0 (tanh é côncava, sempre abaixo
// da reta tangente em 0, que tem inclinação 1), o ganho de pequeno
// sinal dessa forma é:
//
//     dy/dx |x=0 = drive / tanh(drive)  >  drive / drive  =  1
//
// SEMPRE maior que 1, pra qualquer drive>0 -- e cresce sem limite
// conforme drive aumenta (tanh(drive)->1, então a razão -> drive).
// Dentro de um loop de feedback de 95%, isso facilmente ultrapassa
// loopGain=1 mesmo com "saída limitada" (bounded output não implica
// decaimento correto -- é exatamente o alerta do briefing).
//
// Forma adotada aqui: y = tanh(k*x) / k, normalizada pela INCLINAÇÃO
// (k), não pelo valor de pico:
//
//     dy/dx |x=0 = k * sech²(0) / k = k * 1 / k = 1        (EXATO,
//                                                            pra
//                                                            qualquer
//                                                            k > 0)
//
// Ganho de pequeno sinal exatamente 1.0, sempre -- loopGain nunca
// passa de feedbackGain*1.0 = feedbackGain <= 0.95 < 1, preservando
// a mesma prova de estabilidade da FASE 4 (extendida agora pro
// estágio de Character). Mais que isso: |tanh(k*x)/k| < |x| pra todo
// x != 0 e k>0 (mesma desigualdade tanh(u)<u aplicada a u=kx), então
// a função é uma CONTRAÇÃO GLOBAL, não só perto de zero -- o módulo
// da saída nunca excede o da entrada, em nenhum nível de sinal. É
// por isso que characterAmount nunca deixa o repeat mais "alto",
// só mais colorido (ver Seção 12 -- RMS só pode diminuir ou ficar
// igual, nunca aumentar).
//
// O parâmetro k varia por modo (ver kForMode) -- k maior = curva
// "dobra" mais cedo = saturação/compressão mais evidente -- mas o
// ganho de pequeno sinal continua exatamente 1 em QUALQUER k, então
// a escolha de k é livre pra desenhar o caráter de cada modo sem
// nunca comprometer a estabilidade.
// ============================================================

class CharacterProcessor
{
public:
    void prepare(double sampleRateIn) noexcept
    {
        sampleRate = sampleRateIn;
        reset();
    }

    void reset() noexcept
    {
        hfState[0] = 0.0f;
        hfState[1] = 0.0f;
    }

    // k e hfCutoffHz já vêm calculados e suavizados pelo chamador (o
    // DelayEngine faz o smoothing por bloco, mesmo padrão dos
    // filtros -- ver kForMode/hfCutoffHzForMode abaixo pra obter os
    // valores-alvo a partir do modo/amount).
    void setCoefficients(float newK, float hfCutoffHz) noexcept
    {
        k = std::max(newK, 0.0001f);

        const float clampedCutoff = std::min(std::max(hfCutoffHz, 200.0f), (float) (sampleRate * 0.49));
        constexpr float twoPi = 6.28318530717958647692f;
        hfCoeff = 1.0f - std::exp(-twoPi * clampedCutoff / (float) sampleRate);
    }

    float process(int channel, float input) noexcept
    {
        const float saturated = std::tanh(k * input) / k;

        float& state = hfState[channel];
        state += (saturated - state) * hfCoeff;
        return state;
    }

    // amount0to1 já clampado -- devolve k pro modo/quantidade dados.
    // k tende a ~0 (quase identidade) em amount=0 em QUALQUER modo --
    // é isso que garante "0% = praticamente neutro" uniformemente
    // (Seção 12), os modos só se diferenciam conforme amount sobe.
    static float kForMode(CharacterMode mode, float amount0to1) noexcept
    {
        const float a = std::min(std::max(amount0to1, 0.0f), 1.0f);

        switch (mode)
        {
            case CharacterMode::Digital: return 0.001f + (0.15f - 0.001f) * a;
            case CharacterMode::Analog:  return 0.05f + (2.2f - 0.05f) * a;
            case CharacterMode::Tape:    return 0.08f + (2.8f - 0.08f) * a;
            default: return 0.001f;
        }
    }

    static float hfCutoffHzForMode(CharacterMode mode, float amount0to1) noexcept
    {
        const float a = std::min(std::max(amount0to1, 0.0f), 1.0f);

        switch (mode)
        {
            case CharacterMode::Digital: return 20000.0f - (20000.0f - 18000.0f) * a; // quase nada
            case CharacterMode::Analog:  return 20000.0f - (20000.0f - 9000.0f) * a;
            case CharacterMode::Tape:    return 20000.0f - (20000.0f - 5500.0f) * a;
            default: return 20000.0f;
        }
    }

    // ========================================================
    // FASE 6 -- correção de nível SÓ pra apresentação do wet (nunca
    // aplicada ao caminho que volta pro feedback -- ver DelayEngine.h/
    // process(), onde o fork acontece). A contração de tanh(k*x)/k
    // (Seção 10 da FASE 5) reduz o RMS medido em ~-0.2dB (Digital),
    // ~-3.84dB (Analog) e ~-6.49dB (Tape) -- perceptível ao trocar de
    // modo. Compensação CONSERVADORA (não iguala 100%, só evita "muito
    // mais baixo" -- ainda sobra uma diferença musical real entre os
    // modos, que é o ponto de ter modos diferentes): metade a dois
    // terços da perda medida, escalada linearmente com characterAmount
    // (a perda de nível cresce com o amount, então a compensação
    // também precisa). Devolve GANHO LINEAR, não dB.
    // ========================================================

    static float wetMakeupGainForMode(CharacterMode mode, float amount0to1) noexcept
    {
        const float a = std::min(std::max(amount0to1, 0.0f), 1.0f);

        float trimDb = 0.0f;
        switch (mode)
        {
            case CharacterMode::Digital: trimDb = 0.3f; break;
            case CharacterMode::Analog:  trimDb = 2.0f; break;
            case CharacterMode::Tape:    trimDb = 3.5f; break;
            default: trimDb = 0.0f; break;
        }

        return std::pow(10.0f, (trimDb * a) / 20.0f);
    }

private:
    double sampleRate = 44100.0;
    float k = 0.15f;
    float hfCoeff = 1.0f;
    float hfState[2] { 0.0f, 0.0f };
};
}
