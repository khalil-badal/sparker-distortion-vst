#include "OutputStage.h"

void OutputStage::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    // 20 ms ramp — long enough to be inaudible as a click, short enough
    // that the knob still feels responsive.
    gainSmoothed.reset (sr, 0.02);

    // Default: level=0.5 → gain = 0.5^2 * 2.0 = 0.5 (−6 dB)
    gainSmoothed.setCurrentAndTargetValue (0.5f); // level=0.5 → 0.5^2 * 2.0

    (void) samplesPerBlock; // OutputStage doesn't need buffer-size info
}

// ---------------------------------------------------------------------------
// WHAT CHANGED (post-Phase-11 level-fidelity revision) AND WHY:
//
// This used to be  gain = level^2 * 0.5f.  The 0.5 had no circuit basis — its
// own comment justified it as giving "sensible pedal levels", i.e. it was an
// arbitrary 6 dB ATTENUATION bolted onto the output. That directly
// contradicts what U2A actually is: this header describes U2A as a fixed-gain
// RECOVERY buffer that restores level after the tone stack. A recovery stage
// has gain >= 1 by definition; the model had it at 0.5.
//
// The result was a pedal that sat ~12 dB below where the hardware sits: even
// with Level maxed the plugin could only reach unity, whereas a real Riot has
// plenty of output on tap to slam an amp's front end (that hot output is a
// large part of why the pedal is used the way it is).
//
// Measurement note: the modelled tone stack (ToneStack.h — a unity-DC
// first-order LPF) was measured to be essentially lossless in broadband terms
// (0.99x at tone=0.5, 0.93x worst case at tone=0), so the recovery gain here
// is NOT loss compensation for that filter. It represents U2A's own fixed
// non-inverting gain in the real circuit.
//
// kU2ARecoveryGain = 2.0 (+6 dB) is an ENGINEERING ESTIMATE — the exact
// Rf/Rg pair around U2A was not recoverable from the traced schematic, and is
// flagged here the same way other non-wire-verified values in this codebase
// are (see BurnerCircuit.h / BlazerCircuit.h). 2x is a standard recovery
// value for a non-inverting op-amp makeup stage in a drive pedal. Adjust this
// one constant if the real ratio is ever confirmed.
// ---------------------------------------------------------------------------
void OutputStage::setLevel (float level)
{
    // A10k (audio-taper) approximation: gain = level^2.
    //
    // Why squared? An audio-taper pot has a logarithmic response because the
    // human ear perceives loudness logarithmically (every doubling of amplitude
    // sounds like the same perceptual step). Squaring maps the 0..1 linear
    // knob range to a curve that feels like a proper volume control.
    //
    // Times U2A's fixed recovery gain:
    //   level=0.0 → gain=0.0   (silence — pot fully down)
    //   level=0.5 → gain=0.5   (−6 dB, noon)
    //   level=1.0 → gain=2.0   (+6 dB, genuine makeup on tap)
    const float gain = level * level * kU2ARecoveryGain;
    gainSmoothed.setTargetValue (gain);
}

void OutputStage::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (gainSmoothed.isSmoothing())
    {
        // Knob is moving — apply per-sample ramp (same as DriveStage).
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        // Knob is stable — JUCE's SIMD-optimised multiply is faster.
        buffer.applyGain (gainSmoothed.getCurrentValue());
    }
}

void OutputStage::reset()
{
    gainSmoothed.setCurrentAndTargetValue (0.5f); // level=0.5 → 0.5^2 * 2.0
}
