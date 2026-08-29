// Testes de integração da FASE 6 -- DelayEngine + OutputStage juntos,
// reproduzindo exatamente a sequência de PluginProcessor::processChunk()
// (captura de dry -> applyBypassInputGain -> delayEngine.process() ->
// mixAndOutput()), mas sem APVTS/host, pra poder ser um executável de
// console (mesmo padrão de SyncIntegrationTests.cpp na FASE 3). Cobre
// os itens 8, 10, 11 e 12 do briefing da FASE 6, que exigem o motor
// completo (não dá pra provar decaimento de tail, "sem explosão ao
// desligar bypass" ou compatibilidade de nível entre modos só com
// OutputStage isolado -- ver OutputStageTests.cpp pros testes
// determinísticos de mix/output/bypass isolados).
//
// NOTA: strings passadas pra expect()/juce::String() ficam em inglês
// (ASCII puro), mesmo padrão do resto da suite -- ver nota equivalente
// no topo de OutputStageTests.cpp.
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
    // Reproduz PluginProcessor::processChunk() sem APVTS -- os
    // parâmetros do DelayEngine são configurados externamente pelo
    // chamador (delayEngine.setXXX()) antes de chamar processChunk();
    // este helper só cuida da parte de dry/wet/output/bypass (Seções
    // 1-7 do briefing da FASE 6).
    struct Phase6Chunk
    {
        DelayEngine& engine;
        OutputStage& stage;
        juce::AudioBuffer<float> dryBuffer, delayInputBuffer;

        Phase6Chunk(DelayEngine& e, OutputStage& s, int numChannels, int capacity)
            : engine(e), stage(s), dryBuffer(numChannels, capacity), delayInputBuffer(numChannels, capacity)
        {
        }

        // buffer é entrada E saída (in-place, como processBlock).
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

    // Tom continuo (não ruído) -- usado especificamente no teste de
    // "sem glitch/zipper" (item 8): ruído branco não tem nenhuma
    // continuidade amostra-a-amostra por natureza (duas amostras
    // consecutivas de ruído independente já podem saltar quase 2x a
    // amplitude sozinhas), então comparar deltas amostra-a-amostra
    // com ruído como estímulo não prova nada sobre a suavidade da
    // automação -- só mediria a aleatoriedade do próprio ruído. Um
    // tom senoidal contínuo tem derivada pequena e conhecida por
    // construção, isolando de verdade qualquer salto real introduzido
    // pela automação de mix/output/bypass.
    struct SineGenerator
    {
        float phase = 0.0f;
        float phaseIncrement = 0.0f;

        void prepare(double sampleRate, float freqHz) noexcept
        {
            phaseIncrement = (float) (freqHz / sampleRate);
        }

        void fill(juce::AudioBuffer<float>& buffer, float amplitude) noexcept
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * phase);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.setSample(ch, i, s);

                phase += phaseIncrement;
                if (phase >= 1.0f)
                    phase -= 1.0f;
            }
        }
    };

    float measurePeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
        return peak;
    }
}

