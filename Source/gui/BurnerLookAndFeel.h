#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"
#include "PluginColors.h"

// ---------------------------------------------------------------------------
// BurnerLookAndFeel — Burner (REVV G3) knob accent. Matches AI Studio's
// CIRCUIT_THEMES.BURNER (sky blue) / RED_BURNER_THEME (rose) depending on
// the active Aggression channel — accent is settable at runtime since it
// changes with the aggression parameter, not fixed per circuit like
// Ember/Blazer. Subclasses PedalLookAndFeel additively: only
// drawRotarySlider is overridden, so PedalLookAndFeel.cpp/.h are untouched.
// ---------------------------------------------------------------------------
class BurnerLookAndFeel : public PedalLookAndFeel
{
public:
    void setAccent (juce::Colour c) noexcept { accent = c; }
    juce::Colour getAccent() const noexcept { return accent; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

private:
    juce::Colour accent { PluginColors::kAggressionBlue };
};
