#include "TransientProtectionEngine.h"
#include <cmath>
#include <algorithm>

void TransientProtectionEngine::prepare(double sr, int fft, int hop)
{
    sampleRate = sr; fftSize = fft; hopSize = hop; bins = fft / 2 + 1;
    bandMagDb.assign((size_t) bins, -120.0f);
    prevMagDb.assign((size_t) bins, -120.0f);
    fastEnvLin.assign((size_t) bins, 0.0f);
    slowEnvLin.assign((size_t) bins, 0.0f);
    transientEvidence.assign((size_t) bins, 0.0f);
    protection.assign((size_t) bins, 0.0f);

    double hopMs = 1000.0 * (double) hopSize / sampleRate;
    // Fixed ~15ms/~200ms short-term-vs-long-term envelope pair for the
    // rise-ratio feature (separate from the caller-configurable
    // attack/release below, which shapes the OUTPUT protection envelope,
    // not this internal feature-extraction envelope pair).
    fastEnvCoeff = std::exp(-hopMs / 15.0);
    slowEnvCoeff = std::exp(-hopMs / 200.0);

    setTransientTimeConstants(5.0f, 60.0f); // provisional defaults, see header -- item 9/5 measure the actual result
    primed = false;
}

void TransientProtectionEngine::reset()
{
    std::fill(bandMagDb.begin(), bandMagDb.end(), -120.0f);
    std::fill(prevMagDb.begin(), prevMagDb.end(), -120.0f);
    std::fill(fastEnvLin.begin(), fastEnvLin.end(), 0.0f);
    std::fill(slowEnvLin.begin(), slowEnvLin.end(), 0.0f);
    std::fill(transientEvidence.begin(), transientEvidence.end(), 0.0f);
    std::fill(protection.begin(), protection.end(), 0.0f);
    primed = false;
}

void TransientProtectionEngine::setTransientTimeConstants(float attackMs, float releaseMs)
{
    double hopMs = 1000.0 * (double) hopSize / sampleRate;
    attackCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) attackMs));
    releaseCoeff = (float) std::exp(-hopMs / juce::jmax(0.1, (double) releaseMs));
}

// ~2dB -> 0, ~15dB -> 1 per-frame positive flux: a real attack's onset
// frame commonly jumps far more than this in the affected band; a
// naturally busy-but-not-transient mix rarely produces a single-frame
// positive jump this large in the same bin two frames running.
float TransientProtectionEngine::dbFluxToEvidence(float fluxDb)
{
    return juce::jlimit(0.0f, 1.0f, (fluxDb - 2.0f) / 13.0f);
}

// fast/slow linear-energy ratio: 1.0 (no rise) -> 0, 3.0x -> 1.
float TransientProtectionEngine::riseRatioToEvidence(float ratio)
{
    return juce::jlimit(0.0f, 1.0f, (ratio - 1.0f) / 2.0f);
}

int TransientProtectionEngine::nearestBin(float hz) const
{
    return juce::jlimit(0, bins - 1, (int) std::round((double) hz / binHz()));
}

