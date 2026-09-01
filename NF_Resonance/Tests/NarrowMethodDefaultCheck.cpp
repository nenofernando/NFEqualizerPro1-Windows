// Confirms SpectralProminenceEngineV5's default NarrowMethod is the
// actually-validated A5C combination (O1_RobustSideSlope), not silently
// falling back to an unvalidated method for this geometry. Every future
// caller (Sonic Alpha included) should still call setNarrowMethod()
// explicitly and assert activeNarrowMethod() -- this test exists so a
// regression in the DEFAULT itself is caught immediately, independent of
// any one caller's own explicit setting.
#include <JuceHeader.h>
#include "DSP/SpectralProminenceEngineV5.h"

int main()
{
    using NM = SpectralProminenceEngineV5::NarrowMethod;
    SpectralProminenceEngineV5 eng;
    eng.prepare(1025, 48000.0, 2048);
    bool defaultOk = eng.activeNarrowMethod() == NM::O1_RobustSideSlope;
    std::printf("Fresh SpectralProminenceEngineV5 default NarrowMethod = %s (expected O1_RobustSideSlope)\n",
        defaultOk ? "O1_RobustSideSlope" : "SOMETHING ELSE");
    std::printf("%s\n", defaultOk ? "PASS: default matches the validated A5C combination." : "FAIL: default drifted from the validated A5C combination.");
    return defaultOk ? 0 : 1;
}
