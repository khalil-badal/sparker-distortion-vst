#include "PluginProcessor.h"
#include "PluginEditor.h"

// ===========================================================================
// Parameter layout
// ===========================================================================

// This function runs once, during construction, to tell the APVTS which
// parameters exist and what their ranges/defaults are.
// The DAW reads this list to display automation lanes.
juce::AudioProcessorValueTreeState::ParameterLayout
SuhrRiotProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Drive (RV1, B100k linear taper on the real pedal).
    // 0.0 = minimum gain (~10x), 1.0 = maximum gain (~470x).
    // Linear in the parameter, but DriveStage applies a circuit-derived curve.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Tone (10kC reverse-log pot + 22n cap on the real pedal).
    // A simple variable treble cut: 0.0 = dark (LPF at ~723 Hz),
    // 1.0 = bright (LPF at ~12 kHz, effectively open).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Level (RV3, A10k audio taper on the real pedal).
    // OutputStage applies a level^2 curve to simulate the log taper.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "level", 1 }, "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Mode — the 3-way slide switch on the pedal, selects clipping diode set.
    // 0 = Germanium (1N34A, soft/warm),  1 = Silicon (1N914, tighter),
    // 2 = LED (red LEDs, high threshold, most dynamic headroom)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Germanium", "Silicon", "LED" }, 0));

    // Circuit — top-level pedal-model selector. 0=Ember (Suhr Riot),
    // 1=Burner (REVV G3, stub), 2=Blazer (Friedman BE-OD, stub).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "circuit", 1 }, "Circuit",
        juce::StringArray { "Ember", "Burner", "Blazer" }, 0));

    // Active (bypass) — true = effect engaged, false = hard bypass (dry pass-through).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "active", 1 }, "Active", true));

    // Oversampling order — 0=2x, 1=4x, 2=8x (default).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "osOrder", 1 }, "Oversampling",
        juce::StringArray { "2x", "4x", "8x" }, 2));

    // Input noise gate threshold.
    // -70 dBFS = essentially off;  -20 dBFS = very aggressive.
    // Default -50 dBFS sits above typical pickup/cable noise but below soft picks.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateThresh", 1 }, "Gate",
        juce::NormalisableRange<float> (-70.0f, -20.0f), -50.0f));

    // Input Trim — pickup-level matching before the drive chain.
    // 0 dB = unity, negative values attenuate (reduce saturation),
    // positive values push harder into the gain stage.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputTrim", 1 }, "Input Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    // Tight — 2nd-order Butterworth HPF cutoff placed before distortion.
    // Skewed range (skew=0.3) so most of the knob travel covers 20–150 Hz
    // where pickup low-end bloom is musically significant.
    // 20 Hz = fully open (flat response), 500 Hz = maximum tightness.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tightFreq", 1 }, "Tight",
        juce::NormalisableRange<float> (20.0f, 500.0f, 0.0f, 0.3f), 20.0f));

    // Output Trim — post-chain makeup gain.
    // Compensates for level changes caused by the other controls.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputTrim", 1 }, "Output Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    // -----------------------------------------------------------------------
    // Burner (REVV G3) parameters — ranges/tapers from getParameterRanges()
    // (Phase 4). Gain/Mid are linear (B1M/B100k pots feeding stages whose
    // own gain math is already logarithmic); Volume/Bass/Treble get an
    // audio-taper skew (A50k/A100k attenuators/shelves). See
    // ParameterRanges.h for the full rationale.
    // -----------------------------------------------------------------------
    const auto burnerRanges = getParameterRanges (CircuitMode::BURNER);

    auto makeBurnerParam = [&burnerRanges] (const char* paramId, const char* displayName)
    {
        const auto& r = burnerRanges.at (paramId);
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId, 1 }, displayName,
            juce::NormalisableRange<float> (r.min, r.max, 0.0f, r.skew), r.defaultValue);
    };

    params.push_back (makeBurnerParam ("burnerGain",   "Burner Gain"));
    params.push_back (makeBurnerParam ("burnerVolume", "Burner Volume"));
    params.push_back (makeBurnerParam ("burnerBass",   "Burner Bass"));
    params.push_back (makeBurnerParam ("burnerMid",    "Burner Mid"));
    params.push_back (makeBurnerParam ("burnerTreble", "Burner Treble"));

    // Aggression — 2-position switch (SW1, DPDT ON-ON). 0 = one 2n2 cap
    // active, 1 = both 2n2 caps in parallel (4n4). See BurnerCircuit.h.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "burnerAggression", 1 }, "Aggression",
        juce::StringArray { "1", "2" }, 0));

    // -----------------------------------------------------------------------
    // Blazer (Friedman BE-OD) parameters — ranges/tapers from
    // getParameterRanges() (Phase 9). Several are C-taper (reverse-audio):
    // Tight/Bass/Presence. See ParameterRanges.h for the full rationale.
    // -----------------------------------------------------------------------
    const auto blazerRanges = getParameterRanges (CircuitMode::BLAZER);

    auto makeBlazerParam = [&blazerRanges] (const char* paramId, const char* displayName)
    {
        const auto& r = blazerRanges.at (paramId);
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId, 1 }, displayName,
            juce::NormalisableRange<float> (r.min, r.max, 0.0f, r.skew), r.defaultValue);
    };

    params.push_back (makeBlazerParam ("blazerGain",     "Blazer Gain"));
    params.push_back (makeBlazerParam ("blazerVolume",   "Blazer Volume"));
    params.push_back (makeBlazerParam ("blazerBass",     "Blazer Bass"));
    params.push_back (makeBlazerParam ("blazerTreble",   "Blazer Treble"));
    params.push_back (makeBlazerParam ("blazerPresence", "Blazer Presence"));
    params.push_back (makeBlazerParam ("blazerTight",    "Blazer Tight"));
    params.push_back (makeBlazerParam ("blazerTrim",     "Blazer Trim"));

    return { params.begin(), params.end() };
}

