#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// PluginColors — single source of truth for the AI Studio UI redesign's
// per-circuit color themes. Extracted directly from the AI Studio mockup's
// constants.ts (CIRCUIT_THEMES / RED_BURNER_THEME) — see
// AI Studio/extracted/src/constants.ts. Hex values are copied verbatim,
// not approximated.
// ---------------------------------------------------------------------------
struct CircuitTheme
{
    juce::Colour primary;      // main accent (knob arcs, active elements)
    juce::Colour primaryGlow;  // glow/shadow tint (primary at ~0.6 alpha)
    juce::Colour accent;       // lighter accent (badge text, active labels)
    juce::Colour border;       // chassis border
    juce::Colour text;         // LCD/readout text, DISTORTION subtitle
    juce::Colour ledColor;     // footswitch LED when engaged
};

namespace PluginColors
{
    // EMBER — from constants.ts CIRCUIT_THEMES.EMBER
    inline const CircuitTheme kEmber
    {
        juce::Colour (0xffd946ef), // primary
        juce::Colour (0xffd946ef).withAlpha (0.65f), // primaryGlow
        juce::Colour (0xfff0abfc), // accent
        juce::Colour (0xffa21caf), // border
        juce::Colour (0xfff5d0fe), // text
        juce::Colour (0xfff0abfc), // ledColor
    };

    // BURNER (Blue / default aggression) — from CIRCUIT_THEMES.BURNER
    inline const CircuitTheme kBurnerBlue
    {
        juce::Colour (0xff38bdf8),
        juce::Colour (0xff38bdf8).withAlpha (0.6f),
        juce::Colour (0xff7dd3fc),
        juce::Colour (0xff0284c7),
        juce::Colour (0xffbae6fd),
        juce::Colour (0xff38bdf8),
    };

    // BURNER (Red aggression) — from RED_BURNER_THEME
    inline const CircuitTheme kBurnerRed
    {
        juce::Colour (0xfff43f5e),
        juce::Colour (0xfff43f5e).withAlpha (0.6f),
        juce::Colour (0xfffb7185),
        juce::Colour (0xff9f1239),
        juce::Colour (0xfffecdd3),
        juce::Colour (0xfff43f5e),
    };

    // BLAZER — from CIRCUIT_THEMES.BLAZER
    inline const CircuitTheme kBlazer
    {
        juce::Colour (0xfff59e0b),
        juce::Colour (0xfff59e0b).withAlpha (0.6f),
        juce::Colour (0xfffbbf24),
        juce::Colour (0xffb45309),
        juce::Colour (0xfffef08a),
        juce::Colour (0xfff59e0b),
    };

    // Fixed aggression-cell colors (BurnerPage.tsx: buttonColor literals,
    // independent of the active theme — the BLUE/RED cells are always
    // these two colors regardless of which one is selected).
    constexpr uint32_t kAggressionBlue = 0xff38bdf8;
    constexpr uint32_t kAggressionRed  = 0xfff43f5e;

    // Shared dark chassis/knob-track colors used throughout the mockup,
    // independent of circuit theme.
    constexpr uint32_t kChassisBg      = 0xff07050a; // App.tsx root bg
    constexpr uint32_t kHeaderBg       = 0xe608060d; // PluginHeader bg (90% alpha)
    constexpr uint32_t kHeaderBorder   = 0xff1f1730;
    constexpr uint32_t kLcdBg          = 0xff040208;
    constexpr uint32_t kLcdBorder      = 0xff221838;
    constexpr uint32_t kKnobTrack      = 0xff1e1828; // Knob.tsx background arc
    constexpr uint32_t kKnobCapBorder  = 0xff312845; // Knob.tsx idle cap border
    constexpr uint32_t kMeterInactive  = 0xff272235; // LevelMeter inactive tick
    constexpr uint32_t kLabelText      = 0xffe2e8f0; // slate-200, knob labels
    constexpr uint32_t kReadoutText    = 0xffa1a1aa; // slate-400, knob readout
    constexpr uint32_t kInactiveTabText = 0xff64748b; // slate-500, inactive switch cell text

    inline CircuitTheme themeFor (int circuitIndex, int aggressionState) noexcept
    {
        if (circuitIndex == 0) return kEmber;
        if (circuitIndex == 1) return (aggressionState == 1) ? kBurnerRed : kBurnerBlue;
        return kBlazer;
    }
}
