#pragma once
#include <JuceHeader.h>
#include <limits>

// ============================================================================
// FROZEN — Phase 11 final sign-off. Sparker Distortion v2.0 is
// feature-complete: Blazer (Friedman BE-OD), Ember (Suhr Riot, see
// EmberCircuit.h), and Burner (REVV G3, see BurnerCircuit.h) are ALL frozen
// as of this point. No further modifications to this file, its .cpp,
// Ember, or Burner are permitted without a new, explicitly-scoped phase
// sequence.
//
// EXCEPTION ON RECORD (Phase 10): the Newton-Raphson clipper's vout clamp
// was tightened to +-4V to prevent exp() overflow.
//
// EXCEPTION ON RECORD (post-v2.0, explicit new instruction): added fixed
// 8x oversampling around the Newton-Raphson clipper loop only, same
// pattern as BurnerCircuit/Ember's ClippingStage. Solver math untouched.
// Oversampling is no longer user-selectable anywhere in the plugin.
//
// EXCEPTION ON RECORD (post-Phase-11 LEVEL-FIDELITY REVISION, explicitly
// authorised): the VOLUME gain mapping was corrected to match the hardware.
// VOLUMEN is a BOM-confirmed A50K (AUDIO taper) pot, but the model applied it
// LINEARLY and capped total output at unity — a real BE-OD is a notoriously
// loud pedal with makeup to spare. Now: audio taper (volumeNorm^2) times a
// fixed output makeup gain (new `makeupGain` member, mirroring Burner's IC2
// stage). The noon value is deliberately unchanged (0.5^2 * 2.0 = 0.5,
// matching the old linear map at 0.5), so this corrects taper and headroom
// without moving the calibrated default. Clipping character, the NR solver,
// oversampling, the tone stack and the Tight control are all untouched.
// Blazer is re-frozen as of this revision.
// ============================================================================
//
// ---------------------------------------------------------------------------
// BlazerCircuit — Friedman BE-OD circuit model. See Phase 7 analysis
// (session record) for the schematic/BOM reconciliation — summary of
// what's WIRE-VERIFIED vs. ENGINEERING ESTIMATE:
//
//   VERIFIED (Freeman BE-OD BOM, page 3 of Friedman BE-OD.pdf):
//     - IC1/IC2/IC3 all TL072, no TL074 (6 op-amp sections, simpler than
//       Burner's 8)
//     - Zero switches anywhere on the board — TIGHT is a continuous pot
//     - Pot values/tapers: TIGHT=C100K, TREBLE=B100K, VOLUMEN=A50K,
//       BASS=C100K, PRES=C10K, GAIN=B1M; TRIM trimpot=100K
//     - Clipping diodes: D2-D5, four 1N4148, confirmed count (same family
//       as Ember/Burner)
//     - Input network: R1=1M, R2=470k, C1=22n
//     - Gain-stage stability caps: C2=47p, C9=47p
//     - Output coupling: C17=220n, C18=220n
//     - Power: D8=1N5817, C20=100uF, C22=22uF
//     - D6/D7/LED1/LED2 confirmed indicator LEDs, not signal path
//     - No separate Mid control (3-band Bass/Treble/Presence only)
//
//   ENGINEERING ESTIMATE (not wire-traced from the OCR'd schematic):
//     - Exact op-amp stage roles/order beyond input buffer -> gain+clip
//     - TIGHT's exact wiring point (modelled as a pre-clip HPF, strongly
//       supported by naming convention and this codebase's own Ember Tight
//       control, but not wire-confirmed)
//     - TRIM's exact role (modelled as a gain-floor offset added to the
//       clipper's base feedback resistor, per the doc's own description)
//     - Rin/Rf values feeding the diode-feedback clipper
//
// NONLINEARITY: same Newton-Raphson diode-in-feedback solve pattern as
// BurnerCircuit (same 1N4148 family, same equation) — re-implemented here
// rather than shared, since BurnerCircuit is frozen (Phase 6) and must not
// be refactored to extract a shared helper.
// ---------------------------------------------------------------------------
class BlazerCircuit
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameters, all normalized 0..1. Phase 9 maps circuit-accurate
    // pot curves (several C-taper/reverse-audio) onto these.
    void setGain     (float norm) noexcept { gainNorm  = juce::jlimit (0.0f, 1.0f, norm); }
    void setVolume    (float norm) noexcept { volumeNorm = juce::jlimit (0.0f, 1.0f, norm); }
    void setBass      (float norm) noexcept { bassNorm   = juce::jlimit (0.0f, 1.0f, norm); }
    void setTreble    (float norm) noexcept { trebleNorm = juce::jlimit (0.0f, 1.0f, norm); }
    void setPresence (float norm) noexcept { presenceNorm = juce::jlimit (0.0f, 1.0f, norm); }
    void setTight     (float norm) noexcept { tightNorm  = juce::jlimit (0.0f, 1.0f, norm); }
    void setTrim      (float norm) noexcept { trimNorm   = juce::jlimit (0.0f, 1.0f, norm); } // internal/advanced