void TransientProtectionEngine::process(const std::vector<float>& magDb)
{
    if ((int) magDb.size() != bins) return; // defensive: caller must match prepare()'s fftSize

    // Band-smooth (3-bin box-car, LINEAR power domain) before doing anything
    // else. A non-bin-aligned low-frequency tone's own leakage skirt
    // interferes with a NEIGHBOURING harmonic's leakage skirt at the bins
    // between them -- since the two partials aren't bin-locked, their
    // relative phase there rotates every hop (a genuine STFT property, not
    // a bug), producing real, double-digit-dB, frame-to-frame magnitude
    // swings at that ONE bin even while the partials' OWN peak bins stay
    // rock-stable (measured directly: an 8-partial clean 110Hz series held
    // its own h1/h2 bins within 0.03dB frame-to-frame, but the interference
    // bin between them swung up to 16dB, single-handedly driving spurious
    // transientProtection up to ~0.7 through the +-2 bin query window).
    // Summing power over a small band damps any ONE bin's interference
    // swing relative to its more-stable neighbours, without needing extra
    // per-query state.
    for (int b = 0; b < bins; ++b)
    {
        double sumLinPow = 0.0;
        for (int k = juce::jmax(0, b - 1); k <= juce::jmin(bins - 1, b + 1); ++k)
        {
            float g = juce::Decibels::decibelsToGain(magDb[(size_t) k], -120.0f);
            sumLinPow += (double) g * (double) g;
        }
        bandMagDb[(size_t) b] = juce::Decibels::gainToDecibels((float) std::sqrt(sumLinPow), -120.0f);
    }

    for (int b = 0; b < bins; ++b)
    {
        float magLin = juce::Decibels::decibelsToGain(bandMagDb[(size_t) b], -120.0f);
        if (! primed) { fastEnvLin[(size_t) b] = magLin; slowEnvLin[(size_t) b] = magLin; } // seed directly on the first frame -- content already present when analysis starts must never read as "rising from silence" (that's a cold-start artifact of the envelope filters, not a real transient)
        else
        {
            fastEnvLin[(size_t) b] = magLin + (fastEnvLin[(size_t) b] - magLin) * fastEnvCoeff;
            slowEnvLin[(size_t) b] = magLin + (slowEnvLin[(size_t) b] - magLin) * slowEnvCoeff;
        }

        float ev = 0.0f;
        // Near the noise floor, both the flux and (especially) the
        // fast/slow RATIO features become numerically unstable: dividing by
        // a near-zero slowEnv turns ordinary FFT rounding noise on an
        // essentially-silent bin into huge relative "rise" readings that
        // have nothing to do with an audible transient. Gate both features
        // on the band's OWN current absolute level -- a band that hasn't
        // reached a reasonable absolute loudness yet contributes no
        // evidence, no matter how large its relative rise looks.
        constexpr float kMinAbsoluteMagDb = -70.0f;
        if (primed && bandMagDb[(size_t) b] > kMinAbsoluteMagDb)
        {
            float fluxDb = juce::jmax(0.0f, bandMagDb[(size_t) b] - prevMagDb[(size_t) b]);
            float ratio = fastEnvLin[(size_t) b] / juce::jmax(1.0e-9f, slowEnvLin[(size_t) b]);
            // Non-diluting: either signature alone is real evidence of a
            // rising transient (same max()-not-blend philosophy used
            // throughout PHYSICAL C -- a weak reading on one feature must
            // not suppress a strong reading on the other).
            ev = juce::jmax(dbFluxToEvidence(fluxDb), riseRatioToEvidence(ratio));
        }
        transientEvidence[(size_t) b] = ev;

        // Asymmetric attack/release envelope -- fast rise to meet a genuine
        // onset, slower fall so the protection outlives the single onset
        // frame long enough to cover the whole attack transient, then
        // decays away once evidence stops being regenerated (ringing tails
        // don't keep re-triggering flux/rise-ratio, so protection decays on
        // schedule even while the region's own persistence stays high).
        float target = ev;
        float coeff = (target > protection[(size_t) b]) ? attackCoeff : releaseCoeff;
        protection[(size_t) b] = target + (protection[(size_t) b] - target) * coeff;
    }

    std::copy(bandMagDb.begin(), bandMagDb.end(), prevMagDb.begin());
    primed = true;
}

float TransientProtectionEngine::transientEvidenceFor(float queryHz) const
{
    int c = nearestBin(queryHz);
    float best = 0.0f;
    for (int b = juce::jmax(0, c - 2); b <= juce::jmin(bins - 1, c + 2); ++b)
        best = juce::jmax(best, transientEvidence[(size_t) b]);
    return best;
}

float TransientProtectionEngine::transientProtectionFor(float queryHz) const
{
    int c = nearestBin(queryHz);
    float best = 0.0f;
    for (int b = juce::jmax(0, c - 2); b <= juce::jmin(bins - 1, c + 2); ++b)
        best = juce::jmax(best, protection[(size_t) b]);
    return best;
}
