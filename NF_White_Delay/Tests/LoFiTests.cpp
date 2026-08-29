// Testes do Lo-Fi -- reescrito na FASE 6.6 (a versão da FASE 5 tinha
// um piso de ruído de dither audível, removido por completo junto
// com a quantização de bit-depth -- ver nota no topo de
// LoFiProcessor.h). Cobre: (1) o decaimento normal em Feedback=95%
// continua correto sem a quantização, (2) silêncio precisa dar
// EXATAMENTE zero agora (não mais "piso de dither desprezível" --
// sem nenhuma fonte de ruído no processador, isso é garantido por
// construção, e o teste confirma isso na prática), (3) comparação
// musical OFF vs ON (RMS não aumenta, conteúdo de alta frequência
// cai -- "mais estreito, vintage, suave", não "o mesmo delay + chiado").
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/DSP/LoFiProcessor.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

using namespace NF;
using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

int runLoFiTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Lo-Fi tests (FASE 6.6 -- sem dither/quantizacao) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Impulso, Feedback=95%, Lo-Fi=ON, rodado por bastante tempo --
    // sem quantização no loop, o decaimento é só o feedbackGain<0.95
    // geométrico de sempre (mesma prova de estabilidade da FASE 3/4),
    // mas confirma que a cadeia nova (HPF+antialiasLPF+hold+LPF de
    // amaciamento com micro-instabilidade+saturação) não introduz
    // nenhum comportamento preso/oscilante.
    // ------------------------------------------------------------
    {
        std::cout << "-- Impulse, Feedback=95%, Lo-Fi=ON: converges to negligible level --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(15.0f);   // curto -- muitos repeats numa janela razoável
        engine.setFeedback(0.95f);
        engine.setLoFiEnabled(true);

        constexpr int totalSamples = (int) (sampleRate * 6.0);   // 6 segundos -- centenas de repeats

        std::vector<float> out;
        out.reserve((size_t) totalSamples);

        bool allocationDetected = false;
        bool nanOrInfDetected = false;

        int done = 0;
        bool first = true;

        while (done < totalSamples)
        {
            const int n = juce::jmin(blockSize, totalSamples - done);
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

        expect(failures, ! allocationDetected, "Feedback 95% + Lo-Fi ON, 6s: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "Feedback 95% + Lo-Fi ON, 6s: no NaN/Inf");

        auto rmsOfWindow = [&](int start, int length)
        {
            double sumSquares = 0.0;
            for (int i = start; i < start + length; ++i)
                sumSquares += (double) out[(size_t) i] * (double) out[(size_t) i];
            return std::sqrt(sumSquares / (double) length);
        };

        const int windowSize = (int) (sampleRate * 0.2);   // janelas de 200ms
        bool allWindowsNegligible = true;
        float worstWindowRms = 0.0f;

        for (int start = totalSamples - windowSize * 5; start < totalSamples - windowSize; start += windowSize)
        {
            const float windowRms = (float) rmsOfWindow(start, windowSize);
            worstWindowRms = juce::jmax(worstWindowRms, windowRms);

            if (windowRms > 0.02f)
                allWindowsNegligible = false;
        }

        expect(failures, allWindowsNegligible,
              "every 200ms window in the last second of a 6s run has RMS below 0.02 (negligible) -- "
              "no stuck oscillation (a limit cycle would show a SUSTAINED non-negligible RMS in these "
              "late windows, not a decaying one), worst window RMS = " + juce::String(worstWindowRms, 5));

        const float rmsEarly = (float) rmsOfWindow(totalSamples - windowSize * 5, windowSize);
        const float rmsLate = (float) rmsOfWindow(totalSamples - windowSize, windowSize);

        expect(failures, rmsLate <= rmsEarly * 1.5f,
              "RMS in the last window is not higher than earlier windows (still decaying/settled, not "
              "growing) -- early=" + juce::String(rmsEarly, 5) + ", late=" + juce::String(rmsLate, 5));
    }

    // ------------------------------------------------------------
    // Silêncio total (sem impulso) através de Lo-Fi+Feedback 95% --
    // AGORA precisa dar EXATAMENTE zero (tolerância só de erro de
    // ponto flutuante), não mais "piso de dither desprezível": sem
    // nenhuma fonte de ruído no LoFiProcessor (nem juce::Random, nem
    // dither), todo estágio é linear ou tanh(0)=0 -- entrada 0 +
    // estado inicial 0 tem que devolver 0 pra sempre, por construção.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Digital silence in: stays EXACTLY zero through Lo-Fi + Feedback 95% --" << std::endl;

        constexpr double sampleRate = 48000.0;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(20.0f);
        engine.setFeedback(0.95f);
        engine.setLoFiEnabled(true);

        constexpr int totalSamples = (int) (sampleRate * 2.0);
        float peakMagnitude = 0.0f;
        int done = 0;

        while (done < totalSamples)
        {
            const int n = juce::jmin(512, totalSamples - done);
            juce::AudioBuffer<float> buffer(2, n);
            buffer.clear();

            engine.process(buffer);

            for (int i = 0; i < n; ++i)
                peakMagnitude = juce::jmax(peakMagnitude, std::abs(buffer.getSample(0, i)));

            done += n;
        }

        expect(failures, peakMagnitude < 1.0e-6f,
              "pure digital silence through Lo-Fi + Feedback 95% stays EXACTLY zero (no dither, no "
              "quantization, no noise source at all -- linear filters and tanh(0)=0 can't generate signal "
              "from nothing), peak = " + juce::String(peakMagnitude, 8));
    }

    // ------------------------------------------------------------
    // Lo-Fi isolado em silêncio -- mesma garantia, mas no processador
    // sozinho (sem o resto do motor/feedback), pra isolar a fonte:
    // confirma que é o LoFiProcessor em si que nunca gera ruído, não
    // uma coincidência de outro estágio mascarando algo.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- LoFiProcessor isolated: silence stays exactly zero, any input, any state --" << std::endl;

        LoFiProcessor loFi;
        loFi.prepare(48000.0);

        juce::Random rng(99);
        // Alimenta ruído por um tempo pra "sujar" todo o estado interno
        // (HPF/anti-alias/hold/amaciamento), depois para totalmente.
        for (int i = 0; i < 20000; ++i)
        {
            loFi.process(0, (rng.nextFloat() * 2.0f - 1.0f) * 0.8f);
            loFi.process(1, (rng.nextFloat() * 2.0f - 1.0f) * 0.8f);
        }

        // O DECAIMENTO em si leva um tempo real (os filtros têm
        // "memória" -- não zeram instantaneamente no primeiro sample
        // de silêncio, isso seria fisicamente impossível pra qualquer
        // filtro com estado). O que importa é que CONVERGE de fato,
        // não que já está zero desde o primeiro instante -- por isso
        // medimos o pico só nas ÚLTIMAS 1000 amostras de uma janela de
        // 20000 (bem além de qualquer constante de tempo envolvida
        // aqui -- HPF 110Hz/anti-alias 9kHz/amaciamento ~7kHz convergem
        // dentro de poucas centenas de amostras a 48kHz).
        constexpr int totalSilentSamples = 20000;
        constexpr int tailWindowSamples = 1000;
        float maxInFinalWindow = 0.0f;

        for (int i = 0; i < totalSilentSamples; ++i)
        {
            const float a = loFi.process(0, 0.0f);
            const float b = loFi.process(1, 0.0f);
            if (i >= totalSilentSamples - tailWindowSamples)
                maxInFinalWindow = juce::jmax(maxInFinalWindow, std::abs(a), std::abs(b));
        }

        expect(failures, maxInFinalWindow < 1.0e-6f,
              "LoFiProcessor with dirty internal state, fed pure silence: output settles to EXACTLY "
              "zero (all internal stages are linear filters + tanh(0)=0, no noise source), "
              "peak in the final 1000 of 20000 silent samples = " + juce::String(maxInFinalWindow, 8));
    }

    // ------------------------------------------------------------
    // Comparação musical: Digital + Lo-Fi OFF vs ON. O que precisa
    // acontecer: RMS não aumenta (Lo-Fi é uma cadeia de contrações,
    // nunca ganha nível) e conteúdo de ALTA frequência cai
    // claramente (o "menos brilho" pedido) -- não "o mesmo delay +
    // chiado" (isso seria RMS igual/maior e sem mudança espectral).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Musical comparison: Digital + Lo-Fi OFF vs ON --" << std::endl;

        auto measure = [&](bool loFiOn, float toneHz) -> float
        {
            constexpr double sampleRate = 48000.0;
            constexpr int blockSize = 512;

            DelayEngine engine;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
            engine.prepare(spec);
            engine.setDelayTimeMs(120.0f);
            engine.setFeedback(0.5f);
            engine.setCharacterMode(CharacterMode::Digital);
            engine.setCharacterAmount(0.0f);
            if (loFiOn)
                engine.setLoFiEnabled(true);

            constexpr float amplitude = 0.4f;
            const int warmupSamples = (int) (sampleRate * 0.5);
            const int measureSamples = (int) (sampleRate * 0.2);

            int phase = 0;
            int done = 0;
            while (done < warmupSamples)
            {
                const int n = juce::jmin(blockSize, warmupSamples - done);
                juce::AudioBuffer<float> buffer(2, n);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        buffer.setSample(ch, i, amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) (phase + i) / (float) sampleRate));
                engine.process(buffer);
                phase += n;
                done += n;
            }

            double sumSq = 0.0;
            int count = 0;
            done = 0;
            while (done < measureSamples)
            {
                const int n = juce::jmin(blockSize, measureSamples - done);
                juce::AudioBuffer<float> buffer(2, n);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        buffer.setSample(ch, i, amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) (phase + i) / (float) sampleRate));
                engine.process(buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        const double s = buffer.getSample(ch, i);
                        sumSq += s * s;
                        ++count;
                    }
                phase += n;
                done += n;
            }

            return (float) std::sqrt(sumSq / count);
        };

        // Tom grave/médio (1kHz) -- não deve perder muito nível, só o
        // que os estágios de contração naturalmente tiram.
        const float rmsOff1k = measure(false, 1000.0f);
        const float rmsOn1k = measure(true, 1000.0f);

        // Tom agudo (12kHz) -- é aqui que "menos brilho" precisa
        // aparecer claramente (LPF de amaciamento ~7kHz + anti-alias
        // ~9kHz atenuam isso bastante).
        const float rmsOff12k = measure(false, 12000.0f);
        const float rmsOn12k = measure(true, 12000.0f);

        std::cout << "   1kHz: OFF=" << rmsOff1k << " ON=" << rmsOn1k << std::endl;
        std::cout << "   12kHz: OFF=" << rmsOff12k << " ON=" << rmsOn12k << std::endl;

        expect(failures, rmsOn1k <= rmsOff1k * 1.02f,
               "Lo-Fi ON never gets LOUDER than OFF at 1kHz (contraction-only chain), OFF="
               + juce::String(rmsOff1k, 5) + " ON=" + juce::String(rmsOn1k, 5));
        expect(failures, rmsOn12k < rmsOff12k * 0.7f,
               "Lo-Fi ON is clearly darker than OFF at 12kHz (less brightness, not \"same delay + hiss\"), "
               "OFF=" + juce::String(rmsOff12k, 5) + " ON=" + juce::String(rmsOn12k, 5));
    }

    // ------------------------------------------------------------
    // Verificação pedida antes de considerar o Lo-Fi definitivo: com
    // downsampleFactor FIXO, a taxa efetiva do hold escalava com o
    // sample rate da sessão (44.1kHz/4≈11kHz ... 192kHz/4=48kHz),
    // esvaziando o caráter nas taxas altas. Corrigido: downsampleFactor
    // agora é calculado a partir do sample rate, mirando uma taxa
    // efetiva ~constante (~12kHz). Este teste mede a CONTRIBUIÇÃO real
    // do hold (diferença RMS entre "com hold" via o fator de produção
    // e "sem hold" via setDownsampleFactorForTesting(1), mesmo tom de
    // teste) em 44.1/48/96/192kHz -- precisa ficar CONSISTENTE entre
    // as taxas, não caindo como antes.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Sample-rate consistency: hold contribution across 44.1/48/96/192kHz --" << std::endl;

        auto measureHoldContributionRms = [](double sampleRate) -> float
        {
            LoFiProcessor withHold, withoutHold;
            withHold.prepare(sampleRate);
            withoutHold.prepare(sampleRate);
            withoutHold.setDownsampleFactorForTesting(1);

            constexpr float toneHz = 5000.0f;
            constexpr float amplitude = 0.4f;
            const int warmup = (int) (sampleRate * 0.05);
            const int measure = (int) (sampleRate * 0.05);

            int phase = 0;
            for (int i = 0; i < warmup; ++i)
            {
                const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                withHold.process(0, s);
                withoutHold.process(0, s);
                ++phase;
            }

            double sumSqDiff = 0.0;
            int count = 0;
            for (int i = 0; i < measure; ++i)
            {
                const float s = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) phase / (float) sampleRate);
                const float a = withHold.process(0, s);
                const float b = withoutHold.process(0, s);
                const double d = (double) a - (double) b;
                sumSqDiff += d * d;
                ++count;
                ++phase;
            }
            return (float) std::sqrt(sumSqDiff / count);
        };

        const double rates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
        float contributions[4] {};
        for (int i = 0; i < 4; ++i)
            contributions[i] = measureHoldContributionRms(rates[i]);

        std::cout << "   hold contribution (RMS diff, 5kHz tone): 44.1kHz=" << contributions[0]
                  << " 48kHz=" << contributions[1] << " 96kHz=" << contributions[2]
                  << " 192kHz=" << contributions[3] << std::endl;

        const float minContribution = juce::jmin(contributions[0], contributions[1], contributions[2], contributions[3]);
        const float maxContribution = juce::jmax(contributions[0], contributions[1], contributions[2], contributions[3]);

        expect(failures, minContribution > 0.1f,
               "hold contribution stays clearly audible at every sample rate (> 0.1), worst = "
               + juce::String(minContribution, 4));
        expect(failures, minContribution > maxContribution * 0.6f,
               "hold contribution stays consistent across sample rates (worst/best ratio > 0.6 -- "
               "before the fix, 192kHz was only 22% of 44.1kHz), min=" + juce::String(minContribution, 4)
               + " max=" + juce::String(maxContribution, 4));
    }

    return failures;
}
