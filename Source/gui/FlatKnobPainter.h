#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// FlatKnobPainter — shared rotary-knob paint routine matching the AI Studio
// mockup's Knob.tsx exactly: dark radial-gradient cap, 270-degree SVG-style
// ring arc (background track + colored active arc), white rim indicator dot.
// Shared by PedalLookAndFeel/BurnerLookAndFeel/BlazerLookAndFeel so the
// paint code isn't tripled — only the accent color differs per mode.
// ---------------------------------------------------------------------------
inline void drawFlatRotaryKnob (juce::Graphics& g,
                                 int x, int y, int width, int height,
                                 float sliderPos,
                                 float rotaryStartAngle, float rotaryEndAngle,
                                 juce::Colour accent)
{
    const float cx  = (float) x + (float) width  * 0.5f;
    const float cy  = (float) y + (float) height * 0.5f;
    const float diameter = juce::jmin ((float) width, (float) height);
    const float r   = diameter * 0.5f;
    const float arc = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // --- Ring arc track (270-degree sweep, matches Knob.tsx's SVG circle) ---
    const float strokeWidth = diameter * 0.054f;
    const float ringR       = r - strokeWidth;

    juce::Path track;
    track.addCentredArc (cx, cy, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff1e1828));
    g.strokePath (track, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path active;
        active.addCentredArc (cx, cy, ringR, ringR, 0.0f, rotaryStartAngle, arc, true);
        // Soft glow pass, then crisp core pass (mirrors the glow/core
        // double-stroke technique already used elsewhere in this codebase).
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (active, juce::PathStrokeType (strokeWidth * 1.6f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // --- Knob cap (radial gradient dark bezel, ~80% of diameter) ---
    const float capR = r * 0.80f;
    {
        juce::ColourGradient grad (
            juce::Colour (0xff2a243a), cx - capR * 0.3f, cy - capR * 0.3f,
            juce::Colour (0xff0d0915), cx + capR * 0.5f, cy + capR * 0.5f,
            true);
        grad.addColour (0.6, juce::Colour (0xff151020));
        g.setGradientFill (grad);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

        g.setColour (juce::Colour (0xff312845));
        g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.5f);
    }

    // --- Inner disc (82% of cap) ---
    const float discR = capR * 0.82f;
    {
        juce::ColourGradient grad (
            juce::Colour (0xff1a1426), cx, cy,
            juce::Colour (0xff0d0a14), cx + discR, cy + discR,
            true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - discR, cy - discR, discR * 2.0f, discR * 2.0f);
        g.setColour (juce::Colour (0xff302644).withAlpha (0.5f));
        g.drawEllipse (cx - discR, cy - discR, discR * 2.0f, discR * 2.0f, 1.0f);
    }

    // --- White rim indicator dot, rotated to the current angle ---
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (arc, cx, cy));
        const float dotW = diameter * 0.036f;
        const float dotH = diameter * 0.13f;
        const float dotY = cy - discR + dotH * 0.35f;
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (cx - dotW * 0.5f, dotY, dotW, dotH, dotW * 0.5f);
        g.restoreState();
    }
}
