#pragma once
#include <JuceHeader.h>
#include "InputStage.h"
#include "TrimStage.h"
#include "TightFilter.h"
#include "DriveStage.h"
#include "ClippingStage.h"
#include "ToneStack.h"
#include "OutputStage.h"
#include "NoiseGate.h"

// ---------------------------------------------------------------------------
// EmberCircuit — the Suhr Riot circuit model, wrapped for CircuitMode routing.
//
// This is a verbatim move of the pre-existing Riot signal chain out of
// SuhrRiotProcessor and into its own class. None of the wrapped DSP stages
// (InputStage, TrimStage, TightFilter, DriveStage, ClippingStage, ToneStack,
// OutputStage, NoiseGate) were modified in any way to make this wrapper —
// same objects, same call sequence, same parameters.
//
// ============================================================================
// FROZEN — Phase 11 final sign-off. Sparker Distortion v2.0 is
// feature-complete: Ember (Suhr Riot), Burner (REVV G3, see
// BurnerCircuit.h), and Blazer (Friedman BE-OD, see BlazerCircuit.h) are
// ALL frozen as of this point. No further modifications to any circuit
// model DSP — this file, any of the DSP stages it wraps (InputStage/
// TrimStage/TightFilter/DriveStage/ClippingStage/ToneStack/OutputStage/
// NoiseGate), BurnerCircuit, or BlazerCircuit — are permitted without a new,
// explicitly-scoped phase sequence. Burner received one narrow, signed-off
// exception during Phase 10 (a numerical-safety clamp fix in its NR
// clipper — see the FREEZE EXCEPTION comment in BurnerCircuit.cpp) and is
// re-frozen as of that fix.
//
// EXCEPTION ON RECORD (post-Phase-11 LEVEL-FIDELITY REVISION, explicitly
// authorised): Ember's output-level staging was corrected to match the real
// pedal. This supersedes the "Ember has zero modifications" claim above.
// Scope was strictly output level — the clipping character, the ADAA math,
// the oversampling, the tone/EQ filter math and the Tight circuitry are all
// bit-for-bit unchanged. Two changes:
//
//   1. ClippingStage — removed the per-mode `outGain` normalisation that
//      forced CLASSIC and HOT down to WARM's germanium loudness. The clipper
//      is a shunt clamp, so the output peak IS the conducting diode's forward
//      voltage: the LED modes really are ~2.6x (+8.35 dB) louder than the
//      germanium mode on the hardware. The old code cancelled that ratio out
//      exactly, making mode switching timbre-only. Diode ceilings, pre-gains
//      and the rail constant are untouched.
//
//   2. OutputStage — U2A is described in that file as a fixed-gain RECOVERY
//      buffer, but the model applied a 0.5x ATTENUATION whose own comment
//      admitted it was chosen to give "sensible pedal levels". Replaced with
//      a genuine +6 dB recovery gain, so Level can put out more than unity
//      the way the hardware does. The A10k audio taper (level^2) is unchanged.
//
// Ember is re-frozen as of this revision.
//
// EXCEPTION ON RECORD (Ember CLIP-VOICING FIDELITY revision, explicitly
// authorised — "make Ember sound less like a Big Muff"): the clipping-stage
// diode selection was corrected. Previously all three CLIP positions used the
// SAME symmetric diode (red LED), distinguished only by an invented per-mode
// pre-gain, so CLASSIC and HOT were measurably identical (RMS within 0.006%)
// and every mode was a pure-odd-harmonic symmetric clip — the defining trait
// of a Big Muff. Now each mode selects a genuinely different diode string, as
// the real 3-way toggle does:
//   WARM    = 2× germanium, symmetric  (~0.65 V, warm/soft/quiet)
//   CLASSIC = silicon, ASYMMETRIC      (~0.6 V / ~1.2 V — even harmonics)
//   HOT     = red LED, symmetric       (~1.7 V, open/dynamic/loud)
// The per-mode pre-gain (1.0/2.6/5.2) is removed (the real switch swaps diodes
// only). A 10 Hz post-clip DC blocker was added to ClippingStage to remove the
// offset the asymmetric CLASSIC mode produces (the hardware's output coupling
// cap). The faked mode-dependent post-EQ "character filter" in the processor
// is removed, its job now done by the physics. Rail saturator, ADAA math,
// oversampling, drive stage, tone stack and Tight are all unchanged.
// Ember is re-frozen as of this revision.
// ============================================================================
// ---------------------------------------------------------------------------
class EmberCircuit
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    // Parameters mirror the existing "drive"/"tone"/"level"/"mode"/etc.
    // APVTS params exactly as read in the old SuhrRiotProcessor::processBlock.
    void process (juce::AudioBuffer<float>& buffer,
                  float drive, float tone, float level, int mode,
                  float inputTrimDb, float tightFreq, float outTrimDb,
                  int osOrder, float gateThresh);

private:
    TrimStage     inputTrim;        // ±12 dB gain before drive chain
    NoiseGate     noiseGate;        // input gate — silences idle noise before the drive
    InputStage    inputStage;       // C1/R1 AC coupling + RF filter
    TightFilter   tightFilter;      // pre-distortion Butterworth HPF (20–500 Hz)
    DriveStage    driveStage;       // U1B variable gain + bass shaping
    ClippingStage clippingStage;    // D1-D8 diode clipper (3 modes, oversampled)
    ToneStack     toneStack;        // TONE: variable first-order treble cut (10kC + 22n)
    OutputStage   outputStage;      // U2A recovery gain + Level pot
    TrimStage     outputTrim;       // ±12 dB final makeup gain
};
