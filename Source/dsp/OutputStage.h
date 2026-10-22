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
//   U2A's fixed recovery gain and the variable level pot are collapsed into a
//   single smoothed gain multiplication:
//
//     linear_gain = level^2 * kU2ARecoveryGain
//
//   level^2 approximates the A-taper: a logarithmic feel, because the ear
//   perceives loudness on a log scale. kU2ARecoveryGain is U2A itself — the
//   fixed makeup that lets the pedal put out more than it takes in, which is
//   what gives a real Riot the headroom to drive an amp's front end hard.
//
//   At level=0.5 (noon): gain = 0.5 (−6 dB). At level=1.0: gain = 2.0 (+6 dB).
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
    // U2A's fixed recovery/makeup gain. ENGINEERING ESTIMATE — the exact
    // Rf/Rg pair around U2A was not recoverable from the traced schematic.
    // 2.0 (+6 dB) is a standard non-inverting recovery value for a drive
    // pedal's post-tone-stack makeup stage. See OutputStage.cpp::setLevel()
    // for the full rationale and the tone-stack loss measurement that
    // established this is a real makeup gain, not loss compensation.
    static constexpr float kU2ARecoveryGain = 2.0f;

    // Linear smoothing is appropriate here: we explicitly want to be able to
    // ramp to zero (silence). Multiplicative smoothing cannot reach zero, so
    // we use Linear instead.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;

    double sampleRate = 44100.0;
};
