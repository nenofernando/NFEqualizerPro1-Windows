// Visual-geometry confirmation for the LOW/HIGH curve-shape correction.
// Mirrors EXACTLY the two functions ControlCurveComponent::paint() sums per
// sample point (globalRangeShapeAt + ResonanceDetector::combinedSensitivityAt),
// and prints an ASCII plot for each of the 5 requested cases so the curve's
// geometry can be confirmed without any GUI/screenshot risk.
#include <JuceHeader.h>
#include "DSP/ResonanceDetector.h"
#include <cstdio>

static float globalRangeShapeAt(float hz, float lowHz, float highHz, float lowTaperOct = 2.5f, float highTaperOct = 2.5f)
{
    const double floorDb = -60.0;
    double hzLog = std::log2(juce::jmax(1.0f, hz));
    double distBelow = std::log2(juce::jmax(1.0f, lowHz)) - hzLog;
    double distAbove = hzLog - std::log2(juce::jmax(1.0f, highHz));
    auto slope = [&](double distOct, double taperOct) -> double
    {
        if (distOct <= 0.0) return 0.0;
        double kneeOct = juce::jlimit(0.15, 1.0, taperOct * 0.35);
        double eased = distOct < kneeOct ? (distOct * distOct) / (2.0 * kneeOct) : distOct - kneeOct * 0.5;
        double dbPerOct = -floorDb / juce::jmax(0.1, taperOct);
        return juce::jmax(floorDb, -dbPerOct * eased);
    };
    return (float) juce::jmin(slope(distBelow, (double) lowTaperOct), slope(distAbove, (double) highTaperOct));
}

static void plotCase(const char* title, float lowHz, float highHz,
                      const float freqArr[ResonanceDetector::kMaxBands],
                      const float sensArr[ResonanceDetector::kMaxBands],
                      const float widthArr[ResonanceDetector::kMaxBands],
                      const int shapeArr[ResonanceDetector::kMaxBands],
                      const float focusArr[ResonanceDetector::kMaxBands],
                      const bool activeArr[ResonanceDetector::kMaxBands])
{
    printf("\n=== %s  (LOW=%.0fHz HIGH=%.0fHz) ===\n", title, lowHz, highHz);
    const int cols = 70, rows = 15;
    for (int row = 0; row < rows; ++row)
    {
        float dbTop = 12.0f - (float) row * (24.0f / (float) (rows - 1));
        char line[cols + 1];
        for (int c = 0; c < cols; ++c) line[c] = ' ';
        line[cols] = 0;
        for (int c = 0; c < cols; ++c)
        {
            float t = (float) c / (float) (cols - 1);
            float logHz = std::log10(20.0f) + t * (std::log10(20000.0f) - std::log10(20.0f));
            float hz = std::pow(10.0f, logHz);
            float bands = ResonanceDetector::combinedSensitivityAt(hz, freqArr, sensArr, widthArr, shapeArr, focusArr, activeArr);
            float sens = juce::jlimit(-12.0f, 12.0f, globalRangeShapeAt(hz, lowHz, highHz) + bands);
            float dbBottom = dbTop - (24.0f / (float) (rows - 1));
            if (sens <= dbTop && sens > dbBottom) line[c] = '*';
            else if (std::abs(dbTop) < 0.01f || (dbTop > 0 && dbBottom <= 0)) line[c] = (line[c]==' ') ? '-' : line[c];
        }
        printf("%6.1f | %s\n", dbTop, line);
    }
    printf("        20Hz%*s20kHz\n", cols - 4, "");
}

