#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// AggressionSwitch — Burner-only 2-position toggle for "burnerAggression"
// (SW1, DPDT ON-ON). Mirrors ModeSwitch/CircuitSwitch's pattern at N=2.
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
    const char* const kLabels[kNumStates] = { "1", "2" };
};
