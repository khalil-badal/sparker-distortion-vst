#include "TightFilter.h"

void TightFilter::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    hpf.prepare (spec);
    updateCoefficients();
}

void TightFilter::setFrequency (float hz)
{
    // Rebuild coefficients only when the change is audible (> 0.5 Hz deadband)
    // to avoid unnecessary computation on blocks where the knob is stationary.
    if (std::abs (hz - currentFreq) > 0.5f)
    {
        currentFreq = hz;
        updateCoefficients();
    }
}

void TightFilter::updateCoefficients()
{
    // 2nd-order Butterworth HPF: Q = 1/sqrt(2) ≈ 0.707 gives maximally flat
    // pass-band and a smooth -12 dB/oct rolloff below the cutoff frequency.
    // makeHighPass returns a new Coefficients object; we copy it into the
    // existing shared state so both stereo filter channels update atomically.
    *hpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate,
        currentFreq,    // already float — no cast needed
        0.7071f);       // Q = 1/sqrt(2) — Butterworth pole quality factor
}

void TightFilter::process (juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    hpf.process (ctx);
}

void TightFilter::reset()
{
    hpf.reset();
}
