#include "PedalLookAndFeel.h"
#include <BinaryData.h>

static juce::Typeface::Ptr makeTypeface (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

// ----------------------------------------------------------------- constructor --
PedalLookAndFeel::PedalLookAndFeel()
{
    interRegular    = makeTypeface (BinaryData::InterRegular_ttf,
                                   BinaryData::InterRegular_ttfSize);
    interMedium     = makeTypeface (BinaryData::InterMedium_ttf,
                                   BinaryData::InterMedium_ttfSize);
    interSemiBold   = makeTypeface (BinaryData::InterSemiBold_ttf,
                                   BinaryData::InterSemiBold_ttfSize);
    interBold       = makeTypeface (BinaryData::InterBold_ttf,
                                   BinaryData::InterBold_ttfSize);
    barlowExtraBold = makeTypeface (BinaryData::BarlowCondensedExtraBold_ttf,
                                   BinaryData::BarlowCondensedExtraBold_ttfSize);
    barlowRegular   = makeTypeface (BinaryData::BarlowCondensedRegular_ttf,
                                   BinaryData::BarlowCondensedRegular_ttfSize);
    barlowLightFace = makeTypeface (BinaryData::BarlowCondensedLight_ttf,
                                   BinaryData::BarlowCondensedLight_ttfSize);

    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (kText));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (kBg));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0xff2a1840));
    setColour (juce::Label::textColourId,               juce::Colour (kText));
    setColour (juce::TextButton::buttonColourId,        juce::Colour (kBg));
    setColour (juce::TextButton::textColourOffId,       juce::Colour (kText));
}

// ---------------------------------------------------------- plugin background --
void PedalLookAndFeel::drawPluginBackground (juce::Graphics& g, int width, int height)
{
    // Flat dark base
    g.fillAll (juce::Colour (kBg));

    // Radial purple bloom from the center — subtle, not dramatic
    const float cx     = (float) width  * 0.5f;
    const float cy     = (float) height * 0.48f;
    const float radius = (float) juce::jmax (width, height) * 0.65f;

    juce::ColourGradient bloom (
        juce::Colour (kPurple).withAlpha (0.055f), cx, cy,
        juce::Colour (kBg).withAlpha     (0.0f),   cx, cy + radius,
        true);
    g.setGradientFill (bloom);
    g.fillRect (0, 0, width, height);
}

