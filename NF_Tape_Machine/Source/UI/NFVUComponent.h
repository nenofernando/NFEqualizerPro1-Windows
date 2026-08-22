#pragma once
#include <JuceHeader.h>

// A pair of classic analog VU dials (LEFT/RIGHT) with cream aged-paper
// faces, an amber backlight glow and needles that follow real VU
// ballistics (a ~300ms integration time), not an instant digital jump.
// setLevels() is fed raw (fast) dB readings from the processor on a
// UI-rate timer elsewhere; the smoothing/animation happens in here.
class NFVUComponent : public juce::Component, private juce::Timer
{
public:
    NFVUComponent();
    ~NFVUComponent() override;

    void setLevels(float leftDb, float rightDb);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void drawDial(juce::Graphics& g, juce::Rectangle<float> area,
                  float smoothedDb, const juce::String& channelLabel);

    static float dbToAngle(float db);
    static const juce::Image& getPaperTexture();

    float targetLeftDb = -60.0f, targetRightDb = -60.0f;
    float smoothedLeftDb = -60.0f, smoothedRightDb = -60.0f;

    static constexpr float minDb = -20.0f;
    static constexpr float maxDb = 3.0f;

    // The printed scale on a real VU meter is NOT linear in dB — the
    // numbers are spaced roughly evenly around the arc regardless of the
    // (very uneven) dB gaps between them. dbToAngle interpolates through
    // these points by INDEX, not by raw dB proportion, so both the tick
    // marks and the needle share one non-linear scale instead of the
    // ticks bunching up on one side.
    static constexpr float scaleDb[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
    static constexpr int numScalePoints = 10;
};
