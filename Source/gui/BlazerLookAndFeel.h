#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"
#include "PluginColors.h"

// ---------------------------------------------------------------------------
// BlazerLookAndFeel — Blazer (Friedman BE-OD) knob accent. Matches AI
// Studio's CIRCUIT_THEMES.BLAZER (amber). Subclasses PedalLookAndFeel
// additively: only drawRotarySlider is overridden, so PedalLookAndFeel.cpp/
// .h are untouched.
// ---------------------------------------------------------------------------
class BlazerLookAndFeel : public PedalLookAndFeel
{
public:
    static constexpr uint32_t kAccent = 0xfff59e0b; // Blazer primary (amber)

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};
