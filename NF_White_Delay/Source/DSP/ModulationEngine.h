#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace NF
{
enum class ModShape { Sine = 0, Triangle, SoftRandom };

// ============================================================
// FASE 4 -- LFO estéreo pro chorus/tape-movement do delay time.
// Devolve OFFSET em ms (não o tempo final) -- quem soma no
// baseDelayMs e clampa é o DelayEngine (ver DelayEngine.h, nota
// "MODULAÇÃO x CROSSFADE").
//
// BUFFER-SIZE-AGNOSTIC: o acumulador de fase avança POR AMOSTRA
// (phase += rate/sampleRate a cada chamada de getNextOffsets()), nunca
// por bloco -- processar em blocos de 32 ou 1024 amostras dá
// exatamente o mesmo resultado amostra-a-amostra.
//
// DEPTH MUSICAL: 0% = 0ms, 100% = ±maxDepthMs (8ms -- ver a constante
// abaixo). Nada de mapear 100% pra dezenas/centenas de ms -- 8ms é
// suficiente pra um chorus/tape-wow nitidamente audível sem soar como
// desafinação ou virar um efeito de flanger/vibrato agressivo em
// material percussivo. É próximo do wow/flutter típico de delays a
// fita reais.
//
// SOFT RANDOM: NÃO gera um número aleatório novo por amostra (isso
// soaria como ruído/zipper). Sorteia um novo alvo por CICLO do LFO
// (duração 1/rate segundos, a mesma "velocidade" que Sine/Triangle
// usam) e desliza até lá com SMOOTHSTEP (3t²-2t³), não interpolação
// linear -- a derivada de smoothstep é exatamente 0 em t=0 e em t=1,
// então o fim de um segmento e o início do próximo têm inclinação
// zero nos dois lados: a curva inteira fica C¹ contínua (sem quina de
// slope) na fronteira de cada ciclo, mesmo a posição já sendo
// contínua com interpolação linear. Passeio aleatório orgânico,
// contínuo E suave, sem clique. A decorrelação
// L/R é feita crossfadeando (via Spread) entre "R segue o mesmo
// passeio de L" (spread=0, mono) e "R tem seu próprio passeio
// independente" (spread=1, totalmente decorrelacionado) -- aplicar
// "fase" a um sinal aleatório não faz sentido (não existe uma "fase"
// de ruído), então aqui Spread controla CORRELAÇÃO, não deslocamento
// temporal, como faz para Sine/Triangle.
// ============================================================

class ModulationEngine
{
public:
    static constexpr float minRateHz = 0.05f;
    static constexpr float maxRateHz = 10.0f;
    static constexpr float maxDepthMs = 8.0f;

    void prepare(double sampleRateIn) noexcept;
    void reset() noexcept;

    void setRate(float hz) noexcept;
    void setDepth(float depth0to1) noexcept;
    void setShape(ModShape newShape) noexcept;
    void setSpread(float spread0to1) noexcept;

    // Avança uma amostra; devolve o offset em ms pra cada canal
    // (ainda sem ser somado ao delay base -- isso é trabalho do
    // DelayEngine).
    void getNextOffsets(float& offsetMsL, float& offsetMsR) noexcept;

private:
    static float shapeValue(ModShape shape, float phase01) noexcept;
    static float wrapPhase(float p) noexcept;

    double sampleRate = 44100.0;
    float rateHz = 1.0f;
    float depthMs = 0.0f;
    ModShape shape = ModShape::Sine;
    float spread = 0.0f;

    float phase = 0.0f;

    // Soft Random: dois passeios independentes (L e o próprio de R),
    // cada um interpolado com smoothstep entre o valor no início do
    // ciclo atual e o alvo sorteado pro fim dele.
    juce::Random rngL { 1 };
    juce::Random rngR { 2 };

    float randomCurrentL = 0.0f, randomStartL = 0.0f, randomTargetL = 0.0f;
    float randomCurrentR = 0.0f, randomStartR = 0.0f, randomTargetR = 0.0f;
    int cycleLengthSamples = 1;
    int cycleSampleIndex = 0;

    static float smoothstep01(float t) noexcept { return t * t * (3.0f - 2.0f * t); }
};
}
