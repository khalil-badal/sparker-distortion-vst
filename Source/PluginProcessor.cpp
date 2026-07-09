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

    // Tone (RV2, C10k reverse-log taper on the real pedal).
    // 0.0 = bass-heavy / low-pass dominant
    // 0.5 = mid-scooped (both poles converge, classic Big Muff tone)
    // 1.0 = treble-heavy / high-pass dominant
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

    // Active (bypass) — true = effect engaged, false = hard bypass (dry pass-through).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "active", 1 }, "Active", true));

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
    // Give each DSP stage a chance to allocate its internal buffers and
    // compute sample-rate-dependent values (filter coefficients, etc.).
    inputTrim    .prepare (sampleRate, samplesPerBlock);
    inputStage   .prepare (sampleRate, samplesPerBlock);
    tightFilter  .prepare (sampleRate, samplesPerBlock);
    driveStage   .prepare (sampleRate, samplesPerBlock);
    clippingStage.prepare (sampleRate, samplesPerBlock);
    toneStack    .prepare (sampleRate, samplesPerBlock);
    outputStage  .prepare (sampleRate, samplesPerBlock);
    outputTrim   .prepare (sampleRate, samplesPerBlock);
}

void SuhrRiotProcessor::releaseResources()
{
    // Flush all internal filter states and delay lines.
    // Called when playback stops or the plugin is unloaded.
    inputTrim    .reset();
    inputStage   .reset();
    tightFilter  .reset();
    driveStage   .reset();
    clippingStage.reset();
    toneStack    .reset();
    outputStage  .reset();
    outputTrim   .reset();
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
    const float drive       = apvts.getRawParameterValue ("drive")     ->load();
    const float tone        = apvts.getRawParameterValue ("tone")      ->load();
    const float level       = apvts.getRawParameterValue ("level")     ->load();
    const int   mode        = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const float inputTrimDb = apvts.getRawParameterValue ("inputTrim") ->load();
    const float tightFreq   = apvts.getRawParameterValue ("tightFreq") ->load();
    const float outTrimDb   = apvts.getRawParameterValue ("outputTrim")->load();

    inputTrim    .setTrimDb    (inputTrimDb);
    tightFilter  .setFrequency (tightFreq);
    driveStage   .setDrive     (drive);
    clippingStage.setMode      (mode);
    toneStack    .setTone      (tone);
    outputStage  .setLevel     (level);
    outputTrim   .setTrimDb    (outTrimDb);

    // Signal chain:
    //   Input Trim → AC buffer → Tight HPF → Drive amp → Clipper → Tone → Level → Output Trim
    inputTrim    .process (buffer);
    inputStage   .process (buffer);
    tightFilter  .process (buffer);
    driveStage   .process (buffer);
    clippingStage.process (buffer);
    toneStack    .process (buffer);
    outputStage  .process (buffer);
    outputTrim   .process (buffer);
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
