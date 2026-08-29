// Testes obrigatórios da FASE 4, itens 2 e 9 (filtros + estabilidade
// de ressonância). SVFFilter é testado isolado primeiro (sem
// DelayEngine, sem feedback) pra provar a matemática do filtro em si
// -- exatamente o método que o usuário pediu pra manter: testar o DSP
// isoladamente, não descobrir problemas só dentro de uma DAW.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/SVFFilter.h"
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

namespace
{
    // Mede o ganho linear (saída/entrada) de um filtro isolado em
    // regime permanente: alimenta um tom senoidal contínuo, descarta o
    // início (transiente de acomodação do filtro) e compara o RMS do
    // resto com o RMS teórico da entrada (amplitude/sqrt(2)).
    float steadyStateGain(SVFFilter& filter, double sampleRate, float toneHz, float amplitude, int totalSamples)
    {
        double sumSquares = 0.0;
        int counted = 0;
        const int discardSamples = totalSamples / 4;   // descarta o primeiro quarto (transiente)

        for (int i = 0; i < totalSamples; ++i)
        {
            const float in = amplitude * std::sin(juce::MathConstants<float>::twoPi * toneHz * (float) i / (float) sampleRate);
            const float out = filter.process(in);

            if (i >= discardSamples)
            {
                sumSquares += (double) out * (double) out;
                ++counted;
            }
        }

        const double outRms = std::sqrt(sumSquares / (double) counted);
        const double inRms = amplitude / std::sqrt(2.0);

        return (float) (outRms / inRms);
    }
}

int runFilterTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Filter tests (FASE 4) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Prova da fórmula de compensação de ganho -- isolada, sem
    // processar áudio nenhum, só a matemática (ver SVFFilter.h pra
    // derivação completa).
    // ------------------------------------------------------------
    {
        std::cout << "-- Resonance gain compensation formula --" << std::endl;

        // Abaixo do Q crítico (1/sqrt(2)): sem pico, compensação = 1.
        expect(failures, std::abs(SVFFilter::resonanceGainCompensationFor(0.5f) - 1.0f) < 1e-6f,
              "Q=0.5 (below critical Q): compensation = 1.0 (no boost needed)");

        // No Q crítico exato: peakGain=1, compensação=1.
        expect(failures, std::abs(SVFFilter::resonanceGainCompensationFor(0.70710678f) - 1.0f) < 1e-4f,
              "Q=1/sqrt(2) (critical Q): compensation ~= 1.0");

        // Em Q alto, compensação fica pequena (o pico é grande, então
        // compensa bastante) -- mas o PRODUTO compensação*peakGain tem
        // que dar exatamente 1.0, pra qualquer Q.
        bool productIsOne = true;
        for (float q : { 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f })
        {
            const float peakGain = q / std::sqrt(1.0f - 1.0f / (4.0f * q * q));
            const float compensation = SVFFilter::resonanceGainCompensationFor(q);

            if (std::abs(peakGain * compensation - 1.0f) > 1e-4f)
                productIsOne = false;
        }
        expect(failures, productIsOne, "compensation * peakGain == 1.0 exactly, for Q from 1 to 50 "
                                      "(this is the ||H||inf <= 1 guarantee)");
    }

    // ------------------------------------------------------------
    // HPF reduz conteúdo ABAIXO do cutoff, deixa passar ACIMA.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- HPF: 20 Hz and 1 kHz cutoff --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float amplitude = 0.5f;
        constexpr int totalSamples = 48000;   // 1s -- vários ciclos mesmo nas frequências mais baixas testadas

        {
            SVFFilter hpf;
            hpf.prepare(sampleRate);
            hpf.setMode(SVFFilter::Mode::Highpass);
            hpf.setCutoffAndQ(1000.0f, 0.7071f);   // Q baixo, sem pico de ressonância atrapalhando a medida

            const float gainBelow = steadyStateGain(hpf, sampleRate, 100.0f, amplitude, totalSamples);
            expect(failures, gainBelow < 0.3f, "HPF 1kHz: 100Hz (well below cutoff) is attenuated, gain = "
                                              + juce::String(gainBelow, 4));
        }
        {
            SVFFilter hpf;
            hpf.prepare(sampleRate);
            hpf.setMode(SVFFilter::Mode::Highpass);
            hpf.setCutoffAndQ(1000.0f, 0.7071f);

            const float gainAbove = steadyStateGain(hpf, sampleRate, 8000.0f, amplitude, totalSamples);
            expect(failures, gainAbove > 0.85f, "HPF 1kHz: 8kHz (well above cutoff) passes through, gain = "
                                               + juce::String(gainAbove, 4));
        }
        {
            // Cutoff no mínimo (20Hz) -- praticamente tudo no espectro
            // audível deve passar.
            SVFFilter hpf;
            hpf.prepare(sampleRate);
            hpf.setMode(SVFFilter::Mode::Highpass);
            hpf.setCutoffAndQ(20.0f, 0.7071f);

            const float gainMid = steadyStateGain(hpf, sampleRate, 1000.0f, amplitude, totalSamples);
            expect(failures, gainMid > 0.9f, "HPF 20Hz (minimum): 1kHz passes through almost untouched, gain = "
                                            + juce::String(gainMid, 4));
        }
    }

    // ------------------------------------------------------------
    // LPF reduz conteúdo ACIMA do cutoff, deixa passar ABAIXO.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- LPF: 2 kHz and 20 kHz cutoff --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float amplitude = 0.5f;
        constexpr int totalSamples = 48000;

        {
            SVFFilter lpf;
            lpf.prepare(sampleRate);
            lpf.setMode(SVFFilter::Mode::Lowpass);
            lpf.setCutoffAndQ(2000.0f, 0.7071f);

            const float gainBelow = steadyStateGain(lpf, sampleRate, 200.0f, amplitude, totalSamples);
            expect(failures, gainBelow > 0.85f, "LPF 2kHz: 200Hz (well below cutoff) passes through, gain = "
                                               + juce::String(gainBelow, 4));
        }
        {
            SVFFilter lpf;
            lpf.prepare(sampleRate);
            lpf.setMode(SVFFilter::Mode::Lowpass);
            lpf.setCutoffAndQ(2000.0f, 0.7071f);

            const float gainAbove = steadyStateGain(lpf, sampleRate, 12000.0f, amplitude, totalSamples);
            expect(failures, gainAbove < 0.3f, "LPF 2kHz: 12kHz (well above cutoff) is attenuated, gain = "
                                              + juce::String(gainAbove, 4));
        }
        {
            SVFFilter lpf;
            lpf.prepare(sampleRate);
            lpf.setMode(SVFFilter::Mode::Lowpass);
            lpf.setCutoffAndQ(20000.0f, 0.7071f);

            const float gainMid = steadyStateGain(lpf, sampleRate, 1000.0f, amplitude, totalSamples);
            expect(failures, gainMid > 0.9f, "LPF 20kHz (maximum): 1kHz passes through almost untouched, gain = "
                                            + juce::String(gainMid, 4));
        }
    }

    // ------------------------------------------------------------
    // RESO altera a região perto do cutoff -- 0%, 50%, 100% (mapeados
    // pra Q 0.5/10.75/20 -- ver DelayEngine::minResonanceQ/maxResonanceQ).
    // Ganho no cutoff deve CRESCER com o Q (pico de ressonância ficando
    // mais evidente), mas NUNCA passar de ~1.0 (a compensação de ganho
    // em ação, mesmo no Q máximo).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- RESO: 0% / 50% / 100%, gain AT the cutoff frequency --" << std::endl;

        constexpr double sampleRate = 48000.0;
        constexpr float amplitude = 0.5f;
        constexpr float cutoffHz = 1000.0f;
        constexpr int totalSamples = 96000;   // mais ciclos -- filtros de Q alto assentam mais devagar

        const float qValues[] { 0.5f, 10.75f, 20.0f };   // DelayEngine::minResonanceQ, meio, maxResonanceQ
        float gains[3];

        for (int i = 0; i < 3; ++i)
        {
            SVFFilter lpf;
            lpf.prepare(sampleRate);
            lpf.setMode(SVFFilter::Mode::Lowpass);
            lpf.setCutoffAndQ(cutoffHz, qValues[i]);

            gains[i] = steadyStateGain(lpf, sampleRate, cutoffHz, amplitude, totalSamples);

            expect(failures, gains[i] <= 1.05f, "Reso Q=" + juce::String(qValues[i], 2)
                                               + ": gain AT cutoff stays <= ~1.0 (compensated), got "
                                               + juce::String(gains[i], 4));
        }

        expect(failures, gains[1] > gains[0] && gains[2] >= gains[1],
              "gain at cutoff increases with Q (0%=" + juce::String(gains[0], 4)
              + " < 50%=" + juce::String(gains[1], 4) + " <= 100%=" + juce::String(gains[2], 4)
              + ") -- resonance is audibly evident, not neutered by the safety compensation");
    }

    // ------------------------------------------------------------
    // Prova de estabilidade do item 2: Feedback=95% + Resonance=100%,
    // por muito tempo, através do DelayEngine de verdade (filtros no
    // caminho do feedback) -- não pode dar runaway.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Feedback 95% + Resonance 100%: no runaway (real DelayEngine, long run) --" << std::endl;

        constexpr double sampleRate = 48000.0;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
        engine.prepare(spec);
        engine.setDelayTimeMs(30.0f);   // curto -- muitos repeats numa janela razoável
        engine.setFeedback(0.95f);
        engine.setHighPassHz(20.0f);    // HPF no mínimo (não atrapalha o teste de reso do LPF)
        engine.setLowPassHz(1000.0f);
        engine.setResonance(1.0f);      // 100% -- Q máximo

        constexpr int totalSamples = (int) (sampleRate * 5.0);   // 5 segundos

        bool allocationDetected = false;
        bool nanOrInfDetected = false;
        float peakMagnitude = 0.0f;

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
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float s = buffer.getSample(ch, i);
                    if (std::isnan(s) || std::isinf(s))
                        nanOrInfDetected = true;
                    peakMagnitude = juce::jmax(peakMagnitude, std::abs(s));
                }
            }

            done += n;
        }

        expect(failures, ! allocationDetected, "Feedback 95% + Reso 100%, 5s: no allocation in process()");
        expect(failures, ! nanOrInfDetected, "Feedback 95% + Reso 100%, 5s: no NaN/Inf");
        expect(failures, peakMagnitude < 3.0f, "Feedback 95% + Reso 100%, 5s: peak magnitude stays bounded "
                                              "(no runaway/oscillator), got " + juce::String(peakMagnitude, 4));
    }

    return failures;
}
