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
// explicitly-scoped phase sequence. Ember itself has zero modifications
// across the entire Ember->Burner->Blazer implementation arc (verified via
// diff at every phase gate). Burner received one narrow, signed-off
// exception during Phase 10 (a numerical-safety clamp fix in its NR
// clipper — see the FREEZE EXCEPTION comment in BurnerCircuit.cpp) and is
// re-frozen as of that fix.
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
    ToneStack     toneStack;        // Big Muff RC tone control
    OutputStage   outputStage;      // U2A recovery gain + Level pot
    TrimStage     outputTrim;       // ±12 dB final makeup gain
};
