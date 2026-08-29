// Testes obrigatórios da FASE 5, itens 15 e 17: stress extremo (tudo
// no limite ao mesmo tempo, Analog e Tape) e a grade de sample rates x
// buffer sizes com Ducking+Lo-Fi+Character ativos.
#include "AllTests.h"
#include "TestUtils.h"
#include "AllocationGuard.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/DSP/HostSync.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

using namespace NF;
using NFTests::expect;
using NFTests::gGuardActive;
using NFTests::gAllocationSeenInGuard;

namespace
{
    bool runExtremeStress(CharacterMode mode, float& outPeak)
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        DelayEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2u };
        engine.prepare(spec);

        engine.setDelayTimeMs(90.0f);
        engine.setFeedback(0.95f);
        engine.setPingPong(true);
        engine.setHighPassHz(150.0f);
        engine.setLowPassHz(4000.0f);
        engine.setResonance(1.0f);
        engine.setModRate(10.0f);
        engine.setModDepth(1.0f);
        engine.setModShape(ModShape::SoftRandom);
        engine.setModSpread(1.0f);
        engine.setDuckingAmount(0.5f);
        engine.setLoFiEnabled(true);
        engine.setCharacterMode(mode);
        engine.setCharacterAmount(1.0f);

        juce::Random rng(3141 + (int) mode);
        bool allocationDetected = false, nanOrInfDetected = false;
        float peakMagnitude = 0.0f;

        const NF::SyncDivision divisions[] { SyncDivision::Div1_16, SyncDivision::Div1_4, SyncDivision::Div1_2 };
        const CharacterMode modes[] { CharacterMode::Digital, CharacterMode::Analog, CharacterMode::Tape };

        constexpr int totalSamples = (int) (sampleRate * 8.0);
        int done = 0;
        int blockIndex = 0;

        while (done < totalSamples)
        {
            const int n = juce::jmin(blockSize, totalSamples - done);

            if (blockIndex % 12 == 0)
            {
                const double ms = NF::syncDivisionMs(120.0, divisions[blockIndex % 3], SyncModifier::Straight);
                engine.setDelayTimeMs((float) ms);
            }
            if (blockIndex % 20 == 0)
                engine.setCharacterMode(modes[(blockIndex / 20) % 3]);

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

        outPeak = peakMagnitude;
        return (! allocationDetected) && (! nanOrInfDetected);
    }
}

int runPhase5StressTests()
{
    int failures = 0;

    std::cout << std::endl << "=== Phase 5 stress tests (itens 15 e 17) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Item 15: tudo no extremo, Analog e depois Tape, com mudanças
    // periódicas de delay time (sync), divisão e modo de character.
    // ------------------------------------------------------------
    {
        std::cout << "-- Extreme stress: Feedback 95%, Reso 100%, ModDepth 100%, ModRate 10Hz, "
                     "Lo-Fi ON, Character 100% -- Analog and Tape, periodic time/division/mode changes --" << std::endl;

        float peakAnalog = 0.0f;
        const bool okAnalog = runExtremeStress(CharacterMode::Analog, peakAnalog);
        expect(failures, okAnalog, "extreme stress starting in Analog, 8s: no allocation, no NaN/Inf");
        expect(failures, peakAnalog < 6.0f, "extreme stress starting in Analog, 8s: output stays bounded, peak = "
                                            + juce::String(peakAnalog, 4));

        float peakTape = 0.0f;
        const bool okTape = runExtremeStress(CharacterMode::Tape, peakTape);
        expect(failures, okTape, "extreme stress starting in Tape, 8s: no allocation, no NaN/Inf");
        expect(failures, peakTape < 6.0f, "extreme stress starting in Tape, 8s: output stays bounded, peak = "
                                          + juce::String(peakTape, 4));
    }

    // ------------------------------------------------------------
    // Item 17: sample rates x buffer sizes, com Ducking+Lo-Fi+
    // Character (+ filtros + modulação da FASE 4) todos ativos.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Sample rate x buffer size grid, full FASE 5 feature set active --" << std::endl;

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

                engine.setDelayTimeMs(120.0f);
                engine.setFeedback(0.75f);
                engine.setPingPong(true);
                engine.setHighPassHz(120.0f);
                engine.setLowPassHz(7000.0f);
                engine.setResonance(0.5f);
                engine.setModRate(4.0f);
                engine.setModDepth(0.6f);
                engine.setModShape(ModShape::Sine);
                engine.setModSpread(0.6f);
                engine.setDuckingAmount(0.4f);
                engine.setLoFiEnabled(true);
                engine.setCharacterMode(CharacterMode::Analog);
                engine.setCharacterAmount(0.7f);

                juce::Random rng(500 + (int) sampleRate + blockSize);
                bool nanOrInf = false, outOfRange = false, allocationDetected = false;

                const int totalSamples = (int) (sampleRate * 0.5);
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
                            if (std::abs(s) > 6.0f) outOfRange = true;
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
        expect(failures, allOk, "all 20 combinations: no NaN/Inf, no blow-up, no allocation, with the full "
                               "FASE 5 feature set active (ducking + lo-fi + character + filters + modulation)");
    }

    return failures;
}
