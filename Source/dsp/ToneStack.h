#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// ToneStack — the Riot's tone control: a simple variable treble cut.
//
// From the Aion Fusion (traced Suhr Riot) schematic:
//   TONE is a 10kC (reverse-log) pot with C8 (22 nF) to ground — a classic
//   variable first-order low-pass. NOT a Big Muff LP/HP blend; the Riot
//   never scoops mids.
//
//   Pot fully counter-clockwise (t=0): full 10 kΩ in circuit,
//     fc = 1/(2π·10k·22n) ≈ 723 Hz  (dark)
//   Pot fully clockwise (t=1): pot resistance ≈ 0, the cap has nothing to
//     roll off against → effectively no treble cut (bright).
//
// DSP: one first-order LPF whose cutoff sweeps exponentially from 723 Hz
// to ~12 kHz. The exponential sweep stands in for the C-taper pot, which
// concentrates most of the audible change in the lower half of rotation.
//
// (The internal 20k PRESENCE trimmer that the Aion build adds is not part
// of the original pedal and is not modelled.)
// ---------------------------------------------------------------------------
class ToneStack
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // tone: 0.0 = dark (fc ≈ 723 Hz), 1.0 = bright (fc ≈ 12 kHz)
    void setTone (float tone);

private:
    void updateCoefficients();

    double sampleRate  = 44100.0;
    float  currentTone = 0.5f;

    static constexpr float kFcMin = 723.0f;    // pot fully in circuit
    static constexpr float kFcMax = 16000.0f;  // pot dialed out (bright, open)

    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lpFilter;
};
