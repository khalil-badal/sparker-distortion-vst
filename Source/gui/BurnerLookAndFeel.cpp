#include "BurnerLookAndFeel.h"

// Adapted from PedalLookAndFeel::drawRotarySlider — identical shape/depth,
// kPurple swapped for the Burner electric-violet accent (kAccent).
void BurnerLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    const float cx  = (float) x + (float) width  * 0.5f;
    const float cy  = (float) y + (float) height * 0.5f;
    const float r   = juce::jmin ((float) width, (float) height) * 0.42f;
    const float arc = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 3.0f, cy - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);

    g.setColour (juce::Colour (kKnobRim));
    g.fillEllipse (cx - r - 1.5f, cy - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);

    {
        juce::ColourGradient grad (
            juce::Colour (0xFF1e1828), cx - r * 0.3f, cy - r * 0.3f,
            juce::Colour (kKnobBody),  cx + r * 0.5f, cy + r * 0.5f,
            false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    {
        juce::Path shine;
        shine.addArc (cx - r * 0.85f, cy - r * 0.85f, r * 1.7f, r * 1.7f,
                      rotaryStartAngle - 0.5f, rotaryStartAngle + 1.8f, true);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (shine, juce::PathStrokeType (r * 0.18f));
    }

    {
        juce::Path track;
        track.addArc (cx - r * 0.88f, cy - r * 0.88f,
                      r * 1.76f, r * 1.76f,
                      rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xFF0A0810));
        g.strokePath (track, juce::PathStrokeType (r * 0.10f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    if (sliderPos > 0.001f)
    {
        juce::Path fill;
        fill.addArc (cx - r * 0.88f, cy - r * 0.88f,
                     r * 1.76f, r * 1.76f,
                     rotaryStartAngle, arc, true);

        g.setColour (juce::Colour (kAccent).withAlpha (0.22f));
        g.strokePath (fill, juce::PathStrokeType (r * 0.18f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (kAccent).withAlpha (0.9f));
        g.strokePath (fill, juce::PathStrokeType (r * 0.09f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    {
        const float ix = cx + std::sin (arc) * r * 0.60f;
        const float iy = cy - std::cos (arc) * r * 0.60f;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawLine (cx, cy, ix, iy, 1.2f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (ix - 2.5f, iy - 2.5f, 5.0f, 5.0f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f);
}
