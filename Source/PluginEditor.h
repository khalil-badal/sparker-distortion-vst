#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/PedalLookAndFeel.h"
#include "gui/BurnerLookAndFeel.h"
#include "gui/BlazerLookAndFeel.h"
#include "gui/ModeSwitch.h"
#include "gui/AggressionSwitch.h"
#include "gui/VoicingSwitch.h"
#include "gui/PluginHeader.h"
#include "gui/LevelMeter.h"

// ---------------------------------------------------------------------------
// SuhrRiotEditor — Sparker Distortion GUI, reproducing the AI Studio mockup
// (AI Studio/extracted/) exactly: flat modern chassis, per-circuit color
// theme, header with preset stepper + circuit badge.
//
// Window: 800 x 600 (4:3, matches App.tsx's aspect-[4/3])
//
// Layout per page (Ember/Burner/Blazer all share this grid):
//   Header:      SPARKER v1.0 | preset LCD stepper | circuit badge
//   Top row:     INPUT+meter | GAIN | KHYLE AUDIO | LEVEL/VOLUME | OUTPUT+meter
//   Middle row:  Ember: single TONE knob. Burner/Blazer: 3 knobs.
//   Bottom row:  GATE+TIGHT (left) | Footswitch+SPARKER wordmark (center)
//                | mode-specific switch/knob (right)
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

    void resized () override;

private:
    void timerCallback() override;

    // Places the AMP/DRIVE switch in the bottom right. Blazer sits it lower
    // than Ember/Burner to clear the TRIM knob's caption — called from both
    // resized() and timerCallback(), since the circuit can change without a
    // resize. See the definition for the geometry.
    void layOutVoicingSwitch();

    // -----------------------------------------------------------------------
    // UI scaling (Small / Medium / Large).
    //
    // The whole GUI is designed in a fixed 800x600 base coordinate space. All
    // controls and all chassis painting live inside `root`, a single child
    // component sized at that base 800x600. A size choice simply applies an
    // AffineTransform scale to `root` (and sizes the editor to match), so the
    // ENTIRE UI — every knob, switch, label and each component's own internal
    // text — enlarges uniformly with one transform. Small (1.0x) is default.
    // The chosen scale persists in the APVTS state tree (survives reload).
    // -----------------------------------------------------------------------
    void  paintChassis  (juce::Graphics&);            // draws the chassis in base coords
    void  rootMouseDown (const juce::MouseEvent&);    // right-click -> size menu
    void  applyUiScale  (float newScale, bool persist);
    void  showSizeMenu();
    float uiScale = 1.0f;

    // Content root — parent of every control, painter of the chassis. Scaled
    // as a whole to realise the UI size. Forwards paint/right-click back to
    // the editor (nested class can access the editor's private members).
    struct Root : juce::Component
    {
        SuhrRiotEditor& owner;
        explicit Root (SuhrRiotEditor& o) : owner (o) {}
        void paint (juce::Graphics& g) override             { owner.paintChassis (g); }
        void mouseDown (const juce::MouseEvent& e) override { owner.rootMouseDown (e); }
    };

    SuhrRiotProcessor& processorRef;

    // LookAndFeel — declared first so it outlives every component.
    PedalLookAndFeel laf;
    BurnerLookAndFeel burnerLaf;   // violet accent, used only by Burner's own knobs
    BlazerLookAndFeel blazerLaf;   // gold accent, used only by Blazer's own knobs

    // Declared before all controls so it outlives them (they are its children).
    Root root { *this };

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

    // Gate threshold knob (lower-left, below TIGHT knob)
    juce::Slider gateKnob;

    // Dynamic dB readout labels beneath the trim knobs
    juce::Label inputTrimDbLabel;
    juce::Label outputTrimDbLabel;

    // -----------------------------------------------------------------------
    // Top header bar — owns its own CircuitSwitch (Ember/Burner/Blazer badge)
    // and preset stepper. Visibility of mode-specific controls below is
    // toggled in timerCallback() based on the "circuit" parameter — Ember's
    // own controls/behavior are never modified, only shown/hidden.
    // -----------------------------------------------------------------------
    PluginHeader header;

    // Input/Output level meters (universal, next to the trim knobs)
    LevelMeter inputMeter;
    LevelMeter outputMeter;

    juce::Slider burnerGainKnob;
    juce::Slider burnerVolumeKnob;
    juce::Slider burnerBassKnob;
    juce::Slider burnerMidKnob;      // Burner-only — hidden in Ember mode
    juce::Slider burnerTrebleKnob;
    juce::Slider burnerTightKnob;    // Burner-only — new, not schematic-accurate, see BurnerCircuit.h
    AggressionSwitch aggressionSwitch; // Burner-only — hidden in Ember mode

    // Global AMP/DRIVE voicing. Universal (visible in every circuit), so it
    // is NOT in any of the per-circuit visibility lists. Lives in the bottom
    // right, above the mode/aggression panel — it was in the header next to
    // the circuit badge, which made that corner read as cluttered.
    VoicingSwitch voicingSwitch;

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
    SliderAtt burnerTightAtt;

    SliderAtt blazerGainAtt;
    SliderAtt blazerVolumeAtt;
    SliderAtt blazerBassAtt;
    SliderAtt blazerTrebleAtt;
    SliderAtt blazerPresenceAtt;
    SliderAtt blazerTightAtt;
    SliderAtt blazerTrimAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotEditor)
};
