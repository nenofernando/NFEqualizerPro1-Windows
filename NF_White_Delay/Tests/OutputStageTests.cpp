// Testes da FASE 6 (Dry/Wet + Output + Bypass), itens 3, 4, 5 (parcial)
// e 9/10 -- OutputStage isolado, sem DelayEngine. Determinístico:
// dry/wet são buffers conhecidos, não ruído -- dá pra comparar contra
// a fórmula exata (linear mix, decibelsToGain, crossfade de bypass).
//
// NOTA: as strings passadas pra expect()/juce::String() aqui ficam em
// inglês (ASCII puro), mesmo padrão de todos os outros arquivos de
// teste (ver DelayEngineTests.cpp etc.) -- juce::String(const char*)
// assume ASCII e dá assert em qualquer byte > 127, então acento
// direto nessas strings (diferente dos comentários, que são só
// texto-fonte, nunca viram juce::String em runtime) quebra a
// suite. Comentários continuam em português, igual ao resto do código.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/OutputStage.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

using NF::OutputStage;
using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    // Roda blocos até os SmoothedValue internos convergirem pro alvo
    // mais recente -- 20ms (mix/output) e 15ms (bypass) a 48kHz são,
    // respectivamente, 960 e 720 amostras; usamos uma margem generosa
    // (>=40x o bloco) pra garantir convergência total, não só "perto o
    // suficiente".
    void runSilentBlocks(OutputStage& stage, juce::AudioBuffer<float>& dry,
                          juce::AudioBuffer<float>& wet, juce::AudioBuffer<float>& out,
                          int totalSamples)
    {
        int done = 0;
        while (done < totalSamples)
        {
            const int n = juce::jmin(kBlockSize, totalSamples - done);
            stage.mixAndOutput(dry, wet, out, n);
            done += n;
        }
    }

    // Preenche um AudioBuffer estéreo inteiro com um valor constante --
    // usado pra criar dry/wet "conhecidos" (não ruído) nos testes
    // determinísticos de mix/output.
    void fillConstant(juce::AudioBuffer<float>& buffer, float value)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(ch, i, value);
    }
}

