#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// PresetManager — real file-based user preset save/load, wired to the full
// APVTS state (not just a hand-picked subset of params, unlike the factory
// presets in PresetData.h — those stay as quick param-recipes; user-saved
// presets are complete, accurate snapshots of every parameter).
//
// Format choice: JUCE's own state serialization (apvts.copyState() ->
// ValueTree -> XML), the same round-trip already used by
// getStateInformation()/setStateInformation() for DAW session save/restore.
// This is deliberately NOT Steinberg's binary .vstpreset container format —
// hand-rolling that chunked binary spec without the actual VST3 SDK source
// risks producing a file that LOOKS VST3-native (by extension) but is
// silently malformed, which a host's native preset scanner could reject or
// mishandle. A custom extension (.sparkerpreset) makes the format honest
// about what it actually is, while still being the fully-JUCE-native,
// round-trip-tested serialization path.
//
// Directory: matches the standard VST3 user-presets convention
// (%APPDATA%/VST3 Presets/<Company>/<Plugin>/ on Windows, via JUCE's
// cross-platform special-location API so macOS/Linux get their own
// equivalents automatically) even though the file format itself isn't the
// binary VST3 one.
// ---------------------------------------------------------------------------
namespace PresetManager
{
    inline juce::File getUserPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("VST3 Presets")
                      .getChildFile ("Khyle Audio")
                      .getChildFile ("Sparker Distortion");
        if (! dir.exists())
            dir.createDirectory();
        return dir;
    }

    inline juce::String sanitizeFileName (const juce::String& name)
    {
        return juce::File::createLegalFileName (name.isNotEmpty() ? name : "Untitled Preset");
    }

    // Saves the FULL current APVTS state (every parameter) under the given
    // name. Returns the file written, or an invalid File on failure.
    inline juce::File savePreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
    {
        auto file = getUserPresetsDirectory().getChildFile (sanitizeFileName (name) + ".sparkerpreset");

        juce::XmlElement root ("SPARKER_PRESET");
        root.setAttribute ("name", name);
        root.setAttribute ("formatVersion", 1);

        if (auto stateXml = apvts.copyState().createXml())
            root.addChildElement (stateXml.release());

        return root.writeTo (file) ? file : juce::File();
    }

    // Applies a full state replace from a saved preset file.
    inline bool loadPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
    {
        auto doc = juce::XmlDocument::parse (file);
        if (doc == nullptr || doc->getTagName() != "SPARKER_PRESET")
            return false;

        auto* stateXml = doc->getChildElement (0);
        if (stateXml == nullptr)
            return false;

        auto tree = juce::ValueTree::fromXml (*stateXml);
        if (! tree.isValid())
            return false;

        apvts.replaceState (tree);
        return true;
    }

    // Display name stored as an attribute so the browser doesn't need to
    // fully parse+apply each file just to list it.
    inline juce::String readPresetName (const juce::File& file)
    {
        auto doc = juce::XmlDocument::parse (file);
        if (doc == nullptr) return file.getFileNameWithoutExtension();
        return doc->getStringAttribute ("name", file.getFileNameWithoutExtension());
    }

    inline juce::Array<juce::File> listUserPresetFiles()
    {
        return getUserPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.sparkerpreset");
    }
}
