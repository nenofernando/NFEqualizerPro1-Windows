#include "HostSync.h"
#include <cmath>

namespace NF
{
double sanitiseBpm(double hostBpmOrInvalid) noexcept
{
    if (! std::isfinite(hostBpmOrInvalid) || hostBpmOrInvalid <= 0.0)
        return fallbackBpm;

    return hostBpmOrInvalid;
}

double quarterNoteMs(double bpm) noexcept
{
    const double safeBpm = sanitiseBpm(bpm);
    return 60000.0 / safeBpm;
}

double syncDivisionMs(double bpm, SyncDivision division, SyncModifier modifier,
                      int timeSigNumerator, int timeSigDenominator) noexcept
{
    const double qNoteMs = quarterNoteMs(bpm);

    // Quantas notas de 1/4 cada divisão vale, relativo a uma nota de 1/4
    // (1/4 = 1.0). Compasso só entra pra Bar/2 Bars.
    double quartersInDivision = 1.0;

    switch (division)
    {
        case SyncDivision::Div1_64: quartersInDivision = 1.0 / 16.0; break;
        case SyncDivision::Div1_32: quartersInDivision = 1.0 / 8.0;  break;
        case SyncDivision::Div1_16: quartersInDivision = 1.0 / 4.0;  break;
        case SyncDivision::Div1_8:  quartersInDivision = 1.0 / 2.0;  break;
        case SyncDivision::Div1_4:  quartersInDivision = 1.0;        break;
        case SyncDivision::Div1_2:  quartersInDivision = 2.0;        break;

        case SyncDivision::Div1Bar:
        case SyncDivision::Div2Bar:
        {
            const int numerator = timeSigNumerator > 0 ? timeSigNumerator : 4;
            const int denominator = timeSigDenominator > 0 ? timeSigDenominator : 4;

            // Quantas notas de 1/4 cabem num compasso nessa assinatura de
            // tempo -- ex.: 4/4 = 4 quarters, 3/4 = 3, 6/8 = 3.
            const double quartersPerBar = (double) numerator * (4.0 / (double) denominator);

            quartersInDivision = (division == SyncDivision::Div1Bar)
                                      ? quartersPerBar
                                      : quartersPerBar * 2.0;
            break;
        }
    }

    double modifierMultiplier = 1.0;

    switch (modifier)
    {
        case SyncModifier::Straight: modifierMultiplier = 1.0;        break;
        case SyncModifier::Dotted:   modifierMultiplier = 1.5;        break;
        case SyncModifier::Triplet:  modifierMultiplier = 2.0 / 3.0;  break;
    }

    return qNoteMs * quartersInDivision * modifierMultiplier;
}
}
