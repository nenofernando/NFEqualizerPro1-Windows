// Testes obrigatórios da FASE 4, itens 10 e 11: stress test crítico
// (todos os parâmetros no extremo ao mesmo tempo) e a grade de sample
// rates x buffer sizes (agora incluindo 192kHz) com filtros e
// modulação ativos de verdade, não só o delay puro da FASE 2.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

using namespace NF;
using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

int runStressTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Stress tests (FASE 4, itens 10 e 11) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Item 10: tudo no extremo ao mesmo tempo, ruído contínuo, com
    // mudanças periódicas de delay time (disparando crossfades) --
    // sem runaway, sem NaN/Inf, saída limitada.
    // ------------------------------------------------------------
    {
        std::cout << "-- Critical stress: Feedback=95%, Reso=100%, ModDepth=100%, ModRate=10Hz, periodic time changes --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);

        engine.setDelayTimeMs(80.0f);
        engine.setFeedback(0.95f);
        engine.setPingPong(true);
        engine.setHighPassHz(200.0f);
        engine.setLowPassHz(3000.0f);
        engine.setResonance(1.0f);
        engine.setModRate(10.0f);
        engine.setModDepth(1.0f);
        engine.setModShape(ModShape::SoftRandom);
        engine.setModSpread(1.0f);

        juce::Random rng(777);
        bool allocationDetected = false;
        bool nanOrInfDetected = false;
        float peakMagnitude = 0.0f;

        constexpr int totalSamples = (int) (sampleRate * 8.0);   // 8 segundos
        int done = 0;
        int blockIndex = 0;

        while (done < totalSamples)
        {
            const int n = juce::jmin(blockSize, totalSamples - done);

            // Muda o delay time periodicamente -- às vezes um ajuste
            // pequeno, às vezes um salto grande (dispara o crossfade
            // de duas cabeças no meio do caos).
            if (blockIndex % 15 == 0)
            {
                const float newMs = 20.0f + rng.nextFloat() * 400.0f;
                engine.setDelayTimeMs(newMs);
            }

            juce::AudioBuffer<float> buffer(2, n);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buffer.setSample(ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.6f);

            gAllocationSeenInGuard = false;
            gGuardActive = true;
            engine.process(buffer);
            gGuardActive = false;

            if (gAllocationSeenInGuard)
                allocationDetected = true;

            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float s = buffer.getSample(ch, i);
                    if (std::isnan(s) || std::isinf(s))
                        nanOrInfDetected = true;
                    peakMagnitude = juce::jmax(peakMagnitude, std::abs(s));
                }
            }

            done += n;
            ++blockIndex;
        }

        expect(failures, ! allocationDetected, "critical stress, 8s: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "critical stress, 8s: no NaN/Inf");
        expect(failures, peakMagnitude < 5.0f, "critical stress, 8s: output stays bounded (no runaway), peak = "
                                              + juce::String(peakMagnitude, 4));
    }

    // ------------------------------------------------------------
    // Item 11: sample rates x buffer sizes, com filtros + modulação
    // ativos de verdade (não só o delay puro da FASE 2/3).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Sample rate x buffer size grid, filters + modulation active --" << std::endl;

        const double sampleRates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
        const int blockSizes[] { 32, 64, 128, 512, 1024 };

        int casesRun = 0;
        bool allOk = true;

        for (double sampleRate : sampleRates)
        {
            for (int blockSize : blockSizes)
            {
                DelayEngine engine;
                juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
                engine.prepare(spec);

                engine.setDelayTimeMs(150.0f);
                engine.setFeedback(0.7f);
                engine.setPingPong(true);
                engine.setHighPassHz(150.0f);
                engine.setLowPassHz(6000.0f);
                engine.setResonance(0.6f);
                engine.setModRate(3.0f);
                engine.setModDepth(0.8f);
                engine.setModShape(ModShape::Triangle);
                engine.setModSpread(0.7f);

                juce::Random rng(101 + (int) sampleRate + blockSize);
                bool nanOrInf = false, outOfRange = false, allocationDetected = false;

                const int totalSamples = (int) (sampleRate * 0.5);   // 0.5s por combinação
                int done = 0;

                while (done < totalSamples)
                {
                    const int n = juce::jmin(blockSize, totalSamples - done);
                    juce::AudioBuffer<float> buffer(2, n);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < n; ++i)
                            buffer.setSample(ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.5f);

                    gAllocationSeenInGuard = false;
                    gGuardActive = true;
                    engine.process(buffer);
                    gGuardActive = false;

                    if (gAllocationSeenInGuard)
                        allocationDetected = true;

                    for (int ch = 0; ch < 2; ++ch)
                    {
                        for (int i = 0; i < n; ++i)
                        {
                            const float s = buffer.getSample(ch, i);
                            if (std::isnan(s) || std::isinf(s)) nanOrInf = true;
                            if (std::abs(s) > 5.0f) outOfRange = true;
                        }
                    }

                    done += n;
                }

                ++casesRun;

                if (nanOrInf || outOfRange || allocationDetected)
                {
                    allOk = false;
                    expect(failures, false, juce::String(sampleRate, 0) + "Hz / block " + juce::String(blockSize)
                                           + ": nanInf=" + juce::String((int) nanOrInf)
                                           + " outOfRange=" + juce::String((int) outOfRange)
                                           + " allocation=" + juce::String((int) allocationDetected));
                }
            }
        }

        expect(failures, casesRun == 4 * 5, "ran all 20 sample-rate/block-size combinations (incl. 192kHz)");
        expect(failures, allOk, "all 20 combinations: no NaN/Inf, no blow-up, no allocation, "
                               "with filters + modulation active");
    }

    return failures;
}
