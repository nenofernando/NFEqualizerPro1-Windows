// Testes obrigatórios da FASE 3, item 7 (host sync).
#include "AllTests.h"
#include "TestUtils.h"
#include "../Source/DSP/HostSync.h"
#include <cmath>
#include <limits>
#include <vector>

using namespace NF;
using NFTests::expect;

namespace
{
    bool close(double a, double b, double tolerance)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int runHostSyncTests()
{
    int failures = 0;

    std::cout << "=== Host Sync tests (FASE 3) ===" << std::endl << std::endl;

    // ------------------------------------------------------------
    // Valores exatos dados no briefing, a 120 BPM -- checagem
    // independente de "ground truth", não só reconferir a própria
    // fórmula.
    // ------------------------------------------------------------
    {
        std::cout << "-- Worked examples from the briefing (120 BPM) --" << std::endl;

        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1_4, SyncModifier::Straight), 500.0, 1e-9),
              "120 BPM, 1/4 Straight = 500 ms");
        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1_8, SyncModifier::Straight), 250.0, 1e-9),
              "120 BPM, 1/8 Straight = 250 ms");
        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1_8, SyncModifier::Dotted), 375.0, 1e-9),
              "120 BPM, 1/8 Dotted = 375 ms");
        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1_8, SyncModifier::Triplet), 166.66666666, 1e-6),
              "120 BPM, 1/8 Triplet ~= 166.667 ms");
    }

    // ------------------------------------------------------------
    // Mais valores calculados à mão (independentes da implementação)
    // pros outros BPMs pedidos.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Hand-computed reference values, other BPMs --" << std::endl;

        // 60 BPM -> quarterNoteMs = 1000 ms exato.
        expect(failures, close(syncDivisionMs(60.0, SyncDivision::Div1_4, SyncModifier::Straight), 1000.0, 1e-9),
              "60 BPM, 1/4 Straight = 1000 ms");
        expect(failures, close(syncDivisionMs(60.0, SyncDivision::Div1Bar, SyncModifier::Straight, 4, 4), 4000.0, 1e-9),
              "60 BPM, 1 Bar (4/4) Straight = 4000 ms");
        expect(failures, close(syncDivisionMs(60.0, SyncDivision::Div2Bar, SyncModifier::Straight, 4, 4), 8000.0, 1e-9),
              "60 BPM, 2 Bars (4/4) Straight = 8000 ms");

        // 90 BPM -> quarterNoteMs = 666.6667 ms.
        expect(failures, close(syncDivisionMs(90.0, SyncDivision::Div1_4, SyncModifier::Straight), 666.66666666, 1e-6),
              "90 BPM, 1/4 Straight ~= 666.667 ms");

        // 140 BPM -> quarterNoteMs = 428.5714 ms.
        expect(failures, close(syncDivisionMs(140.0, SyncDivision::Div1_16, SyncModifier::Straight), 107.142857, 1e-5),
              "140 BPM, 1/16 Straight ~= 107.143 ms");

        // 180 BPM -> quarterNoteMs = 333.3333 ms.
        expect(failures, close(syncDivisionMs(180.0, SyncDivision::Div1_2, SyncModifier::Straight), 666.66666666, 1e-6),
              "180 BPM, 1/2 Straight ~= 666.667 ms");

        // Assinatura de tempo diferente de 4/4 -- 3/4: 1 Bar = 3 quarters.
        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1Bar, SyncModifier::Straight, 3, 4), 1500.0, 1e-9),
              "120 BPM, 1 Bar em 3/4 = 1500 ms (3 x 500ms)");

        // 6/8: 1 Bar = 6 * (4/8) = 3 quarters (mesmo total que 3/4 em quarters).
        expect(failures, close(syncDivisionMs(120.0, SyncDivision::Div1Bar, SyncModifier::Straight, 6, 8), 1500.0, 1e-9),
              "120 BPM, 1 Bar em 6/8 = 1500 ms (3 quarters)");
    }

    // ------------------------------------------------------------
    // 1 Bar / 2 Bars respeitando a assinatura de tempo de verdade --
    // 4/4, 3/4, 6/8, 5/4, 7/8, a 120 BPM (quarterNoteMs = 500ms).
    // quartersPerBar = numerator * (4/denominator); barMs =
    // quarterNoteMs * quartersPerBar; 2 Bars = 2 x barMs.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- 1 Bar / 2 Bars across time signatures (120 BPM) --" << std::endl;

        struct SigCase { int numerator; int denominator; double expectedBarMs; };

        const SigCase cases[] {
            { 4, 4, 2000.0 },   // 4 quarters/bar
            { 3, 4, 1500.0 },   // 3 quarters/bar
            { 6, 8, 1500.0 },   // 6 * (4/8) = 3 quarters/bar
            { 5, 4, 2500.0 },   // 5 quarters/bar
            { 7, 8, 1750.0 },   // 7 * (4/8) = 3.5 quarters/bar
        };

        for (const auto& c : cases)
        {
            const double barMs = syncDivisionMs(120.0, SyncDivision::Div1Bar, SyncModifier::Straight, c.numerator, c.denominator);
            const double twoBarMs = syncDivisionMs(120.0, SyncDivision::Div2Bar, SyncModifier::Straight, c.numerator, c.denominator);

            const juce::String sig = juce::String(c.numerator) + "/" + juce::String(c.denominator);

            expect(failures, close(barMs, c.expectedBarMs, 1e-9),
                  "120 BPM, 1 Bar em " + sig + " = " + juce::String(c.expectedBarMs, 0) + " ms");
            expect(failures, close(twoBarMs, c.expectedBarMs * 2.0, 1e-9),
                  "120 BPM, 2 Bars em " + sig + " = " + juce::String(c.expectedBarMs * 2.0, 0) + " ms (2x 1 Bar)");
        }
    }

    // ------------------------------------------------------------
    // Invariantes independentes da fórmula -- relações que TÊM que
    // valer não importa como o cálculo é feito internamente. Isso
    // pega bugs que um teste "recalcula a mesma fórmula" não pegaria.
    // Cobre os 5 BPMs pedidos x todas as 8 divisões x os 3
    // modificadores (120 combinações).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Cross-checked invariants (120 BPM x division x modifier combinations) --" << std::endl;

        const double bpms[] { 60.0, 90.0, 120.0, 140.0, 180.0 };
        const SyncDivision divisions[] {
            SyncDivision::Div1_64, SyncDivision::Div1_32, SyncDivision::Div1_16, SyncDivision::Div1_8,
            SyncDivision::Div1_4, SyncDivision::Div1_2, SyncDivision::Div1Bar, SyncDivision::Div2Bar
        };

        int casesChecked = 0;
        bool allInvariantsHeld = true;

        for (double bpm : bpms)
        {
            const double quarterMs = quarterNoteMs(bpm);

            for (auto division : divisions)
            {
                const double straightMs = syncDivisionMs(bpm, division, SyncModifier::Straight);
                const double dottedMs = syncDivisionMs(bpm, division, SyncModifier::Dotted);
                const double tripletMs = syncDivisionMs(bpm, division, SyncModifier::Triplet);

                // Dotted = 1.5x Straight, Triplet = (2/3)x Straight --
                // pra qualquer divisão, sempre.
                if (! close(dottedMs, straightMs * 1.5, straightMs * 1e-6 + 1e-9)) allInvariantsHeld = false;
                if (! close(tripletMs, straightMs * (2.0 / 3.0), straightMs * 1e-6 + 1e-9)) allInvariantsHeld = false;

                casesChecked += 3;
            }

            // Cada divisão é exatamente o dobro da anterior na lista (todas
            // são potências de 2 relativas a 1/4, exceto Bar/2Bar que
            // dependem de compasso -- checados à parte).
            const double d64 = syncDivisionMs(bpm, SyncDivision::Div1_64, SyncModifier::Straight);
            const double d32 = syncDivisionMs(bpm, SyncDivision::Div1_32, SyncModifier::Straight);
            const double d16 = syncDivisionMs(bpm, SyncDivision::Div1_16, SyncModifier::Straight);
            const double d8  = syncDivisionMs(bpm, SyncDivision::Div1_8,  SyncModifier::Straight);
            const double d4  = syncDivisionMs(bpm, SyncDivision::Div1_4,  SyncModifier::Straight);
            const double d2  = syncDivisionMs(bpm, SyncDivision::Div1_2,  SyncModifier::Straight);
            const double bar1 = syncDivisionMs(bpm, SyncDivision::Div1Bar, SyncModifier::Straight, 4, 4);
            const double bar2 = syncDivisionMs(bpm, SyncDivision::Div2Bar, SyncModifier::Straight, 4, 4);

            if (! close(d32, d64 * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(d16, d32 * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(d8,  d16 * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(d4,  d8  * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(d2,  d4  * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(bar1, d4 * 4.0, 1e-9)) allInvariantsHeld = false;   // 4/4 default
            if (! close(bar2, bar1 * 2.0, 1e-9)) allInvariantsHeld = false;
            if (! close(d4, quarterMs, 1e-9)) allInvariantsHeld = false;   // 1/4 Straight == a própria nota de 1/4

            casesChecked += 8;
        }

        expect(failures, casesChecked == 5 * (8 * 3 + 8), "checked all 5 BPM x division x modifier combinations");
        expect(failures, allInvariantsHeld, "Dotted=1.5x/Triplet=2/3x Straight, and each division is exactly "
                                            "double the previous one, hold for every BPM/division tested");
    }

    // ------------------------------------------------------------
    // BPM inexistente/zero/inválido -- fallback seguro, sem
    // divisão por zero, sem NaN/Inf.
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- Missing / zero / invalid BPM --" << std::endl;

        const double invalidBpms[] {
            -1.0,                                  // sentinela "host não informou" (como PluginProcessor usa)
            0.0,                                    // BPM zero
            -120.0,                                 // BPM negativo
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity()
        };

        bool allFellBackTo120 = true;
        bool allFinite = true;

        for (double invalidBpm : invalidBpms)
        {
            const double sanitised = sanitiseBpm(invalidBpm);
            if (! close(sanitised, 120.0, 1e-9))
                allFellBackTo120 = false;

            const double ms = syncDivisionMs(invalidBpm, SyncDivision::Div1_4, SyncModifier::Straight);
            if (! std::isfinite(ms) || ms <= 0.0)
                allFinite = false;
        }

        expect(failures, allFellBackTo120, "invalid BPM (missing/zero/negative/NaN/Inf) always sanitises to 120 BPM");
        expect(failures, allFinite, "syncDivisionMs() with invalid BPM never produces NaN/Inf/non-positive");

        // Denominador de compasso zero/negativo também não pode gerar
        // divisão por zero.
        const double barWithBadDenominator = syncDivisionMs(120.0, SyncDivision::Div1Bar, SyncModifier::Straight, 4, 0);
        expect(failures, std::isfinite(barWithBadDenominator) && barWithBadDenominator > 0.0,
              "time signature denominator <= 0 falls back safely (no division by zero)");
    }

    // ------------------------------------------------------------
    // BPM mudando durante o "playback" -- chamadas sucessivas são
    // independentes (função pura, sem estado escondido).
    // ------------------------------------------------------------
    {
        std::cout << std::endl << "-- BPM changing mid-playback (statelessness) --" << std::endl;

        const double atStart = syncDivisionMs(100.0, SyncDivision::Div1_4, SyncModifier::Straight);
        const double atMiddle = syncDivisionMs(150.0, SyncDivision::Div1_4, SyncModifier::Straight);
        const double backToStart = syncDivisionMs(100.0, SyncDivision::Div1_4, SyncModifier::Straight);

        expect(failures, close(atStart, 600.0, 1e-9), "100 BPM, 1/4 Straight = 600 ms");
        expect(failures, close(atMiddle, 400.0, 1e-9), "150 BPM, 1/4 Straight = 400 ms");
        expect(failures, close(backToStart, atStart, 1e-9),
              "returning to the same BPM after a change gives the identical result (no leftover state)");
    }

    return failures;
}
