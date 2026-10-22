#pragma once
#include <JuceHeader.h>
#include <map>
#include "CircuitMode.h"

// ---------------------------------------------------------------------------
// getParameterRanges(CircuitMode) — single source of truth for per-mode
// knob min/max/default/taper, keyed by APVTS parameter ID.
//
// `skew` follows JUCE's juce::NormalisableRange convention: 1.0 = linear,
// < 1.0 biases the perceptual curve toward the low end of the range (used
// here for audio/log-feeling "A"-taper controls).
//
// EMBER entries mirror the ranges already hardcoded in
// SuhrRiotProcessor::createParameterLayout() for drive/tone/level — they are
// NOT wired back into that registration (Ember's live parameter definitions
// are untouched, per the freeze on Ember code). This entry exists purely as
// a documented reference so both modes' ranges are visible in one place.
//
// BURNER entries ARE wired into createParameterLayout() for
// burnerGain/Volume/Bass/Mid/Treble.
//
// Taper rationale (see session record for the Gain discussion):
//   - Gain (B1M) and Mid (B100k) are linear-taper pots feeding a feedback
//     resistor / active EQ stage respectively — the stage's own gain math
//     (dB = 20*log10(Rf/Rin)) already supplies the perceptually-log feel,
//     so the parameter itself stays linear (skew 1.0). Doubling up a log
//     taper on top would crush most of the range into the first ~10% of
//     travel.
//   - Volume/Bass/Treble (A50k/A100k) are simple attenuators/shelves with
//     no such compensating stage math, so they get an audio-taper skew
//     (0.35, matching the precedent set by Ember's own tightFreq skew=0.3
//     in PluginProcessor.cpp) so perceived loudness/tone change is even
//     across the knob's travel.
//   - Aggression is a discrete 2-state switch (AudioParameterChoice),
//     not a ranged control — not represented here.
// ---------------------------------------------------------------------------
struct ParamRange
{
    float min          = 0.0f;
    float max           = 1.0f;
    float defaultValue = 0.5f;
    float skew         = 1.0f;   // 1.0 = linear, <1.0 = audio/log-leaning taper
};

// JUCE's rotary knob renders at the RAW normalized slider proportion
// (convertTo0to1(value)), not the "real" value directly. JUCE's actual
// NormalisableRange::convertFrom0to1 (non-symmetric-skew path) computes
// value = min + (max-min) * pow(proportion, 1/skew) — note the 1/skew,
// not skew directly. So proportion=0.5 (12 o'clock) happens at
// value = min + (max-min) * pow(0.5, 1/skew), NOT pow(0.5, skew).
//
// CORRECTION: an earlier version of this function used pow(0.5, skew)
// (exponent inverted), which put every skewed knob's default at the WRONG
// raw proportion — e.g. Volume (skew 0.35) landed at proportion ~0.86
// (~4 o'clock) instead of 0.5 (12 o'clock). Verified by direct calculation
// against JUCE's source before this fix, not just re-guessed.
inline float midpointDefault (float min, float max, float skew) noexcept
{
    return min + (max - min) * std::pow (0.5f, 1.0f / skew);
}

inline std::map<juce::String, ParamRange> getParameterRanges (CircuitMode mode)
{
    std::map<juce::String, ParamRange> ranges;

    switch (mode)
    {
        case CircuitMode::EMBER:
            // Reference only — matches createParameterLayout()'s existing
            // drive/tone/level registrations exactly. Not wired back in.
            ranges["drive"] = { 0.0f, 1.0f, 0.5f, 1.0f };
            ranges["tone"]  = { 0.0f, 1.0f, 0.5f, 1.0f };
            ranges["level"] = { 0.0f, 1.0f, 0.5f, 1.0f };
            break;

        case CircuitMode::BURNER:
        {
            // All controls linear: the NR clipper's own gain math (rf/rin) and
            // the EQ shelves' dB formula already supply the perceptual curve.
            // Previous audio-taper (skew 0.35) on Volume/Bass/Treble put the
            // 12 o'clock default at 0.138, which was too quiet (Volume) and
            // cut ~9 dB (Bass/Treble — the EQ formula treats 0.5 as flat).
            ranges["burnerGain"]   = { 0.0f, 1.0f, 0.5f, 1.0f }; // B1M, linear
            ranges["burnerVolume"] = { 0.0f, 1.0f, 0.5f, 1.0f }; // A50k — linear for sensible default
            ranges["burnerBass"]   = { 0.0f, 1.0f, 0.5f, 1.0f }; // A100K — 0.5 = flat EQ
            ranges["burnerTreble"] = { 0.0f, 1.0f, 0.5f, 1.0f }; // A50k — 0.5 = flat EQ
            ranges["burnerMid"]    = { 0.0f, 1.0f, 0.5f, 1.0f }; // B100k, linear (active mid)
            ranges["burnerTight"]  = { 0.0f, 1.0f, 0.0f, 1.0f }; // fully open default
            break;
        }

        case CircuitMode::BLAZER:
        {
            // Linear on Volume (same fix as Burner — audio taper put 12 o'clock
            // at 0.138, far too quiet). Bass/Presence/Tight keep reverse taper
            // (C-taper pots on the real pedal).
            const float reverseTaperSkew = 2.857f;
            const float reverseDefault   = midpointDefault (0.0f, 1.0f, reverseTaperSkew);

            ranges["blazerGain"]     = { 0.0f, 1.0f, 0.5f,           1.0f            }; // B1M, linear
            ranges["blazerVolume"]   = { 0.0f, 1.0f, 0.5f,           1.0f            }; // A50K — linear for sensible default
            // Bass: LINEAR with a flat (0.5) default so 12 o'clock = 0 dB, like
            // Burner's bass. Previously a reverse C-taper whose 0.784 default
            // put visual-noon at +6.8 dB @100 Hz — a hidden bass boost that
            // made Blazer read as "too fat". The real Friedman BE-OD is a
            // TIGHT, punchy modded-Marshall voice, not a woofy one, so it
            // should sit neutral at noon and let the player add low end.
            ranges["blazerBass"]     = { 0.0f, 1.0f, 0.5f,           1.0f            }; // noon = flat (was reverse C-taper, +6.8 dB)
            ranges["blazerTreble"]   = { 0.0f, 1.0f, 0.5f,           1.0f            }; // B100K, linear
            // Presence: LINEAR with a flat (0.5) default so 12 o'clock = 0 dB.
            // Same latent bug the Bass control had: the reverse C-taper put the
            // visual-noon VALUE at 0.784, i.e. a hidden +6.8 dB at 5 kHz. Pink-
            // noise measurement showed Blazer sitting +11 to +13 dB at 8 kHz
            // relative to 1 kHz — far too bright/fizzy, and bright glare also
            // masks definition, compounding the "muddy" impression.
            ranges["blazerPresence"] = { 0.0f, 1.0f, 0.5f,           1.0f            }; // noon = flat (was reverse C-taper, +6.8 dB)
            ranges["blazerTight"]    = { 0.0f, 1.0f, reverseDefault, reverseTaperSkew }; // C100K, reverse audio
            ranges["blazerTrim"]     = { 0.0f, 1.0f, 0.5f,           1.0f            }; // 100K trimpot, linear
            break;
        }
    }

    return ranges;
}
