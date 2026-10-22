#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// CircuitSwitch — top-level circuit badge, matching AI Studio's
// PluginHeader.tsx "Right: Clickable Circuit Badge Toggle" exactly: a
// single pill button showing the active circuit name (EMBER/BURNER/BLAZER),
// colored per that circuit's theme, that cycles forward through all three
// on click. Replaces the old 3-cell segmented-tab design.
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
};
