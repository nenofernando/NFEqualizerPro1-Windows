// Testes obrigatórios da FASE 3, item 8 (feedback).
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
    // Processa um impulso (canal 0 e 1) e devolve o canal 0 inteiro --
    // feedback fixo (setado uma única vez, antes de qualquer process(),
    // então não tem rampa de suavização atrapalhando a leitura exata
    // dos repeats).
    std::vector<float> impulseWithFeedback(double sampleRate, float delayMs, float feedback0to1,
                                           int totalSamples, bool& allocationDetected,
                                           bool& nanOrInfDetected)
    {
        NF::DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(delayMs);
        engine.setFeedback(feedback0to1);

        std::vector<float> out;
        out.reserve((size_t) totalSamples);

        int done = 0;
        bool first = true;
        allocationDetected = false;
        nanOrInfDetected = false;

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
                allocationDetected = true;

            for (int i = 0; i < n; ++i)
            {
                const float s = buffer.getSample(0, i);
                if (std::isnan(s) || std::isinf(s))
                    nanOrInfDetected = true;
                out.push_back(s);
            }

            done += n;
        }

        return out;
    }
}

int runFeedbackTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Feedback tests (FASE 3) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Decaimento exato em 50% -- arquitetura adotada (feedback
    // simples, um tap, sem saturação): repeat N = feedbackGain^(N-1).
    // ------------------------------------------------------------
    {
        std::cout << "-- 50% feedback: exact repeat decay --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float delayMs = 100.0f;
        constexpr float feedback = 0.5f;

        const int delaySamples = (int) std::round(delayMs * 0.001 * sampleRate);
        const int totalSamples = delaySamples * 5 + 32;

        bool allocationDetected, nanOrInfDetected;
        auto out = impulseWithFeedback(sampleRate, delayMs, feedback, totalSamples, allocationDetected, nanOrInfDetected);

        expect(failures, ! allocationDetected, "50% feedback: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "50% feedback: no NaN/Inf");

        const float expectedAmplitudes[] { 1.0f, 0.5f, 0.25f, 0.125f };

        for (int repeat = 1; repeat <= 4; ++repeat)
        {
            const int index = delaySamples * repeat;
            const float actual = (index < (int) out.size()) ? out[(size_t) index] : 0.0f;
            const float expectedAmp = expectedAmplitudes[repeat - 1];

            expect(failures, std::abs(actual - expectedAmp) < 0.03f,
                  "repeat " + juce::String(repeat) + " ~= " + juce::String(expectedAmp, 3)
                  + " (got " + juce::String(actual, 4) + ")");
        }
    }

    // ------------------------------------------------------------
    // 0%, 25%, 75%, 95% -- decaimento genérico feedbackGain^(N-1)
    // pros primeiros repeats, mais checagem de estabilidade longa em
    // 95%.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- 0% / 25% / 75% / 95% feedback --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float delayMs = 50.0f;
        const float feedbacks[] { 0.0f, 0.25f, 0.75f, 0.95f };

        const int delaySamples = (int) std::round(delayMs * 0.001 * sampleRate);

        for (float feedback : feedbacks)
        {
            const int totalSamples = delaySamples * 4 + 32;
            bool allocationDetected, nanOrInfDetected;
            auto out = impulseWithFeedback(sampleRate, delayMs, feedback, totalSamples, allocationDetected, nanOrInfDetected);

            expect(failures, ! allocationDetected && ! nanOrInfDetected,
                  juce::String(feedback * 100.0f, 0) + "% feedback: no allocation, no NaN/Inf");

            bool decayOk = true;
            for (int repeat = 1; repeat <= 3; ++repeat)
            {
                const int index = delaySamples * repeat;
                const float actual = (index < (int) out.size()) ? out[(size_t) index] : 0.0f;
                const float expectedAmp = std::pow(feedback, (float) (repeat - 1));

                if (std::abs(actual - expectedAmp) >= 0.03f)
                    decayOk = false;
            }

            expect(failures, decayOk, juce::String(feedback * 100.0f, 0) + "% feedback: repeats 1-3 match feedbackGain^(N-1)");
        }
    }

    // ------------------------------------------------------------
    // 95% por muito tempo -- sem NaN/Inf/runaway, e decai de verdade
    // (não sustenta nem cresce) numa janela longa.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- 95% feedback, long run: stability --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float delayMs = 20.0f;   // curto -- cabe muitos repeats numa janela razoável
        constexpr float feedback = 0.95f;

        // ~4 segundos -- centenas de repeats num delay de 20ms.
        const int totalSamples = (int) (sampleRate * 4.0);

        bool allocationDetected, nanOrInfDetected;
        auto out = impulseWithFeedback(sampleRate, delayMs, feedback, totalSamples, allocationDetected, nanOrInfDetected);

        expect(failures, ! allocationDetected, "95% feedback, 4s: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "95% feedback, 4s: no NaN/Inf across the entire run");

        float peakMagnitude = 0.0f;
        for (float s : out)
            peakMagnitude = juce::jmax(peakMagnitude, std::abs(s));

        expect(failures, peakMagnitude <= 1.5f, "95% feedback, 4s: peak magnitude stays bounded (no runaway), got "
                                               + juce::String(peakMagnitude, 4));

        // Últimas ~1000 amostras devem estar bem próximas de zero --
        // prova de decaimento de verdade, não sustentação/crescimento.
        float tailPeak = 0.0f;
        for (int i = (int) out.size() - 1000; i < (int) out.size(); ++i)
            tailPeak = juce::jmax(tailPeak, std::abs(out[(size_t) i]));

        expect(failures, tailPeak < 0.01f, "95% feedback, 4s: last 1000 samples have decayed below 0.01 "
                                          "(genuine decay, not sustain/runaway), got " + juce::String(tailPeak, 5));

        expect(failures, (int) out.size() == totalSamples, "95% feedback, 4s: finite, exact number of samples produced");
    }

    return failures;
}
