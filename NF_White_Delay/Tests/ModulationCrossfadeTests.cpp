// Teste obrigatório da FASE 4, item 8: LFO ativo + mudança grande de
// divisão Sync acontecendo ao mesmo tempo. Precisa continuar sem
// clique, sem pitch jump abrupto, sem NaN, sem descontinuidade -- o
// offset de modulação tem que permanecer coerente durante a troca das
// cabeças A/B.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
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

int runModulationCrossfadeTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Modulation + large sync jump, simultaneously (FASE 4, item 8) ===" << std::endl << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr float amplitude = 0.5f;
    constexpr float toneHz = 300.0f;
    constexpr int blockSize = 256;

    DelayEngine engine;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
    engine.prepare(spec);
    engine.setDelayTimeMs(100.0f);
    engine.setFeedback(0.3f);

    // Modulação bem ativa -- rate rápido, depth máximo -- exatamente o
    // cenário que poderia expor o crossfade sendo perturbado pelo LFO.
    engine.setModRate(6.0f);
    engine.setModDepth(1.0f);
    engine.setModShape(ModShape::Sine);
    engine.setModSpread(0.5f);

    const float naturalMaxDelta = amplitude * juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate;
    const float clickThreshold = naturalMaxDelta * 10.0f;   // folga um pouco maior que o teste sem modulação

    constexpr int totalSamples = (int) (sampleRate * 1.5);
    const int jumpAtSample = (int) (sampleRate * 0.3);

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
            // Salto grande (1/16 -> 1/2 style): dispara o crossfade de
            // duas cabeças ENQUANTO o LFO continua rodando por cima.
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
            const float l = buffer.getSample(0, i);
            const float r = buffer.getSample(1, i);
            if (std::isnan(l) || std::isinf(l) || std::isnan(r) || std::isinf(r))
                nanOrInfDetected = true;
            out.push_back(l);
        }

        done += n;
    }

    expect(failures, ! allocationDetected, "LFO + large jump simultaneously: no allocation in process()");
    expect(failures, ! nanOrInfDetected, "LFO + large jump simultaneously: no NaN/Inf");

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
          "no click/discontinuity anywhere, even with the LFO running through the crossfade: "
          "max sample-to-sample delta = " + juce::String(maxDelta, 5) + " (threshold "
          + juce::String(clickThreshold, 5) + "), worst point at sample " + juce::String(maxDeltaIndex));

    float peakMagnitude = 0.0f;
    for (float s : out)
        peakMagnitude = juce::jmax(peakMagnitude, std::abs(s));

    expect(failures, peakMagnitude < 3.0f,
          "output stays bounded through the simultaneous LFO + crossfade transition, peak = "
          + juce::String(peakMagnitude, 4));

    return failures;
}
