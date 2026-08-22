#pragma once
#include <JuceHeader.h>

// One decorative reel-to-reel tape spool: a machined metal flange with
// cutout windows, tape wound around the hub, and a fixed (non-rotating)
// centre cap carrying the brand mark. The flange spins while the editor is
// open; clicking the reel toggles it on/off (onToggle fires afterwards so
// the caller can keep both reels in sync). The repaint timer is fully
// stopped while frozen rather than just skipping its work, so a paused
// reel costs nothing — not even an idle callback.
class NFTapeReels : public juce::Component, private juce::Timer
{
public:
    explicit NFTapeReels(bool mirrored = false);
    ~NFTapeReels() override;

    void setSpinning(bool shouldSpin);
    bool isSpinning() const { return spinning; }

    // 0 = GP9 (gold), 1 = 456 (silver), 2 = 499 (red), 3 = 250 (black) —
    // matches the TAPE TYPE selector index, so the reel's finish changes
    // with the stock the user picks.
    void setTapeType(int typeIndex);

    // 0 = 7.5 IPS, 1 = 15 IPS, 2 = 30 IPS — matches the TAPE SPEED selector
    // index, so the reels visibly spin faster at higher transport speeds.
    void setSpeedIndex(int speedIndex);

    std::function<void()> onToggle;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    bool mirroredFlange;
    bool spinning = true;
    float rotationAngle = 0.0f;
    float speedMultiplier = 1.0f;
    int tapeTypeIndex = 0;
};
