#pragma once
#include <cmath>
#include <algorithm>

namespace NF
{
// ============================================================
// FASE 4 -- filtro state-variable TPT (topology-preserving transform),
// um canal, seguindo Zavalishin "The Art of VA Filter Design". Zero
// dependência de JUCE, de propósito (mesmo motivo do HostSync: fica
// totalmente testável sozinho).
//
// PROVA DE ESTABILIDADE DO LOOP DE FEEDBACK (Seção 2 da correção da
// FASE 4 -- "Feedback 95% + Resonance 100% não pode gerar runaway"):
//
// O filtro em si (isolado, sem feedback externo) é incondicionalmente
// estável (BIBO) pra qualquer Q > 0 -- é um filtro linear de 2 polos,
// nunca "explode" sozinho. O que fica perigoso é o LOOP externo do
// delay: loopGain = feedbackGain * ganhoDoFiltroNaFrequênciaDeRessonância.
// Um filtro ressonante sem compensação pode ter ganho de pico BEM
// acima de 1.0 (0dB) perto do cutoff -- com Q alto isso facilmente
// ultrapassa 1/feedbackGain, e o loop diverge (o clássico "delay
// ressonante uivando").
//
// Solução adotada: COMPENSAÇÃO DE GANHO NORMALIZADA (uma das 3 opções
// que você propôs), matematicamente justificada:
//
// O protótipo padrão de 2ª ordem H_LP(s) = wn² / (s² + (wn/Q)s + wn²)
// (e H_HP, com o mesmo ganho de pico por simetria) tem ganho de pico
// EXATO:
//
//     peakGain(Q) = Q / sqrt(1 - 1/(4Q²))      para Q >= 1/sqrt(2)
//     peakGain(Q) = 1                          para Q <  1/sqrt(2)
//                                               (sem pico -- já <= 1)
//
// (resultado clássico -- pico de |H(jw)| ocorre em
// w_peak = wn*sqrt(1 - 1/(2Q²)), e substituindo de volta em |H(jw)|
// dá exatamente essa fórmula). Compensando a saída do filtro por
// 1/peakGain(Q), o ganho MÁXIMO possível do filtro (o norma H-infinito,
// ||H||∞ = max sobre todo w de |H(jw)|) fica travado em EXATAMENTE
// 1.0, não importa o Q -- de Q=0.5 até Q=20 (nosso teto, ver
// DelayEngine.h) ou além.
//
// Por que isso garante estabilidade do loop: pra um sistema linear
// estável, ||H||∞ é o pior caso de amplificação possível pra QUALQUER
// entrada limitada, não só senos isolados. Com ||H||∞ <= 1 garantido
// por construção, o ganho de volta do loop nunca passa de
// feedbackGain * 1.0 = feedbackGain <= 0.95 < 1 -- a mesma prova de
// decaimento geométrico já usada pro feedback sem filtro na FASE 3,
// agora estendida pro caminho filtrado. Não é um limiter escondido:
// é a resposta do filtro sendo normalizada matematicamente, o pico
// de ressonância continua lá (mais estreito/acentuado com Q alto,
// bem audível), só não amplifica além do que entrou.
// ============================================================

class SVFFilter
{
public:
    enum class Mode { Lowpass, Highpass };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void setMode(Mode newMode) noexcept { mode = newMode; }

    void setCutoffAndQ(float cutoffHz, float q) noexcept
    {
        const float nyquistMargin = (float) (sampleRate * 0.49);
        const float clampedCutoff = std::min(std::max(cutoffHz, 10.0f), nyquistMargin);
        const float clampedQ = std::max(q, 0.001f);

        constexpr float pi = 3.14159265358979323846f;
        g = std::tan(pi * clampedCutoff / (float) sampleRate);
        k = 1.0f / clampedQ;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        currentQ = clampedQ;
    }

    float process(float input) noexcept
    {
        const float v3 = input - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        const float raw = (mode == Mode::Lowpass) ? v2 : (input - k * v1 - v2);
        return raw * resonanceGainCompensationFor(currentQ);
    }

    // Público e static só pra ser testável isoladamente (confirmar a
    // fórmula sem precisar processar áudio).
    static float resonanceGainCompensationFor(float q) noexcept
    {
        constexpr float criticalQ = 0.70710678f; // 1/sqrt(2)

        if (q <= criticalQ)
            return 1.0f;

        const float peakGain = q / std::sqrt(1.0f - 1.0f / (4.0f * q * q));
        return 1.0f / peakGain;
    }

private:
    double sampleRate = 44100.0;
    Mode mode = Mode::Lowpass;

    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float currentQ = 0.5f;
    float ic1eq = 0.0f, ic2eq = 0.0f;
};
}
