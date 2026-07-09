#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// OutputStage — models the recovery op-amp (U2A) and Level pot (RV3, A10k).
//
// REAL CIRCUIT:
//   U2A is a fixed-gain recovery buffer after the passive tone stack.
//   (The tone stack has insertion loss, so U2A restores unity gain.)
//   RV3 (A10k, audio/log taper) is a simple voltage divider after U2A.
//   R15 (100 Ω) is an output series resistor for driving cable capacitance.
//
// DSP MODEL:
//   The fixed recovery gain and variable level pot are collapsed into a single
//   smoothed gain multiplication. The A-taper curve is approximated as:
//
//     linear_gain = level^2
//
//   This gives a logarithmic feel because the ear perceives loudness on a
//   log scale. At level=0.5 (noon): gain = 0.25 (−12 dB), which matches the
//   perceptual midpoint of an audio-taper pot. At level=1.0: gain = 1.0 (0 dB).
//
//   R15 (100 Ω) is negligible in DSP and is not modelled.
// ---------------------------------------------------------------------------
class OutputStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // level: 0.0 = silent, 1.0 = full output
    void setLevel (float level);

private:
    // Linear smoothing is appropriate here: the gain range is 0–1 and we
    // explicitly want to be able to ramp to zero (silence). Multiplicative
    // smoothing cannot reach zero, so we use Linear instead.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;

    double sampleRate = 44100.0;
};
