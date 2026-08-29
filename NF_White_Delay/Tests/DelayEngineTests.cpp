// Testes obrigatórios da FASE 2 (núcleo do delay). Continuam
// passando na FASE 3 -- feedback=0 (default do DelayEngine antes de
// setFeedback() ser chamado) reduz exatamente ao comportamento puro
// da FASE 2 (ver runImpulseThroughDelay abaixo, que nunca chama
// setFeedback -- fica em 0, seu default).
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

namespace
{
    // Processa um impulso (1.0 na primeira amostra, resto zero) por
    // totalSamples amostras, em blocos de exatamente blockSize (o
    // último bloco é menor se totalSamples não for múltiplo -- testa
    // blocos parciais também, comportamento real de host). Feedback
    // fica em 0 (default) -- isso é o núcleo puro da FASE 2.
    struct ImpulseResult
    {
        std::vector<float> output;
        bool allocationDetected = false;
        bool nanOrInfDetected = false;
        bool outOfRangeDetected = false;
    };

    ImpulseResult runImpulseThroughDelay(double sampleRate, int blockSize,
                                         float delayMs, int totalSamples)
    {
        NF::DelayEngine engine;

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(delayMs);

        ImpulseResult result;
        result.output.reserve((size_t) totalSamples);

        int samplesProcessed = 0;
        bool firstBlock = true;

        while (samplesProcessed < totalSamples)
        {
            const int thisBlockSize = juce::jmin(blockSize, totalSamples - samplesProcessed);

            juce::AudioBuffer<float> buffer(2, thisBlockSize);
            buffer.clear();

            if (firstBlock)
            {
                buffer.setSample(0, 0, 1.0f);
                buffer.setSample(1, 0, 1.0f);
                firstBlock = false;
            }

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            engine.process(buffer);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                result.allocationDetected = true;

            for (int i = 0; i < thisBlockSize; ++i)
            {
                const float sample = buffer.getSample(0, i);

                if (std::isnan(sample) || std::isinf(sample))
                    result.nanOrInfDetected = true;

                if (std::abs(sample) > 4.0f)
                    result.outOfRangeDetected = true;

                result.output.push_back(sample);
            }

            samplesProcessed += thisBlockSize;
        }

        return result;
    }

    // Índice de maior magnitude absoluta no vetor -- ponto onde o
    // impulso atrasado deve estar.
    int indexOfPeak(const std::vector<float>& data)
    {
        int bestIndex = -1;
        float bestValue = -1.0f;

        for (int i = 0; i < (int) data.size(); ++i)
        {
            const float magnitude = std::abs(data[(size_t) i]);
            if (magnitude > bestValue)
            {
                bestValue = magnitude;
                bestIndex = i;
            }
        }

        return bestIndex;
    }
}

