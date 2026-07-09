#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// ClippingStage — oversampled diode soft-clipper.
//
// TOPOLOGY (inferred from schematic):
//   Anti-parallel diodes in the op-amp feedback path (tube-screamer style).
//   R7/R8 (470 Ω) are series resistors that soften the clipping knee.
//
// DSP MODEL — closed-form symmetric diode clipper:
//   For two anti-parallel identical diodes in op-amp feedback (no parallel Rf),
//   KCL at the virtual-ground node gives:
//
//       Vin/Rin + 2·Is·sinh(Vout / (n·Vt)) = 0
//
//   Solving analytically for Vout:
//
//       Vout = −n·Vt · arcsinh( Vin / (2·Is·Rin) )
//
//   This is a memoryless waveshaper — no iteration needed.
//   All three modes use symmetric diode pairs, so the closed form applies.
//
// SIGNAL NORMALISATION:
//   DSP works in normalised floating-point; the diode equation works in
//   physical volts. We bridge the two worlds with a constant Vg = 0.10 V,
//   which represents the peak amplitude of a guitar signal before the drive
//   stage. Everything is scaled so that 1.0 in DSP == Vg volts at the input.
//
//   With x = DSP sample, Vin_phys = x·Vg:
//       y = −(n·Vt/Vg) · arcsinh( x·Vg / (2·Is·Rin) )
//         = −kOut · arcsinh( x·kIn )
//
//   where kIn = Vg/(2·Is·Rin)  and  kOut = n·Vt/Vg  (precomputed per mode).
//
// MODES (3-way slide switch):
//   0 — Germanium 1N34A  (2 diodes/polarity in series: n doubles, Is same)
//   1 — Silicon   1N914  (1 diode/polarity)
//   2 — LED RED          (1 LED/polarity, high Vf → most headroom)
//
// OVERSAMPLING:
//   The arcsinh waveshaper generates harmonics above Nyquist. Without
//   oversampling they alias back into the audio band. 4× upsampling raises
//   the effective Nyquist to ~88/96 kHz, well above guitar harmonic content.
// ---------------------------------------------------------------------------
class ClippingStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setMode (int mode); // 0 = Germanium, 1 = Silicon, 2 = LED

private:
    // Thermal voltage at room temperature (25 °C).
    // Vt = k·T/q = 1.38e-23 × 298 / 1.6e-19 ≈ 25.85 mV
    static constexpr float Vt = 0.02585f;

    // Guitar peak amplitude used as the normalisation bridge between DSP and volts.
    // A typical single-coil pickup outputs ~100 mV peak; humbuckers ~200 mV.
    // Using 100 mV keeps the numbers in a numerically comfortable range.
    static constexpr float Vg = 0.10f;

    // Effective input resistance for the clipper op-amp stage.
    // Derived from R2 (1 kΩ) in the schematic. This controls how much input
    // current the diodes must absorb, which sets the clipping threshold.
    static constexpr float Rin = 1000.0f;

    // Precomputed per-mode waveshaper constants.
    struct ModeCoeffs
    {
        float kIn;   // = Vg / (2 · Is · Rin) — scales DSP x into the arcsinh argument
        float kOut;  // = n · Vt / Vg          — scales arcsinh result back to DSP
    };

    // Computed in prepare() once the mode table is set up.
    // Array index matches the mode parameter (0/1/2).
    ModeCoeffs modeCoeffs[3];

    int currentMode = 0;

    // 4× oversampling (factor = 2^2 = 4), polyphase IIR half-band filter.
    // Template args: <sample type, numChannels, oversamplingFactor, filterType>
    juce::dsp::Oversampling<float> oversampler { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
};
