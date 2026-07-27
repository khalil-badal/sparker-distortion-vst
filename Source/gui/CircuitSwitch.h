#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// CircuitSwitch — top-level 3-position selector for the "circuit" parameter
// (Ember / Burner / Blazer). This is distinct from ModeSwitch, which drives
// Ember's own internal "mode" (WARM/CLASSIC/HOT) parameter and is untouched.
//
// Mirrors ModeSwitch's paint/interaction pattern for visual consistency.
// ---------------------------------------------------------------------------
class CircuitSwitch : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    CircuitSwitch (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);
    ~CircuitSwitch() override;

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getCurrentCircuit() const noexcept { return currentCircuit; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    int currentCircuit { 0 };

    static constexpr int kNumCircuits = 3;
    const char* const kLabels[kNumCircuits] = { "EMBER", "BURNER", "BLAZER" };

    // Distinct accent per circuit — Ember keeps the pedal's existing purple;
    // Burner gets the violet/electric-purple colorway.
    static constexpr uint32_t kAccentColours[kNumCircuits] =
    {
        0xff7733bb,  // Ember  — existing Sparker purple
        0xffaa33ff,  // Burner — electric violet
        0xffc9963c,  // Blazer — warm tan/gold
    };
};
