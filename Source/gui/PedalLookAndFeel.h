#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// PedalLookAndFeel — Rootbound-vibe visual style for the Sparker Distortion.
//
// Embeds Inter (Regular/Medium/SemiBold/Bold) and Barlow Condensed
// (ExtraBold/Regular/Light) from BinaryData.  Typefaces are registered on
// construction and remain valid for the plugin's lifetime.
//
// Color palette: Sparker's original purple/black — nothing cyan.
// ---------------------------------------------------------------------------
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Sparker palette constants (purple/black — unchanged from original)
    static constexpr uint32_t kPurple   = 0xFF6633AA;   // value arc / accent
    static constexpr uint32_t kBg       = 0xFF0C0A11;   // flat background
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
    juce::Typeface::Ptr interRegular, interMedium, interSemiBold, interBold;
    juce::Typeface::Ptr barlowExtraBold, barlowRegular, barlowLightFace;
};
