#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// BurnerLookAndFeel — violet/electric-purple knob accent for Burner (REVV
// G3) mode. Subclasses PedalLookAndFeel purely additively: only
// drawRotarySlider is overridden (with kAccent swapped in for kPurple), so
// PedalLookAndFeel.cpp/.h — and therefore Ember's rendering — are untouched.
// Fonts, background, and every other draw routine are inherited as-is.
// ---------------------------------------------------------------------------
class BurnerLookAndFeel : public PedalLookAndFeel
{
public:
    static constexpr uint32_t kAccent = 0xffaa33ff; // electric violet

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};
