#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/PedalLookAndFeel.h"
#include "gui/ModeSwitch.h"

// ---------------------------------------------------------------------------
// SuhrRiotEditor — "Khyle Audio Sparker" pedal GUI.
//
// Window: 590 × 430
//
// Layout overview:
//
//  [Input Trim]  ┌──────────── pedal body ────────────┐  [Output Trim]
//  (left outer)  │ DIST   Khyle Audio   LEVEL          │  (right outer)
//                │  ()    [VOICE sw]     ()             │
//                │           TONE                       │
//                │            ()                        │
//                │           ◉ LED  (painted dot)       │
//                │         (footswitch toggle)          │
//                │          SPARKER                     │
//                │        DISTORTION                    │
//                └──────────────────────────────────────┘
//  [Tight]                                         [Mode panel]
//  (lower-left)                                  (lower-right)
//
// Bypass is wired to the footswitch (ToggleButton, styled as stomp-switch).
// The LED dot is painted in paint() — it is NOT a component.
// ---------------------------------------------------------------------------
class SuhrRiotEditor  : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit SuhrRiotEditor (SuhrRiotProcessor&);
    ~SuhrRiotEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;

    SuhrRiotProcessor& processorRef;

    // LookAndFeel — declared first so it outlives every component.
    PedalLookAndFeel laf;

    // Inner pedal knobs
    juce::Slider driveKnob;      // DIST
    juce::Slider toneKnob;       // TONE
    juce::Slider levelKnob;      // LEVEL

    // Outer knobs (flanking the pedal body)
    juce::Slider inputTrimKnob;
    juce::Slider outputTrimKnob;
    juce::Slider tightKnob;

    // Footswitch: large stomp button; clicking toggles "active".
    // Drawn as a chrome pedal button (componentID "footswitch").
    juce::ToggleButton footswitch;

    // 3-way mode selector (lower-right, outside pedal)
    ModeSwitch modeSwitch;

    // Dynamic dB readout labels beneath the trim knobs
    juce::Label inputTrimDbLabel;
    juce::Label outputTrimDbLabel;

    // Attachments — declared last, destroyed first.
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SliderAtt driveAtt;
    SliderAtt toneAtt;
    SliderAtt levelAtt;
    SliderAtt inputTrimAtt;
    SliderAtt outputTrimAtt;
    SliderAtt tightAtt;
    ButtonAtt footswitchAtt;    // "active"

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotEditor)
};