// -------------------------------------------------------------- rotary knob --
//
// Adapted from RootboundLookAndFeel::drawRotarySlider — same depth/polish,
// cyan references replaced with Sparker's purple, knob body kept in the
// original dark-purple register.
void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g,
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

    // Shadow / depth ring
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 3.0f, cy - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);

    // Outer rim (dark purple)
    g.setColour (juce::Colour (kKnobRim));
    g.fillEllipse (cx - r - 1.5f, cy - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);

    // Knob body — dark purple metallic gradient, lit from top-left
    {
        juce::ColourGradient grad (
            juce::Colour (0xFF1e1828), cx - r * 0.3f, cy - r * 0.3f,
            juce::Colour (kKnobBody),  cx + r * 0.5f, cy + r * 0.5f,
            false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // Subtle highlight arc (top-left shine)
    {
        juce::Path shine;
        shine.addArc (cx - r * 0.85f, cy - r * 0.85f, r * 1.7f, r * 1.7f,
                      rotaryStartAngle - 0.5f, rotaryStartAngle + 1.8f, true);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (shine, juce::PathStrokeType (r * 0.18f));
    }

    // Track arc (unfilled, very dark)
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

    // Filled arc — purple, double-pass (glow then core)
    if (sliderPos > 0.001f)
    {
        juce::Path fill;
        fill.addArc (cx - r * 0.88f, cy - r * 0.88f,
                     r * 1.76f, r * 1.76f,
                     rotaryStartAngle, arc, true);

        g.setColour (juce::Colour (kPurple).withAlpha (0.22f));
        g.strokePath (fill, juce::PathStrokeType (r * 0.18f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (kPurple).withAlpha (0.9f));
        g.strokePath (fill, juce::PathStrokeType (r * 0.09f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    // Indicator line + dot
    {
        const float ix = cx + std::sin (arc) * r * 0.60f;
        const float iy = cy - std::cos (arc) * r * 0.60f;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawLine (cx, cy, ix, iy, 1.2f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (ix - 2.5f, iy - 2.5f, 5.0f, 5.0f);
    }

    // Inner shadow
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f);
}

// ------------------------------------------------------------------ toggle --
void PedalLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool highlighted, bool /*down*/)
{
    const juce::String id = button.getComponentID();

    if (id == "footswitch")
    {
        const float w      = (float) button.getWidth();
        const float h      = (float) button.getHeight();
        const float cx     = w * 0.5f;
        const float cy     = h * 0.5f;
        const float outerR = juce::jmin (w, h) * 0.5f - 1.0f;

        // Rubber outer ring
        g.setColour (juce::Colour (0xff0e0c16));
        g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
        g.setColour (juce::Colour (0xff1a1230));
        g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.5f);

        // Chrome cap — dark metal gradient
        const float capR = outerR * 0.76f;
        juce::ColourGradient cap (
            juce::Colour (0xff2a2240), cx - capR * 0.25f, cy - capR * 0.3f,
            juce::Colour (0xff0c0a14), cx + capR * 0.25f, cy + capR * 0.3f,
            true);
        g.setGradientFill (cap);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

        // Concentric detail ring
        const float detR = capR * 0.72f;
        g.setColour (juce::Colour (0x28201840));
        g.drawEllipse (cx - detR, cy - detR, detR * 2.0f, detR * 2.0f, 0.8f);

        // Cap outline
        g.setColour (juce::Colour (0xff221a38));
        g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        if (highlighted)
        {
            g.setColour (juce::Colour (0x18ffffff));
            g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
        }

        return;
    }

    // Fallback — should not be reached in normal use
    g.setColour (juce::Colour (0xff333333));
    g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 4.0f);
}

// --------------------------------------------------------- button background --
void PedalLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour&,
                                              bool isMouseOver,
                                              bool isButtonDown)
{
    auto  bounds  = button.getLocalBounds().toFloat().reduced (1.5f);
    float corner  = 6.0f;
    bool  toggled = button.getToggleState();

    juce::Colour fillCol = (isButtonDown || toggled)
                           ? juce::Colour (kPurple).withAlpha (0.18f)
                           : juce::Colour (0xFF0A0810);
    juce::Colour rimCol  = (isMouseOver || toggled)
                           ? juce::Colour (kPurple)
                           : juce::Colour (kPurple).withAlpha (0.5f);

    if (isMouseOver || toggled)
    {
        g.setColour (juce::Colour (kPurple).withAlpha (0.12f));
        g.fillRoundedRectangle (bounds.expanded (3.0f), corner + 1.5f);
    }

    g.setColour (fillCol);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (rimCol);
    g.drawRoundedRectangle (bounds, corner, 1.4f);
}

// ------------------------------------------------------------- button text --
void PedalLookAndFeel::drawButtonText (juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (juce::Colour (kText));

    const int yIndent    = juce::jmin (4, button.proportionOfHeight (0.3f));
    const int cornerSize = juce::jmin (button.getHeight(), button.getWidth()) / 2;
    const int leftIndent  = juce::jmin (cornerSize, 2 + cornerSize / (button.isConnectedOnLeft()  ? 4 : 2));
    const int rightIndent = juce::jmin (cornerSize, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));

    g.drawFittedText (button.getButtonText(),
                      leftIndent, yIndent,
                      button.getWidth() - leftIndent - rightIndent,
                      button.getHeight() - yIndent * 2,
                      juce::Justification::centred, 2);
}

// --------------------------------------------------------------- font overrides --
juce::Font PedalLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions().withTypeface (interMedium).withHeight (10.0f));
}

juce::Font PedalLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions().withTypeface (interSemiBold).withHeight (11.0f));
}

// --------------------------------------------------------- direct accessors --
juce::Font PedalLookAndFeel::regularFont (float height) const
{
    if (interRegular)
        return juce::Font (juce::FontOptions().withTypeface (interRegular).withHeight (height));
    return juce::Font (juce::FontOptions().withHeight (height));
}

juce::Font PedalLookAndFeel::uiFont (float height, bool bold) const
{
    auto tf = bold ? interBold : interMedium;
    if (tf) return juce::Font (juce::FontOptions().withTypeface (tf).withHeight (height));
    return juce::Font (juce::FontOptions().withHeight (height));
}

juce::Font PedalLookAndFeel::semiBoldFont (float height) const
{
    if (interSemiBold)
        return juce::Font (juce::FontOptions().withTypeface (interSemiBold).withHeight (height));
    return juce::Font (juce::FontOptions().withHeight (height));
}

juce::Font PedalLookAndFeel::brandFont (float height) const
{
    if (barlowExtraBold)
        return juce::Font (juce::FontOptions().withTypeface (barlowExtraBold).withHeight (height));
    return juce::Font (juce::FontOptions().withHeight (height));
}

juce::Font PedalLookAndFeel::barlowLight (float height) const
{
    if (barlowLightFace)
        return juce::Font (juce::FontOptions().withTypeface (barlowLightFace).withHeight (height));
    return juce::Font (juce::FontOptions().withHeight (height));
}
