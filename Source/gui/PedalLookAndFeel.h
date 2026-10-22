#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// PedalLookAndFeel — AI Studio UI redesign visual style for Sparker
// Distortion. Reproduces the AI Studio mockup (see AI Studio/extracted/)
// exactly: flat dark chassis, per-circuit radial-gradient background, SVG
// -style ring-arc knobs. Embeds Inter (Regular/Medium/SemiBold/Bold) and
// Barlow Condensed (ExtraBold/Regular/Light) from BinaryData.
//
// Color palette: Ember = magenta (#d946ef), matching constants.ts
// CIRCUIT_THEMES.EMBER. See gui/PluginColors.h for the full per-circuit
// theme set (Burner blue/red, Blazer amber).
// ---------------------------------------------------------------------------
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Ember palette (from AI Studio constants.ts CIRCUIT_THEMES.EMBER)
    static constexpr uint32_t kPurple   = 0xFFD946EF;   // Ember primary accent
    static constexpr uint32_t kBg       = 0xFF07050A;   // App.tsx root background
    static constexpr uint32_t kKnobBody = 0xFF0F0C1A;   // knob body dark
    static constexpr uint32_t kKnobRim  = 0xFF1A1228;   // knob outer rim
    static constexpr uint32_t kText     = 0xFFEEF1F8;   // near-white label

    PedalLookAndFeel();

    // Draw the plugin background (flat dark + purple radial bloom).
    // Call first in PluginEditor::paint().
    void drawPluginBackground (juce::Graphics& g, int width, int height);

    // LookAndFeel overrides
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                                const juce::Colour& backgroundColour,
                                bool isMouseOver, bool isButtonDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isMouseOver, bool isButtonDown) override;

    juce::Font getLabelFont      (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Direct font accessors — use the stored Typeface::Ptr so name-lookup is
    // never needed (avoids platform font cache issues).
    juce::Font regularFont  (float height) const;                    // Inter Regular
    juce::Font uiFont      (float height, bool bold = false) const;  // Inter Medium / Bold
    juce::Font semiBoldFont (float height) const;                    // Inter SemiBold
    juce::Font brandFont   (float height) const;                     // Barlow Condensed ExtraBold
    juce::Font barlowLight  (float height) const;                    // Barlow Condensed Light

private:
    // Build a Font from the embedded OpenDyslexic typeface at the given height.
    // All UI text routes through here so the whole GUI renders in OpenDyslexic.
    // Extra kerning is forced to 0 so it reads as a normal (if quirky) sans-
    // serif rather than the wide accessibility spacing OpenDyslexic ships with.
    juce::Font dyslexicFont (float height) const;

    // OpenDyslexic — the typeface for all UI text. The Inter/Barlow faces below
    // are kept loaded (BinaryData still embeds them) but every accessor now
    // routes through dyslexicFont, so they are no longer the rendered face.
    juce::Typeface::Ptr openDyslexic;
    juce::Typeface::Ptr interRegular, interMedium, interSemiBold, interBold;
    juce::Typeface::Ptr barlowExtraBold, barlowRegular, barlowLightFace;
};
