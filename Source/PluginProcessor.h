#pragma once

#include <JuceHeader.h>
#include "dsp/InputStage.h"
#include "dsp/TrimStage.h"
#include "dsp/TightFilter.h"
#include "dsp/DriveStage.h"
#include "dsp/ClippingStage.h"
#include "dsp/ToneStack.h"
#include "dsp/OutputStage.h"
#include "dsp/NoiseGate.h"

// ---------------------------------------------------------------------------
// SuhrRiotProcessor — the "brain" of the plugin.
//
// Responsibilities:
//   1. Owns all DSP objects (one per circuit block).
//   2. Owns the parameter state (APVTS) — the single source of truth for
//      every knob and switch value.
//   3. The DAW calls prepareToPlay() before audio starts and processBlock()
//      for each buffer of audio during playback.
// ---------------------------------------------------------------------------
class SuhrRiotProcessor : public juce::AudioProcessor
{
public:
    SuhrRiotProcessor();
    ~SuhrRiotProcessor() override;

    // -----------------------------------------------------------------------
    // DAW lifecycle callbacks
    // -----------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // -----------------------------------------------------------------------
    // Plugin identity
    // -----------------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override              { return true; }

    const juce::String getName() const override  { return "Suhr Riot"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Single preset slot — we use APVTS for save/load instead of programs.
    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // -----------------------------------------------------------------------
    // Preset persistence — DAW calls these to save/restore plugin state.
    // -----------------------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -----------------------------------------------------------------------
    // Parameter state (public so the editor can attach sliders/boxes to it).
    // AudioProcessorValueTreeState (APVTS) manages thread-safe parameter access
    // and handles DAW automation for free.
    // -----------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

private:
    // Called once during construction to register all parameters.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // -----------------------------------------------------------------------
    // DSP chain — one object per circuit block, processed in signal-flow order.
    // -----------------------------------------------------------------------
    TrimStage     inputTrim;        // ±12 dB gain before drive chain
    NoiseGate     noiseGate;       // input gate — silences idle noise before the drive
    InputStage    inputStage;      // C1/R1 AC coupling + RF filter
    TightFilter   tightFilter;     // pre-distortion Butterworth HPF (20–500 Hz)
    DriveStage    driveStage;      // U1B variable gain + bass shaping
    ClippingStage clippingStage;   // D1-D8 diode clipper (3 modes, oversampled)
    ToneStack     toneStack;       // Big Muff RC tone control
    OutputStage   outputStage;     // U2A recovery gain + Level pot
    TrimStage     outputTrim;      // ±12 dB final makeup gain

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotProcessor)
};