int runDelayEngineTests()
{
    int failures = 0;

    std::cout << "=== NF White Delay -- FASE 2 (DelayEngine) tests ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // 1) Impulso -- grade completa: tempos x sample rates x buffer
    //    sizes pedida no briefing (5 x 3 x 5 = 75 combinações).
    // ------------------------------------------------------------
    {
        std::cout << "-- Impulse timing / NaN-Inf / allocation grid --" << std::endl;

        const double sampleRates[] { 44100.0, 48000.0, 96000.0 };
        const float delayTimesMs[] { 10.0f, 100.0f, 300.0f, 1000.0f, 4000.0f };
        const int blockSizes[] { 32, 64, 128, 512, 1024 };

        int casesRun = 0;

        for (double sampleRate : sampleRates)
        {
            for (float delayMs : delayTimesMs)
            {
                const int expectedDelaySamples =
                    (int) std::round((double) delayMs * 0.001 * sampleRate);

                for (int blockSize : blockSizes)
                {
                    const int totalSamples = expectedDelaySamples + 64;

                    auto result = runImpulseThroughDelay(sampleRate, blockSize, delayMs, totalSamples);
                    ++casesRun;

                    const int peakIndex = indexOfPeak(result.output);
                    const int error = std::abs(peakIndex - expectedDelaySamples);

                    const juce::String tag =
                        juce::String(sampleRate, 0) + "Hz / " + juce::String(delayMs, 0) + "ms / block " + juce::String(blockSize);

                    if (error > 1 || result.nanOrInfDetected || result.outOfRangeDetected || result.allocationDetected)
                    {
                        expect(failures, false, "impulse @ " + tag + " (peak at " + juce::String(peakIndex)
                                       + ", expected " + juce::String(expectedDelaySamples)
                                       + ", nanInf=" + juce::String((int) result.nanOrInfDetected)
                                       + ", outOfRange=" + juce::String((int) result.outOfRangeDetected)
                                       + ", allocation=" + juce::String((int) result.allocationDetected) + ")");
                    }
                }
            }
        }

        expect(failures, casesRun == 3 * 5 * 5, "ran all 75 sample-rate/time/block-size combinations");
        std::cout << "  (" << casesRun << " combinations checked individually above)" << std::endl;
    }

    // ------------------------------------------------------------
    // 2) Extremos da faixa (1 ms e 4000 ms).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Range extremes --" << std::endl;

        auto low = runImpulseThroughDelay(48000.0, 512, NF::DelayEngine::minDelayMs, 200);
        expect(failures, ! low.nanOrInfDetected && ! low.outOfRangeDetected,
              "1 ms (minDelayMs) does not produce NaN/Inf or blow up");

        auto high = runImpulseThroughDelay(48000.0, 512, NF::DelayEngine::maxFreeDelayMs,
                                           (int) std::round(4000.0 * 0.001 * 48000.0) + 64);
        expect(failures, ! high.nanOrInfDetected && ! high.outOfRangeDetected,
              "4000 ms (maxFreeDelayMs) does not produce NaN/Inf or blow up");

        // Capacidade interna (SYNC pode pedir mais que o limite do knob
        // manual -- ver Tests/SyncIntegrationTests.cpp pro teste
        // completo de ponta a ponta com sync de verdade).
        auto internal = runImpulseThroughDelay(48000.0, 512, NF::DelayEngine::maxInternalDelayMs,
                                               (int) std::round((double) NF::DelayEngine::maxInternalDelayMs * 0.001 * 48000.0) + 64);
        expect(failures, ! internal.nanOrInfDetected && ! internal.outOfRangeDetected,
              "16000 ms (maxInternalDelayMs) does not produce NaN/Inf or blow up");
    }

    // ------------------------------------------------------------
    // 3) Mudança de delay time em tempo real, contínua.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Continuous use: delay time changes mid-stream --" << std::endl;

        NF::DelayEngine engine;
        juce::dsp::ProcessSpec spec { 48000.0, 256u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(300.0f);

        juce::Random rng(42);
        bool nanInf = false, outOfRange = false, allocation = false;

        for (int block = 0; block < 200; ++block)
        {
            if (block % 20 == 0)
                engine.setDelayTimeMs(rng.nextFloat() * (NF::DelayEngine::maxFreeDelayMs - NF::DelayEngine::minDelayMs) + NF::DelayEngine::minDelayMs);

            juce::AudioBuffer<float> buffer(2, 256);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample(ch, i, rng.nextFloat() * 2.0f - 1.0f);

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            engine.process(buffer);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                allocation = true;

            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = buffer.getSample(ch, i);
                    if (std::isnan(s) || std::isinf(s)) nanInf = true;
                    if (std::abs(s) > 4.0f) outOfRange = true;
                }
            }
        }

        expect(failures, ! nanInf, "200 blocks of noise with random delay-time jumps: no NaN/Inf");
        expect(failures, ! outOfRange, "200 blocks of noise with random delay-time jumps: no blow-up");
        expect(failures, ! allocation, "200 blocks of noise with random delay-time jumps: no allocation in process()");
    }

    // ------------------------------------------------------------
    // 4) prepare()/reset() de novo, sample rate/buffer size diferentes.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Re-prepare with a different sample rate/block size --" << std::endl;

        NF::DelayEngine engine;

        juce::dsp::ProcessSpec specA { 44100.0, 512u, 2u };
        engine.prepare(specA);
        engine.setDelayTimeMs(300.0f);

        juce::AudioBuffer<float> bufferA(2, 512);
        bufferA.clear();
        bufferA.setSample(0, 0, 1.0f);
        engine.process(bufferA);

        juce::dsp::ProcessSpec specB { 96000.0, 128u, 2u };
        engine.prepare(specB);
        engine.reset();
        engine.setDelayTimeMs(300.0f);

        const int expected = (int) std::round(300.0 * 0.001 * 96000.0);
        const int total = expected + 64;

        std::vector<float> out;
        out.reserve((size_t) total);
        int done = 0;
        bool first = true;

        while (done < total)
        {
            const int n = juce::jmin(128, total - done);
            juce::AudioBuffer<float> b(2, n);
            b.clear();
            if (first) { b.setSample(0, 0, 1.0f); first = false; }
            engine.process(b);
            for (int i = 0; i < n; ++i) out.push_back(b.getSample(0, i));
            done += n;
        }

        const int peak = indexOfPeak(out);
        expect(failures, std::abs(peak - expected) <= 1,
              "after re-prepare (44.1kHz/512 -> 96kHz/128), delay is exactly 300ms at the new rate");
    }

    return failures;
}
