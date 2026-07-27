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
            ranges["burnerGain"]   = { 0.0f, 1.0f, 0.5f, 1.0f  }; // B1M, linear
            ranges["burnerVolume"] = { 0.0f, 1.0f, 0.5f, 0.35f }; // A50k, audio taper
            ranges["burnerBass"]   = { 0.0f, 1.0f, 0.5f, 0.35f }; // A100K, audio taper
            ranges["burnerTreble"] = { 0.0f, 1.0f, 0.5f, 0.35f }; // A50k, audio taper
            ranges["burnerMid"]    = { 0.0f, 1.0f, 0.5f, 1.0f  }; // B100k, linear (active mid)
            break;

        case CircuitMode::BLAZER:
            // C-taper (reverse-audio) controls use skew = 1/0.35 ≈ 2.857 —
            // the JUCE-convention mirror of the 0.35 audio-taper skew used
            // elsewhere (skew>1 biases resolution toward the top of the
            // range, matching a reverse-audio pot's feel).
            ranges["blazerGain"]     = { 0.0f, 1.0f, 0.5f, 1.0f   }; // B1M, linear
            ranges["blazerVolume"]   = { 0.0f, 1.0f, 0.5f, 0.35f  }; // A50K, audio taper
            ranges["blazerBass"]     = { 0.0f, 1.0f, 0.5f, 2.857f }; // C100K, reverse audio
            ranges["blazerTreble"]   = { 0.0f, 1.0f, 0.5f, 1.0f   }; // B100K, linear
            ranges["blazerPresence"] = { 0.0f, 1.0f, 0.5f, 2.857f }; // C10K, reverse audio
            ranges["blazerTight"]    = { 0.0f, 1.0f, 0.0f, 2.857f }; // C100K, reverse audio; default fully open
            ranges["blazerTrim"]     = { 0.0f, 1.0f, 0.5f, 1.0f   }; // 100K trimpot, linear, default midpoint
            break;
    }

    return ranges;
}
