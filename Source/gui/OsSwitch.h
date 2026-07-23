#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// OsSwitch — 3-position oversampling selector (2x / 4x / 8x).
//
// Mirrors the ModeSwitch pattern: listens to the "osOrder" APVTS parameter
// and updates it on click. No ButtonParameterAttachment needed.
// ---------------------------------------------------------------------------
class OsSwitch : public juce::Component,
                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    OsSwitch (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);
    ~OsSwitch() override;

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String& paramID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    int currentOrder = 2;  // 0=2x, 1=4x, 2=8x

    static constexpr int kNum = 3;
    const char* const kLabels[kNum] = { "2x", "4x", "8x" };
};
