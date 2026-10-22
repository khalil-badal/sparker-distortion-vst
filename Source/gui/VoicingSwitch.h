#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// VoicingSwitch — global AMP / DRIVE selector, shown in the header next to the
// circuit badge. Drives the "voicing" APVTS parameter, which applies to
// whichever circuit is active (it describes the RIG, not the circuit, so it is
// deliberately one global setting rather than per-circuit).
//
//   AMP   — the "amp in a box" voicing. Carries the shaping a real amp would
//           otherwise provide (shared 400 Hz tone-stack scoop, extra HF
//           rolloff, power-amp sag). Use straight into an IR / FRFR.
//   DRIVE — pedal voicing for going into a REAL clean amp: that shaping is
//           stripped back out so the amp supplies it, instead of it being
//           applied twice (which double-scoops and double-dulls).
//
// Styled to match ModeSwitch/AggressionSwitch: a two-cell segmented pill. The
// active cell is tinted with the ACTIVE CIRCUIT's theme colour so the header
// stays visually coherent as the circuit changes.
// ---------------------------------------------------------------------------
class VoicingSwitch : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    VoicingSwitch (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);
    ~VoicingSwitch() override;

    void paint     (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    int currentVoicing { 0 };   // 0 = Amp, 1 = Drive

    static constexpr int kNumStates = 2;
    const char* const kLabels[kNumStates] = { "AMP", "DRIVE" };
};