int main()
{
    float freq[ResonanceDetector::kMaxBands]{}, sens[ResonanceDetector::kMaxBands]{}, width[ResonanceDetector::kMaxBands]{}, focus[ResonanceDetector::kMaxBands]{};
    int shape[ResonanceDetector::kMaxBands]{}; bool active[ResonanceDetector::kMaxBands]{};
    for (auto& fo : focus) fo = 0.5f; // neutral focus everywhere -- geometry unchanged from before focus existed

    printf("================================================================\n");
    printf("Global Range Shape (LOW/HIGH) visual geometry check\n");
    printf("================================================================\n");

    // Case 0: REAL factory default (lowHz=100, highHz=16000) -- must read as
    // a flat white line "no centro", tapering only near the very edges.
    plotCase("Case 0: LOW=100 / HIGH=16k, no bands (REAL FACTORY DEFAULT)", 100.0f, 16000.0f, freq, sens, width, shape, focus, active);

    // Case 1: fully open, no bands -- must be a perfectly flat 0dB line.
    plotCase("Case 1: LOW=20 / HIGH=20k, no bands (fully open)", 20.0f, 20000.0f, freq, sens, width, shape, focus, active);

    // Case 2: LOW=100, HIGH=20k -- left side should taper down near 100Hz.
    plotCase("Case 2: LOW=100 / HIGH=20k, no bands", 100.0f, 20000.0f, freq, sens, width, shape, focus, active);

    // Case 3: LOW=20, HIGH=12k -- right side should taper down near 12kHz.
    plotCase("Case 3: LOW=20 / HIGH=12k, no bands", 20.0f, 12000.0f, freq, sens, width, shape, focus, active);

    // Case 4: LOW=100, HIGH=12k -- both sides taper.
    plotCase("Case 4: LOW=100 / HIGH=12k, no bands", 100.0f, 12000.0f, freq, sens, width, shape, focus, active);

    // Case 5: fully open range, one isolated Bell band at 2kHz, sens=+8, default width(0.4oct)
    active[0] = true; freq[0] = 2000.0f; sens[0] = 8.0f; width[0] = 0.4f; shape[0] = 0;
    plotCase("Case 5: LOW=20 / HIGH=20k, isolated Bell @2kHz +8dB", 20.0f, 20000.0f, freq, sens, width, shape, focus, active);

    // Sanity assertions (fail loudly if geometry regresses):
    bool ok = true;
    float emptyFreq[ResonanceDetector::kMaxBands]{}, emptySens[ResonanceDetector::kMaxBands]{}, emptyWidth[ResonanceDetector::kMaxBands]{}, emptyFocus[ResonanceDetector::kMaxBands]{};
    int emptyShape[ResonanceDetector::kMaxBands]{}; bool emptyActive[ResonanceDetector::kMaxBands]{};
    float flatCheck = globalRangeShapeAt(1000.0f, 20.0f, 20000.0f) +
                       ResonanceDetector::combinedSensitivityAt(1000.0f, emptyFreq, emptySens, emptyWidth, emptyShape, emptyFocus, emptyActive);
    if (std::abs(flatCheck) > 0.001f) { printf("FAIL: default not flat at 1kHz (%.4f)\n", flatCheck); ok = false; }

    float below = globalRangeShapeAt(20.0f, 1000.0f, 20000.0f);
    if (below > -5.0f) { printf("FAIL: LOW=1000Hz should dip well below 0dB at 20Hz (got %.2f)\n", below); ok = false; }
    float insideLow = globalRangeShapeAt(2000.0f, 1000.0f, 20000.0f);
    if (std::abs(insideLow) > 0.001f) { printf("FAIL: should be flat well inside LOW boundary (got %.4f)\n", insideLow); ok = false; }

    float bellFar = ResonanceDetector::bandContribution(200.0f, 2000.0f, 8.0f, 0.4f, 0, 0.5f);
    if (std::abs(bellFar) > 0.05f) { printf("FAIL: isolated Bell at 2kHz should be ~0 far away at 200Hz (got %.4f)\n", bellFar); ok = false; }

    printf("\n%s\n", ok ? "ALL GEOMETRY SANITY CHECKS PASSED" : "SOME GEOMETRY SANITY CHECKS FAILED");
    return ok ? 0 : 1;
}
