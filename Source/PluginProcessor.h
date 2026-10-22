#pragma once

#include <JuceHeader.h>
#include "CircuitMode.h"
#include "ParameterRanges.h"
#include "BurnerCascade.h"
#include "dsp/EmberCircuit.h"
#include "dsp/BurnerCircuit.h"
#include "dsp/BlazerCircuit.h"
#include "dsp/TrimStage.h"
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

    // Real input/output peak levels (linear, 0..1+ — can exceed 1.0 during
    // clipping), for the UI's LevelMeter. Written on the audio thread in
    // processBlock, read on the message thread by the editor's timer —
    // atomic<float> is safe for that single-writer/single-reader pattern
    // without locking. Instant attack, decayed release (see .cpp) so the
    // meter reads like a real peak meter, not a raw sample-and-hold.
    std::atomic<float> inputPeakLevel  { 0.0f };
    std::atomic<float> outputPeakLevel { 0.0f };

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

    // Input/Output Trim for Burner and Blazer only. Ember already applies
    // its own inputTrim/outputTrim internally as part of its frozen wrapped
    // chain (EmberCircuit.cpp) — these are separate TrimStage instances
    // wrapped around Burner/Blazer's process() calls at the routing level,
    // so Ember is never double-trimmed and neither BurnerCircuit nor
    // BlazerCircuit's own files need to be touched to gain this control.
    TrimStage burnerBlazerInputTrim;
    TrimStage burnerBlazerOutputTrim;

    // Noise gate for Burner and Blazer only, same precedent as the Trim
    // stages above — Ember already has its own internal NoiseGate as part
    // of its frozen wrapped chain (EmberCircuit.cpp). The AI Studio UI
    // redesign shows a GATE knob on all three pages, so this makes it
    // functional everywhere it's shown rather than decorative on two of
    // three pages.
    NoiseGate burnerBlazerGate;

    // -----------------------------------------------------------------------
    // Character-shaping post-EQ filters (routing level, downstream of each
    // frozen circuit — not touching the circuit code).
    //
    // These exist to correct three researched shortcomings the frozen DSP
    // itself cannot address:
    //
    //   EMBER: DriveStage's Stage 2 has a 1.6 kHz LPF ahead of the clipper
    //   (schematic-accurate: C5=100pF across R4=1M), rolling off top-end
    //   BEFORE the tone knob sees it. To keep the pedal from reading as dark
    //   as a Big Muff, a permanent post-clip high shelf restores presence,
    //   plus a tone-knob-following brightness boost that only kicks in above
    //   tone=0.5 (the real Riot's tone control is a variable LPF that can only
    //   cut, so we extend it additively above noon).
    //
    //   Mode differentiation is now handled entirely inside ClippingStage,
    //   which selects three genuinely different diode strings (WARM germanium
    //   / CLASSIC asymmetric silicon / HOT red LED). The old mode-dependent
    //   post-EQ "character filter" that faked the difference has been removed.
    //
    //   BURNER (REVV G3): the real amp/pedal's signature aggression sits
    //   in the 800 Hz-1.2 kHz upper-mid range. Fixed via a permanent
    //   +4 dB peak @850 Hz (growl) + a smaller +2 dB peak @180 Hz for
    //   palm-mute chunk. User can still cut mids via the Mid EQ knob if
    //   they want a scooped voicing.
    //
    //   BLAZER (Friedman BE-OD): real BE-OD is fat with punchy low-mids
    //   (modded-Marshall JCM800 lineage). Fixed via +3 dB peak @250 Hz
    //   (body) + a +2 dB low shelf @150 Hz (warmth).
    // -----------------------------------------------------------------------
    using IIRDup = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRDup emberTrebleShelf;   // fixed high shelf, compensates DriveStage 1.6 kHz LPF
    IIRDup emberToneBoost;     // tone-following extra brightness (tone > 0.5 only)

    // SHARED tone-stack mid scoop, applied to ALL THREE circuits.
    //
    // Every real guitar amp has a passive TMB tone stack with an inherent
    // 8-15 dB notch around 400-600 Hz. All three Sparker circuits are PEDAL
    // models, so in a real rig the amp would supply that notch — but running
    // them into an IR (or into a clean platform that barely scoops) nothing
    // ever does, and all the low-mid harmonic energy the clippers generate
    // passes straight through. Measured with pink noise, all three ramped
    // monotonically up to 1 kHz with NO notch, leaving 250-500 Hz only 2-6 dB
    // under 1 kHz — which is exactly "boxy and muddy".
    //
    // Applied once, after the circuit switch, so all three get identical
    // treatment (the complaint was identical on all three) and so the
    // cross-circuit loudness compensation stays valid — a shared gain/EQ
    // cancels in the circuit-to-circuit ratio.
    IIRDup sharedMidScoop;

    IIRDup burnerGrindPeak;    // upper-mid "gnarl"/cut boost @2.2 kHz — REVV aggression
    IIRDup burnerBodyPeak;     // low-mid palm-mute "chug" thump — heaviness
    IIRDup burnerPresenceShelf;// presence/bite shelf — djent top-end cut

    // Power-amp emulation for Burner (filters here; the oversampler is with the
    // other OvS members below). The REVV Generator 120 is a full amp; the
    // pedal-into-clean-amp path (our G3 into e.g. a Morgan SW50R) is missing
    // the Generator's tight, saturated POWER section, which supplies much of
    // the "heaviness". Modelled as: tight HPF (power-amp/OT low-end firmness)
    // -> gentle symmetric tanh saturation (oversampled) -> OT-style high-cut.
    IIRDup burnerPowerHpf;     // ~75 Hz — power-amp/OT low-end tightness
    IIRDup burnerPowerLpf;     // ~10 kHz — output-transformer top rolloff

    IIRDup blazerBodyPeak;     // midrange "bark" @900 Hz — the BE-OD's mid-forward voice
    IIRDup blazerWarmthShelf;  // low shelf @180 Hz — weight/authority below the shared scoop
    IIRDup blazerTopTame;      // 2nd-order LPF @6 kHz — Blazer's only HF rolloff (BOTH modes)
    IIRDup blazerPresencePeak; // +4 dB @2.8 kHz, DRIVE mode only — see PluginProcessor.cpp

    // Blazer power-amp stage. Blazer was structurally the thinnest circuit —
    // a single pre-gain into ONE diode clipper, with no cascade, no Miller
    // rolloff and (unlike Burner) no power-amp stage — which is why it read
    // as "weak" next to the other two. It gets the same dynamic-sag power
    // section as Burner, for the same reason: it is a pedal driving a clean
    // amp, so nothing downstream supplies power-section density or give.
    float blazerSagEnv = 0.0f;

    // -----------------------------------------------------------------------
    // Burner cascaded soft-saturation "preamp" (oversampled).
    //
    // The frozen BurnerCircuit's diode-in-feedback clipper is a HARD limiter:
    // it clamps output to a near-constant amplitude for any input past a tiny
    // threshold. With a high fixed pre-gain the signal is already a hard
    // square at every Gain-knob position, so the Gain knob feels "dead" and
    // the result is a static buzzy square rather than the dense, sustaining
    // saturation of a real high-gain amp. The REVV G3 (and the Generator amp
    // it models) derives its gain from MULTIPLE cascaded tube stages, each
    // softly clipping — that cumulative density IS the high-gain feel.
    //
    // This stage models that: a THREE-stage tanh cascade with progressive
    // inter-stage high-pass filtering (see BurnerCascade.h) ahead of the diode
    // clipper — the inter-stage tightening is what makes it read as a REVV
    // (tight/articulate) rather than a mushy or buzzy generic distortion.
    // Oversampled (8×) to keep the three stages' harmonics from aliasing.
    // -----------------------------------------------------------------------
    using OvS = juce::dsp::Oversampling<float>;
    OvS burnerDriveOs   { 2, 4, OvS::filterHalfBandPolyphaseIIR }; // 2 ch, 16× (2^4)
    // 8x (was 4x). Measured as NOT the dominant aliasing source, but it was
    // the lowest-rate nonlinearity in the plugin and it operates on an
    // already harmonic-rich signal, so it is cheap insurance to match the
    // rest of the chain.
    OvS burnerPowerAmpOs { 2, 3, OvS::filterHalfBandPolyphaseIIR }; // 2 ch, 8× — power-amp sat
    OvS blazerPowerAmpOs { 2, 3, OvS::filterHalfBandPolyphaseIIR }; // 2 ch, 8× — power-amp sat

    burner::CascadeState burnerCascadeState[2];  // per-channel filter memory
    burner::Coeffs       burnerCascadeCoeffs;    // set in prepareToPlay (at the OS rate)

    // Power-amp sag state: a single STEREO-LINKED envelope (one detector for
    // all channels, so the sag can't shift the stereo image). Drives the
    // dynamic headroom in the power-amp stage — see kBurnerSagDepth.
    float burnerSagEnv     = 0.0f;
    float powerSagAttack  = 0.0f;   // one-pole coefficients at the power-amp OS rate
    float powerSagRelease = 0.0f;

    // State tracking so per-block coefficient updates only run when the
    // relevant knob/switch actually changed. (Fixed filters are set once
    // in prepareToPlay and never touched again — no tracker needed.)
    float lastEmberTone = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuhrRiotProcessor)
};
