#include "ModulationEngine.h"

namespace NF
{
void ModulationEngine::prepare(double sampleRateIn) noexcept
{
    sampleRate = sampleRateIn;
    reset();
}

void ModulationEngine::reset() noexcept
{
    phase = 0.0f;
    randomCurrentL = randomStartL = randomTargetL = 0.0f;
    randomCurrentR = randomStartR = randomTargetR = 0.0f;
    cycleLengthSamples = 1;
    cycleSampleIndex = 0;
}

void ModulationEngine::setRate(float hz) noexcept
{
    rateHz = juce::jlimit(minRateHz, maxRateHz, hz);
}

void ModulationEngine::setDepth(float depth0to1) noexcept
{
    depthMs = juce::jlimit(0.0f, 1.0f, depth0to1) * maxDepthMs;
}

void ModulationEngine::setShape(ModShape newShape) noexcept
{
    shape = newShape;
}

void ModulationEngine::setSpread(float spread0to1) noexcept
{
    spread = juce::jlimit(0.0f, 1.0f, spread0to1);
}

float ModulationEngine::wrapPhase(float p) noexcept
{
    while (p >= 1.0f) p -= 1.0f;
    while (p < 0.0f) p += 1.0f;
    return p;
}

float ModulationEngine::shapeValue(ModShape shapeIn, float phase01) noexcept
{
    switch (shapeIn)
    {
        case ModShape::Sine:
            return std::sin(juce::MathConstants<float>::twoPi * phase01);

        case ModShape::Triangle:
        {
            // -1 em phase=0, sobe até +1 em phase=0.5, desce de volta
            // até -1 em phase=1 (wrap).
            if (phase01 < 0.5f)
                return 4.0f * phase01 - 1.0f;

            return 3.0f - 4.0f * phase01;
        }

        case ModShape::SoftRandom:
        default:
            return 0.0f; // tratado à parte em getNextOffsets()
    }
}

void ModulationEngine::getNextOffsets(float& offsetMsL, float& offsetMsR) noexcept
{
    if (shape == ModShape::SoftRandom)
    {
        if (cycleSampleIndex >= cycleLengthSamples)
        {
            cycleLengthSamples =
                juce::jmax(1, (int) std::round(sampleRate / (double) juce::jmax(minRateHz, rateHz)));
            cycleSampleIndex = 0;

            // O ponto de partida do novo segmento é o valor ATUAL (já
            // bem perto do alvo antigo depois do smoothstep completo)
            // -- não um snap pro alvo antigo. Continuidade de posição
            // garantida por construção; continuidade de INCLINAÇÃO vem
            // de smoothstep ter derivada 0 nas duas pontas (ver nota no
            // .h).
            randomStartL = randomCurrentL;
            randomTargetL = rngL.nextFloat() * 2.0f - 1.0f;

            randomStartR = randomCurrentR;
            randomTargetR = rngR.nextFloat() * 2.0f - 1.0f;
        }

        const float t = (float) cycleSampleIndex / (float) cycleLengthSamples;
        const float smoothT = smoothstep01(t);

        randomCurrentL = randomStartL + (randomTargetL - randomStartL) * smoothT;
        randomCurrentR = randomStartR + (randomTargetR - randomStartR) * smoothT;
        ++cycleSampleIndex;

        // Spread crossfada entre "R segue o passeio de L" (correlacionado)
        // e "R tem seu próprio passeio" (independente) -- ver nota no .h.
        const float blendedR = randomCurrentL * (1.0f - spread) + randomCurrentR * spread;

        offsetMsL = randomCurrentL * depthMs;
        offsetMsR = blendedR * depthMs;
    }
    else
    {
        const float phaseR = wrapPhase(phase + spread * 0.5f);

        offsetMsL = shapeValue(shape, phase) * depthMs;
        offsetMsR = shapeValue(shape, phaseR) * depthMs;
    }

    // Sempre avança a fase (mesmo em Soft Random, que não a consome) --
    // assim trocar de shape em tempo real nunca acha a fase "parada"
    // num valor arbitrário de antes.
    phase += (float) (rateHz / sampleRate);
    if (phase >= 1.0f)
        phase -= 1.0f;
}
}
