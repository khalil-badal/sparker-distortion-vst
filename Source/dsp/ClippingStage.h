#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// ClippingStage — oversampled op-amp-rail + shunt-diode clipper.
//
// TOPOLOGY (Aion Fusion / traced Suhr Riot schematic):
//   The drive stages feed R5 (470 Ω) into diode strings shunted to ground,
//   selected by the DPDT CLIP switch:
//
//   Mode 0 (WARM): two 1N34A germanium in series per direction, symmetric.
//     Total knee ≈ 0.65 V.
//   Mode 1 (CLASSIC): one red LED per direction (≈1.7 V), symmetric, with
//     2.6× pre-gain so the LEDs saturate as deeply as WARM's Ge pair.
//   Mode 2 (HOT): same red-LED voicing driven twice as hard (5.2× pre-gain)
//     — deepest saturation, the lead mode.
//
// DSP MODEL — two saturators in series, both inside the oversampled region:
//
//   1. OP-AMP RAILS: the drive stages can ask for up to ~80 dB of gain, but
//      the real op-amp saturates at its ±4 V supply rails first. Modelled as
//      x = Rail·tanh(x/Rail) with Rail = 4 V. This is where most of the
//      Riot's aggression and edge comes from at high drive.
//
//   2. SHUNT DIODES: below the knee the diodes conduct nothing — the signal
//      passes at UNITY gain (this is what keeps idle noise from being
//      amplified by the clipper). Above the knee the string clamps near its
//      total forward voltage. Modelled per half-cycle as c·tanh(x/c) where
//      c = Vf/Vg is the normalised ceiling.
//
//   Output is normalised so the red-LED ceiling ≈ 1.0; quieter modes come
//   out proportionally lower, matching the real pedal's mode loudness.
//
// SIGNAL NORMALISATION: Vg = 0.10 V (guitar peak); 1.0 DSP == Vg.
// ---------------------------------------------------------------------------
class ClippingStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setMode             (int mode);   // 0 = Ge, 1 = Si asym, 2 = LED
    void setOversamplingOrder (int order); // 0 = 2x, 1 = 4x, 2 = 8x (default)

private:
    int currentOsOrder = 2;  // 0=2x, 1=4x, 2=8x

    // Guitar peak amplitude — the bridge between DSP units and volts.
    static constexpr float Vg = 0.10f;

    // Op-amp rail limit: ±4 V on a 9 V supply, in DSP units (= 40 × Vg).
    static constexpr float kRail = 4.0f / Vg;

    // Absolute output normalisation (unchanged from the level the user
    // approved for WARM): red-LED reference.
    static constexpr float kOutRef = 1.7f / Vg;

    // Loudness target every mode is matched to: the Ge (WARM) ceiling.
    // Each mode divides by its own ceiling and multiplies by this, so
    // switching modes changes character, not loudness.
    static constexpr float kGeCeil = 0.65f / Vg;

    struct ModeCoeffs
    {
        float cPos;      // positive half-cycle ceiling (Vf/Vg)
        float cNeg;      // negative half-cycle ceiling
        float outGain;   // per-mode makeup so peaks match WARM's level
        float preGain;   // per-mode drive boost into the clipper
    };

    ModeCoeffs modeCoeffs[3];

    int currentMode = 0;

    // Three pre-allocated oversamplers — JUCE's Oversampling order is fixed at
    // construction, so all three live in memory; only the selected one is used.
    using OvS = juce::dsp::Oversampling<float>;
    OvS os2x { 2, 1, OvS::filterHalfBandPolyphaseIIR };  // 2x  (2^1)
    OvS os4x { 2, 2, OvS::filterHalfBandPolyphaseIIR };  // 4x  (2^2)
    OvS os8x { 2, 3, OvS::filterHalfBandPolyphaseIIR };  // 8x  (2^3)
};
