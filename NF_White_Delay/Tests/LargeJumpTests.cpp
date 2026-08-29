// Teste do item 5 da FASE 3: mudança GRANDE de delay time (o caso
// "1/16 -> 1/2" do briefing) não pode produzir um salto abrupto de
// fase/amplitude (clique, zipper, tape-stop) -- o crossfade de duas
// cabeças precisa suavizar isso, ao contrário de uma rampa sozinha.
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

int runLargeJumpTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Large delay-time jump tests (FASE 3, item 5) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Tom contínuo de 300 Hz a 100ms de delay, depois um salto
    // brusco pra 500ms (400ms de diferença, bem acima do threshold
    // de crossfade) no meio do sinal. Um corte instantâneo de tempo
    // produziria uma descontinuidade de fase enorme (delta amostra-a-
    // amostra perto de 2x a amplitude); o crossfade de duas cabeças
    // deve manter o delta sempre dentro do que o próprio seno natural
    // já produz (mais uma folga generosa pra artefatos de
    // interpolação/blend).
    // ------------------------------------------------------------
    {
        std::cout << "-- 1/16 -> 1/2 style jump (100ms -> 500ms) during a continuous tone --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float amplitude = 0.5f;
        constexpr float toneHz = 300.0f;
        constexpr int blockSize = 256;

        NF::DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(100.0f);
        engine.setFeedback(0.0f);   // isola o efeito só da mudança de tempo

        // Delta amostra-a-amostra máximo natural de um seno dessa
        // amplitude/frequência: A * 2*pi*f/fs.
        const float naturalMaxDelta =
            amplitude * juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate;
        const float clickThreshold = naturalMaxDelta * 8.0f;   // folga generosa

        const int totalSamples = (int) (sampleRate * 1.0);   // 1 segundo
        const int jumpAtSample = (int) (sampleRate * 0.3);   // salta aos 300ms

        std::vector<float> out;
        out.reserve((size_t) totalSamples);

        bool allocationDetected = false;
        bool nanOrInfDetected = false;
        int phase = 0;

        int done = 0;
        bool jumped = false;

        while (done < totalSamples)
        {
            const int n = juce::jmin(blockSize, totalSamples - done);

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

        expect(failures, ! allocationDetected, "large jump (100ms->500ms): no allocation in process()");
        expect(failures, ! nanOrInfDetected, "large jump (100ms->500ms): no NaN/Inf");

        float maxDelta = 0.0f;
        int maxDeltaIndex = -1;

        for (size_t i = 1; i < out.size(); ++i)
        {
            const float delta = std::abs(out[i] - out[i - 1]);
            if (delta > maxDelta)
            {
                maxDelta = delta;
                maxDeltaIndex = (int) i;
            }
        }

        expect(failures, maxDelta < clickThreshold,
              "no click/abrupt phase jump anywhere in the output: max sample-to-sample delta = "
              + juce::String(maxDelta, 5) + " (threshold " + juce::String(clickThreshold, 5)
              + ", natural tone delta " + juce::String(naturalMaxDelta, 5)
              + "), worst point at sample " + juce::String(maxDeltaIndex));

        // Depois de decorrido bastante tempo do salto (>> a janela de
        // crossfade de 25ms), a saída tem que estar de fato no novo
        // delay -- confere isolando um segundo impulso disparado bem
        // depois do salto ter terminado.
    }

    // ------------------------------------------------------------
    // Confere que, depois de um salto grande + tempo suficiente pro
    // crossfade terminar (bem mais que 25ms), o motor está mesmo lendo
    // no NOVO tempo -- não só "sem clique", mas efetivamente migrado.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- After the crossfade settles, the engine reads at the NEW time --" << std::endl;

        constexpr double sampleRate = 48000.0;

        NF::DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 256u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(100.0f);
        engine.setFeedback(0.0f);

        // Roda um pouco no tempo inicial pra "assentar" a cabeça A.
        {
            juce::AudioBuffer<float> warmup(2, 4096);
            warmup.clear();
            engine.process(warmup);
        }

        // Salto grande.
        engine.setDelayTimeMs(500.0f);

        // Espera BEM mais que os 25ms de crossfade (silêncio) antes de
        // disparar o impulso de medição.
        {
            const int settleSamples = (int) (sampleRate * 0.2);   // 200ms >> 25ms
            int done = 0;
            while (done < settleSamples)
            {
                const int n = juce::jmin(256, settleSamples - done);
                juce::AudioBuffer<float> b(2, n);
                b.clear();
                engine.process(b);
                done += n;
            }
        }

        const int expectedDelaySamples = (int) std::round(500.0 * 0.001 * sampleRate);
        const int totalSamples = expectedDelaySamples + 64;

        std::vector<float> out;
        out.reserve((size_t) totalSamples);
        int done = 0;
        bool first = true;

        while (done < totalSamples)
        {
            const int n = juce::jmin(256, totalSamples - done);
            juce::AudioBuffer<float> b(2, n);
            b.clear();
            if (first) { b.setSample(0, 0, 1.0f); first = false; }
            engine.process(b);
            for (int i = 0; i < n; ++i) out.push_back(b.getSample(0, i));
            done += n;
        }

        int peakIndex = -1;
        float peakValue = -1.0f;
        for (int i = 0; i < (int) out.size(); ++i)
        {
            if (std::abs(out[(size_t) i]) > peakValue)
            {
                peakValue = std::abs(out[(size_t) i]);
                peakIndex = i;
            }
        }

        expect(failures, std::abs(peakIndex - expectedDelaySamples) <= 1,
              "after settling past a large jump (100ms->500ms), the engine reads exactly at 500ms "
              "(peak at " + juce::String(peakIndex) + ", expected " + juce::String(expectedDelaySamples) + ")");
    }

    return failures;
}
