#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// ModeSwitch — 3-position segmented control for the "mode" parameter.
//
// Each cell shows WARM / CLASSIC / HOT centred inside the button.
// "MODE" label runs along the bottom.
//
// A PedalLookAndFeel reference is required for font access.
// ---------------------------------------------------------------------------
class ModeSwitch : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    ModeSwitch (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);
    ~ModeSwitch() override;

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    int currentMode { 0 };

    static constexpr int kNumModes = 3;
    const char* const kLabels[kNumModes] = { "WARM", "CLASSIC", "HOT" };
};