int runOutputStageTests()
{
    int failures = 0;

    std::cout << std::endl << "=== OutputStage tests (FASE 6) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // FASE 6.6: Mix determinístico -- 0% = dry exato, 100% = sem dry
    // nenhum, 50% = bate exatamente com a fórmula EQUAL-POWER
    // (dryGain=cos(mix*pi/2), wetGain=sin(mix*pi/2)) -- decisão
    // revertida após teste real em host (ver nota no topo de
    // OutputStage.h): a lei linear fazia a voz "cair de volume" ao
    // abrir uma fração de wet.
    // ------------------------------------------------------------
    {
        std::cout << "-- Deterministic mix: 0% / 50% / 100% --" << std::endl;

        constexpr float dryValue = 0.4f;
        constexpr float wetValue = -0.9f;
        constexpr int convergeSamples = kBlockSize * 40; // >> 960 samples of smoothing

        juce::AudioBuffer<float> dry(2, kBlockSize), wet(2, kBlockSize), out(2, kBlockSize);
        fillConstant(dry, dryValue);
        fillConstant(wet, wetValue);

        // Mix = 0%
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(0.0f, 0.0f, false);
            runSilentBlocks(stage, dry, wet, out, convergeSamples);

            stage.mixAndOutput(dry, wet, out, kBlockSize);
            bool allExact = true;
            for (int ch = 0; ch < 2 && allExact; ++ch)
                for (int i = 0; i < kBlockSize && allExact; ++i)
                    if (std::abs(out.getSample(ch, i) - dryValue) > 1.0e-5f)
                        allExact = false;

            expect(failures, allExact, "Mix=0%: output equals dry exactly (no wet), tolerance 1e-5");
        }

        // Mix = 100%
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(1.0f, 0.0f, false);
            runSilentBlocks(stage, dry, wet, out, convergeSamples);

            stage.mixAndOutput(dry, wet, out, kBlockSize);
            bool allExact = true;
            for (int ch = 0; ch < 2 && allExact; ++ch)
                for (int i = 0; i < kBlockSize && allExact; ++i)
                    if (std::abs(out.getSample(ch, i) - wetValue) > 1.0e-5f)
                        allExact = false;

            expect(failures, allExact, "Mix=100%: output equals wet exactly (no dry), tolerance 1e-5");
        }

        // Mix = 50% -- confere a fórmula EQUAL-POWER exata (dryGain=
        // wetGain=cos(pi/4)=sin(pi/4)=sqrt(0.5)), não a linear antiga
        // (que seria dry*0.5+wet*0.5, valor diferente).
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(0.5f, 0.0f, false);
            runSilentBlocks(stage, dry, wet, out, convergeSamples);

            stage.mixAndOutput(dry, wet, out, kBlockSize);

            const float expectedEqualPower = dryValue * std::sqrt(0.5f) + wetValue * std::sqrt(0.5f);
            const float expectedLinear = dryValue * 0.5f + wetValue * 0.5f;

            bool matchesEqualPower = true;
            for (int ch = 0; ch < 2 && matchesEqualPower; ++ch)
                for (int i = 0; i < kBlockSize && matchesEqualPower; ++i)
                    if (std::abs(out.getSample(ch, i) - expectedEqualPower) > 1.0e-5f)
                        matchesEqualPower = false;

            expect(failures, matchesEqualPower, "Mix=50%: output matches the exact EQUAL-POWER formula "
                                                "(dry*sqrt(0.5) + wet*sqrt(0.5)), not linear");
            expect(failures, std::abs(expectedLinear - expectedEqualPower) > 0.01f,
                   "sanity check: linear and equal-power give DIFFERENT values in this case "
                   "(confirms the test above discriminates between the two curves)");
        }

        // Mix = 20% -- ponto intermediário qualquer, confere a mesma
        // fórmula equal-power num valor que não é o meio exato da
        // faixa (mais uma checagem, além do Mix=50% acima).
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(0.2f, 0.0f, false);
            runSilentBlocks(stage, dry, wet, out, convergeSamples);
            stage.mixAndOutput(dry, wet, out, kBlockSize);

            const float angle20 = 0.2f * juce::MathConstants<float>::halfPi;
            const float expected20 = dryValue * std::cos(angle20) + wetValue * std::sin(angle20);
            expect(failures, std::abs(out.getSample(0, 0) - expected20) < 1.0e-4f,
                   "Mix=20%: output matches the equal-power formula, got "
                   + juce::String(out.getSample(0, 0), 6) + ", expected " + juce::String(expected20, 6));
        }
    }

    // ------------------------------------------------------------
    // Item 4: Output gain -- valores exatos pedidos no briefing.
    // 0dB=x1.0, +6dB≈x1.995262, -6dB≈x0.501187. Mix=100% (só wet) pra
    // isolar o ganho de output sem interferência do dry.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Output gain: exact dB values --" << std::endl;

        constexpr float wetValue = 1.0f;
        constexpr int convergeSamples = kBlockSize * 40;

        juce::AudioBuffer<float> dry(2, kBlockSize), wet(2, kBlockSize), out(2, kBlockSize);
        fillConstant(dry, 0.0f);
        fillConstant(wet, wetValue);

        auto testGainDb = [&](float db, float expectedGain, const juce::String& label)
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(1.0f, db, false);
            runSilentBlocks(stage, dry, wet, out, convergeSamples);

            stage.mixAndOutput(dry, wet, out, kBlockSize);

            const float got = out.getSample(0, 0);
            expect(failures, std::abs(got - expectedGain) < 1.0e-4f,
                   label + ": output = " + juce::String(got, 6)
                         + ", expected ~" + juce::String(expectedGain, 6));
        };

        testGainDb(0.0f, 1.0f, "0dB");
        testGainDb(6.0f, 1.995262f, "+6dB");
        testGainDb(-6.0f, 0.501187f, "-6dB");
        testGainDb(18.0f, 7.943282f, "+18dB (upper limit)");
        testGainDb(-18.0f, 0.125893f, "-18dB (lower limit)");
    }

    // ------------------------------------------------------------
    // Item 10: Bypass com parâmetros extremos -- Mix=100%, Output=+18dB.
    // Depois que o crossfade de bypass termina, output tem que ser
    // dry EXATO -- Output+18dB não pode continuar amplificando o sinal
    // bypassado (Seção 5/10 do briefing).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Bypass with extreme parameters (Mix=100%, Output=+18dB) --" << std::endl;

        constexpr float dryValue = 0.33f;
        constexpr float wetValue = 0.95f; // bem diferente do dry, pra não mascarar um bug
        constexpr int convergeSamples = kBlockSize * 40; // >> 720 samples (15ms @ 48kHz)

        juce::AudioBuffer<float> dry(2, kBlockSize), wet(2, kBlockSize), out(2, kBlockSize);
        fillConstant(dry, dryValue);
        fillConstant(wet, wetValue);

        OutputStage stage;
        stage.prepare(kSampleRate, kBlockSize);
        // Primeiro estabelece um estado "tocando" normal (não bypassado)
        // com os parâmetros extremos, exatamente como o briefing pede.
        stage.updateParameters(1.0f, 18.0f, false);
        runSilentBlocks(stage, dry, wet, out, convergeSamples);

        // Confirma que SEM bypass o output+18dB de fato amplifica
        // (senão o teste abaixo não provaria nada).
        stage.mixAndOutput(dry, wet, out, kBlockSize);
        const float outBeforeBypass = out.getSample(0, 0);
        expect(failures, std::abs(outBeforeBypass - wetValue) > 0.5f,
               "pre-bypass: Output+18dB actually amplifies the wet signal (output="
               + juce::String(outBeforeBypass, 4) + ")");

        // Agora engaja o bypass, mantendo Mix=100%/Output=+18dB -- e
        // deixa o crossfade de bypass convergir totalmente.
        stage.updateParameters(1.0f, 18.0f, true);
        runSilentBlocks(stage, dry, wet, out, convergeSamples);

        stage.mixAndOutput(dry, wet, out, kBlockSize);

        bool allExactDry = true;
        float maxDeviation = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float deviation = std::abs(out.getSample(ch, i) - dryValue);
                maxDeviation = juce::jmax(maxDeviation, deviation);
                if (deviation > 1.0e-4f)
                    allExactDry = false;
            }
        }

        expect(failures, allExactDry, "after full bypass crossfade: output equals dry exactly "
                                      "(Mix=100%/Output=+18dB have no effect at all), max deviation = "
                                      + juce::String(maxDeviation, 6));
    }

    // ------------------------------------------------------------
    // Item 6 (parcial): applyBypassInputGain -- zera suavemente o que
    // entra no DelayEngine quando bypassado, e deixa passar
    // (essencialmente) inalterado quando não bypassado.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- applyBypassInputGain: zeroes the delay engine input when bypassed --" << std::endl;

        constexpr int convergeSamples = kBlockSize * 40;
        constexpr float inputValue = 0.77f;

        // Não bypassado -- ganho deve convergir pra 1.0 (passa reto).
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(0.5f, 0.0f, false);

            juce::AudioBuffer<float> delayInput(2, kBlockSize);
            int done = 0;
            while (done < convergeSamples)
            {
                const int n = juce::jmin(kBlockSize, convergeSamples - done);
                fillConstant(delayInput, inputValue);
                stage.applyBypassInputGain(delayInput, n);
                done += n;
            }

            fillConstant(delayInput, inputValue);
            stage.applyBypassInputGain(delayInput, kBlockSize);
            expect(failures, std::abs(delayInput.getSample(0, 0) - inputValue) < 1.0e-4f,
                   "not bypassed: delay engine input gain converges to ~1.0 (signal passes through unchanged)");
        }

        // Bypassado -- ganho deve convergir pra 0.0 (motor para de
        // RECEBER sinal novo, mas continua rodando -- ver Phase6
        // integration tests pro tail continuando a decair).
        {
            OutputStage stage;
            stage.prepare(kSampleRate, kBlockSize);
            stage.updateParameters(0.5f, 0.0f, true);

            juce::AudioBuffer<float> delayInput(2, kBlockSize);
            int done = 0;
            while (done < convergeSamples)
            {
                const int n = juce::jmin(kBlockSize, convergeSamples - done);
                fillConstant(delayInput, inputValue);
                stage.applyBypassInputGain(delayInput, n);
                done += n;
            }

            fillConstant(delayInput, inputValue);
            stage.applyBypassInputGain(delayInput, kBlockSize);
            expect(failures, std::abs(delayInput.getSample(0, 0)) < 1.0e-4f,
                   "bypassed: delay engine input gain converges to ~0.0 (engine stops receiving new signal)");
        }
    }

    // ------------------------------------------------------------
    // Realtime safety: updateParameters/applyBypassInputGain/
    // mixAndOutput não alocam.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Realtime safety: no allocation --" << std::endl;

        OutputStage stage;
        stage.prepare(kSampleRate, kBlockSize);

        juce::AudioBuffer<float> dry(2, kBlockSize), wet(2, kBlockSize), out(2, kBlockSize);
        fillConstant(dry, 0.1f);
        fillConstant(wet, -0.2f);

        gAllocationSeenInGuard = false;
        gGuardActive = true;

        for (int block = 0; block < 200; ++block)
        {
            const bool bypassed = (block % 50) < 10;
            const float mix = 0.5f + 0.5f * std::sin((float) block * 0.1f);
            const float outDb = 6.0f * std::sin((float) block * 0.07f);

            stage.updateParameters(mix, outDb, bypassed);
            stage.applyBypassInputGain(dry, kBlockSize);
            stage.mixAndOutput(dry, wet, out, kBlockSize);
        }

        gGuardActive = false;
        expect(failures, ! gAllocationSeenInGuard, "200 blocks with automated mix/output/bypass: zero allocation");
    }

    // ------------------------------------------------------------
    // Auditoria pedida pelo usuario (2026-08-26), item 2: "Dry/Wet=0%,
    // Output=0dB -> output[n] == input[n] sample por sample. O dry nao
    // pode passar pela DelayLine." Testado aqui isolando OutputStage
    // (o "wet" simula o que sairia do DelayEngine -- deliberadamente
    // preenchido com um valor DIFERENTE do dry, pra provar que ele nao
    // vaza pra saida quando wetGain=0 exatamente).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Exact identity: Dry/Wet=0%, Output=0dB -> output == input bit-a-bit --" << std::endl;

        OutputStage stage;
        stage.prepare(kSampleRate, kBlockSize);

        juce::AudioBuffer<float> dry(2, kBlockSize), wet(2, kBlockSize), out(2, kBlockSize);

        // Dry: um sinal real (nao constante) -- tom + ruido, pra nao
        // mascarar nenhum erro que so apareceria com valores nao-triviais.
        juce::Random rng(4242);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                dry.setSample(ch, i, 0.6f * std::sin(0.05f * (float) i) + 0.05f * (rng.nextFloat() * 2.0f - 1.0f));

        // Wet: deliberadamente um sinal DIFERENTE (nao apenas != dry em
        // valor, mas de natureza diferente) -- se algo dele vazar pra
        // saida com Dry/Wet=0%, este teste pega.
        fillConstant(wet, 0.777f);

        // updateParameters() na PRIMEIRA chamada faz snap direto pro
        // alvo (ver "initialised" em OutputStage) -- ja no bloco 0,
        // mix=0/output=0dB/bypass=false estao exatamente no valor
        // final, sem rampa. mixAndOutput() le dry/wet no MESMO indice
        // de amostra e escreve em out nesse indice -- sem nenhum
        // deslocamento temporal (a DelayLine so existe dentro do
        // DelayEngine, que fica FORA do OutputStage por construcao).
        stage.updateParameters(0.0f, 0.0f, false);
        stage.mixAndOutput(dry, wet, out, kBlockSize);

        bool exactMatch = true;
        float worstDelta = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float delta = std::abs(out.getSample(ch, i) - dry.getSample(ch, i));
                worstDelta = juce::jmax(worstDelta, delta);
                if (out.getSample(ch, i) != dry.getSample(ch, i))
                    exactMatch = false;
            }
        }

        expect(failures, exactMatch,
              "output[n] == input[n] EXACTLY (bit-a-bit, nao so 'perto') em todas as " + juce::String(2 * kBlockSize)
              + " amostras -- maior delta observado = " + juce::String(worstDelta, 9));

        // Confere tambem que o buffer "wet" (que simula o output do
        // DelayEngine) realmente foi ignorado, nao só coincidiu por
        // sorte com o dry em algum ponto -- prova que dryGain=1/
        // wetGain=0 estao mesmo exatos, nao aproximados.
        expect(failures, wet.getSample(0, 0) != dry.getSample(0, 0),
              "sanity: wet e dry sao valores diferentes neste teste (senao o teste acima seria vazio)");
    }

    return failures;
}
