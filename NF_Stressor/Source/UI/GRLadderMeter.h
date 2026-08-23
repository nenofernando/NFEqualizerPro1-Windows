#pragma once
#include <JuceHeader.h>

// Vertical gain-reduction LED ladder, matching the classic hardware-strip
// look: a fixed set of dB steps that light up from the top down as more
// reduction is applied.
class GRLadderMeter : public juce::Component
{
public:
    GRLadderMeter();

    void setGainReductionDb(float newGrDb);
    void paint(juce::Graphics& g) override;

private:
    static constexpr std::array<float, 16> steps
        { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 12.0f, 15.0f, 18.0f, 21.0f, 24.0f, 26.0f };

    float gainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRLadderMeter)
};
