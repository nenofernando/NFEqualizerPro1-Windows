#include "GRLadderMeter.h"
#include "NFStressorLookAndFeel.h"

GRLadderMeter::GRLadderMeter()
{
    setInterceptsMouseClicks(false, false);
}

void GRLadderMeter::setGainReductionDb(float newGrDb)
{
    if (std::abs(newGrDb - gainReductionDb) > 0.01f)
    {
        gainReductionDb = newGrDb;
        repaint();
    }
}

void GRLadderMeter::paint(juce::Graphics& g)
{
    using namespace NFStressorColours;

    auto bounds = getLocalBounds().toFloat();
    const int numSteps = (int) steps.size();
    const float rowHeight = bounds.getHeight() / (float) numSteps;

    for (int i = 0; i < numSteps; ++i)
    {
        auto row = bounds.removeFromTop(rowHeight);
        const bool lit = gainReductionDb >= steps[i] - 0.001f;

        // Three-tier colour grading like a real analogue GR ladder: green
        // for light reduction, amber for moderate, red once NUKE territory
        // (12 dB+) is reached.
        juce::Colour onColour = green;
        juce::Colour offColour = greenDim;
        if (steps[i] >= 12.0f)
        {
            onColour = red;
            offColour = redDim;
        }
        else if (steps[i] >= 6.0f)
        {
            onColour = amber;
            offColour = amberDim;
        }

        auto numberArea = row.removeFromLeft(row.getWidth() * 0.55f);
        g.setColour(lit ? textLight : textLight.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(juce::jmin(row.getHeight() * 0.6f, 12.5f)).withStyle("Bold")));
        g.drawFittedText(juce::String((int) steps[i]), numberArea.toNearestInt(),
                         juce::Justification::centredRight, 1);

        auto ledArea = row.reduced(row.getWidth() * 0.2f, rowHeight * 0.24f);
        const float diameter = juce::jmin(ledArea.getWidth(), ledArea.getHeight());
        auto led = juce::Rectangle<float>(diameter, diameter).withCentre(ledArea.getCentre());

        // Recessed bezel behind every LED, lit or not, so the unlit ones
        // read as a proper dark housing rather than a flat dot.
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(led.expanded(diameter * 0.18f));

        if (lit)
        {
            g.setColour(onColour.withAlpha(0.30f));
            g.fillEllipse(led.expanded(diameter * 0.45f));
            g.setColour(onColour.withAlpha(0.55f));
            g.fillEllipse(led.expanded(diameter * 0.2f));
        }

        juce::ColourGradient ledGradient(lit ? onColour.brighter(0.4f) : ledOff.brighter(0.15f),
                                        led.getX(), led.getY(),
                                        lit ? onColour.darker(0.3f) : ledOff.darker(0.2f),
                                        led.getX(), led.getBottom(), false);
        g.setGradientFill(ledGradient);
        g.fillEllipse(led);

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(led, 0.8f);

        if (lit)
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(led.reduced(diameter * 0.62f).translated(-diameter * 0.08f, -diameter * 0.1f));
        }
    }
}
