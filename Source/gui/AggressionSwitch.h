#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// AggressionSwitch — Burner-only 2-position toggle for "burnerAggression"
// (SW1, DPDT ON-ON). Mirrors ModeSwitch/CircuitSwitch's pattern at N=2.
//
// Cells are labeled BLUE/RED (matching the real G3's own channel naming,
// per user request). State 0 = BLUE = hotter/higher gain (BurnerCircuit.cpp
// already applies 1.35x pre-clip drive + a brighter shelf to state 0), state
// 1 = RED = lower gain (0.85x, darker shelf) — this ordering is what makes
// Blue the hotter channel, per explicit correction; no DSP change was
// needed since state 0 was already the hotter internal state, only the UI
// label order was swapped from the initial RED/BLUE guess.
// ---------------------------------------------------------------------------
class AggressionSwitch : public juce::Component,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    AggressionSwitch (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);
    ~AggressionSwitch() override;

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    int currentState { 0 };

    static constexpr int kNumStates = 2;
    const char* const kLabels[kNumStates] = { "BLUE", "RED" };
};
