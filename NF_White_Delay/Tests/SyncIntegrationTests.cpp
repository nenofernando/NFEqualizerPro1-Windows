// Teste do item 3 da correção da FASE 3: ponta a ponta, HostSync ->
// DelayEngine -> impulso atrasado no tempo calculado. Não basta
// testar HostSync::syncDivisionMs() isolado -- isso prova que o
// valor calculado realmente chega inteiro no motor, sem ser cortado
// pela capacidade interna (ver DelayEngine::maxInternalDelayMs).
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/DSP/HostSync.h"
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
    bool close_enough(double a, double b, double tolerance = 1e-6)
    {
        return std::abs(a - b) <= tolerance;
    }

    // Calcula o tempo via HostSync (exatamente como PluginProcessor
    // faria) e manda direto pro DelayEngine -- ponta a ponta de
    // verdade, não reimplementa o cálculo no teste.
    struct EndToEndResult
    {
        double calculatedMs = 0.0;
        int peakSampleIndex = -1;
        bool allocationDetected = false;
        bool nanOrInfDetected = false;
    };

    EndToEndResult runSyncEndToEnd(double sampleRate, double bpm, SyncDivision division, SyncModifier modifier,
                                   int timeSigNumerator, int timeSigDenominator)
    {
        EndToEndResult result;
        result.calculatedMs = syncDivisionMs(bpm, division, modifier, timeSigNumerator, timeSigDenominator);

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs((float) result.calculatedMs);
        engine.setFeedback(0.0f);

        const int expectedSamples = (int) std::round(result.calculatedMs * 0.001 * sampleRate);
        const int totalSamples = expectedSamples + 128;

        std::vector<float> out;
        out.reserve((size_t) totalSamples);

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
                out.push_back(s);
            }

            done += n;
        }

        float bestValue = -1.0f;
        for (int i = 0; i < (int) out.size(); ++i)
        {
            if (std::abs(out[(size_t) i]) > bestValue)
            {
                bestValue = std::abs(out[(size_t) i]);
                result.peakSampleIndex = i;
            }
        }

        return result;
    }
}

int runSyncIntegrationTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Sync -> DelayEngine end-to-end tests (FASE 3 correction, item 3) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // 60 BPM / 4/4 / 2 Bars Straight = 8000ms -- tem que repetir em
    // ~8.000s de verdade, NÃO em 4.000s por um clamp interno antigo.
    // ------------------------------------------------------------
    {
        constexpr double sampleRate = 48000.0;

        auto result = runSyncEndToEnd(sampleRate, 60.0, SyncDivision::Div2Bar, SyncModifier::Straight, 4, 4);

        expect(failures, close_enough(result.calculatedMs, 8000.0), "60 BPM/4-4/2 Bars Straight calculates to 8000 ms");
        expect(failures, ! result.allocationDetected, "60 BPM/4-4/2 Bars Straight: no allocation in process()");
        expect(failures, ! result.nanOrInfDetected, "60 BPM/4-4/2 Bars Straight: no NaN/Inf");

        const int expectedSamples = (int) std::round(8000.0 * 0.001 * sampleRate);
        const int wrongClampSamples = (int) std::round(4000.0 * 0.001 * sampleRate);

        expect(failures, std::abs(result.peakSampleIndex - expectedSamples) <= 1,
              "repeat lands at ~8.000s (sample " + juce::String(expectedSamples)
              + "), got sample " + juce::String(result.peakSampleIndex));

        expect(failures, std::abs(result.peakSampleIndex - wrongClampSamples) > 1000,
              "repeat is NOT at the old 4000ms clamp point (sample " + juce::String(wrongClampSamples) + ")");
    }

    // ------------------------------------------------------------
    // 60 BPM / 4/4 / 2 Bars Dotted = 12000ms -- confirma que
    // maxInternalDelayMs (16000ms) comporta isso sem cortar.
    // ------------------------------------------------------------
    {
        constexpr double sampleRate = 48000.0;

        auto result = runSyncEndToEnd(sampleRate, 60.0, SyncDivision::Div2Bar, SyncModifier::Dotted, 4, 4);

        expect(failures, close_enough(result.calculatedMs, 12000.0), "60 BPM/4-4/2 Bars Dotted calculates to 12000 ms");
        expect(failures, ! result.allocationDetected, "60 BPM/4-4/2 Bars Dotted: no allocation in process()");
        expect(failures, ! result.nanOrInfDetected, "60 BPM/4-4/2 Bars Dotted: no NaN/Inf");

        const int expectedSamples = (int) std::round(12000.0 * 0.001 * sampleRate);

        expect(failures, std::abs(result.peakSampleIndex - expectedSamples) <= 1,
              "repeat lands at ~12.000s (sample " + juce::String(expectedSamples)
              + "), got sample " + juce::String(result.peakSampleIndex)
              + " -- confirms maxInternalDelayMs (16000ms) comfortably fits this");
    }

    return failures;
}
