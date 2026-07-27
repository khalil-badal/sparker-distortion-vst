#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/PedalLookAndFeel.h"
#include "gui/BurnerLookAndFeel.h"
#include "gui/BlazerLookAndFeel.h"
#include "gui/ModeSwitch.h"
#include "gui/OsSwitch.h"
#include "gui/CircuitSwitch.h"
#include "gui/AggressionSwitch.h"

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
    BurnerLookAndFeel burnerLaf;   // violet accent, used only by Burner's own knobs
    BlazerLookAndFeel blazerLaf;   // gold accent, used only by Blazer's own knobs

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

    // 3-way mode selector (lower-right, outside pedal) — Ember's own
    // WARM/CLASSIC/HOT voicing selector. Untouched, unrelated to CircuitSwitch.
    ModeSwitch modeSwitch;

    // Oversampling selector (lower-right, below MODE panel)
    OsSwitch osSwitch;

    // Gate threshold knob (lower-left, below TIGHT knob)
    juce::Slider gateKnob;

    // Dynamic dB readout labels beneath the trim knobs
    juce::Label inputTrimDbLabel;
    juce::Label outputTrimDbLabel;

    // -----------------------------------------------------------------------
    // Top-level circuit selector (Ember / Burner / Blazer) and Burner-only
    // controls. Visibility of these vs. the Ember-only controls above is
    // toggled in timerCallback() based on the "circuit" parameter — Ember's
    // own controls/behavior are never modified, only shown/hidden.
    // -----------------------------------------------------------------------
    CircuitSwitch circuitSwitch;

    juce::Slider burnerGainKnob;
    juce::Slider burnerVolumeKnob;
    juce::Slider burnerBassKnob;
    juce::Slider burnerMidKnob;      // Burner-only — hidden in Ember mode
    juce::Slider burnerTrebleKnob;
    AggressionSwitch aggressionSwitch; // Burner-only — hidden in Ember mode

    // Blazer-only controls (Phase 8). No Mid, no Aggression, no voicing
    // switch — Blazer has zero switches on the real pedal (Phase 7
    // analysis). Trim is the internal/advanced 100k trimpot, shown as a
    // small knob rather than a main control.
    juce::Slider blazerGainKnob;
    juce::Slider blazerVolumeKnob;
    juce::Slider blazerBassKnob;
    juce::Slider blazerTrebleKnob;
    juce::Slider blazerPresenceKnob;  // Blazer-only — hidden outside Blazer mode
    juce::Slider blazerTightKnob;     // Blazer-only — hidden outside Blazer mode
    juce::Slider blazerTrimKnob;      // small "advanced" knob, not a main control

    int lastEditorCircuit = -1;   // forces first visibility sync

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
    SliderAtt gateAtt;          // "gateThresh"

    SliderAtt burnerGainAtt;
    SliderAtt burnerVolumeAtt;
    SliderAtt burnerBassAtt;
    SliderAtt burnerMidAtt;
    SliderAtt burnerTrebleAtt;

    SliderAtt blazerGainAtt;
    SliderAtt blazerVolumeAtt;
    SliderAtt blazerBassAtt;
    SliderAtt blazerTrebleAtt;
    SliderAtt blazerPresenceAtt;
    SliderAtt blazerTightAtt;
    SliderAtt blazerTrimAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotEditor)
};
