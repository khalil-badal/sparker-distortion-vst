#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// InputStage — models the input section of the Suhr Riot.
//
// Real circuit: C1 (22 nF) AC coupling cap → R2 (1 kΩ) series resistor →
//               op-amp U1A input. R1 (1 MΩ) loads the input to virtual ground.
//               C2 (47 pF) filters RF before the op-amp.
//
// DSP model: a first-order high-pass filter.
//   The C1/R1 pair forms an HPF: fc = 1/(2π × R1 × C1)
//                                   = 1/(2π × 1 000 000 × 22e-9)
//                                   ≈ 7.2 Hz
//
// This is inaudible (well below any musical content) but matters because the
// nonlinear clipping stage downstream must not see any DC offset — even a tiny
// DC value gets exponentially amplified by the diode equation.
// ---------------------------------------------------------------------------
class InputStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

private:
    // ProcessorDuplicator wraps a single-channel filter and duplicates it for
    // stereo. The shared "state" pointer holds the filter coefficients; both
    // channels read from it but maintain their own internal history (z^-1 delays).
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> hpFilter;

    double sampleRate = 44100.0;
};
