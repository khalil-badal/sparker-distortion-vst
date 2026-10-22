#pragma once
#include <JuceHeader.h>
#include <limits>

// ============================================================================
// FROZEN — Phase 11 final sign-off. Sparker Distortion v2.0 is
// feature-complete: Burner (REVV G3), Ember (Suhr Riot, see EmberCircuit.h),
// and Blazer (Friedman BE-OD, see BlazerCircuit.h) are ALL frozen as of this
// point. No further modifications to this file, its .cpp, Ember, or Blazer
// are permitted without a new, explicitly-scoped phase sequence.
//
// EXCEPTION ON RECORD (Phase 10): the Newton-Raphson diode clipper's vout
// clamp was tightened from +-20V to +-4V to prevent exp() overflow.
//
// EXCEPTION ON RECORD (post-v2.0, explicit new instruction): added fixed
// 8x oversampling around the Newton-Raphson clipper loop only (matching
// Ember's ClippingStage pattern exactly — linear filters stay at base
// rate). The NR solver math itself was NOT touched, only wrapped in an
// upsample/downsample pair. Oversampling is no longer user-selectable
// anywhere in the plugin (was Ember-only before; now implicit and fixed
// at 8x for all three circuits).
//
// EXCEPTION ON RECORD (post-Phase-11 LEVEL-FIDELITY REVISION, explicitly
// authorised): the VOLUME gain mapping was corrected to match the hardware.
// VOLUME is a BOM-confirmed A50k (AUDIO taper) pot, but the model applied it
// LINEARLY and capped total output at unity. Now: audio taper (volumeNorm^2)
// multiplied by IC2's fixed makeup gain, which restores the pedal's real
// ability to drive an amp past unity. The `makeupGain` member existed but was
// never referenced (a dead stub) and is now wired into the gain calculation.
// The noon value is deliberately unchanged (0.5^2 * 2.0 = 0.5, matching the
// old linear map at 0.5), so this corrects taper and headroom without moving
// the calibrated default. Clipping character, the NR solver, oversampling,
// the tone stack and the aggression circuitry are all untouched.
// Burner is re-frozen as of this revision.
//
// EXCEPTION ON RECORD (post-v2.0, explicit new instruction): added a Tight
// control (continuous pre-clip HPF). IMPORTANT — the real REVV G3 has NO
// Tight knob; the confirmed Phase 2 BOM lists exactly six controls (Gain,
// Volume, Bass, Treble, Mid, Aggression), no Tight pot anywhere. This is a
// new, non-schematic-accurate feature added purely at explicit request, not
// a circuit-derived control like everything else in this file. Modelled
// after Blazer's own Tight implementation (which IS schematic-accurate for
// that pedal) purely for a consistent feel across modes.
// ============================================================================
//
// ---------------------------------------------------------------------------
// BurnerCircuit — REVV G3 (Revolution III / Aion "Revvolution" clone) circuit
// model. See Phase 2 analysis (session record) for the schematic/BOM
// reconciliation this is built from — summary of what's WIRE-VERIFIED vs.
// ENGINEERING ESTIMATE:
//
//   VERIFIED  (Revolution III BOM, page 3 of Revv G3.pdf):
//     - SW1 = DPDT ON-ON (2-position aggression switch)
//     - GAIN=B1M, VOLUME=A50k, BASS=A100K, TREBLE=A50k, MID=B100k
//     - Clipping diodes: D2-D5, four 1N4148, confirmed count
//     - Aggression caps: C3=2n2, C4=2n2
//     - Input network: R1=1M pulldown, R2=470k/C1=22n coupling HPF
//     - Family: doc's own text confirms "Tube-to-JFET" style, same family
//       as Friedman BE-OD
//     - Aggression-switch drawing-error correction: confirmed via
//       r/diypedals thread (per user, 2020) — SW1_B treated as flipped,
//       giving two real states (C3 alone / C3‖C4=4n4), not a permanent short
//
//   ENGINEERING ESTIMATE (schematic wire-tracing wasn't recoverable from the
//   OCR'd PDF — flagged during Phase 2, not independently wire-verified):
//     - Exact Rin/Rf values feeding the diode-feedback clipper (uses R4=47k
//       as Rin, R5=82k as the Rf floor beneath the B1M gain pot)
//     - Which resistor pairs with C3/C4 for the aggression corner (uses
//       R31=4k7, physically adjacent to the switch in the BOM/schematic)
//     - Exact op-amp stage count/order beyond IC1_A (buffer) → IC1_B
//       (gain+clip); IC2 is treated as a fixed-gain makeup buffer since its
//       role couldn't be confirmed
//   These are the values to ear-tune once auditioned against the real
//   pedal/plugin comparisons — not wire-verified constants like the diode
//   count or pot tapers are.
//
// NONLINEARITY: the diode-in-feedback clipper (D2-D5, two antiparallel
// pairs) is solved per-sample with Newton-Raphson on the op-amp feedback
// node equation — this is the physically correct way to model diodes
// inside a feedback loop (as opposed to Ember's closed-form tanh, which
// approximates a diode-to-ground shunt clipper, a different topology).
// The stage is memoryless (no reactive element inside the same KCL loop
// solved here), so no state carries between samples except the previous
// Newton-Raphson solution, kept only to seed the next iteration.
// ---------------------------------------------------------------------------
class BurnerCircuit
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameters, all normalized 0..1 except aggression (0 or 1).
    // Phase 4 will map these from circuit-appropriate pot curves; for now
    // they're consumed directly as normalized gain-stage controls.
    void setGain       (float norm) noexcept { gainNorm  = juce::jlimit (0.0f, 1.0f, norm); }
    void setVolume      (float norm) noexcept { volumeNorm = juce::jlimit (0.0f, 1.0f, norm); }
    void setBass        (float norm) noexcept { bassNorm   = juce::jlimit (0.0f, 1.0f, norm); }
    void setMid         (float norm) noexcept { midNorm    = juce::jlimit (0.0f, 1.0f, norm); }
    void setTreble       (float norm) noexcept { trebleNorm = juce::jlimit (0.0f, 1.0f, norm); }
    void setAggression  (int state)  noexcept { aggression = juce::jlimit (0, 1, state); }
    // Not schematic-accurate — see class-level comment. 0 = fully open
    // (no HPF action), matching Blazer/Ember's Tight convention.
    void setTight        (float norm) noexcept { tightNorm  = juce::jlimit (0.0f, 1.0f, norm); }

