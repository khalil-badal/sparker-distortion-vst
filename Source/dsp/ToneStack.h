#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// ToneStack — Big Muff π tone control (R10, R11, C8, C9, RV2).
//
// CIRCUIT VALUES:
//   R10 = 12 kΩ,  R11 = 8.2 kΩ,  C8 = C9 = 22 nF,  RV2 = 10 kΩ
//
// HOW THE BIG MUFF TONE CONTROL WORKS:
//   The pot (RV2) has its two ends connected to two separate RC networks.
//   The wiper position blends continuously between:
//
//     t = 0 → bass-heavy: a low-pass RC path dominates
//     t = 1 → treble-heavy: a high-pass RC path dominates
//
//   At t = 0.5 (noon), both paths are equally active. Because the LP and HP
//   outputs are partially out of phase at mid-frequencies, they partially cancel,
//   creating the characteristic mid-scoop.
//
// DSP MODEL — two parallel first-order filters, blended:
//
//   H(s, t) = (1−t)·H_LP(s) + t·H_HP(s)
//
//   Low-pass branch:
//     fc_LP = 1 / (2π · (R10 + (1−t)·RV2) · C8)
//     At t=0: fc_LP = 1/(2π·22k·22n) ≈ 330 Hz   (gentle cut above 330 Hz)
//     At t=1: fc_LP = 1/(2π·12k·22n) ≈ 603 Hz   (steeper cut, more treble removed)
//
//   High-pass branch:
//     fc_HP = 1 / (2π · (R11 + t·RV2) · C9)
//     At t=0: fc_HP = 1/(2π·8.2k·22n) ≈ 882 Hz  (gentle cut below 882 Hz)
//     At t=1: fc_HP = 1/(2π·18.2k·22n) ≈ 397 Hz (steeper cut, more bass removed)
//
//   Mid-scoop at t=0.5:
//     fc_LP ≈ 425 Hz, fc_HP ≈ 547 Hz — both poles converge near 500 Hz,
//     causing a ~6–10 dB notch at that frequency from the partial cancellation.
//
// IMPLEMENTATION:
//   1. Run the input through lpFilter → LP output
//   2. Run the same input through hpFilter → HP output (uses a pre-allocated copy)
//   3. output = (1−t)·LP + t·HP  (sample-by-sample blend)
//
// The bilinear transform converts the continuous-time H(s) to discrete-time
// coefficients at the current sample rate. Coefficients update whenever TONE moves.
// ---------------------------------------------------------------------------
class ToneStack
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // tone: 0.0 = bass-heavy, 0.5 = mid-scooped, 1.0 = treble-heavy
    void setTone (float tone);

private:
    void updateCoefficients();

    double sampleRate  = 44100.0;
    float  currentTone = 0.5f;

    // LP branch filter (cutoff sweeps with tone)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lpFilter;

    // HP branch filter (cutoff sweeps with tone, opposite direction)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> hpFilter;

    // Pre-allocated buffer for the HP path (avoids allocation in processBlock).
    // The LP path runs in-place on the main buffer; this holds the HP copy.
    juce::AudioBuffer<float> hpBuffer;
};
