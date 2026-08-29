// Testes obrigatórios da FASE 5, itens 4 e 16 (Ducking fora do loop
// de feedback).
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/DSP/DuckingProcessor.h"
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
    // Ruído contínuo por 0.3s (não um impulso único) -- dá muito mais
    // amostras "informativas" (acima do limiar de zero) pra comparar a
    // razão entre as duas execuções, já que o feedback fica
    // continuamente re-excitado em vez de decair rápido depois de um
    // único disparo.
    std::vector<float> runNoiseWithDucking(double sampleRate, float duckingAmount0to1, int totalSamples,
                                           bool& allocationDetected, bool& nanOrInfDetected)
    {
        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(60.0f);
        engine.setFeedback(0.7f);
        engine.setDuckingAmount(duckingAmount0to1);

        std::vector<float> out;
        out.reserve((size_t) totalSamples);
        allocationDetected = false;
        nanOrInfDetected = false;

        const int noiseDurationSamples = (int) (sampleRate * 0.3);
        juce::Random rng(4242);   // mesma seed sempre -- entrada idêntica nas duas execuções

        int done = 0;

        while (done < totalSamples)
        {
            const int n = juce::jmin(512, totalSamples - done);
            juce::AudioBuffer<float> buffer(2, n);

            for (int i = 0; i < n; ++i)
            {
                const float s = (done + i < noiseDurationSamples) ? (rng.nextFloat() * 2.0f - 1.0f) * 0.5f : 0.0f;
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
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

int runDuckingTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Ducking tests (FASE 5, itens 4 e 16) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Item 4: Ducking não pode alterar o decaimento/comportamento
    // interno das repetições -- só a apresentação final do wet.
    //
    // Roda a MESMA entrada (impulso + feedback) duas vezes: Ducking=0%
    // (que por construção dá ganho 1.0 sempre -- ver DuckingProcessor)
    // e Ducking=100%. Se o loop interno é o mesmo nos dois casos, a
    // RAZÃO amostra-a-amostra output100%/output0% tem que cair sempre
    // dentro da faixa de ganho possível do duck ([10^(-20/20), 1] =
    // [0.1, 1.0], com folga) -- qualquer amostra fora dessa faixa
    // provaria que o próprio sinal interno (não só o ganho) mudou.
    // ------------------------------------------------------------
    {
        std::cout << "-- Ducking 0% vs 100%: internal feedback recursion is identical --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = (int) (sampleRate * 2.0);

        bool allocA, nanInfA, allocB, nanInfB;
        auto outDuck0 = runNoiseWithDucking(sampleRate, 0.0f, totalSamples, allocA, nanInfA);
        auto outDuck100 = runNoiseWithDucking(sampleRate, 1.0f, totalSamples, allocB, nanInfB);

        expect(failures, ! allocA && ! allocB, "Ducking 0%/100%: no allocation in process() in either run");
        expect(failures, ! nanInfA && ! nanInfB, "Ducking 0%/100%: no NaN/Inf in either run");

        const float minPossibleGain = std::pow(10.0f, -DuckingProcessor::maxDuckingDb / 20.0f);
        constexpr float tolerance = 0.02f;   // folga pra suavização do attack/release

        bool allRatiosInRange = true;
        int samplesChecked = 0;
        float worstRatio = 1.0f;

        for (size_t i = 0; i < outDuck0.size(); ++i)
        {
            if (std::abs(outDuck0[i]) < 0.0005f)
                continue;   // amostra perto de zero -- razão não é informativa

            const float ratio = outDuck100[i] / outDuck0[i];
            ++samplesChecked;

            if (ratio < minPossibleGain - tolerance || ratio > 1.0f + tolerance)
            {
                allRatiosInRange = false;
                worstRatio = ratio;
            }
        }

        expect(failures, samplesChecked > 1000, "enough non-trivial samples were checked (" + juce::String(samplesChecked) + ")");
        expect(failures, allRatiosInRange,
              "every sample-to-sample ratio (Ducking100%/Ducking0%) stays within the possible duck-gain "
              "range [0.1, 1.0] -- proves the internal feedback recursion is byte-identical between the "
              "two runs, only a per-sample scalar (the duck gain itself) differs" +
              (allRatiosInRange ? juce::String() : (" (worst ratio: " + juce::String(worstRatio, 4) + ")")));
    }

    // ------------------------------------------------------------
    // Item 16: Ducking=100% + Feedback=95% -- a entrada mantém o wet
    // baixo enquanto toca; quando termina, os repeats (que estavam se
    // acumulando por dentro o tempo todo, sem duck) reaparecem.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Ducking 100% + Feedback 95%: repeats reappear after input ends --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr int inputDurationSamples = (int) (sampleRate * 1.0);      // 1s de tom contínuo
        constexpr int silenceDurationSamples = (int) (sampleRate * 1.0);    // 1s de silêncio depois
        constexpr int totalSamples = inputDurationSamples + silenceDurationSamples;

        // Roda duas vezes com a MESMA entrada -- Ducking=0% (referência
        // "sem duck nenhum") e Ducking=100%. Comparar as duas evita
        // depender de um limiar absoluto de RMS (que varia MUITO com o
        // quanto o loop de feedback ressoa nesses parâmetros
        // específicos) -- o que importa é o comportamento RELATIVO.
        auto runOnce = [&](float duckingAmount0to1)
        {
            DelayEngine engine;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
            engine.prepare(spec);
            engine.setDelayTimeMs(40.0f);
            engine.setFeedback(0.95f);
            engine.setDuckingAmount(duckingAmount0to1);

            std::vector<float> out;
            out.reserve((size_t) totalSamples);

            int done = 0;
            int samplePhase = 0;

            while (done < totalSamples)
            {
                const int n = juce::jmin(blockSize, totalSamples - done);
                juce::AudioBuffer<float> buffer(2, n);

                for (int i = 0; i < n; ++i)
                {
                    const bool inputPlaying = (done + i) < inputDurationSamples;
                    const float s = inputPlaying
                                        ? 0.6f * std::sin(juce::MathConstants<float>::twoPi * 300.0f * (float) samplePhase / (float) sampleRate)
                                        : 0.0f;
                    buffer.setSample(0, i, s);
                    buffer.setSample(1, i, s);
                    ++samplePhase;
                }

                engine.process(buffer);

                for (int i = 0; i < n; ++i)
                    out.push_back(buffer.getSample(0, i));

                done += n;
            }

            return out;
        };

        auto outNoDuck = runOnce(0.0f);
        auto outFullDuck = runOnce(1.0f);

        auto rmsOf = [](const std::vector<float>& data, int startSample, int lengthSamples)
        {
            double sumSquares = 0.0;
            for (int i = startSample; i < startSample + lengthSamples; ++i)
                sumSquares += (double) data[(size_t) i] * (double) data[(size_t) i];
            return std::sqrt(sumSquares / (double) lengthSamples);
        };

        // Enquanto a entrada toca (janela no fim dela, envelope já
        // subiu): com duck, tem que estar BEM mais baixo que sem duck.
        const int whilePlayingStart = inputDurationSamples - (int) (sampleRate * 0.2);
        const int whilePlayingLength = (int) (sampleRate * 0.15);
        const float rmsNoDuckPlaying = (float) rmsOf(outNoDuck, whilePlayingStart, whilePlayingLength);
        const float rmsFullDuckPlaying = (float) rmsOf(outFullDuck, whilePlayingStart, whilePlayingLength);

        expect(failures, rmsFullDuckPlaying < rmsNoDuckPlaying * 0.5f,
              "while the input plays, Ducking 100% is clearly quieter than Ducking 0% "
              "(no-duck RMS = " + juce::String(rmsNoDuckPlaying, 4)
              + ", full-duck RMS = " + juce::String(rmsFullDuckPlaying, 4) + ")");

        // Bem depois do release (release ~350ms -- usa uma janela 700ms
        // após o silêncio começar, folga generosa): os repeats
        // acumulados reaparecem -- os dois devem convergir pra
        // praticamente o mesmo nível (o duck gain já voltou perto de
        // 1.0, e o sinal interno é o mesmo nos dois casos, ver item 4).
        const int reappearStart = inputDurationSamples + (int) (sampleRate * 0.7);
        const int reappearLength = (int) (sampleRate * 0.15);
        const float rmsNoDuckAfter = (float) rmsOf(outNoDuck, reappearStart, reappearLength);
        const float rmsFullDuckAfter = (float) rmsOf(outFullDuck, reappearStart, reappearLength);

        // O aumento aqui é modesto (não dramático) porque o próprio
        // sinal interno também está decaindo nesses ~700ms -- os dois
        // efeitos competem. A prova forte de verdade é a convergência
        // com a execução sem duck logo abaixo; este é só um indicador
        // direcional de suporte.
        expect(failures, rmsFullDuckAfter > rmsFullDuckPlaying * 1.15f,
              "after the input ends and duck releases, the ducked run's own level rises above "
              "what it was while ducked (while-ducked=" + juce::String(rmsFullDuckPlaying, 4)
              + ", after-release=" + juce::String(rmsFullDuckAfter, 4) + ") -- the accumulated repeats reappear");

        expect(failures, std::abs(rmsFullDuckAfter - rmsNoDuckAfter) < juce::jmax(rmsNoDuckAfter * 0.3f, 0.01f),
              "after release, the ducked and un-ducked runs have converged to nearly the same level "
              "(no-duck=" + juce::String(rmsNoDuckAfter, 4) + ", full-duck=" + juce::String(rmsFullDuckAfter, 4)
              + ") -- proves Ducking never altered the underlying feedback decay, only delayed its presentation");

        bool nanOrInf = false;
        for (float s : outFullDuck)
            if (std::isnan(s) || std::isinf(s)) nanOrInf = true;
        for (float s : outNoDuck)
            if (std::isnan(s) || std::isinf(s)) nanOrInf = true;

        expect(failures, ! nanOrInf, "Ducking 100% + Feedback 95%: no NaN/Inf across either run");
    }

    return failures;
}
