#pragma once

#include <JuceHeader.h>
#include "CircuitMode.h"
#include "ParameterRanges.h"
#include "dsp/EmberCircuit.h"
#include "dsp/BurnerCircuit.h"
#include "dsp/BlazerCircuit.h"

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
    // DSP chain — one circuit model per CircuitMode. Only the active mode's
    // circuit is processed each block; the other two sit idle.
    //
    // EMBER (Suhr Riot) is a verbatim wrap of the pre-existing Riot signal
    // chain — see dsp/EmberCircuit.h. BURNER (REVV G3) and BLAZER
    // (Friedman BE-OD) are silent pass-through stubs pending their own DSP
    // implementation.
    // -----------------------------------------------------------------------
    EmberCircuit  emberCircuit;
    BurnerCircuit burnerCircuit;
    BlazerCircuit blazerCircuit;

    // Tracks the previously-active circuit so a mode switch can reset all
    // three circuits' internal filter state — an inactive circuit's filters
    // otherwise freeze mid-state while un-called, then resume from that
    // stale state on reactivation, producing an audible discontinuity
    // (measured in Phase 5 integration testing). This never touches
    // EmberCircuit's internals — only calls its existing public reset().
    int lastCircuitMode = -1;

    // Short gain ramp applied across a circuit switch. Resetting a circuit's
    // filters to a cold-zero state while the live input is already
    // mid-signal causes its own startup transient (measured: resetting
    // alone was not sufficient — see Phase 5 findings), so this fades the
    // output in over ~8ms after any switch, masking the transient
    // regardless of its cause.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> circuitSwitchGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotProcessor)
};
