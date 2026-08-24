#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFStressorLookAndFeel.h"
#include "UI/GRLadderMeter.h"

// Fixed-canvas editor: the whole panel is laid out at designWidth x
// designHeight and the host window scales it as one block, matching the
// approach used by the sibling NF Audio Tools plugins.
class NFStressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NFStressorAudioProcessorEditor(NFStressorAudioProcessor&);
    ~NFStressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 410;
    static constexpr int designHeight = 1086;

private:
    void timerCallback() override;
    void layOutContent();
    void applyDefaultSize();

    // True only when RATIO is at 10:1 AND the ATTACK/RELEASE knobs are
    // still sitting at their OPTO detent (10 / 0) — dragging either knob
    // away from that position leaves OPTO even though RATIO itself stays
    // at 10:1. Checked continuously (see timerCallback) since it depends
    // on live knob position, not just a parameter-change event.
    bool isOptoActive() const;

    // Preset save/load, reached via the hamburger menu button top-left.
    void showPresetMenu();
    void savePresetAs();
    void loadPresetFrom();
    void loadPresetFile(const juce::File& file);
    void resetToDefault();
    juce::File getPresetsDirectory() const;

    juce::Slider& setupKnob(juce::Slider& knob);
    juce::TextButton& setupSegmentButton(juce::TextButton& button, const juce::String& text);
    juce::TextButton& setupToggleButton(juce::TextButton& button, const juce::String& text);
    void setupCaption(juce::Label& label, const juce::String& text, float pointSize, bool bold = false,
                      juce::Justification justification = juce::Justification::centred);

    struct BackgroundPanel : public juce::Component
    {
        void paint(juce::Graphics& g) override;
        void setInsetPanels(std::vector<juce::Rectangle<int>> panels) { insetPanels = std::move(panels); repaint(); }

        // Double-clicking the bare top-left corner of the chassis (no knob
        // or button lives there) resets the window back to its default
        // launch size — a quick escape hatch after dragging the resize
        // corner around, without hunting for that corner again. The same
        // reset also fires when double-clicking directly on the "NF -
        // STRESSOR" logo text itself (see logoHotZone below).
        std::function<void()> onCornerDoubleClicked;
        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            const bool onCorner = e.position.x < cornerHotZone && e.position.y < cornerHotZone;
            if (onCornerDoubleClicked && (onCorner || logoHotZone.contains(e.getPosition())))
                onCornerDoubleClicked();
        }
        static constexpr float cornerHotZone = 50.0f;

        // Set from layOutContent() to match the title label's bounds, so
        // double-clicking the visible "NF - STRESSOR" logo also resets the
        // window size, not just the bare corner.
        void setLogoHotZone(juce::Rectangle<int> area) { logoHotZone = area; }

        // Knob centre + radius + current 0..1 value — a warm glow is drawn at
        // each, as if light from the circuit board behind the panel were
        // leaking out around the knob's base. Intensity tracks the knob's
        // value (turned up = brighter) rather than sitting at a fixed
        // strength. Drawn on the panel (never clipped by the knob components
        // sitting on top of it).
        struct KnobGlow { juce::Point<float> centre; float radius; float value = 0.0f; };
        void setKnobGlows(std::vector<KnobGlow> glows) { knobGlows = std::move(glows); repaint(); }
        void setKnobGlowValues(const std::vector<float>& values)
        {
            bool changed = false;
            for (size_t i = 0; i < knobGlows.size() && i < values.size(); ++i)
            {
                if (std::abs(knobGlows[i].value - values[i]) > 0.005f)
                {
                    knobGlows[i].value = values[i];
                    changed = true;
                }
            }
            if (changed)
                repaint();
        }

        // The "NF Audio Tools" brand mark, replacing the old horizontal
        // footer strip — set sideways in the space beside MIX (mirroring
        // NUKE on the other side), read bottom-to-top like a rack-gear
        // spine label.
        void setBrandLabelArea(juce::Rectangle<int> area) { brandLabelArea = area; repaint(); }

        // Short radial tick marks beside the ATTACK/RELEASE knobs, pointing
        // at their "0" position (SLOW for Attack, FAST for Release — see
        // the matching text labels set up alongside these in the editor).
        void setIndicatorTicks(std::vector<juce::Line<float>> newTicks) { ticks = std::move(newTicks); repaint(); }

    private:
        std::vector<juce::Rectangle<int>> insetPanels;
        std::vector<KnobGlow> knobGlows;
        juce::Rectangle<int> brandLabelArea;
        juce::Rectangle<int> logoHotZone;
        std::vector<juce::Line<float>> ticks;
    };

    // Small indicator LED, lit whenever RATIO is set to 10:1 (OPTO). Also
    // clickable — it's a second way to engage OPTO besides the 10:1 RATIO
    // button itself, both driving the same ratio parameter.
    struct OptoLed : public juce::Component
    {
        void paint(juce::Graphics& g) override;
        void setOn(bool shouldBeOn) { if (isOn != shouldBeOn) { isOn = shouldBeOn; repaint(); } }
        std::function<void()> onClick;
        void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }

    private:
        bool isOn = false;
    };

    NFStressorAudioProcessor& audioProcessor;
    NFStressorLookAndFeel lookAndFeel;
    int blinkCounter = 0; // drives the BYPASS LED's blink while engaged

    // Everything is added to `content` and laid out once at designWidth x
    // designHeight; resized() only rescales this single component, so
    // dragging the corner resizer scales the whole panel as one block
    // instead of reflowing it.
    juce::Component content;

    BackgroundPanel panel;

    // Top plate
    juce::Label titleLabel, taglineLabel;
    juce::TextButton powerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    juce::TextButton menuButton; // hamburger icon, top-left — preset save/load
    std::unique_ptr<juce::FileChooser> activeFileChooser;

    // Main knobs
    juce::Slider inputKnob, attackKnob, releaseKnob, outputKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        inputAttachment, attackAttachment, releaseAttachment, outputAttachment;
    juce::Label inputCaption, attackCaption, releaseCaption, outputCaption;

    // Small "SLOW"/"FAST" hints beside ATTACK/RELEASE, next to a short tick
    // mark (drawn on the panel, see BackgroundPanel::setIndicatorTicks)
    // pointing at each knob's two end-stops ("0" and "10") — see
    // mapAttackMs/mapReleaseMs in StressorEngine.cpp for which end is which.
    juce::Label attackSlowLabel, attackFastLabel, releaseFastLabel, releaseSlowLabel;

    // OPTO indicator — lit whenever RATIO is set to 10:1 (see
    // NF::kOptoRatioIndex), sitting between the ATTACK/RELEASE knobs.
    // Clicking it is a second way to engage/disengage OPTO, alongside
    // clicking the 10:1 RATIO button directly — both just move the same
    // ratio parameter, so they can never disagree.
    OptoLed optoLed;
    juce::Label optoLabel;
    int lastNonOptoRatioIndex = 3; // remembers where to go back to when OPTO is switched off via the light

    juce::Label grCaption; // "GR" label, directly above the ladder meter
    GRLadderMeter grMeter;

    // Ratio segmented row
    juce::Label ratioCaption;
    juce::OwnedArray<juce::TextButton> ratioButtons;
    std::unique_ptr<juce::ParameterAttachment> ratioAttachment;
    int currentRatioIndex = 3;

    // Character toggles (DETECTOR: hp/link, AUDIO: dist2/dist3, plus the
    // output HP + NUKE pair added to the right of the AUDIO column)
    juce::Label detectorCaption, audioCaption;
    juce::TextButton hpButton, linkButton, dist2Button, dist3Button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hpAttachment, linkAttachment, dist2Attachment, dist3Attachment;

    // Output high-pass, AUDIO column: a 3-way cycle button (Off/70 Hz/120 Hz)
    // rather than a plain toggle, so it uses a ParameterAttachment like RATIO
    // instead of a ButtonAttachment. Cleans up excess low end left over from
    // the character stage — e.g. on vocals.
    juce::TextButton outHpButton;
    std::unique_ptr<juce::ParameterAttachment> outHpAttachment;
    int currentOutHpMode = 0;

    // Mix knob. NUKE (the brick-wall-limiter toggle) used to sit beside it
    // here but now lives in the AUDIO column, under outHpButton.
    juce::Slider mixKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    juce::Label mixCaption;
    juce::TextButton nukeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> nukeAttachment;

    // Small version tag, tucked in the bottom-right corner of the chassis.
    juce::Label versionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFStressorAudioProcessorEditor)
};