private:
    double sampleRate = 44100.0;

    float gainNorm    = 0.5f;
    float volumeNorm  = 0.5f;
    float bassNorm    = 0.5f;
    float midNorm     = 0.5f;
    float trebleNorm  = 0.5f;
    int   aggression  = 0;
    int   lastAggression = -1;   // forces coefficient recompute on first block
    float tightNorm   = 0.0f;

    // Cached last-applied control values — the tone-stack and Tight IIR
    // coefficients are only rebuilt when the corresponding knob actually
    // moves. Rebuilding every block called juce::dsp's makeXxx factories,
    // each of which heap-allocates a Coefficients object — real-time-unsafe
    // audio-thread allocation. Guarding behind change-detection makes the
    // steady state (knobs still) allocation-free. NaN sentinel forces the
    // first block to compute. (aggression already had this guard.)
    float lastTightNorm  = std::numeric_limits<float>::quiet_NaN();
    float lastBassNorm   = std::numeric_limits<float>::quiet_NaN();
    float lastTrebleNorm = std::numeric_limits<float>::quiet_NaN();
    float lastMidNorm    = std::numeric_limits<float>::quiet_NaN();

    // Input DC-block / coupling HPF (R2=470k, C1=22n -> ~15.4 Hz).
    using IIRDup = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRDup inputHpf;

    // Tight: continuous pre-clip HPF, NOT schematic-accurate (see
    // class-level comment). 20-400 Hz range, matching Blazer's own range.
    IIRDup tightHpf;

    // Aggression corner filters: two states per the corrected aggression
    // switch — see class-level comment. Applied BOTH pre-clip (as a
    // high-shelf that boosts/cuts how hard high frequencies drive the
    // clipper — matches the doc's own note that the pedal "might squeal"
    // with aggression engaged and gain high, i.e. more HF drive into the
    // nonlinearity, not just a static EQ move) AND post-clip (shaping the
    // harmonics clipping actually generates, which is what makes the two
    // states audibly distinguishable regardless of the input's fundamental
    // frequency — see Phase 5 finding: a purely pre-clip low-pass at these
    // corners was measured to make ~0% audible difference on a low
    // fundamental, since guitar/test fundamentals sit far below 7-15 kHz).
    IIRDup aggressionPreFilter;
    IIRDup aggressionPostFilter;

    // Post-clip output coupling (C24=220nf -> ~1.4 Hz, effectively DC-block).
    IIRDup outputHpf;

    // Tone stack: Baxandall bass/treble shelves + active mid peak.
    IIRDup bassShelf;
    IIRDup trebleShelf;
    IIRDup midPeak;

    // Newton-Raphson diode-clipper state: previous solved Vout, seeds the
    // next sample's iteration (purely a convergence aid, not true memory —
    // the stage itself is memoryless).
    static constexpr int kMaxChannels = 2;
    float clipperPrevVout[kMaxChannels] = { 0.0f, 0.0f };

    // IC2's fixed output makeup gain, applied together with the VOLUME pot
    // (see BurnerCircuit.cpp). ENGINEERING ESTIMATE — IC2's exact role and
    // gain were not wire-confirmed (see the estimate list above); 2.0 (+6 dB)
    // is a standard makeup-buffer value and gives the pedal genuine output
    // on tap rather than capping it at unity.
    //
    // This member previously existed but was never referenced — a dead stub.
    // It is wired into the real gain calculation as of the post-Phase-11
    // level-fidelity revision.
    float makeupGain = 2.0f;

    // Fixed 8× oversampling around the NR clipper loop (post-v2.0 exception).
    // filterHalfBandPolyphaseIIR was evaluated against filterHalfBandFIREquiripple:
    // the IIR variant achieves ~−70 dB stopband attenuation; FIR equiripple
    // reaches ~−90 dB but adds significant latency (group delay from the
    // linear-phase FIR). At 8× OS the folding harmonics are already at
    // very high multiples of the fundamental, so −70 dB IIR puts aliases
    // well below the noise floor — the extra 20 dB from FIR is inaudible.
    // IIR is kept for its lower latency.
    using OvS = juce::dsp::Oversampling<float>;
    // 8x. Measured NOT to be a meaningful aliasing source for Burner: its
    // diode is fed a deliberately tiny signal (kBurnerDiodeFeed) so it sits
    // near its soft knee rather than hard-clipping. Raising it to 16x or 32x
    // changed Burner aliasing by <1 dB (and 16x measured marginally worse),
    // so the extra NR/exp() cost buys nothing here. Burner anti-aliasing is
    // handled where it matters — ADAA on the cascade (see BurnerCascade.h).
    OvS os8x { 2, 3, OvS::filterHalfBandPolyphaseIIR };
};
