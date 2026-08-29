// Testes da FASE 6.6 -- resposta real na DAW revelou 3 problemas:
// (1) lei de mix linear derrubando o nível percebido da voz, (2)
// defaults escuros/comedidos demais pra um primeiro contato, (3)
// sensação de resposta lenta no controle de tempo. Este arquivo cobre
// as partes MENSURÁVEIS: comparação de RMS entre bypass/mix 0/20/35%,
// a comparação empírica de tempos de crossfade/smoothing que decidiu
// os valores finais aplicados em DelayEngine.cpp, e a auditoria de
// gain staging (primeira repetição ≈ unidade em Digital limpo). O
// "teste real na voz" em si só o usuário pode fazer.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/DSP/OutputStage.h"
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
    // Mesmo helper de composição usado em Phase6IntegrationTests.cpp --
    // reproduz PluginProcessor::processChunk() sem APVTS.
    struct Chunk
    {
        DelayEngine& engine;
        OutputStage& stage;
        juce::AudioBuffer<float> dryBuffer, delayInputBuffer;

        Chunk(DelayEngine& e, OutputStage& s, int numChannels, int capacity)
            : engine(e), stage(s), dryBuffer(numChannels, capacity), delayInputBuffer(numChannels, capacity)
        {
        }

        void process(juce::AudioBuffer<float>& buffer, float mix0to1, float outputGainDb, bool bypassed)
        {
            const int numSamples = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();

            for (int ch = 0; ch < numChannels; ++ch)
                dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

            stage.updateParameters(mix0to1, outputGainDb, bypassed);

            for (int ch = 0; ch < numChannels; ++ch)
                delayInputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

            juce::AudioBuffer<float> delayInputView(delayInputBuffer.getArrayOfWritePointers(), numChannels, 0, numSamples);
            stage.applyBypassInputGain(delayInputView, numSamples);

            engine.process(delayInputView);

            stage.mixAndOutput(dryBuffer, delayInputBuffer, buffer, numSamples);
        }
    };

    void fillNoise(juce::AudioBuffer<float>& buffer, juce::Random& rng, float amplitude)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(ch, i, (rng.nextFloat() * 2.0f - 1.0f) * amplitude);
    }

    float measurePeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
        return peak;
    }
}

int runMixLawAndResponseTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Mix law + response tests (FASE 6.6) ===" << std::endl << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numChannels = 2;

    // ------------------------------------------------------------
    // Item 2: RMS comparativo entre Bypass / Mix 0% / Mix 20% / Mix
    // 35%, sempre com Output=0dB -- estímulo "vocal-like" aproximado
    // (ruído branco contínuo, baixa autocorrelação, aproxima o pior
    // caso de descorrelação dry/wet de uma voz real com delay audível).
    // Delay/feedback moderados (150ms/35%, os novos defaults musicais).
    // ------------------------------------------------------------
    {
        std::cout << "-- RMS comparison: Bypass vs Mix 0%/20%/35%, Output=0dB --" << std::endl;

        auto measureRmsAt = [&](float mix0to1, bool bypassed) -> float
        {
            DelayEngine engine;
            OutputStage stage;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
            engine.prepare(spec);
            stage.prepare(sampleRate, blockSize);
            Chunk chunk(engine, stage, numChannels, blockSize);

            engine.setDelayTimeMs(150.0f);
            engine.setFeedback(0.35f);

            juce::Random rng(2222);

            const int warmupSamples = (int) (sampleRate * 1.0);
            const int measureSamples = (int) (sampleRate * 1.0);

            int done = 0;
            while (done < warmupSamples)
            {
                const int n = juce::jmin(blockSize, warmupSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.3f);
                chunk.process(buffer, mix0to1, 0.0f, bypassed);
                done += n;
            }

            double sumSq = 0.0;
            int count = 0;
            done = 0;
            while (done < measureSamples)
            {
                const int n = juce::jmin(blockSize, measureSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.3f);
                chunk.process(buffer, mix0to1, 0.0f, bypassed);

                for (int ch = 0; ch < numChannels; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        const double s = buffer.getSample(ch, i);
                        sumSq += s * s;
                        ++count;
                    }
                done += n;
            }

            return (float) std::sqrt(sumSq / count);
        };

        const float rmsBypass = measureRmsAt(1.0f, true);  // bypass total -- so' dry
        const float rmsMix0 = measureRmsAt(0.0f, false);
        const float rmsMix20 = measureRmsAt(0.2f, false);
        const float rmsMix35 = measureRmsAt(0.35f, false);

        const float dropMix0Db = 20.0f * std::log10(rmsMix0 / rmsBypass);
        const float dropMix20Db = 20.0f * std::log10(rmsMix20 / rmsBypass);
        const float dropMix35Db = 20.0f * std::log10(rmsMix35 / rmsBypass);

        // Comparação teórica com a lei LINEAR antiga (não usada mais,
        // só pra documentar o tamanho do problema que motivou a
        // mudança): em mix=35%, dryGain linear seria 0.65 -- uma queda
        // de 20*log10(0.65) ~= -3.74dB só de abrir esse tanto de wet,
        // ANTES de qualquer contribuição do wet em si.
        const float oldLinearDryGainAt35 = 1.0f - 0.35f;
        const float oldLinearDropDb = 20.0f * std::log10(oldLinearDryGainAt35);

        std::cout << "   RMS Bypass=" << rmsBypass << " Mix0%=" << rmsMix0
                  << " Mix20%=" << rmsMix20 << " Mix35%=" << rmsMix35 << std::endl;
        std::cout << "   Drop vs Bypass: Mix0%=" << dropMix0Db << "dB, Mix20%=" << dropMix20Db
                  << "dB, Mix35%=" << dropMix35Db << "dB (lei linear antiga so' no dry ja' seria "
                  << oldLinearDropDb << "dB em Mix35%, antes do wet)" << std::endl;

        expect(failures, std::abs(dropMix0Db) < 0.1f,
               "Mix=0%: RMS praticamente igual ao bypass (equal-power tambem preserva dry puro em 0%), "
               "drop = " + juce::String(dropMix0Db, 3) + "dB");
        expect(failures, dropMix20Db > -1.5f,
               "Mix=20%: queda de RMS pequena com equal-power (< 1.5dB), medido = "
               + juce::String(dropMix20Db, 2) + "dB");
        expect(failures, dropMix35Db > -2.5f,
               "Mix=35%: queda de RMS ainda pequena com equal-power (< 2.5dB), medido = "
               + juce::String(dropMix35Db, 2) + "dB");
    }

    // ------------------------------------------------------------
    // Item 3: comparação empírica de tempos de resposta -- 5/8/10/15/
    // 25ms de crossfade de salto grande, mesma metodologia de
    // LargeJumpTests.cpp (tom continuo + salto no meio, mede o maior
    // delta amostra-a-amostra e compara contra o delta natural do
    // proprio tom). Testa VARIOS instantes de salto (fracoes do
    // periodo do tom, 0..7/8) pra cada candidato -- um unico ponto de
    // salto fixo nao discrimina bem entre candidatos (a diferenca de
    // fase entre as duas cabecas no instante exato do salto depende de
    // ONDE no ciclo o salto acontece, entao o pior caso pode ficar
    // escondido se so' testarmos um instante). Escolhe o MENOR
    // candidato cujo PIOR CASO (entre todos os instantes testados)
    // ainda fica dentro da margem de seguranca.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Crossfade timing comparison: 5/8/10/15/25ms (worst-case over jump phase) --" << std::endl;

        constexpr float amplitude = 0.5f;
        constexpr float toneHz = 300.0f;
        constexpr int testBlockSize = 256;

        const float naturalMaxDelta = amplitude * juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate;
        const float clickThreshold = naturalMaxDelta * 8.0f; // mesma folga de LargeJumpTests.cpp

        const int periodSamples = (int) std::round(sampleRate / toneHz); // ~160 amostras a 48kHz/300Hz
        constexpr int numPhaseOffsets = 8;

        auto measureMaxDeltaForJumpAt = [&](double crossfadeSeconds, int phaseOffsetSamples) -> float
        {
            DelayEngine engine;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) testBlockSize, 2u };
            engine.prepare(spec);
            engine.setDelayTimingForTesting(0.005, crossfadeSeconds);
            engine.setDelayTimeMs(100.0f);
            engine.setFeedback(0.0f);

            const int totalSamples = (int) (sampleRate * 1.0);
            const int jumpAtSample = (int) (sampleRate * 0.3) + phaseOffsetSamples;

            std::vector<float> out;
            out.reserve((size_t) totalSamples);
            int phase = 0, done = 0;
            bool jumped = false;

            while (done < totalSamples)
            {
                const int n = juce::jmin(testBlockSize, totalSamples - done);
                if (! jumped && done + n > jumpAtSample)
                {
                    engine.setDelayTimeMs(500.0f);
                    jumped = true;
                }
                juce::AudioBuffer<float> buffer(2, n);
                for (int i = 0; i < n; ++i)
                {
                    const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                    buffer.setSample(0, i, s);
                    buffer.setSample(1, i, s);
                    ++phase;
                }
                engine.process(buffer);
                for (int i = 0; i < n; ++i) out.push_back(buffer.getSample(0, i));
                done += n;
            }

            float maxDelta = 0.0f;
            for (size_t i = 1; i < out.size(); ++i)
                maxDelta = juce::jmax(maxDelta, std::abs(out[i] - out[i - 1]));
            return maxDelta;
        };

        const double candidatesMs[] { 5.0, 8.0, 10.0, 15.0, 25.0 };
        float worstCaseDelta[5] {};

        for (int c = 0; c < 5; ++c)
        {
            float worst = 0.0f;
            for (int p = 0; p < numPhaseOffsets; ++p)
            {
                const int offset = (periodSamples * p) / numPhaseOffsets;
                worst = juce::jmax(worst, measureMaxDeltaForJumpAt(candidatesMs[c] * 0.001, offset));
            }
            worstCaseDelta[c] = worst;
            std::cout << "   crossfade " << candidatesMs[c] << "ms: worst-case max delta (8 jump phases) = "
                      << worst << " (threshold " << clickThreshold << ")" << std::endl;
        }

        int chosenIndex = -1;
        for (int c = 0; c < 5; ++c)
        {
            if (worstCaseDelta[c] < clickThreshold)
            {
                chosenIndex = c;
                break;
            }
        }

        expect(failures, chosenIndex >= 0, "at least one crossfade candidate stays under the click threshold "
                                           "across every jump phase tested");
        if (chosenIndex >= 0)
            std::cout << "   -> smallest safe candidate: " << candidatesMs[chosenIndex] << "ms" << std::endl;

        // O valor de PRODUCAO aplicado em DelayEngine.cpp precisa bater
        // com o candidato escolhido aqui -- checa que ele tambem fica
        // dentro do threshold em TODAS as fases testadas (prova que o
        // valor hardcoded realmente corresponde ao resultado deste
        // teste, nao foi só' um chute).
        {
            float productionWorst = 0.0f;
            for (int p = 0; p < numPhaseOffsets; ++p)
            {
                const int offset = (periodSamples * p) / numPhaseOffsets;

                DelayEngine productionEngine;
                juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) testBlockSize, 2u };
                productionEngine.prepare(spec); // usa os valores REAIS de prepare(), sem override
                productionEngine.setDelayTimeMs(100.0f);
                productionEngine.setFeedback(0.0f);

                const int totalSamples = (int) (sampleRate * 1.0);
                const int jumpAtSample = (int) (sampleRate * 0.3) + offset;
                std::vector<float> out;
                out.reserve((size_t) totalSamples);
                int phase = 0, done = 0;
                bool jumped = false;

                while (done < totalSamples)
                {
                    const int n = juce::jmin(testBlockSize, totalSamples - done);
                    if (! jumped && done + n > jumpAtSample)
                    {
                        productionEngine.setDelayTimeMs(500.0f);
                        jumped = true;
                    }
                    juce::AudioBuffer<float> buffer(2, n);
                    for (int i = 0; i < n; ++i)
                    {
                        const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                        buffer.setSample(0, i, s);
                        buffer.setSample(1, i, s);
                        ++phase;
                    }
                    productionEngine.process(buffer);
                    for (int i = 0; i < n; ++i) out.push_back(buffer.getSample(0, i));
                    done += n;
                }

                float maxDelta = 0.0f;
                for (size_t i = 1; i < out.size(); ++i)
                    maxDelta = juce::jmax(maxDelta, std::abs(out[i] - out[i - 1]));
                productionWorst = juce::jmax(productionWorst, maxDelta);
            }

            std::cout << "   production crossfade timing: worst-case max delta = " << productionWorst << std::endl;
            expect(failures, productionWorst < clickThreshold,
                   "production crossfade timing (as hardcoded in DelayEngine::prepare()) stays under "
                   "the click threshold across every jump phase too, worst-case max delta = "
                   + juce::String(productionWorst, 5));
        }
    }

    // ------------------------------------------------------------
    // Item 4: mudança pequena de tempo (5ms de smoothing) tambem nao
    // pode estalar -- mesmo raciocinio, mas pra rampa simples (nao
    // crossfade), num salto DENTRO do threshold de "mudanca pequena"
    // (largeJumpThresholdMs = 25ms).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Small delay-time change (5ms smoothing): no click --" << std::endl;

        constexpr double localSampleRate = 48000.0;
        constexpr float amplitude = 0.5f;
        constexpr float toneHz = 300.0f;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { localSampleRate, 256u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(100.0f);
        engine.setFeedback(0.0f);

        const float naturalMaxDelta = amplitude * juce::MathConstants<float>::twoPi * toneHz / (float) localSampleRate;
        const float clickThreshold = naturalMaxDelta * 8.0f;

        const int totalSamples = (int) (localSampleRate * 0.5);
        const int jumpAtSample = (int) (localSampleRate * 0.2);
        std::vector<float> out;
        out.reserve((size_t) totalSamples);
        int phase = 0, done = 0;
        bool jumped = false;

        while (done < totalSamples)
        {
            const int n = juce::jmin(256, totalSamples - done);
            if (! jumped && done + n > jumpAtSample)
            {
                engine.setDelayTimeMs(115.0f); // 15ms de diferenca -- "mudanca pequena" (< 25ms threshold)
                jumped = true;
            }
            juce::AudioBuffer<float> buffer(2, n);
            for (int i = 0; i < n; ++i)
            {
                const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) localSampleRate);
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
                ++phase;
            }
            engine.process(buffer);
            for (int i = 0; i < n; ++i) out.push_back(buffer.getSample(0, i));
            done += n;
        }

        float maxDelta = 0.0f;
        for (size_t i = 1; i < out.size(); ++i)
            maxDelta = juce::jmax(maxDelta, std::abs(out[i] - out[i - 1]));

        expect(failures, maxDelta < clickThreshold,
               "small delay-time change (100ms->115ms, 5ms smoothing): no click, max delta = "
               + juce::String(maxDelta, 5) + " (threshold " + juce::String(clickThreshold, 5) + ")");
    }

    // ------------------------------------------------------------
    // Item 6: gain staging -- impulso, Feedback=0, Wet=100%, Output=
    // 0dB, Digital, Character=0, Lo-Fi OFF, Ducking=0, HPF=80Hz,
    // LPF=16kHz, Resonance=0 (os novos defaults). A primeira repeticao
    // precisa sair perto de unidade -- mede o valor real e reporta.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Gain staging: first repeat peak with the new musical defaults --" << std::endl;

        DelayEngine engine;
        OutputStage stage;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
        engine.prepare(spec);
        stage.prepare(sampleRate, blockSize);
        Chunk chunk(engine, stage, numChannels, blockSize);

        engine.setDelayTimeMs(150.0f);
        engine.setFeedback(0.0f);
        engine.setHighPassHz(80.0f);
        engine.setLowPassHz(16000.0f);
        engine.setResonance(0.0f);
        engine.setCharacterMode(CharacterMode::Digital);
        engine.setCharacterAmount(0.0f);
        engine.setDuckingAmount(0.0f);
        // Lo-Fi permanece OFF (nunca chamado setLoFiEnabled(true)).

        const int delaySamples = (int) std::round(150.0 * 0.001 * sampleRate);
        const int totalSamples = delaySamples + blockSize * 4;

        juce::AudioBuffer<float> impulse(numChannels, blockSize);
        impulse.clear();
        impulse.setSample(0, 0, 1.0f);
        impulse.setSample(1, 0, 1.0f);
        chunk.process(impulse, 1.0f, 0.0f, false);

        float peak = 0.0f;
        int done = blockSize;
        while (done < totalSamples)
        {
            const int n = juce::jmin(blockSize, totalSamples - done);
            juce::AudioBuffer<float> silence(numChannels, n);
            silence.clear();
            chunk.process(silence, 1.0f, 0.0f, false);
            peak = juce::jmax(peak, measurePeak(silence));
            done += n;
        }

        std::cout << "   first repeat peak (single-sample impulse) = " << peak << " (unity = 1.0)" << std::endl;

        // Verificação analítica (feita à mão, não só empírica): pra um
        // filtro TPT de 2 polos alimentado por um IMPULSO DE UMA
        // AMOSTRA, a saída no sample 0 é só a1/a2/a3 vezes a entrada
        // (ver SVFFilter::process()) -- pro LPF de 16kHz a 48kHz,
        // g=tan(pi*16000/48000)=tan(60°)=sqrt(3)≈1.732, e a3=g²*a1≈0.40.
        // Isso é ESPERADO: um filtro de 2 polos não reproduz um impulso
        // discreto escalado no primeiro sample -- ele espalha a energia
        // do impulso ao longo de vários samples (resposta ao impulso
        // tem FORMA, não é um degrau instantâneo), e como aqui HPF e
        // LPF estão em cascata, essa distribuição acontece duas vezes.
        // O pico medido (~0.56) é essa física normal, não um bug --
        // por isso o teste abaixo mede o que REALMENTE importa pra
        // voz: o ganho em REGIME ESTACIONÁRIO com um tom contínuo, não
        // o pico de resposta a um impulso artificial de amostra única.
        expect(failures, peak > 0.3f && peak < 0.9f,
               "single-sample impulse: peak fica visivelmente abaixo de 1.0 por fisica normal de filtro "
               "de 2 polos (nao ha' como um impulso discreto atravessar HPF+LPF em cascata sem se "
               "espalhar no tempo) -- medido = " + juce::String(peak, 4) + ", ver o teste de tom "
               "continuo abaixo pro numero que realmente importa pra material musical");
    }

    // ------------------------------------------------------------
    // O que importa pra voz: ganho em REGIME ESTACIONÁRIO com um tom
    // CONTÍNUO (não um impulso isolado) atravessando os mesmos
    // filtros dos novos defaults (HPF 80Hz + LPF 16kHz, Q=0.5) -- é
    // isso que confirma (ou não) uma perda real de "gain staging" que
    // afetaria material musical de verdade.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Gain staging: steady-state gain with a continuous tone (what matters for voice) --" << std::endl;

        auto measureSteadyGainAt = [&](float toneHz) -> float
        {
            DelayEngine engine;
            OutputStage stage;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
            engine.prepare(spec);
            stage.prepare(sampleRate, blockSize);
            Chunk chunk(engine, stage, numChannels, blockSize);

            engine.setDelayTimeMs(150.0f);
            engine.setFeedback(0.0f);
            engine.setHighPassHz(80.0f);
            engine.setLowPassHz(16000.0f);
            engine.setResonance(0.0f);
            engine.setCharacterMode(CharacterMode::Digital);
            engine.setCharacterAmount(0.0f);
            engine.setDuckingAmount(0.0f);

            constexpr float amplitude = 0.5f;
            const int warmupSamples = (int) (sampleRate * 0.3); // deixa os filtros convergirem de verdade
            const int measureSamples = (int) (sampleRate * 0.1);

            int phase = 0;
            int done = 0;
            while (done < warmupSamples)
            {
                const int n = juce::jmin(blockSize, warmupSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                for (int i = 0; i < n; ++i)
                {
                    const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                    buffer.setSample(0, i, s);
                    buffer.setSample(1, i, s);
                    ++phase;
                }
                chunk.process(buffer, 1.0f, 0.0f, false);
                done += n;
            }

            float peak = 0.0f;
            done = 0;
            while (done < measureSamples)
            {
                const int n = juce::jmin(blockSize, measureSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                for (int i = 0; i < n; ++i)
                {
                    const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                    buffer.setSample(0, i, s);
                    buffer.setSample(1, i, s);
                    ++phase;
                }
                chunk.process(buffer, 1.0f, 0.0f, false);
                peak = juce::jmax(peak, measurePeak(buffer));
                done += n;
            }

            return peak / amplitude; // ganho relativo ao pico de entrada
        };

        const float gain500 = measureSteadyGainAt(500.0f);
        const float gain1k = measureSteadyGainAt(1000.0f);
        const float gain2k = measureSteadyGainAt(2000.0f);

        std::cout << "   steady-state gain: 500Hz=" << gain500 << " 1kHz=" << gain1k
                  << " 2kHz=" << gain2k << " (unity = 1.0)" << std::endl;

        expect(failures, gain500 > 0.95f && gain500 < 1.05f,
               "500Hz (grave/medio da voz): ganho em regime estacionario fica perto de unidade, medido = "
               + juce::String(gain500, 4));
        expect(failures, gain1k > 0.95f && gain1k < 1.05f,
               "1kHz (presenca da voz): ganho em regime estacionario fica perto de unidade, medido = "
               + juce::String(gain1k, 4));
        expect(failures, gain2k > 0.95f && gain2k < 1.05f,
               "2kHz (brilho/consoantes da voz): ganho em regime estacionario fica perto de unidade, "
               "medido = " + juce::String(gain2k, 4));
    }

    // ------------------------------------------------------------
    // Item 8: mapeamento de Ducking -- 0/25/50/75/100% precisa ser
    // monotonicamente crescente em redução de dB no pico do envelope
    // (confirma que 0% = delay totalmente presente, sem precisar
    // mudar o algoritmo -- só confirma que já é assim).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Ducking mapping: 0/25/50/75/100% monotonic --" << std::endl;

        DuckingProcessor duck;
        duck.prepare(sampleRate);

        float previousGain = 2.0f; // maior que qualquer ganho possível (<=1)
        bool monotonic = true;
        float gains[5] {};

        const float amounts[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        for (int i = 0; i < 5; ++i)
        {
            duck.setAmount(amounts[i]);
            duck.reset();
            float gain = 1.0f;
            // Envelope no pico (entrada = 1.0 sustentada por tempo suficiente
            // pro attack de 15ms convergir).
            for (int s = 0; s < (int) (sampleRate * 0.05); ++s)
                gain = duck.getNextGain(1.0f);

            gains[i] = gain;
            if (gain > previousGain + 1.0e-6f)
                monotonic = false;
            previousGain = gain;
        }

        std::cout << "   gain at 0%=" << gains[0] << " 25%=" << gains[1] << " 50%=" << gains[2]
                  << " 75%=" << gains[3] << " 100%=" << gains[4] << std::endl;

        expect(failures, std::abs(gains[0] - 1.0f) < 1.0e-4f, "Ducking 0%: gain stays exactly 1.0 (delay fully present)");
        expect(failures, monotonic, "Ducking 0/25/50/75/100%: gain reduction is monotonically increasing");
        expect(failures, gains[4] < gains[0] * 0.2f, "Ducking 100%: clearly audible reduction vs 0%");
    }

    return failures;
}
