// Testes obrigatórios da FASE 4, itens 4, 5, 6 e 7 (Depth musical,
// Rate independente de buffer, Shapes, Spread). ModulationEngine é
// testado isolado primeiro, depois através do DelayEngine de verdade
// pra confirmar buffer-size-agnosticism ponta a ponta.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/ModulationEngine.h"
#include "../Source/DSP/DelayEngine.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

using namespace NF;
using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

int runModulationTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Modulation tests (FASE 4) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Item 4: Depth musical -- 0% = 0ms, 100% = ~8ms (maxDepthMs).
    // ------------------------------------------------------------
    {
        std::cout << "-- Depth: 0% = 0ms, 100% = ~8ms peak --" << std::endl;

        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(1.0f);
            mod.setDepth(0.0f);
            mod.setShape(ModShape::Sine);

            bool allZero = true;
            for (int i = 0; i < 48000; ++i)
            {
                float l, r;
                mod.getNextOffsets(l, r);
                if (l != 0.0f || r != 0.0f)
                    allZero = false;
            }
            expect(failures, allZero, "Depth 0%: offset is always exactly 0ms on both channels");
        }
        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(2.0f);
            mod.setDepth(1.0f);   // 100%
            mod.setShape(ModShape::Sine);
            mod.setSpread(0.0f);

            float peak = 0.0f;
            for (int i = 0; i < 48000; ++i)
            {
                float l, r;
                mod.getNextOffsets(l, r);
                peak = juce::jmax(peak, std::abs(l));
            }
            expect(failures, std::abs(peak - ModulationEngine::maxDepthMs) < 0.01f,
                  "Depth 100%: peak offset ~= " + juce::String(ModulationEngine::maxDepthMs, 1)
                  + "ms, got " + juce::String(peak, 4));
        }
    }

    // ------------------------------------------------------------
    // Item 6: Shapes -- Sine, Triangle, Soft Random.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Shapes: Sine, Triangle, Soft Random --" << std::endl;

        // Sine: em phase=0.25 (1/4 de ciclo, com rate=1Hz e sampleRate=
        // 48000, isso é a amostra 12000) o valor tem que estar no pico.
        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(1.0f);
            mod.setDepth(1.0f);
            mod.setShape(ModShape::Sine);
            mod.setSpread(0.0f);

            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < 12000; ++i)
                mod.getNextOffsets(l, r);

            expect(failures, std::abs(l - ModulationEngine::maxDepthMs) < 0.05f,
                  "Sine @ 1/4 cycle: at peak (~+8ms), got " + juce::String(l, 4));
        }

        // Triangle: bounded within [-depth, +depth], nunca ultrapassa.
        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(3.0f);
            mod.setDepth(1.0f);
            mod.setShape(ModShape::Triangle);
            mod.setSpread(0.0f);

            bool bounded = true;
            for (int i = 0; i < 48000; ++i)
            {
                float l, r;
                mod.getNextOffsets(l, r);
                if (std::abs(l) > ModulationEngine::maxDepthMs + 0.01f)
                    bounded = false;
            }
            expect(failures, bounded, "Triangle: offset never exceeds +-depthMs");
        }

        // Soft Random: NÃO pode ter saltos grandes amostra-a-amostra
        // (isso provaria que está sorteando um valor novo por amostra,
        // que é exatamente o que o briefing proíbe). O passo máximo
        // possível é depthMs*2 dividido pelo tamanho do ciclo (1/rate
        // segundos) -- calcula o limite teórico e confere que nenhum
        // delta observado passa disso (com uma folga pequena).
        {
            constexpr double sampleRate = 48000.0;
            constexpr float rateHz = 2.0f;
            constexpr float depth = 1.0f;

            ModulationEngine mod;
            mod.prepare(sampleRate);
            mod.setRate(rateHz);
            mod.setDepth(depth);
            mod.setShape(ModShape::SoftRandom);
            mod.setSpread(0.0f);

            // Passo teórico por amostra dentro de um ciclo: no pior caso
            // (alvo novo no extremo oposto do anterior), o passeio anda
            // até 2.0 em unidade normalizada ao longo de cycleSamples
            // amostras -- em ms, isso é 2*depthMs/cycleSamples. Uma
            // margem de 5x cobre erro de arredondamento float acumulado
            // na fronteira de cada ciclo, mas ainda fica ~2000x menor
            // que o salto que sortear um valor novo por amostra
            // produziria (até 2*depthMs de uma vez, não dividido por
            // cycleSamples) -- é isso que o teste realmente prova.
            const int cycleSamples = (int) std::round(sampleRate / rateHz);
            const float maxPossibleStep = (2.0f * ModulationEngine::maxDepthMs) / (float) cycleSamples * 5.0f;

            float prevL = 0.0f;
            float maxDelta = 0.0f;
            bool nanOrInf = false;

            for (int i = 0; i < (int) (sampleRate * 3.0); ++i)   // 3 segundos, vários ciclos
            {
                float l, r;
                mod.getNextOffsets(l, r);

                if (std::isnan(l) || std::isinf(l) || std::isnan(r) || std::isinf(r))
                    nanOrInf = true;

                maxDelta = juce::jmax(maxDelta, std::abs(l - prevL));
                prevL = l;
            }

            expect(failures, ! nanOrInf, "Soft Random: no NaN/Inf over 3s");
            expect(failures, maxDelta <= maxPossibleStep,
                  "Soft Random: no per-sample jump larger than the smooth-glide step allows "
                  "(max delta = " + juce::String(maxDelta, 6) + ", theoretical ceiling = "
                  + juce::String(maxPossibleStep, 6) + ") -- confirms it's NOT drawing a new "
                  "random value every sample");
        }

        // Melhoria pedida antes da FASE 5: smoothstep em vez de
        // interpolação linear entre os alvos -- a derivada tem que ser
        // ~0 nas duas pontas de CADA segmento (fim do ciclo anterior E
        // início do próximo), não só a posição contínua. Mede o delta
        // (1ª diferença) logo no início de um ciclo novo e logo antes do
        // próximo -- os dois têm que ser pequenos (perto de zero),
        // provando que a inclinação não dá um salto brusco na costura.
        {
            constexpr double sampleRate = 48000.0;
            constexpr float rateHz = 4.0f;

            ModulationEngine mod;
            mod.prepare(sampleRate);
            mod.setRate(rateHz);
            mod.setDepth(1.0f);
            mod.setShape(ModShape::SoftRandom);
            mod.setSpread(0.0f);

            const int cycleSamples = (int) std::round(sampleRate / rateHz);

            // Meio do ciclo (t=0.5): é onde smoothstep tem a MAIOR
            // inclinação (derivada de 3t²-2t³ em t=0.5 é 1.5, o pico) --
            // usado como referência de "inclinação normal" pra comparar
            // com a borda.
            std::vector<float> values;
            values.reserve((size_t) cycleSamples * 3);
            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < cycleSamples * 3; ++i)
            {
                mod.getNextOffsets(l, r);
                values.push_back(l);
            }

            auto deltaAt = [&](int index) { return std::abs(values[(size_t) index] - values[(size_t) (index - 1)]); };

            // Amostras logo ANTES de uma borda de ciclo (fim do 2º ciclo,
            // índice cycleSamples*2 - 1) e logo DEPOIS (início do 3º,
            // índice cycleSamples*2) -- ambas devem ter delta pequeno.
            const float deltaBeforeEdge = deltaAt(cycleSamples * 2 - 1);
            const float deltaAfterEdge = deltaAt(cycleSamples * 2 + 1);
            const float deltaAtMidCycle = deltaAt(cycleSamples * 2 + cycleSamples / 2);

            expect(failures, deltaBeforeEdge < deltaAtMidCycle * 0.5f,
                  "Soft Random (smoothstep): slope right before a cycle boundary is much smaller "
                  "than mid-cycle slope (edge=" + juce::String(deltaBeforeEdge, 6)
                  + ", mid-cycle=" + juce::String(deltaAtMidCycle, 6) + ") -- confirms derivative -> 0 at the edge");

            expect(failures, deltaAfterEdge < deltaAtMidCycle * 0.5f,
                  "Soft Random (smoothstep): slope right after a cycle boundary is much smaller "
                  "than mid-cycle slope (edge=" + juce::String(deltaAfterEdge, 6)
                  + ", mid-cycle=" + juce::String(deltaAtMidCycle, 6) + ") -- confirms derivative -> 0 at the edge too, "
                  "no slope discontinuity at the join");
        }
    }

    // ------------------------------------------------------------
    // Item 7: Spread -- 0% = L/R em fase, 100% = ~180 graus.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Spread: 0% in-phase, 100% ~180 degrees --" << std::endl;

        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(1.0f);
            mod.setDepth(1.0f);
            mod.setShape(ModShape::Sine);
            mod.setSpread(0.0f);

            bool allEqual = true;
            for (int i = 0; i < 48000; ++i)
            {
                float l, r;
                mod.getNextOffsets(l, r);
                if (std::abs(l - r) > 1e-5f)
                    allEqual = false;
            }
            expect(failures, allEqual, "Spread 0%: L and R are identical (in phase) at every sample");
        }
        {
            ModulationEngine mod;
            mod.prepare(48000.0);
            mod.setRate(1.0f);
            mod.setDepth(1.0f);
            mod.setShape(ModShape::Sine);
            mod.setSpread(1.0f);   // 100% = 180 graus

            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < 12000; ++i)   // 1/4 de ciclo -- L no pico
                mod.getNextOffsets(l, r);

            expect(failures, std::abs(l + r) < 0.05f,
                  "Spread 100%: at L's peak, R ~= -L (180 degrees apart) -- L=" + juce::String(l, 4)
                  + ", R=" + juce::String(r, 4));
        }
    }

    // ------------------------------------------------------------
    // Item 5: Rate independente do tamanho do buffer -- roda o
    // DelayEngine de verdade (modulação ativa) com um tom senoidal,
    // uma vez em blocos de 32, outra em blocos de 1024, e confere que
    // a saída é BIT-IDÊNTICA -- o acumulador de fase avança por
    // amostra, nunca por bloco.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Rate/phase: identical output regardless of block size (32 vs 1024) --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = 48000;

        auto runWithBlockSize = [&](int blockSize) -> std::vector<float>
        {
            DelayEngine engine;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
            engine.prepare(spec);
            engine.setDelayTimeMs(100.0f);
            engine.setFeedback(0.0f);
            engine.setModRate(3.0f);
            engine.setModDepth(1.0f);
            engine.setModShape(ModShape::Sine);
            engine.setModSpread(0.3f);

            std::vector<float> out;
            out.reserve((size_t) totalSamples);

            int done = 0;
            int phase = 0;

            while (done < totalSamples)
            {
                const int n = juce::jmin(blockSize, totalSamples - done);
                juce::AudioBuffer<float> buffer(2, n);

                for (int i = 0; i < n; ++i)
                {
                    const float s = 0.4f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * (float) phase / (float) sampleRate);
                    buffer.setSample(0, i, s);
                    buffer.setSample(1, i, s);
                    ++phase;
                }

                engine.process(buffer);

                for (int i = 0; i < n; ++i)
                    out.push_back(buffer.getSample(0, i));

                done += n;
            }

            return out;
        };

        auto outSmallBlocks = runWithBlockSize(32);
        auto outLargeBlocks = runWithBlockSize(1024);

        bool identical = (outSmallBlocks.size() == outLargeBlocks.size());
        float maxDiff = 0.0f;

        if (identical)
        {
            for (size_t i = 0; i < outSmallBlocks.size(); ++i)
                maxDiff = juce::jmax(maxDiff, std::abs(outSmallBlocks[i] - outLargeBlocks[i]));
        }

        expect(failures, identical && maxDiff < 1e-5f,
              "block size 32 vs 1024 produce the identical modulated output (max diff = "
              + juce::String(maxDiff, 8) + ")");
    }

    return failures;
}
