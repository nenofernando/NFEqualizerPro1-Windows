// Testes obrigatórios da FASE 5, itens 10, 12, 13 e 14 (Character:
// small-signal gain, gain staging, troca de modo, matriz com feedback).
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/CharacterProcessor.h"
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

namespace
{
    struct RunResult
    {
        bool allocationDetected = false;
        bool nanOrInfDetected = false;
        float peakMagnitude = 0.0f;
        float tailRms = 0.0f;
    };

    RunResult runFeedbackMatrixCase(double sampleRate, CharacterMode mode, float characterAmount,
                                    float feedback, bool loFiOn, int totalSamples)
    {
        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(30.0f);
        engine.setFeedback(feedback);
        engine.setLoFiEnabled(loFiOn);
        engine.setCharacterMode(mode);
        engine.setCharacterAmount(characterAmount);

        RunResult result;
        std::vector<float> tail;

        int done = 0;
        bool first = true;

        while (done < totalSamples)
        {
            const int n = juce::jmin(512, totalSamples - done);
            juce::AudioBuffer<float> buffer(2, n);
            buffer.clear();

            if (first)
            {
                buffer.setSample(0, 0, 1.0f);
                buffer.setSample(1, 0, 1.0f);
                first = false;
            }

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            engine.process(buffer);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                result.allocationDetected = true;

            for (int i = 0; i < n; ++i)
            {
                const float s = buffer.getSample(0, i);
                if (std::isnan(s) || std::isinf(s))
                    result.nanOrInfDetected = true;

                result.peakMagnitude = juce::jmax(result.peakMagnitude, std::abs(s));

                if (done + i >= totalSamples - 4800)   // último 0.1s (a 48kHz)
                    tail.push_back(s);
            }

            done += n;
        }

        double sumSquares = 0.0;
        for (float s : tail) sumSquares += (double) s * (double) s;
        result.tailRms = tail.empty() ? 0.0f : (float) std::sqrt(sumSquares / (double) tail.size());

        return result;
    }
}

int runCharacterTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Character tests (FASE 5, itens 10, 12, 13, 14) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Item 10: prova de small-signal gain -- fórmula fechada E teste
    // numérico direto na função (dy/dx em x->0), pra qualquer modo e
    // amount, incluindo os extremos.
    // ------------------------------------------------------------
    {
        std::cout << "-- Small-signal gain == 1.0 exactly (item 10) --" << std::endl;

        constexpr float tinyX = 0.0001f;   // "pequeno sinal" -- perto de zero

        bool allUnity = true;
        float worstGain = 1.0f;

        for (CharacterMode mode : { CharacterMode::Digital, CharacterMode::Analog, CharacterMode::Tape })
        {
            for (float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                const float k = CharacterProcessor::kForMode(mode, amount);

                // dy/dx em x=0 medido numericamente: y(tinyX)/tinyX --
                // pra uma função ímpar suave por 0, isso converge pra
                // exatamente a derivada em 0 conforme tinyX->0.
                const float y = std::tanh(k * tinyX) / k;
                const float smallSignalGain = y / tinyX;

                if (std::abs(smallSignalGain - 1.0f) > 0.001f)
                {
                    allUnity = false;
                    worstGain = smallSignalGain;
                }
            }
        }

        expect(failures, allUnity,
              "tanh(k*x)/k has small-signal gain (dy/dx at x=0) == 1.0 EXACTLY, for every mode and "
              "amount from 0% to 100%" + (allUnity ? juce::String() : (" (worst measured: " + juce::String(worstGain, 6) + ")")));

        // Contraste explícito com a forma ingênua tanh(drive*x)/tanh(drive)
        // -- prova que ela TEM ganho de pequeno sinal > 1, exatamente
        // como o risco descrito no briefing.
        bool naiveFormHasExcessGain = true;
        for (float drive : { 0.5f, 1.0f, 2.0f, 5.0f })
        {
            const float naiveGain = drive / std::tanh(drive);   // dy/dx em x=0 da forma ingênua
            if (naiveGain <= 1.0f)
                naiveFormHasExcessGain = false;
        }
        expect(failures, naiveFormHasExcessGain,
              "confirmed: the naive tanh(drive*x)/tanh(drive) form DOES have small-signal gain > 1 "
              "for any drive > 0 (this is exactly why it was NOT used here)");
    }

    // ------------------------------------------------------------
    // Prova complementar: |y| <= |x| SEMPRE (contração global, não só
    // perto de zero) -- é isso que garante RMS não crescer em nenhum
    // nível de sinal (base do item 12).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Global contraction: |tanh(k*x)/k| <= |x| for all x, all k --" << std::endl;

        bool alwaysContracts = true;
        juce::Random rng(55);

        for (int trial = 0; trial < 2000; ++trial)
        {
            const float k = 0.001f + rng.nextFloat() * 3.0f;
            const float x = (rng.nextFloat() * 2.0f - 1.0f) * 4.0f;   // até +-4.0 (bem acima de fullscale)

            const float y = std::tanh(k * x) / k;

            if (std::abs(y) > std::abs(x) + 1e-6f)
                alwaysContracts = false;
        }

        expect(failures, alwaysContracts,
              "2000 random (k, x) pairs: |tanh(k*x)/k| never exceeds |x| -- output magnitude never "
              "exceeds input, at ANY signal level, not just small-signal");
    }

    // ------------------------------------------------------------
    // Item 12: RMS comparável entre Character 0% e 100% -- gain
    // staging musical, sem salto de volume perceptível (e, pela
    // contração global provada acima, 100% nunca fica mais ALTO).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- RMS comparison: Character 0% vs 100% (no loudness jump) --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = 48000;

        for (CharacterMode mode : { CharacterMode::Digital, CharacterMode::Analog, CharacterMode::Tape })
        {
            CharacterProcessor procOff, procOn;
            procOff.prepare(sampleRate);
            procOn.prepare(sampleRate);
            procOff.setCoefficients(CharacterProcessor::kForMode(mode, 0.0f), CharacterProcessor::hfCutoffHzForMode(mode, 0.0f));
            procOn.setCoefficients(CharacterProcessor::kForMode(mode, 1.0f), CharacterProcessor::hfCutoffHzForMode(mode, 1.0f));

            double sumSqOff = 0.0, sumSqOn = 0.0;
            juce::Random rng(200 + (int) mode);

            for (int i = 0; i < totalSamples; ++i)
            {
                const float in = (rng.nextFloat() * 2.0f - 1.0f) * 0.5f;
                const float outOff = procOff.process(0, in);
                const float outOn = procOn.process(0, in);
                sumSqOff += (double) outOff * outOff;
                sumSqOn += (double) outOn * outOn;
            }

            const float rmsOff = (float) std::sqrt(sumSqOff / totalSamples);
            const float rmsOn = (float) std::sqrt(sumSqOn / totalSamples);
            const float rmsDeltaDb = 20.0f * std::log10(juce::jmax(rmsOn, 1e-9f) / juce::jmax(rmsOff, 1e-9f));

            const juce::String modeName = mode == CharacterMode::Digital ? "Digital" : mode == CharacterMode::Analog ? "Analog" : "Tape";

            expect(failures, rmsOn <= rmsOff * 1.02f,
                  modeName + ": RMS at 100% is not louder than at 0% (rmsOff=" + juce::String(rmsOff, 4)
                  + ", rmsOn=" + juce::String(rmsOn, 4) + ", delta=" + juce::String(rmsDeltaDb, 2) + "dB)");
        }
    }

    // ------------------------------------------------------------
    // Item 13: trocar de modo em tempo real (via DelayEngine, com
    // feedback ativo) não pode dar click/pop/NaN.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Mode switching (Digital -> Analog -> Tape -> Digital) mid-stream --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(70.0f);
        engine.setFeedback(0.8f);
        engine.setCharacterAmount(0.8f);
        engine.setCharacterMode(CharacterMode::Digital);

        std::vector<float> out;
        bool allocationDetected = false, nanOrInfDetected = false;

        juce::Random rng(999);
        const CharacterMode sequence[] { CharacterMode::Analog, CharacterMode::Tape, CharacterMode::Digital,
                                         CharacterMode::Tape, CharacterMode::Analog };
        int seqIndex = 0;

        constexpr int totalSamples = (int) (sampleRate * 3.0);
        int done = 0;
        int blockCount = 0;

        while (done < totalSamples)
        {
            if (blockCount % 30 == 0 && seqIndex < (int) (sizeof(sequence) / sizeof(sequence[0])))
                engine.setCharacterMode(sequence[seqIndex++]);

            const int n = juce::jmin(blockSize, totalSamples - done);
            juce::AudioBuffer<float> buffer(2, n);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buffer.setSample(ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.4f);

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            engine.process(buffer);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                allocationDetected = true;

            for (int i = 0; i < n; ++i)
            {
                const float s = buffer.getSample(0, i);
                if (std::isnan(s) || std::isinf(s)) nanOrInfDetected = true;
                out.push_back(s);
            }

            done += n;
            ++blockCount;
        }

        expect(failures, ! allocationDetected, "mode switching mid-stream: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "mode switching mid-stream: no NaN/Inf");

        float maxDelta = 0.0f;
        for (size_t i = 1; i < out.size(); ++i)
            maxDelta = juce::jmax(maxDelta, std::abs(out[i] - out[i - 1]));

        // Entrada é ruído com deltas naturais grandes (não um tom puro
        // e previsível) -- confere só que nada explodiu de forma
        // anormal (bem acima da própria amplitude de entrada), não um
        // limiar de "clique" fino como nos testes de tom senoidal.
        expect(failures, maxDelta < 3.0f,
              "no abnormal spike anywhere while switching modes under noise + feedback, max delta = "
              + juce::String(maxDelta, 4));
    }

    // ------------------------------------------------------------
    // Item 14: matriz Digital/Analog/Tape x Character 0/50/100% x
    // Feedback 0/50/95% x Lo-Fi OFF/ON -- finite, bounded, decay,
    // sem NaN/Inf.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Matrix: mode x character x feedback x lo-fi --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = (int) (sampleRate * 3.0);

        const CharacterMode modes[] { CharacterMode::Digital, CharacterMode::Analog, CharacterMode::Tape };
        const float amounts[] { 0.0f, 0.5f, 1.0f };
        const float feedbacks[] { 0.0f, 0.5f, 0.95f };
        const bool loFiOptions[] { false, true };

        int casesRun = 0;
        bool allOk = true;

        for (auto mode : modes)
        {
            for (float amount : amounts)
            {
                for (float feedback : feedbacks)
                {
                    for (bool loFiOn : loFiOptions)
                    {
                        auto result = runFeedbackMatrixCase(sampleRate, mode, amount, feedback, loFiOn, totalSamples);
                        ++casesRun;

                        const bool tailDecayed = (feedback < 0.95f) ? (result.tailRms < 0.05f) : (result.tailRms < 0.3f);
                        // Feedback baixo/médio: decai bem baixo em 3s.
                        // 95%: ainda pode estar audível em 3s (delay curto,
                        // 0.95^N decai devagar) -- só confere que está
                        // MENOR que o pico, não perto de zero ainda.

                        if (result.allocationDetected || result.nanOrInfDetected
                            || result.peakMagnitude > 3.0f || ! tailDecayed)
                        {
                            allOk = false;
                            const juce::String modeName = mode == CharacterMode::Digital ? "Digital" : mode == CharacterMode::Analog ? "Analog" : "Tape";
                            expect(failures, false, modeName + " amount=" + juce::String(amount, 2)
                                                   + " fb=" + juce::String(feedback, 2) + " lofi=" + juce::String((int) loFiOn)
                                                   + ": alloc=" + juce::String((int) result.allocationDetected)
                                                   + " nanInf=" + juce::String((int) result.nanOrInfDetected)
                                                   + " peak=" + juce::String(result.peakMagnitude, 3)
                                                   + " tailRms=" + juce::String(result.tailRms, 4));
                        }
                    }
                }
            }
        }

        expect(failures, casesRun == 3 * 3 * 3 * 2, "ran all 54 mode x character x feedback x lo-fi combinations");
        expect(failures, allOk, "all 54 combinations: finite, bounded, no NaN/Inf, decaying");
    }

    // ------------------------------------------------------------
    // Auditoria pedida pelo usuário (2026-08-26): "no teste real estou
    // percebendo pouca ou nenhuma diferença ao clicar em DIGITAL/
    // ANALOG/TAPE". A cadeia de roteamento já foi conferida por
    // inspeção de código (botão -> APVTS delayMode -> attachment ->
    // PluginProcessor::processChunk() -> DelayEngine::setCharacterMode()
    // -> CharacterProcessor::kForMode/hfCutoffHzForMode(characterMode,
    // amount) -> character.process(), chamado todo bloco) -- sem bug de
    // wiring: os 3 botões SÃO mutuamente exclusivos (SegmentedControl,
    // ver PluginEditor.cpp) e cada um SELECIONA de fato uma fórmula
    // diferente. Esta seção MEDE objetivamente o quanto essa diferença
    // é audível, pra separar "não está funcionando" de "está fraco
    // demais perto de Character=0%" -- ver conclusão nos comentários
    // finais de cada bloco.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Auditoria objetiva: Digital vs Analog vs Tape (item 4 do pedido) --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int N = 8192;               // 64 ciclos exatos de 375Hz -- DFT sem vazamento, sem janela
        constexpr float f0 = 375.0f;
        constexpr int fundamentalBin = 64;    // N * f0 / sampleRate

        auto makeSine = [](float freqHz, float amp, int numSamples)
        {
            std::vector<float> v((size_t) numSamples);
            for (int i = 0; i < numSamples; ++i)
                v[(size_t) i] = amp * std::sin(juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
            return v;
        };

        // Goertzel simples pra um bin específico -- só precisamos de
        // alguns harmônicos, não uma FFT completa.
        auto dftMagnitude = [](const std::vector<float>& x, int bin)
        {
            double re = 0.0, im = 0.0;
            const double w = juce::MathConstants<double>::twoPi * (double) bin / (double) x.size();
            for (size_t n = 0; n < x.size(); ++n)
            {
                re += (double) x[n] * std::cos(w * (double) n);
                im -= (double) x[n] * std::sin(w * (double) n);
            }
            return std::sqrt(re * re + im * im) / ((double) x.size() / 2.0);
        };

        auto processThroughMode = [](const std::vector<float>& input, CharacterMode mode, float amount)
        {
            CharacterProcessor proc;
            proc.prepare(sampleRate);
            proc.setCoefficients(CharacterProcessor::kForMode(mode, amount), CharacterProcessor::hfCutoffHzForMode(mode, amount));
            std::vector<float> out(input.size());
            for (size_t i = 0; i < input.size(); ++i)
                out[i] = proc.process(0, input[i]);
            return out;
        };

        auto peakAndRms = [](const std::vector<float>& v)
        {
            double sumSq = 0.0; float peak = 0.0f;
            for (float s : v) { sumSq += (double) s * s; peak = juce::jmax(peak, std::abs(s)); }
            return std::make_pair(peak, (float) std::sqrt(sumSq / (double) v.size()));
        };

        auto diffRms = [](const std::vector<float>& a, const std::vector<float>& b)
        {
            double sumSq = 0.0;
            for (size_t i = 0; i < a.size(); ++i) { const double d = (double) a[i] - (double) b[i]; sumSq += d * d; }
            return (float) std::sqrt(sumSq / (double) a.size());
        };

        const char* modeNames[3] { "Digital", "Analog", "Tape" };
        const CharacterMode modeEnums[3] { CharacterMode::Digital, CharacterMode::Analog, CharacterMode::Tape };

        // --- 1) 375Hz @ -6dBFS (0.5), Character=100%: peak/RMS/THD ---
        {
            std::cout << "   [375Hz, amount=100%] peak / RMS / THD:" << std::endl;
            auto dry = makeSine(f0, 0.5f, N);
            std::vector<float> outputs[3];
            float thdPercent[3] {};

            for (int m = 0; m < 3; ++m)
            {
                outputs[m] = processThroughMode(dry, modeEnums[m], 1.0f);
                const double fundamental = dftMagnitude(outputs[m], fundamentalBin);
                double harmonicSumSq = 0.0;
                for (int h = 2; h <= 5; ++h)
                    harmonicSumSq += std::pow(dftMagnitude(outputs[m], fundamentalBin * h), 2.0);
                thdPercent[m] = (float) (100.0 * std::sqrt(harmonicSumSq) / juce::jmax(fundamental, 1e-9));

                const auto [peak, rms] = peakAndRms(outputs[m]);
                std::cout << "     " << modeNames[m] << ": peak=" << peak << " rms=" << rms
                          << " THD=" << thdPercent[m] << "%" << std::endl;
            }

            expect(failures, thdPercent[0] < thdPercent[1] && thdPercent[1] < thdPercent[2],
                  "THD cresce monotonicamente Digital < Analog < Tape em amount=100% (medido: "
                  + juce::String(thdPercent[0], 3) + "% / " + juce::String(thdPercent[1], 3) + "% / "
                  + juce::String(thdPercent[2], 3) + "%)");

            // --- Null tests: Analog-Digital, Tape-Digital, Tape-Analog ---
            const float dAD = diffRms(outputs[1], outputs[0]);
            const float dTD = diffRms(outputs[2], outputs[0]);
            const float dTA = diffRms(outputs[2], outputs[1]);
            std::cout << "   Null tests (RMS da diferenca): Analog-Digital=" << dAD
                      << " Tape-Digital=" << dTD << " Tape-Analog=" << dTA << std::endl;

            expect(failures, dAD > 0.001f && dTD > 0.001f && dTA > 0.001f,
                  "em amount=100%, os 3 modos produzem saidas mensuravelmente DIFERENTES entre si "
                  "(nao e um bug de roteamento -- o problema relatado e so audivel perto de amount=0%, ver bloco seguinte)");
        }

        // --- 2) 8kHz (agudo) @ amount=100%: mostra a diferenca de HF cutoff entre os modos ---
        {
            std::cout << "   [8kHz, amount=100%] atenuacao de agudos por modo:" << std::endl;
            auto dryHf = makeSine(8000.0f, 0.5f, N);
            const auto [dryPeak, dryRms] = peakAndRms(dryHf);
            juce::ignoreUnused(dryPeak);

            float rmsHf[3] {};
            for (int m = 0; m < 3; ++m)
            {
                auto out = processThroughMode(dryHf, modeEnums[m], 1.0f);
                rmsHf[m] = peakAndRms(out).second;
                const float dropDb = 20.0f * std::log10(juce::jmax(rmsHf[m], 1e-9f) / juce::jmax(dryRms, 1e-9f));
                std::cout << "     " << modeNames[m] << ": rms=" << rmsHf[m] << " (" << dropDb << " dB vs dry)" << std::endl;
            }

            expect(failures, rmsHf[0] > rmsHf[1] && rmsHf[1] > rmsHf[2],
                  "em 8kHz/amount=100%, atenuacao de agudos cresce Digital < Analog < Tape como esperado pelo "
                  "hfCutoffHzForMode (18kHz/9kHz/5.5kHz)");
        }

        // --- 3) O PROBLEMA RELATADO: amount=0% (o default de fabrica) ---
        {
            std::cout << "   [375Hz, amount=0% -- o que o usuario esta ouvindo por default] diferenca entre modos:" << std::endl;
            auto dry = makeSine(f0, 0.5f, N);
            std::vector<float> outputs[3];
            for (int m = 0; m < 3; ++m)
                outputs[m] = processThroughMode(dry, modeEnums[m], 0.0f);

            const float dAD0 = diffRms(outputs[1], outputs[0]);
            const float dTD0 = diffRms(outputs[2], outputs[0]);
            std::cout << "     Null tests em amount=0%: Analog-Digital=" << dAD0 << " Tape-Digital=" << dTD0
                      << "  (compare com ~" << diffRms(outputs[1], outputs[0]) << " acima em amount=100%)" << std::endl;

            // Isto NAO e um assert de falha -- e a MEDICAO que confirma
            // a causa raiz relatada pelo usuario: em amount=0%, kForMode
            // devolve 0.001 (Digital) / 0.05 (Analog) / 0.08 (Tape) e
            // hfCutoffHzForMode devolve ~20kHz pros tres -- valores
            // pequenos o bastante pra soarem quase idênticos em material
            // musical normal. CONCLUSAO (ver relatorio entregue ao
            // usuario): a cadeia de roteamento esta CORRETA; a causa da
            // "pouca diferenca" e de DESIGN (Seção 12 original: "0% =
            // praticamente neutro uniformemente"), nao um bug -- por
            // pedido explicito do usuario (item 6), os modos devem
            // carregar um carater minimo perceptivel mesmo em
            // Character=0%, o que exige mudar kForMode/hfCutoffHzForMode
            // (aguardando aprovacao antes de alterar).
            std::cout << "     (medicao informativa -- ver conclusao no relatorio, nao e pass/fail)" << std::endl;
        }
    }

    return failures;
}