private:
    double sampleRate = 44100.0;

    float gainNorm     = 0.5f;
    float volumeNorm   = 0.5f;
    float bassNorm     = 0.5f;
    float trebleNorm   = 0.5f;
    float presenceNorm = 0.5f;
    float tightNorm    = 0.0f;   // 0 = fully open (no HPF action), matches Ember's Tight convention
    float trimNorm     = 0.5f;   // internal parameter, defaults to midpoint

    // Cached last-applied control values — the tone-stack and Tight IIR
    // coefficients are rebuilt only when their knob moves. The juce::dsp
    // makeXxx factories heap-allocate a Coefficients object, so rebuilding
    // every block was real-time-unsafe audio-thread allocation; guarding
    // makes the steady state allocation-free. NaN sentinel (reset in
    // prepare()) forces a rebuild on the first block / after a rate change.
    float lastTightNorm    = std::numeric_limits<float>::quiet_NaN();
    float lastBassNorm     = std::numeric_limits<float>::quiet_NaN();
    float lastTrebleNorm   = std::numeric_limits<float>::quiet_NaN();
    float lastPresenceNorm = std::numeric_limits<float>::quiet_NaN();

    using IIRDup = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRDup inputHpf;      // R2=470k, C1=22n input coupling
    IIRDup tightHpf;      // continuous pre-clip HPF (TIGHT control)
    IIRDup outputHpf;     // C17/C18=220n output coupling

    // 3-band passive-style EQ: Bass/Treble/Presence, no active mid.
    IIRDup bassShelf;
    IIRDup trebleShelf;
    IIRDup presenceShelf;

    static constexpr int kMaxChannels = 2;
    float clipperPrevVout[kMaxChannels] = { 0.0f, 0.0f };

    // Fixed output makeup gain, applied together with the VOLUMEN pot (see
    // BlazerCircuit.cpp). ENGINEERING ESTIMATE — the op-amp stage roles beyond
    // input buffer -> gain+clip were not wire-traced (see the estimate list
    // above), so the exact makeup ratio is not confirmed. 2.0 (+6 dB) matches
    // the equivalent stage in BurnerCircuit and gives the pedal real output on
    // tap instead of capping it at unity. Added in the post-Phase-11
    // level-fidelity revision.
    float makeupGain = 2.0f;

    // Fixed 8× oversampling around the NR clipper loop (post-v2.0 exception).
    // IIR half-band filter kept over FIR equiripple: −70 dB stopband is
    // sufficient at 8× (aliases land >90 dB below signal), and IIR has
    // lower latency. See BurnerCircuit.h for the full evaluation.
    using OvS = juce::dsp::Oversampling<float>;
    // 32x (was 8x). The NR diode is a HARD clipper and, unlike Burner, Blazer
    // drives it FAR past its knee, so it approaches an ideal square whose
    // harmonics fall off at only 6 dB/oct — a great deal of energy sits above
    // the oversampled Nyquist and folds back. Measured, this was the single
    // biggest aliasing source in the plugin: -13.9 dB of non-harmonic energy
    // at 48 kHz (vs -54 dB for Ember, which ADAAs its saturators). There is no
    // closed-form antiderivative for this implicit solve to ADAA with, so
    // oversampling is the available lever: 16x gave -28.8 dB and 32x gives
    // -46.8 dB, with the sample-rate dependence collapsing to ~4 dB (i.e.
    // essentially clean). Costly in exp() calls, but this is the one place in
    // the plugin that genuinely needs it.
    OvS os8x { 2, 5, OvS::filterHalfBandPolyphaseIIR };
};
