#pragma once
#include <cmath>
#include <algorithm>

namespace NF
{
// ============================================================
// FASE 5 -- envelope follower + ganho de ducking. Zero dependência de
// JUCE (mesmo motivo do HostSync/SVFFilter -- testável isolado).
//
// IMPORTANTE (Seção 2 do briefing): o detector lê o pico do sinal DRY
// de entrada -- NUNCA a saída do delay, feedback ou repeats. Quem
// alimenta getNextGain() é sempre max(|dryL|,|dryR|) capturado ANTES
// de qualquer processamento (ver DelayEngine::process()). O ganho
// devolvido só é aplicado à apresentação final do wet -- nunca
// realimentado pro loop -- então o Ducking não pode, por construção,
// alterar o decaimento das repetições internas (ver Seção 1: Ducking
// fica FORA do loop de feedback).
//
// Attack/release fixos internamente (só duckingAmount é parâmetro
// do usuário nesta versão): attack ~15ms (reação rápida quando o
// sinal entra), release ~350ms (retorno suave, sem pumping).
//
// Mapeamento: 0% = ganho sempre 1.0 (nenhuma alteração). 100% = até
// -maxDuckingDb (20dB, no meio da faixa -18/-24dB sugerida) quando o
// envelope está no pico -- não mute total. Redução proporcional ao
// envelope (contínua, sem gate/degrau).
// ============================================================

class DuckingProcessor
{
public:
    void prepare(double sampleRateIn) noexcept
    {
        sampleRate = sampleRateIn;

        constexpr double attackSeconds = 0.015;
        constexpr double releaseSeconds = 0.35;

        attackCoeff = (float) (1.0 - std::exp(-1.0 / (attackSeconds * sampleRate)));
        releaseCoeff = (float) (1.0 - std::exp(-1.0 / (releaseSeconds * sampleRate)));

        reset();
    }

    void reset() noexcept { envelope = 0.0f; }

    void setAmount(float amount0to1) noexcept
    {
        amount = std::min(std::max(amount0to1, 0.0f), 1.0f);
    }

    // Chamar UMA vez por amostra, com o pico DRY (max(|L|,|R|)) desta
    // amostra -- nunca com o sinal atrasado/realimentado. Devolve o
    // ganho linear (0-1] a multiplicar só no wet de saída.
    float getNextGain(float dryPeakSample) noexcept
    {
        const float rectified = std::abs(dryPeakSample);
        const float coeff = (rectified > envelope) ? attackCoeff : releaseCoeff;
        envelope += (rectified - envelope) * coeff;

        const float envelopeClamped = std::min(std::max(envelope, 0.0f), 1.0f);
        const float reductionDb = amount * maxDuckingDb * envelopeClamped;

        return std::pow(10.0f, -reductionDb / 20.0f);
    }

    static constexpr float maxDuckingDb = 20.0f;

private:
    double sampleRate = 44100.0;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelope = 0.0f;
    float amount = 0.0f;
};
}
