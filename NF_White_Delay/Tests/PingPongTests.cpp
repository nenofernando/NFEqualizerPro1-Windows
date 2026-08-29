// Testes obrigatórios da FASE 3, item 9 (ping-pong).
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
    struct StereoResult
    {
        std::vector<float> left, right;
        bool allocationDetected = false;
        bool nanOrInfDetected = false;
    };

    // impulseLeftOnly=true: impulso só em L. false: impulso em L e R
    // ao mesmo tempo (entrada estéreo).
    StereoResult runPingPong(double sampleRate, float delayMs, float feedback0to1,
                             bool impulseLeftOnly, int totalSamples)
    {
        NF::DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(delayMs);
        engine.setFeedback(feedback0to1);
        engine.setPingPong(true);

        StereoResult result;
        result.left.reserve((size_t) totalSamples);
        result.right.reserve((size_t) totalSamples);

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
                if (! impulseLeftOnly)
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
                const float l = buffer.getSample(0, i);
                const float r = buffer.getSample(1, i);

                if (std::isnan(l) || std::isinf(l) || std::isnan(r) || std::isinf(r))
                    result.nanOrInfDetected = true;

                result.left.push_back(l);
                result.right.push_back(r);
            }

            done += n;
        }

        return result;
    }
}

int runPingPongTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Ping-Pong tests (FASE 3) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Impulso só em L -- alternância L, R, L, R... com a amplitude
    // decaindo feedbackGain^(N-1) a cada repeat, e o canal "errado"
    // silencioso em cada instante de repeat (cross-feedback de
    // verdade, não um swap da saída final).
    // ------------------------------------------------------------
    {
        std::cout << "-- Left-only impulse: L/R/L/R alternation --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float delayMs = 80.0f;
        constexpr float feedback = 0.6f;

        const int delaySamples = (int) std::round(delayMs * 0.001 * sampleRate);
        const int totalSamples = delaySamples * 5 + 32;

        auto result = runPingPong(sampleRate, delayMs, feedback, true, totalSamples);

        expect(failures, ! result.allocationDetected, "left-only impulse, ping-pong: no allocation in process()");
        expect(failures, ! result.nanOrInfDetected, "left-only impulse, ping-pong: no NaN/Inf");

        // repeat 1 = L (1.0), repeat 2 = R (0.6), repeat 3 = L (0.36),
        // repeat 4 = R (0.216) -- ver derivação no relatório da FASE 3.
        const bool expectedIsLeft[] { true, false, true, false };

        bool alternationOk = true;

        for (int repeat = 1; repeat <= 4; ++repeat)
        {
            const int index = delaySamples * repeat;
            const float l = (index < (int) result.left.size()) ? result.left[(size_t) index] : 0.0f;
            const float r = (index < (int) result.right.size()) ? result.right[(size_t) index] : 0.0f;

            const float expectedAmp = std::pow(feedback, (float) (repeat - 1));
            const float loud = expectedIsLeft[repeat - 1] ? l : r;
            const float quiet = expectedIsLeft[repeat - 1] ? r : l;

            const bool loudOk = std::abs(loud - expectedAmp) < 0.03f;
            const bool quietOk = std::abs(quiet) < 0.03f;

            if (! loudOk || ! quietOk)
                alternationOk = false;

            expect(failures, loudOk && quietOk,
                  "repeat " + juce::String(repeat) + " is in " + juce::String(expectedIsLeft[repeat - 1] ? "L" : "R")
                  + " @ ~" + juce::String(expectedAmp, 3) + " (L=" + juce::String(l, 4) + ", R=" + juce::String(r, 4) + ")");
        }

        expect(failures, alternationOk, "full L/R/L/R alternation pattern holds for repeats 1-4");
    }

    // ------------------------------------------------------------
    // Entrada estéreo (impulso em L e R juntos) -- com ping-pong,
    // ambos os canais ficam simétricos e decaem igualmente (cada um
    // alimenta o feedback do outro em volume igual).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Stereo input (impulse in both L and R) --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float delayMs = 80.0f;
        constexpr float feedback = 0.6f;

        const int delaySamples = (int) std::round(delayMs * 0.001 * sampleRate);
        const int totalSamples = delaySamples * 4 + 32;

        auto result = runPingPong(sampleRate, delayMs, feedback, false, totalSamples);

        expect(failures, ! result.allocationDetected, "stereo impulse, ping-pong: no allocation in process()");
        expect(failures, ! result.nanOrInfDetected, "stereo impulse, ping-pong: no NaN/Inf");

        bool symmetricOk = true;

        for (int repeat = 1; repeat <= 3; ++repeat)
        {
            const int index = delaySamples * repeat;
            const float l = (index < (int) result.left.size()) ? result.left[(size_t) index] : 0.0f;
            const float r = (index < (int) result.right.size()) ? result.right[(size_t) index] : 0.0f;
            const float expectedAmp = std::pow(feedback, (float) (repeat - 1));

            if (std::abs(l - expectedAmp) >= 0.03f || std::abs(r - expectedAmp) >= 0.03f)
                symmetricOk = false;
        }

        expect(failures, symmetricOk, "with a symmetric stereo impulse, L and R stay equal and decay "
                                     "as feedbackGain^(N-1) together (repeats 1-3)");
    }

    return failures;
}
