#include "PedalLookAndFeel.h"
#include "FlatKnobPainter.h"
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

    // OpenDyslexic — the typeface for all UI text. Loaded once, cached, kept
    // alive for the plugin's lifetime. jassert catches a bad/missing resource in
    // debug builds (createSystemTypefaceFor returns null on failure).
    openDyslexic    = makeTypeface (BinaryData::OpenDyslexicRegular_otf,
                                   BinaryData::OpenDyslexicRegular_otfSize);
    jassert (openDyslexic != nullptr);

    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (kText));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (kBg));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0xff2a1840));
    setColour (juce::Label::textColourId,               juce::Colour (kText));
    setColour (juce::TextButton::buttonColourId,        juce::Colour (kBg));
    setColour (juce::TextButton::textColourOffId,       juce::Colour (kText));
}

// ---------------------------------------------------------- plugin background --
// Matches App.tsx's Ember radial gradient exactly:
//   radial-gradient(ellipse at center, #260c33 0%, #170620 60%, #0c0212 100%)
void PedalLookAndFeel::drawPluginBackground (juce::Graphics& g, int width, int height)
{
    // Radius approximates CSS radial-gradient's default "farthest-corner"
    // sizing (distance from center to the box corner) so the gradient
    // actually reaches near-black by the edges instead of staying visibly
    // tinted all the way out — was spread too far (0.75x) before.
    const float cx     = (float) width  * 0.5f;
    const float cy     = (float) height * 0.5f;
    const float radius = std::sqrt (juce::square ((float) width * 0.5f) + juce::square ((float) height * 0.5f));

    juce::ColourGradient grad (
        juce::Colour (0xff260c33), cx, cy,
        juce::Colour (0xff0c0212), cx, cy + radius,
        true);
    grad.addColour (0.6, juce::Colour (0xff170620));
    g.setGradientFill (grad);
    g.fillRect (0, 0, width, height);
}

// -------------------------------------------------------------- rotary knob --
// Delegates to the shared flat-knob painter (see FlatKnobPainter.h) with
// Ember's magenta accent — matches AI Studio's Knob.tsx exactly.
void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider&)
{
    drawFlatRotaryKnob (g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle,
                        juce::Colour (kPurple));
}

// ------------------------------------------------------------------ toggle --
void PedalLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool highlighted, bool /*down*/)
{
    const juce::String id = button.getComponentID();

    if (id == "footswitch")
    {
        // Matches AI Studio's Footswitch.tsx exactly: dark radial dome,
        // outer ring border tinted by the active circuit's TEXT color (not
        // primary — this was the wrong stashed color before) when engaged.
        // PluginEditor stashes the current theme.text ARGB in the button's
        // properties (no direct access to circuit state from here).
        const bool engaged = button.getToggleState();
        const bool pressed = button.isDown();
        const juce::Colour circuitTextColour (
            static_cast<juce::uint32> ((int) button.getProperties().getWithDefault ("circuitColorArgb", (int) 0xff64748b)));

        const float w      = (float) button.getWidth();
        const float h      = (float) button.getHeight();
        const float cx     = w * 0.5f;
        const float cy     = h * 0.5f;
        const float outerR = juce::jmin (w, h) * 0.5f - 1.0f;

        // Outer border ring
        g.setColour (juce::Colour (0xff0a0714));
        g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
        g.setColour (engaged ? circuitTextColour.withAlpha (0.47f) : juce::Colour (0xff221936));
        g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.5f);

        // Inner dome — fills inside the ring with an exact 4px padding
        // (was a proportional 10% shrink before, which drifted from the
        // spec at different sizes).
        const float domeR = outerR - 4.0f;
        juce::ColourGradient dome = pressed
            ? juce::ColourGradient (juce::Colour (0xff231b36), cx - domeR * 0.10f, cy - domeR * 0.10f,
                                    juce::Colour (0xff09060f), cx + domeR * 0.5f,  cy + domeR * 0.5f, true)
            : juce::ColourGradient (juce::Colour (0xff433561), cx - domeR * 0.24f, cy - domeR * 0.36f,
                                    juce::Colour (0xff0e0a17), cx + domeR * 0.5f,  cy + domeR * 0.5f, true);
        dome.addColour (pressed ? 0.7 : 0.55, pressed ? juce::Colour (0xff130e1e) : juce::Colour (0xff211933));
        g.setGradientFill (dome);
        g.fillEllipse (cx - domeR, cy - domeR, domeR * 2.0f, domeR * 2.0f);
        g.setColour (engaged ? circuitTextColour.withAlpha (0.67f) : juce::Colour (0xff312648));
        g.drawEllipse (cx - domeR, cy - domeR, domeR * 2.0f, domeR * 2.0f, 1.0f);

        if (highlighted)
        {
            g.setColour (juce::Colour (0x18ffffff));
            g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
        }

        return;
    }

    // Fallback
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

// ---------------------------------------------------------------- OpenDyslexic --
// Central builder: every UI font routes through here. Extra kerning forced to 0
// so OpenDyslexic renders with normal typographic spacing (a quirky sans-serif),
// not the wide accessibility spacing it ships with by default. Falls back to a
// plain sized font if the typeface somehow failed to load.
juce::Font PedalLookAndFeel::dyslexicFont (float height) const
{
    juce::Font f = (openDyslexic != nullptr)
                     ? juce::Font (juce::FontOptions().withTypeface (openDyslexic).withHeight (height))
                     : juce::Font (juce::FontOptions().withHeight (height));
    f.setExtraKerningFactor (0.0f);   // normal letter spacing, not the wide default
    return f;
}

// --------------------------------------------------------------- font overrides --
// All accessors now render in OpenDyslexic (the weight/bold flags are ignored —
// only the Regular weight is embedded per spec). Heights are unchanged so
// layout, sizing and the overall UI scale are untouched.
juce::Font PedalLookAndFeel::getLabelFont (juce::Label&)
{
    return dyslexicFont (10.0f);
}

juce::Font PedalLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return dyslexicFont (11.0f);
}

// --------------------------------------------------------- direct accessors --
juce::Font PedalLookAndFeel::regularFont (float height) const
{
    return dyslexicFont (height);
}

juce::Font PedalLookAndFeel::uiFont (float height, bool /*bold*/) const
{
    return dyslexicFont (height);
}

juce::Font PedalLookAndFeel::semiBoldFont (float height) const
{
    return dyslexicFont (height);
}

juce::Font PedalLookAndFeel::brandFont (float height) const
{
    return dyslexicFont (height);
}

juce::Font PedalLookAndFeel::barlowLight (float height) const
{
    return dyslexicFont (height);
}
