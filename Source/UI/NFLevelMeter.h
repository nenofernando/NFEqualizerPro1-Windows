#pragma once
#include <JuceHeader.h>
#include "NFLookAndFeel.h"

// Classic LED-ladder VU meter: discrete lit/unlit segments (not a smooth
// gradient bar), two columns for L/R, framed in black. No built-in scale —
// the dB tick labels are drawn by the panel alongside it.
class NFLevelMeter : public juce::Component
{
public:
    void setLevels(float leftDb, float rightDb)
    {
        leftDb = juce::jlimit(minDb, maxDb, leftDb);
        rightDb = juce::jlimit(minDb, maxDb, rightDb);

        smoothedLeft = leftDb > smoothedLeft ? leftDb : smoothedLeft + (leftDb - smoothedLeft) * 0.35f;
        smoothedRight = rightDb > smoothedRight ? rightDb : smoothedRight + (rightDb - smoothedRight) * 0.35f;

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(0xff050505));
        g.fillRoundedRectangle(bounds, 3.0f);

        auto inner = bounds.reduced(2.0f);
        constexpr float gap = 3.0f;
        const float columnWidth = (inner.getWidth() - gap) * 0.5f;

        auto leftBar = inner.removeFromLeft(columnWidth);
        inner.removeFromLeft(gap);
        auto rightBar = inner;

        drawColumn(g, leftBar, smoothedLeft);
        drawColumn(g, rightBar, smoothedRight);

        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.4f);
    }

private:
    static juce::Colour zoneColour(float segmentDb)
    {
        if (segmentDb >= 6.0f)
            return juce::Colour(0xffE0263A);

        if (segmentDb >= -6.0f)
            return juce::Colour(0xffE8C22C);

        return juce::Colour(0xff35D14A);
    }

    void drawColumn(juce::Graphics& g, juce::Rectangle<float> bar, float levelDb)
    {
        constexpr int numSegments = 24;
        constexpr float segGap = 1.6f;
        const float segHeight = (bar.getHeight() - segGap * (float) (numSegments - 1))
                                / (float) numSegments;

        for (int i = 0; i < numSegments; ++i)
        {
            const float segDb = minDb + ((float) i + 0.5f) / (float) numSegments * (maxDb - minDb);
            const bool lit = levelDb >= segDb;

            const float y = bar.getBottom() - (float) i * (segHeight + segGap) - segHeight;
            juce::Rectangle<float> segment(bar.getX(), y, bar.getWidth(), segHeight);

            g.setColour(lit ? zoneColour(segDb) : zoneColour(segDb).darker(0.82f));
            g.fillRect(segment);
        }
    }

    float smoothedLeft = -60.0f;
    float smoothedRight = -60.0f;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 12.0f;
};
