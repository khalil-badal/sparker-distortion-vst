#include "InputStage.h"

void InputStage::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    // ProcessSpec tells JUCE modules how many channels, samples, and what
    // sample rate to expect. Every juce::dsp:: object needs this before use.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    // Set the HPF cutoff to 7.2 Hz (fc = 1 / (2π × 1MΩ × 22nF)).
    // makeHighPass() computes bilinear-transformed IIR coefficients for a
    // first-order Butterworth high-pass at the given frequency and sample rate.
    *hpFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 7.2f);
    hpFilter.prepare (spec);
}

void InputStage::process (juce::AudioBuffer<float>& buffer)
{
    // AudioBlock is a non-owning view over the buffer's sample data.
    // ProcessContextReplacing means the filter writes output back into the
    // same memory it reads from (in-place processing, no extra allocation).
    juce::dsp::AudioBlock<float>            block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    hpFilter.process (ctx);
}

void InputStage::reset()
{
    // Clears the filter's internal delay-line memory (the z^-1 state).
    // Must be called when playback stops so old audio doesn't bleed into
    // the next session.
    hpFilter.reset();
}
