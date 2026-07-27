#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"

// ---------------------------------------------------------------------------
// BlazerLookAndFeel — warm tan/gold knob accent for Blazer (Friedman BE-OD)
// mode, matching the real pedal's black enclosure + gold label. Subclasses
// PedalLookAndFeel purely additively (mirrors BurnerLookAndFeel's approach):
// only drawRotarySlider is overridden, so PedalLookAndFeel.cpp/.h — and
// therefore Ember's and Burner's rendering — are untouched.
// ---------------------------------------------------------------------------
class BlazerLookAndFeel : public PedalLookAndFeel
{
public:
    static constexpr uint32_t kAccent = 0xffc9963c; // warm tan/gold

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};
