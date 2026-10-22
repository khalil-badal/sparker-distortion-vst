#pragma once
#include <JuceHeader.h>
#include "PedalLookAndFeel.h"
#include "CircuitSwitch.h"
#include "PresetData.h"
#include "PresetManager.h"
#include <functional>

// ---------------------------------------------------------------------------
// PluginHeader — top bar matching AI Studio's PluginHeader.tsx: left
// "SPARKER v1.0" branding, center LCD-style preset stepper (prev/next +
// clickable name that opens a dropdown browser), right circuit badge
// (CircuitSwitch). Extended (Section 4) with a SAVE button — not present in
// the read-only mockup, since it never had real persistence — and a unified
// preset list merging the 6 hardcoded factory presets with real user
// presets scanned from PresetManager::getUserPresetsDirectory().
// ---------------------------------------------------------------------------
class PluginHeader : public juce::Component
{
public:
    // One entry in the unified factory+user preset list.
    struct Entry
    {
        juce::String name;
        int circuit = 0;
        bool isUserFile = false;
        juce::File file;             // valid only when isUserFile
        const PluginPreset* factory = nullptr; // valid only when !isUserFile
    };

    PluginHeader (juce::AudioProcessorValueTreeState& apvts, PedalLookAndFeel& laf);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Called when a preset (factory or user file) is selected.
    std::function<void (const Entry&)> onSelectPreset;

    // Re-scans the user presets directory — call after saving a new one.
    void refreshPresetList();

private:
    juce::AudioProcessorValueTreeState& apvts;
    PedalLookAndFeel& laf;
    CircuitSwitch circuitSwitch;

    std::vector<Entry> entries;
    int  currentPresetIndex = 0;
    bool isDropdownOpen     = false;

    juce::Rectangle<int> prevArrowBounds, nextArrowBounds, lcdCenterBounds, lcdBounds;

    void selectPreset (int index);
    void promptAndSaveNewPreset();

    std::unique_ptr<juce::AlertWindow> saveDialog;
};
