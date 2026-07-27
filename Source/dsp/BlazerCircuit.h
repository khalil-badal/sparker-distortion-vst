#pragma once
#include <JuceHeader.h>

// ============================================================================
// FROZEN — Phase 11 final sign-off. Sparker Distortion v2.0 is
// feature-complete: Blazer (Friedman BE-OD), Ember (Suhr Riot, see
// EmberCircuit.h), and Burner (REVV G3, see BurnerCircuit.h) are ALL frozen
// as of this point. No further modifications to this file, its .cpp,
// Ember, or Burner are permitted without a new, explicitly-scoped phase
// sequence. This file's own Newton-Raphson clipper clamp (+-4V, see
// BlazerCircuit.cpp) was tightened during Phase 10 testing — see that
// file's comment for the exp()-overflow rationale — before this freeze.
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
};