// ===========================================================================
// Constructor / Destructor
// ===========================================================================

SuhrRiotProcessor::SuhrRiotProcessor()
    // Declare the bus layout: one stereo input, one stereo output.
    // Guitars are mono but most DAWs send stereo by default; we handle both.
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // The APVTS constructor needs: this processor, an undo manager (nullptr =
      // no undo), a state identifier string, and the parameter layout we built.
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

SuhrRiotProcessor::~SuhrRiotProcessor() {}

// ===========================================================================
// Lifecycle
// ===========================================================================

void SuhrRiotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Give each circuit a chance to allocate its internal buffers and
    // compute sample-rate-dependent values (filter coefficients, etc.).
    // All three are prepared regardless of the active mode, so switching
    // circuits mid-session never hits an unprepared object.
    emberCircuit .prepare (sampleRate, samplesPerBlock);
    burnerCircuit.prepare (sampleRate, samplesPerBlock);
    blazerCircuit.prepare (sampleRate, samplesPerBlock);

    circuitSwitchGain.reset (sampleRate, 0.008); // 8 ms fade across a circuit switch
    circuitSwitchGain.setCurrentAndTargetValue (1.0f);
    lastCircuitMode = -1;
}

void SuhrRiotProcessor::releaseResources()
{
    // Flush all internal filter states and delay lines.
    // Called when playback stops or the plugin is unloaded.
    emberCircuit .reset();
    burnerCircuit.reset();
    blazerCircuit.reset();
}

// ===========================================================================
// Audio processing — the hot path, called by the DAW ~every ~5-20 ms.
// No memory allocation, no locks, no UI calls allowed here.
// ===========================================================================

void SuhrRiotProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& /*midiMessages*/)
{
    // Prevents denormal floats (values extremely close to zero) from causing
    // CPU spikes in some processors. Good practice in any audio plugin.
    juce::ScopedNoDenormals noDenormals;

    // Hard bypass: when inactive, leave the buffer untouched (dry pass-through).
    if (apvts.getRawParameterValue ("active")->load() < 0.5f)
        return;

    // Read current parameter values from the APVTS.
    // getRawParameterValue returns a pointer to an atomic<float>, which is
    // safe to read on the audio thread without locking.
    const float drive       = apvts.getRawParameterValue ("drive")      ->load();
    const float tone        = apvts.getRawParameterValue ("tone")       ->load();
    const float level       = apvts.getRawParameterValue ("level")      ->load();
    const int   mode        = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const float inputTrimDb = apvts.getRawParameterValue ("inputTrim")  ->load();
    const float tightFreq   = apvts.getRawParameterValue ("tightFreq")  ->load();
    const float outTrimDb   = apvts.getRawParameterValue ("outputTrim") ->load();
    const int   osOrder     = static_cast<int> (apvts.getRawParameterValue ("osOrder")->load());
    const float gateThresh  = apvts.getRawParameterValue ("gateThresh")->load();
    const auto  circuitMode = static_cast<CircuitMode> (
                                   static_cast<int> (apvts.getRawParameterValue ("circuit")->load()));

    // Reset all three circuits on a mode switch — an inactive circuit's
    // filters otherwise freeze mid-state while un-called and resume from
    // stale state on reactivation, causing an audible discontinuity
    // (Phase 5 finding). This is a discrete, rare event (user clicking the
    // switch), not a per-block cost.
    if (static_cast<int> (circuitMode) != lastCircuitMode)
    {
        emberCircuit.reset();
        burnerCircuit.reset();
        blazerCircuit.reset();
        lastCircuitMode = static_cast<int> (circuitMode);

        // Fade the output in over ~8ms to mask the filter-startup transient
        // a cold reset causes against an already-live input signal.
        circuitSwitchGain.setCurrentAndTargetValue (0.0f);
        circuitSwitchGain.setTargetValue (1.0f);
    }

    // Route to the active circuit only — the other two sit idle this block.
    switch (circuitMode)
    {
        case CircuitMode::EMBER:
            emberCircuit.process (buffer, drive, tone, level, mode,
                                   inputTrimDb, tightFreq, outTrimDb,
                                   osOrder, gateThresh);
            break;

        case CircuitMode::BURNER:
            burnerCircuit.setGain       (apvts.getRawParameterValue ("burnerGain")      ->load());
            burnerCircuit.setVolume     (apvts.getRawParameterValue ("burnerVolume")    ->load());
            burnerCircuit.setBass       (apvts.getRawParameterValue ("burnerBass")      ->load());
            burnerCircuit.setMid        (apvts.getRawParameterValue ("burnerMid")       ->load());
            burnerCircuit.setTreble     (apvts.getRawParameterValue ("burnerTreble")    ->load());
            burnerCircuit.setAggression (static_cast<int> (apvts.getRawParameterValue ("burnerAggression")->load()));
            burnerCircuit.process (buffer);
            break;

        case CircuitMode::BLAZER:
            blazerCircuit.setGain     (apvts.getRawParameterValue ("blazerGain")    ->load());
            blazerCircuit.setVolume   (apvts.getRawParameterValue ("blazerVolume")  ->load());
            blazerCircuit.setBass     (apvts.getRawParameterValue ("blazerBass")    ->load());
            blazerCircuit.setTreble   (apvts.getRawParameterValue ("blazerTreble")  ->load());
            blazerCircuit.setPresence (apvts.getRawParameterValue ("blazerPresence")->load());
            blazerCircuit.setTight    (apvts.getRawParameterValue ("blazerTight")   ->load());
            blazerCircuit.setTrim     (apvts.getRawParameterValue ("blazerTrim")    ->load());
            blazerCircuit.process (buffer);
            break;
    }

    // Apply the switch fade (a no-op multiply-by-1 once settled).
    if (circuitSwitchGain.isSmoothing())
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = circuitSwitchGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
}

// ===========================================================================
// Preset save / load
// ===========================================================================

void SuhrRiotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialise the entire APVTS state to XML, then to a binary blob that
    // the DAW embeds in the project file.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SuhrRiotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Reverse of getStateInformation: deserialise the blob and restore state.
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ===========================================================================
// Editor factory
// ===========================================================================

juce::AudioProcessorEditor* SuhrRiotProcessor::createEditor()
{
    return new SuhrRiotEditor (*this);
}

// ===========================================================================
// JUCE entry point — tells the DAW how to instantiate this plugin.
// Must be in exactly one .cpp file.
// ===========================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuhrRiotProcessor();
}
