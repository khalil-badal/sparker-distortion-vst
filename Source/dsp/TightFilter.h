#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// TightFilter — 2nd-order Butterworth HPF placed BEFORE the drive/clipping
// stage.  Cutting bass prior to distortion prevents low-frequency
// intermodulation (the "honky" or "woofy" character of high-gain guitar amps
// with too much low end going into the gain stage).
//
// Frequency range: 20 Hz (fully open) → 500 Hz (maximum tightness).
// Coefficients rebuild only when frequency changes by more than 0.5 Hz, so
// the update is cheap enough to call every processBlock.
//
// Uses JUCE's ProcessorDuplicator so the same stereo-ready filter pair is
// handled without manual per-channel loops.
// ---------------------------------------------------------------------------
class TightFilter
{
public:
    void prepare     (double sampleRate, int samplesPerBlock);
    void setFrequency (float hz);                     // call every processBlock
    void process     (juce::AudioBuffer<float>& buffer);
    void reset();

private:
    void updateCoefficients();

    using DuplicatorType = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    DuplicatorType hpf;
    double sampleRate  = 44100.0;
    float  currentFreq = 20.0f;
};