int runPhase6IntegrationTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Phase 6 integration tests (Dry/Wet + Output + Bypass, full engine) ===" << std::endl << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numChannels = 2;

    // ------------------------------------------------------------
    // Item 10: parâmetros extremos (Feedback95%, LoFi ON, Tape,
    // Character100%, Mix100%, Output+18dB) -- depois que o crossfade
    // de bypass termina, output == dry exato, e Output+18dB não
    // continua amplificando.
    // ------------------------------------------------------------
    {
        std::cout << "-- Bypass with extreme parameters (full engine) --" << std::endl;

        DelayEngine engine;
        OutputStage stage;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
        engine.prepare(spec);
        stage.prepare(sampleRate, blockSize);

        Phase6Chunk chunk(engine, stage, numChannels, blockSize);

        engine.setDelayTimeMs(90.0f);
        engine.setFeedback(0.95f);
        engine.setPingPong(true);
        engine.setLoFiEnabled(true);
        engine.setCharacterMode(CharacterMode::Tape);
        engine.setCharacterAmount(1.0f);

        juce::Random rng(2026);

        // Alimenta 2s de ruído contínuo pra construir um tail de
        // feedback de verdade antes de bypassar.
        {
            int done = 0;
            const int total = (int) (sampleRate * 2.0);
            while (done < total)
            {
                const int n = juce::jmin(blockSize, total - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.5f);
                chunk.process(buffer, 1.0f, 18.0f, false);
                done += n;
            }
        }

        // Engaja bypass e deixa convergir totalmente (tail decai +
        // crossfade de bypass completa) -- generoso: 3s.
        {
            int done = 0;
            const int total = (int) (sampleRate * 3.0);
            while (done < total)
            {
                const int n = juce::jmin(blockSize, total - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.5f);
                chunk.process(buffer, 1.0f, 18.0f, true);
                done += n;
            }
        }

        // Um bloco final -- silêncio na entrada, pra isolar o que sai
        // sendo puramente "dry == input deste bloco".
        juce::AudioBuffer<float> finalBlock(numChannels, blockSize);
        fillNoise(finalBlock, rng, 0.5f);
        juce::AudioBuffer<float> dryCopy;
        dryCopy.makeCopyOf(finalBlock);

        chunk.process(finalBlock, 1.0f, 18.0f, true);

        bool allExactDry = true;
        bool allFinite = true;
        float maxDeviation = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float s = finalBlock.getSample(ch, i);
                if (std::isnan(s) || std::isinf(s)) allFinite = false;
                const float deviation = std::abs(s - dryCopy.getSample(ch, i));
                maxDeviation = juce::jmax(maxDeviation, deviation);
                if (deviation > 1.0e-3f)
                    allExactDry = false;
            }
        }

        expect(failures, allFinite, "extreme bypass: output stays finite (no NaN/Inf)");
        expect(failures, allExactDry, "extreme bypass (Feedback95%+LoFi+Tape+Character100%+Mix100%+Output+18dB): "
                                      "after convergence, output equals dry exactly, max deviation = "
                                      + juce::String(maxDeviation, 6));
    }

    // ------------------------------------------------------------
    // Item 11: teste de tail -- impulso com feedback alto, engaja
    // bypass, confirma: (a) sem estalo na transição, (b) o tail
    // continua decaindo internamente (não é resetado), (c) entrada
    // nova não se acumula na delay line enquanto bypassado, (d)
    // desligar bypass depois não causa "explosão" de repeats
    // acumulados.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Tail test: impulse + high feedback + bypass --" << std::endl;

        DelayEngine engine;
        OutputStage stage;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
        engine.prepare(spec);
        stage.prepare(sampleRate, blockSize);

        Phase6Chunk chunk(engine, stage, numChannels, blockSize);

        constexpr float delayMs = 120.0f;
        constexpr float feedback = 0.9f;
        engine.setDelayTimeMs(delayMs);
        engine.setFeedback(feedback);
        engine.setPingPong(false);

        // Impulso único, Mix=100% pra ver o tail claramente na saída.
        {
            juce::AudioBuffer<float> impulse(numChannels, blockSize);
            impulse.clear();
            impulse.setSample(0, 0, 1.0f);
            impulse.setSample(1, 0, 1.0f);
            chunk.process(impulse, 1.0f, 0.0f, false);
        }

        // Deixa alguns repeats acontecerem antes de bypassar (silêncio
        // na entrada -- só o tail do impulso tocando).
        for (int b = 0; b < 6; ++b)
        {
            juce::AudioBuffer<float> silence(numChannels, blockSize);
            silence.clear();
            chunk.process(silence, 1.0f, 0.0f, false);
        }

        // (a) Sem estalo: engaja bypass e mede o salto amostra-a-
        // amostra bem na transição -- um bug abrupto ("if bypass
        // return") produziria um degrau enorme aqui; o crossfade
        // suave (15ms) não deve.
        float maxJumpAtTransition = 0.0f;
        {
            juce::AudioBuffer<float> lastPlaying(numChannels, blockSize);
            lastPlaying.clear();
            chunk.process(lastPlaying, 1.0f, 0.0f, false);
            const float lastSampleBeforeBypass = lastPlaying.getSample(0, blockSize - 1);

            juce::AudioBuffer<float> silence(numChannels, blockSize);
            silence.clear();
            chunk.process(silence, 1.0f, 0.0f, true); // primeiro bloco JÁ bypassado
            const float firstSampleAfterBypass = silence.getSample(0, 0);

            maxJumpAtTransition = std::abs(firstSampleAfterBypass - lastSampleBeforeBypass);
        }
        expect(failures, maxJumpAtTransition < 0.05f,
               "bypass transition: no click (sample-to-sample jump = "
               + juce::String(maxJumpAtTransition, 6) + ", expected << 0.05)");

        // (b) O tail continua decaindo internamente enquanto bypassado
        // -- observa o WET puro chamando engine.process() direto num
        // buffer zerado (exatamente o que o motor recebe quando
        // applyBypassInputGain já convergiu pra ~0). Um impulso ÚNICO
        // produz ecos ESPAÇADOS por delaySamples (~5760 amostras aqui,
        // bem maior que um bloco de 512) -- medir RMS/pico bloco a
        // bloco (512 amostras) seria enganoso perto de um eco isolado
        // (picos bem localizados, não uma queda suave bloco a bloco).
        // Em vez disso, mede o PICO em janelas contíguas e não-
        // sobrepostas MAIORES que um período de delay inteiro -- isso
        // garante que cada janela captura pelo menos um eco completo,
        // e como os ecos só ficam menores ao longo do tempo (fator
        // feedback a cada volta), o pico de uma janela mais tardia
        // nunca pode superar o de uma janela mais cedo, com qualquer
        // alinhamento de fase.
        std::vector<float> windowPeaks;
        {
            const int delaySamples = (int) std::ceil((double) delayMs * 0.001 * sampleRate);
            const int windowSamples = delaySamples + blockSize * 2; // > 1 periodo inteiro, com folga
            // 10 janelas -- com feedback=0.9, decaimento esperado por
            // janela e' 0.9^10 ~= 0.35 do pico inicial, bem mais folgado
            // que o limiar de "caiu abaixo de 50%" abaixo (6 janelas
            // davam 0.9^6 ~= 0.53, perto demais da borda do limiar).
            constexpr int numWindows = 10;

            for (int w = 0; w < numWindows; ++w)
            {
                float windowPeak = 0.0f;
                int done = 0;
                while (done < windowSamples)
                {
                    const int n = juce::jmin(blockSize, windowSamples - done);
                    juce::AudioBuffer<float> zero(numChannels, n);
                    zero.clear();
                    engine.process(zero);
                    windowPeak = juce::jmax(windowPeak, measurePeak(zero));
                    done += n;
                }
                windowPeaks.push_back(windowPeak);
            }
        }

        bool monotonicNonIncreasing = true;
        for (size_t i = 1; i < windowPeaks.size(); ++i)
            if (windowPeaks[i] > windowPeaks[i - 1] * 1.05f + 1.0e-6f)
                monotonicNonIncreasing = false;

        expect(failures, monotonicNonIncreasing, "internal tail decays monotonically window-over-window while "
                                                 "bypassed (window peaks: first="
                                                 + juce::String(windowPeaks.front(), 6) + ", last="
                                                 + juce::String(windowPeaks.back(), 6) + ")");
        expect(failures, windowPeaks.back() < windowPeaks.front() * 0.5f + 1.0e-6f,
               "internal tail has clearly decayed after several windows (not stuck), first="
               + juce::String(windowPeaks.front(), 6) + ", last=" + juce::String(windowPeaks.back(), 6));

        // (c) Entrada nova não se acumula na delay line enquanto
        // bypassado: injeta um impulso "durante" o bypass (já
        // convergido, applyBypassInputGain ~0) e compara o estado
        // INTERNO do motor (engine.process() direto, não a saída de
        // mixAndOutput -- que por design mostra dry puro em bypass
        // total, então comparar SAÍDAS não provaria nada aqui: a
        // saída em bypass é dry por construção, injetar um impulso ali
        // SEMPRE aparece na saída, isso é o comportamento correto de
        // bypass, não um vazamento) com uma corrida de controle SEM
        // esse impulso extra -- devem ser praticamente idênticas, o
        // que prova que o impulso injetado nunca chegou a entrar na
        // delay line.
        {
            DelayEngine engineControl;
            OutputStage stageControl;
            engineControl.prepare(spec);
            stageControl.prepare(sampleRate, blockSize);
            Phase6Chunk chunkControl(engineControl, stageControl, numChannels, blockSize);
            engineControl.setDelayTimeMs(delayMs);
            engineControl.setFeedback(feedback);
            engineControl.setPingPong(false);

            DelayEngine engineWithExtra;
            OutputStage stageWithExtra;
            engineWithExtra.prepare(spec);
            stageWithExtra.prepare(sampleRate, blockSize);
            Phase6Chunk chunkWithExtra(engineWithExtra, stageWithExtra, numChannels, blockSize);
            engineWithExtra.setDelayTimeMs(delayMs);
            engineWithExtra.setFeedback(feedback);
            engineWithExtra.setPingPong(false);

            // Histórico idêntico (impulso original + alguns repeats +
            // bypass convergindo totalmente) nas duas instâncias, em
            // paralelo -- determinístico, então as duas ficam
            // byte-idênticas até o ponto de divergência abaixo.
            auto replicateHistory = [&](Phase6Chunk& c)
            {
                juce::AudioBuffer<float> imp(numChannels, blockSize);
                imp.clear();
                imp.setSample(0, 0, 1.0f);
                imp.setSample(1, 0, 1.0f);
                c.process(imp, 1.0f, 0.0f, false);

                for (int b = 0; b < 6; ++b)
                {
                    juce::AudioBuffer<float> s(numChannels, blockSize);
                    s.clear();
                    c.process(s, 1.0f, 0.0f, false);
                }
                for (int b = 0; b < 30; ++b) // bypass convergindo totalmente (input gain E output mix)
                {
                    juce::AudioBuffer<float> s(numChannels, blockSize);
                    s.clear();
                    c.process(s, 1.0f, 0.0f, true);
                }
            };

            replicateHistory(chunkControl);
            replicateHistory(chunkWithExtra);

            // Injeta um impulso "durante" o bypass (já convergido) só
            // na instância "with extra", através do pipeline completo
            // (Phase6Chunk::process(), como um host de verdade faria).
            juce::AudioBuffer<float> extraImpulse(numChannels, blockSize);
            extraImpulse.clear();
            extraImpulse.setSample(0, 10, 1.0f);
            extraImpulse.setSample(1, 10, 1.0f);
            juce::AudioBuffer<float> extraImpulseDryCopy;
            extraImpulseDryCopy.makeCopyOf(extraImpulse);
            chunkWithExtra.process(extraImpulse, 1.0f, 0.0f, true);

            // Sanity check do design (não é o teste principal): em
            // bypass total, a saída É o dry -- confirma que o pipeline
            // se comporta como esperado (bypass = passagem direta),
            // não como um vazamento pro motor.
            float maxDryPassthroughDiff = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    maxDryPassthroughDiff = juce::jmax(maxDryPassthroughDiff,
                        std::abs(extraImpulse.getSample(ch, i) - extraImpulseDryCopy.getSample(ch, i)));
            expect(failures, maxDryPassthroughDiff < 1.0e-3f,
                   "sanity check: while fully bypassed, output is the direct dry pass-through of new input "
                   "(this is correct bypass behaviour, not a leak) -- diff = "
                   + juce::String(maxDryPassthroughDiff, 6));

            juce::AudioBuffer<float> extraSilence(numChannels, blockSize);
            extraSilence.clear();
            chunkControl.process(extraSilence, 1.0f, 0.0f, true);

            // O teste real: continua rodando as DUAS instâncias com o
            // motor chamado DIRETAMENTE (contorna o dry-passthrough de
            // mixAndOutput, que mascararia um eventual vazamento) por
            // mais de um período de delay inteiro, e compara os
            // traços internos -- se o impulso extra tivesse mesmo
            // entrado na delay line, apareceria como um eco em
            // engineWithExtra ausente em engineControl.
            const int delaySamples = (int) std::ceil((double) delayMs * 0.001 * sampleRate);
            const int probeSamples = delaySamples + blockSize * 4;

            float maxInternalDiff = 0.0f;
            int done = 0;
            while (done < probeSamples)
            {
                const int n = juce::jmin(blockSize, probeSamples - done);
                juce::AudioBuffer<float> zeroControl(numChannels, n), zeroExtra(numChannels, n);
                zeroControl.clear();
                zeroExtra.clear();
                engineControl.process(zeroControl);
                engineWithExtra.process(zeroExtra);

                for (int ch = 0; ch < numChannels; ++ch)
                    for (int i = 0; i < n; ++i)
                        maxInternalDiff = juce::jmax(maxInternalDiff,
                            std::abs(zeroControl.getSample(ch, i) - zeroExtra.getSample(ch, i)));

                done += n;
            }

            expect(failures, maxInternalDiff < 1.0e-3f,
                   "new input injected DURING bypass never enters the delay line: internal engine state stays "
                   "(nearly) identical to a control run without that extra input over a full delay period, "
                   "max internal diff = " + juce::String(maxInternalDiff, 6));

            // (d) Desliga bypass depois de um longo período bypassado
            // (tail já totalmente decaído) -- não pode haver "explosão"
            // de repeats acumulados: o pico ao desligar deve ficar
            // limitado, não um salto muito acima do regime normal.
            for (int b = 0; b < 20; ++b) // mais tempo bypassado, decaindo tudo
            {
                juce::AudioBuffer<float> s(numChannels, blockSize);
                s.clear();
                chunkControl.process(s, 1.0f, 0.0f, true);
            }

            juce::Random unbypassRng(4242);
            float peakAfterUnbypass = 0.0f;
            for (int b = 0; b < 10; ++b)
            {
                juce::AudioBuffer<float> stimulus(numChannels, blockSize);
                fillNoise(stimulus, unbypassRng, 0.3f);
                chunkControl.process(stimulus, 1.0f, 0.0f, false); // desliga bypass
                peakAfterUnbypass = juce::jmax(peakAfterUnbypass, measurePeak(stimulus));
            }

            expect(failures, peakAfterUnbypass < 2.0f, "disabling bypass after the tail has decayed: no explosion "
                                                       "of accumulated repeats, peak = "
                                                       + juce::String(peakAfterUnbypass, 4));
        }
    }

    // ------------------------------------------------------------
    // Item 12: compatibilidade de nível entre modos -- Character=100%,
    // Mix=100%, material representativo (ruído contínuo), confere que
    // o makeup gain (wet-only) reduz a diferença de RMS entre
    // Digital/Analog/Tape em relação aos valores medidos na FASE 5 SEM
    // compensação (~-3.84dB Analog, ~-6.49dB Tape), sem eliminá-la
    // totalmente (a diferença musical entre os modos é intencional).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Level compatibility across modes (Character 100%, with makeup gain) --" << std::endl;

        auto measureModeRms = [&](CharacterMode mode) -> float
        {
            DelayEngine engine;
            OutputStage stage;
            juce::dsp::ProcessSpec modeSpec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
            engine.prepare(modeSpec);
            stage.prepare(sampleRate, blockSize);
            Phase6Chunk chunk(engine, stage, numChannels, blockSize);

            engine.setDelayTimeMs(150.0f);
            engine.setFeedback(0.5f);
            engine.setCharacterMode(mode);
            engine.setCharacterAmount(1.0f);

            juce::Random rng(9000 + (int) mode);

            // Descarta o início (convergência de smoothing + delay
            // preenchendo) e mede RMS num trecho estável.
            const int warmupSamples = (int) (sampleRate * 1.0);
            const int measureSamples = (int) (sampleRate * 1.0);

            int done = 0;
            while (done < warmupSamples)
            {
                const int n = juce::jmin(blockSize, warmupSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.4f);
                chunk.process(buffer, 1.0f, 0.0f, false);
                done += n;
            }

            double sumSq = 0.0;
            int count = 0;
            done = 0;
            while (done < measureSamples)
            {
                const int n = juce::jmin(blockSize, measureSamples - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.4f);
                chunk.process(buffer, 1.0f, 0.0f, false);

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

        const float rmsDigital = measureModeRms(CharacterMode::Digital);
        const float rmsAnalog = measureModeRms(CharacterMode::Analog);
        const float rmsTape = measureModeRms(CharacterMode::Tape);

        const float dropAnalogDb = 20.0f * std::log10(rmsAnalog / rmsDigital);
        const float dropTapeDb = 20.0f * std::log10(rmsTape / rmsDigital);

        std::cout << "   RMS Digital=" << rmsDigital << " Analog=" << rmsAnalog << " Tape=" << rmsTape << std::endl;
        std::cout << "   Drop vs Digital: Analog=" << dropAnalogDb << "dB, Tape=" << dropTapeDb << "dB "
                     "(uncompensated in FASE 5 it was ~-3.84dB and ~-6.49dB)" << std::endl;

        expect(failures, dropAnalogDb > -3.0f, "Analog with makeup gain: smaller drop than the uncompensated "
                                               "-3.84dB (measured: " + juce::String(dropAnalogDb, 2) + "dB)");
        expect(failures, dropTapeDb > -5.0f, "Tape with makeup gain: smaller drop than the uncompensated "
                                             "-6.49dB (measured: " + juce::String(dropTapeDb, 2) + "dB)");
        expect(failures, dropAnalogDb < -0.1f, "Analog still has a real musical difference vs Digital "
                                               "(compensation is conservative, not a 100% match)");
        expect(failures, dropTapeDb < -0.1f, "Tape still has a real musical difference vs Digital "
                                             "(compensation is conservative, not a 100% match)");

        // Confirma que o loop de feedback continua estável (Feedback
        // 95%) com o makeup gain fiado no pipeline completo (engine +
        // OutputStage), não só no DelayEngine isolado (já coberto por
        // Phase5StressTests.cpp) -- o makeup gain é aplicado só na
        // amostra de saída do DelayEngine (ver DelayEngine.cpp), então
        // isso é esperado por construção, mas testamos o pipeline
        // completo mesmo assim.
        for (CharacterMode mode : { CharacterMode::Analog, CharacterMode::Tape })
        {
            DelayEngine engine;
            OutputStage stage;
            juce::dsp::ProcessSpec modeSpec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
            engine.prepare(modeSpec);
            stage.prepare(sampleRate, blockSize);
            Phase6Chunk chunk(engine, stage, numChannels, blockSize);

            engine.setDelayTimeMs(90.0f);
            engine.setFeedback(0.95f);
            engine.setCharacterMode(mode);
            engine.setCharacterAmount(1.0f);

            juce::Random rng(4200 + (int) mode);
            bool nanOrInf = false;
            float peak = 0.0f;

            const int total = (int) (sampleRate * 4.0);
            int done = 0;
            while (done < total)
            {
                const int n = juce::jmin(blockSize, total - done);
                juce::AudioBuffer<float> buffer(numChannels, n);
                fillNoise(buffer, rng, 0.5f);
                chunk.process(buffer, 1.0f, 0.0f, false);

                for (int ch = 0; ch < numChannels; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        const float s = buffer.getSample(ch, i);
                        if (std::isnan(s) || std::isinf(s)) nanOrInf = true;
                        peak = juce::jmax(peak, std::abs(s));
                    }
                done += n;
            }

            const juce::String modeName = (mode == CharacterMode::Analog ? "Analog" : "Tape");
            expect(failures, ! nanOrInf, modeName + " + makeup gain + Feedback95%, 4s: no NaN/Inf");
            expect(failures, peak < 6.0f, modeName + " + makeup gain + Feedback95%, 4s: output stays bounded "
                                         "(loop stability unaffected by the makeup gain), peak = "
                                         + juce::String(peak, 4));
        }
    }

    // ------------------------------------------------------------
    // Item 8: stress de automação -- Dry/Wet, Output e Bypass mudando
    // continuamente. Estímulo é um TOM CONTÍNUO (não ruído -- ver nota
    // em SineGenerator acima) especificamente pra poder detectar um
    // glitch/estalo real introduzido pela automação, sem confundir
    // com a falta de continuidade natural do ruído branco.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Automation stress: continuous Dry/Wet + Output + Bypass changes --" << std::endl;

        DelayEngine engine;
        OutputStage stage;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
        engine.prepare(spec);
        stage.prepare(sampleRate, blockSize);
        Phase6Chunk chunk(engine, stage, numChannels, blockSize);

        engine.setDelayTimeMs(200.0f);
        engine.setFeedback(0.6f);
        engine.setPingPong(true);
        engine.setCharacterMode(CharacterMode::Analog);
        engine.setCharacterAmount(0.5f);

        SineGenerator sine;
        sine.prepare(sampleRate, 220.0f);

        bool nanOrInf = false, allocationDetected = false, extremeGlitch = false;
        float peak = 0.0f;
        float prevSample = 0.0f;
        bool havePrevSample = false;
        float worstJump = 0.0f;

        const int total = (int) (sampleRate * 8.0);
        int done = 0;
        int blockIndex = 0;

        while (done < total)
        {
            const int n = juce::jmin(blockSize, total - done);

            const float mix = 0.5f + 0.5f * std::sin((float) blockIndex * 0.05f);
            const float outDb = 12.0f * std::sin((float) blockIndex * 0.03f);
            const bool bypassed = (blockIndex / 25) % 4 == 0; // liga/desliga periodicamente

            juce::AudioBuffer<float> buffer(numChannels, n);
            sine.fill(buffer, 0.5f);

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            chunk.process(buffer, mix, outDb, bypassed);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                allocationDetected = true;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float s = buffer.getSample(ch, i);
                    if (std::isnan(s) || std::isinf(s)) nanOrInf = true;
                    peak = juce::jmax(peak, std::abs(s));

                    if (ch == 0)
                    {
                        if (havePrevSample)
                        {
                            const float jump = std::abs(s - prevSample);
                            worstJump = juce::jmax(worstJump, jump);
                            if (jump > 1.0f)
                                extremeGlitch = true;
                        }
                        prevSample = s;
                        havePrevSample = true;
                    }
                }
            }

            done += n;
            ++blockIndex;
        }

        expect(failures, ! nanOrInf, "automation stress, 8s: no NaN/Inf");
        expect(failures, ! allocationDetected, "automation stress, 8s: zero allocation");
        expect(failures, ! extremeGlitch, "automation stress, 8s: no abrupt sample-to-sample glitch with a "
                                          "continuous tone stimulus, worst jump = " + juce::String(worstJump, 6));
        expect(failures, peak < 12.0f, "automation stress, 8s: output stays bounded, peak = " + juce::String(peak, 4));
    }

    // ------------------------------------------------------------
    // Grade sample rate x buffer size, com Dry/Wet/Output/Bypass
    // automados (não só o motor puro -- itens 14/17 estendidos pra
    // FASE 6).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Sample rate x buffer size grid, with Dry/Wet/Output/Bypass automation --" << std::endl;

        const double sampleRates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
        const int blockSizes[] { 32, 64, 128, 512, 1024 };

        int casesRun = 0;
        bool allOk = true;

        for (double sr : sampleRates)
        {
            for (int bs : blockSizes)
            {
                DelayEngine engine;
                OutputStage stage;
                juce::dsp::ProcessSpec gridSpec { sr, (juce::uint32) bs, (juce::uint32) numChannels };
                engine.prepare(gridSpec);
                stage.prepare(sr, bs);
                Phase6Chunk gridChunk(engine, stage, numChannels, bs);

                engine.setDelayTimeMs(180.0f);
                engine.setFeedback(0.65f);
                engine.setPingPong(true);
                engine.setCharacterMode(CharacterMode::Tape);
                engine.setCharacterAmount(0.6f);

                juce::Random rng(3000 + (int) sr + bs);
                bool nanOrInf = false, outOfRange = false, allocationDetected = false;

                const int total = (int) (sr * 0.5);
                int done = 0;
                int blockIndex = 0;

                while (done < total)
                {
                    const int n = juce::jmin(bs, total - done);

                    const float mix = 0.5f + 0.5f * std::sin((float) blockIndex * 0.1f);
                    const float outDb = 6.0f * std::sin((float) blockIndex * 0.07f);
                    const bool bypassed = (blockIndex / 8) % 3 == 0;

                    juce::AudioBuffer<float> buffer(numChannels, n);
                    fillNoise(buffer, rng, 0.5f);

                    gAllocationSeenInGuard = false;
                    gGuardActive = true;
                    gridChunk.process(buffer, mix, outDb, bypassed);
                    gGuardActive = false;

                    if (gAllocationSeenInGuard)
                        allocationDetected = true;

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        for (int i = 0; i < n; ++i)
                        {
                            const float s = buffer.getSample(ch, i);
                            if (std::isnan(s) || std::isinf(s)) nanOrInf = true;
                            if (std::abs(s) > 8.0f) outOfRange = true;
                        }
                    }

                    done += n;
                    ++blockIndex;
                }

                ++casesRun;

                if (nanOrInf || outOfRange || allocationDetected)
                {
                    allOk = false;
                    expect(failures, false, juce::String(sr, 0) + "Hz / block " + juce::String(bs)
                                           + ": nanInf=" + juce::String((int) nanOrInf)
                                           + " outOfRange=" + juce::String((int) outOfRange)
                                           + " allocation=" + juce::String((int) allocationDetected));
                }
            }
        }

        expect(failures, casesRun == 4 * 5, "ran all 20 sample-rate/block-size combinations (incl. 192kHz)");
        expect(failures, allOk, "all 20 combinations: no NaN/Inf, no blow-up, no allocation, "
                               "with Dry/Wet/Output/Bypass automation active");
    }

    return failures;
}
