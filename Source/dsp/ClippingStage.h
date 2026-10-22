#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// ClippingStage — oversampled op-amp-rail + shunt-diode clipper.
//
// TOPOLOGY (Aion Fusion / traced Suhr Riot schematic):
//   The drive stages feed R5 (470 Ω) into diode strings shunted to ground,
//   selected by the DPDT CLIP switch:
//
//   Mode 0 (WARM): two 1N34A germanium in series per direction, SYMMETRIC.
//     Total knee ≈ 0.65 V — clips earliest/softest, warmest, quietest.
//   Mode 1 (CLASSIC): ASYMMETRIC silicon — one 1N914 (≈0.6 V) positive, two
//     in series (≈1.2 V) negative. The unequal ceilings add EVEN-order
//     harmonics (the tube-like grit a symmetric Big-Muff-style clipper can't
//     make); this is the mode that most distinguishes the Riot from a Muff.
//   Mode 2 (HOT): one red LED per direction (≈1.7 V), SYMMETRIC. Highest
//     knee — clips last/least, the most dynamic/open and loudest, lead mode.
//
//   There is NO per-mode pre-gain (the real CLIP switch swaps diodes only,
//   never the drive into them — see kDiodeTable in the .cpp for the full
//   rationale). The modes differ by diode knee + asymmetry and their genuine
//   natural loudness, exactly as the hardware toggle behaves.
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
//   Output is normalised so the red-LED (HOT) ceiling ≈ 1.0; the lower-knee
//   modes come out proportionally quieter, matching the real pedal's mode
//   loudness — the germanium WARM mode clamps at 0.65 V against the LED's
//   1.70 V, so it is genuinely ~8.35 dB quieter, and the asymmetric CLASSIC
//   mode sits between them.
//
// A first-order 10 Hz DC blocker follows the clip (post-downsample) to
// remove the offset the asymmetric CLASSIC mode generates — the hardware's
// output coupling cap; a no-op for the symmetric WARM/HOT modes.
//
// SIGNAL NORMALISATION: Vg = 0.10 V (guitar peak); 1.0 DSP == Vg. (This is
// the Ember/shunt-clipper voltage convention; Burner/Blazer's feedback-diode
// NR solver in DiodeClipperNR.h works in a different, un-unified domain — a
// documented inconsistency, harmless because each circuit is self-contained.)
// ---------------------------------------------------------------------------
class ClippingStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setMode             (int mode);   // 0 = WARM/Ge, 1 = CLASSIC/Si-asym, 2 = HOT/LED
    void setOversamplingOrder (int order); // 0 = 2x, 1 = 4x, 2 = 8x (default)

private:
    int currentOsOrder = 2;  // 0=2x, 1=4x, 2=8x

    // Guitar peak amplitude — the bridge between DSP units and volts.
    static constexpr float Vg = 0.10f;

    // Op-amp rail limit: ±4 V on a 9 V supply, in DSP units (= 40 × Vg).
    static constexpr float kRail = 4.0f / Vg;

    // Absolute output normalisation: the red-LED ceiling (the HIGHEST of the
    // three diode sets) maps to 1.0 full scale. This is a single global
    // constant applied identically to every mode — it sets where the loudest
    // mode sits, it does NOT equalise the modes against each other.
    //
    // The per-mode `outGain` makeup that used to sit alongside this was
    // removed in the post-Phase-11 level-fidelity revision: it forced the LED
    // modes down to the germanium mode's loudness, cancelling the real
    // +8.35 dB the higher-Vf diodes produce. See ClippingStage.cpp::prepare().
    static constexpr float kOutRef = 1.7f / Vg;

    struct ModeCoeffs
    {
        float cPos;      // positive half-cycle ceiling (Vf/Vg)
        float cNeg;      // negative half-cycle ceiling
        float preGain;   // per-mode drive boost into the clipper
    };

    ModeCoeffs modeCoeffs[3];

    int currentMode = 0;

    // ADAA state — one (x, y) pair per channel, carried across blocks so
    // there's no seam at block boundaries. BOTH stages use FIRST-order ADAA
    // (F₁ only), each ADAA'd independently — there's no closed-form
    // antiderivative for the two composed together. The rail previously used
    // second-order ADAA; that was removed because the F₂ expression is
    // catastrophically ill-conditioned in float32 at the rail's k (it was
    // generating ~39 dB of numerical noise) — see ClippingStage.cpp.
    static constexpr int kMaxChannels = 2;
    float railXPrev[kMaxChannels]  = { 0.0f, 0.0f };
    float diodeYPrev[kMaxChannels] = { 0.0f, 0.0f };

    // Three pre-allocated oversamplers — JUCE's Oversampling order is fixed at
    // construction, so all three live in memory; only the selected one is used.
    using OvS = juce::dsp::Oversampling<float>;
    OvS os2x { 2, 1, OvS::filterHalfBandPolyphaseIIR };  // 2x  (2^1)
    OvS os4x { 2, 2, OvS::filterHalfBandPolyphaseIIR };  // 4x  (2^2)
    OvS os8x { 2, 3, OvS::filterHalfBandPolyphaseIIR };  // 8x  (2^3)

    // Post-clip DC blocker (10 Hz first-order HPF) — removes the offset the
    // asymmetric CLASSIC mode produces. Runs at base rate after downsampling.
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> dcBlocker;
};
