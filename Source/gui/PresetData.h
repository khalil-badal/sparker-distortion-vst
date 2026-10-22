#pragma once
#include <JuceHeader.h>
#include <vector>

// ---------------------------------------------------------------------------
// PresetData — factory preset list, copied verbatim (names, descriptions,
// values) from AI Studio's constants.ts PRESETS array. Values there are on
// a 0-10 scale; converted to this plugin's actual normalized (0..1) or
// direct (dB) parameter ranges here.
//
// This is the Section 3 UI shell's data source (header preset stepper).
// Section 4 will add real file-based save/load to the standard user
// presets directory — this list becomes the seeded factory bank once that
// lands, without changing the header UI built against it now.
// ---------------------------------------------------------------------------
struct PresetParamValue
{
    juce::String paramId;
    float value; // already converted to the param's real range
};

struct PluginPreset
{
    juce::String name;
    juce::String description;
    int circuit; // 0=Ember, 1=Burner, 2=Blazer
    std::vector<PresetParamValue> values;
};

inline const std::vector<PluginPreset>& getFactoryPresets()
{
    static const std::vector<PluginPreset> presets =
    {
        {
            "Ember - Warm Tube Lead",
            "Smooth liquid lead tone with rich mid harmonic saturation & warm tube response",
            0,
            {
                { "drive", 0.72f }, { "level", 0.68f }, { "tone", 0.58f },
                { "tightFreq", 20.0f + 0.25f * 480.0f }, { "gateThresh", -70.0f + 0.15f * 50.0f },
                { "mode", 0.0f }, // WARM
            }
        },
        {
            "Ember - Hot Germanium Fuzz",
            "Screaming high-gain fuzzy crunch with aggressive upper bite and sustain",
            0,
            {
                { "drive", 0.90f }, { "level", 0.55f }, { "tone", 0.72f },
                { "tightFreq", 20.0f + 0.10f * 480.0f }, { "gateThresh", -70.0f + 0.30f * 50.0f },
                { "mode", 2.0f }, // HOT
            }
        },
        {
            "Burner - Modern Rhythm",
            "Aggressive scoop with punchy low end and roaring dual gain stage",
            1,
            {
                { "burnerGain", 0.85f }, { "burnerVolume", 0.65f }, { "burnerBass", 0.70f },
                { "burnerMid", 0.42f }, { "burnerTreble", 0.75f },
                { "burnerAggression", 0.0f }, // BLUE
                { "gateThresh", -70.0f + 0.35f * 50.0f },
            }
        },
        {
            "Burner - Red Thrash Metal",
            "High-speed palm-mute attack with tight noise gate and extreme aggression",
            1,
            {
                { "burnerGain", 0.95f }, { "burnerVolume", 0.60f }, { "burnerBass", 0.82f },
                { "burnerMid", 0.30f }, { "burnerTreble", 0.88f },
                { "burnerAggression", 1.0f }, // RED
                { "gateThresh", -70.0f + 0.65f * 50.0f },
            }
        },
        {
            "Blazer - Searing Crunch",
            "Tight focus with maximum presence, zero mud, and razor sharp clarity",
            2,
            {
                { "blazerGain", 0.80f }, { "blazerVolume", 0.72f }, { "blazerBass", 0.50f },
                { "blazerTreble", 0.68f }, { "blazerPresence", 0.75f }, { "blazerTight", 0.50f },
                { "blazerTrim", 0.40f },
            }
        },
        {
            "Blazer - Djent Edge Cut",
            "Ultra-tight low-cut boost designed to push high-gain tube amps into modern djent precision",
            2,
            {
                { "blazerGain", 0.65f }, { "blazerVolume", 0.85f }, { "blazerBass", 0.35f },
                { "blazerTreble", 0.80f }, { "blazerPresence", 0.88f }, { "blazerTight", 0.85f },
                { "blazerTrim", 0.55f },
            }
        },
    };
    return presets;
}
