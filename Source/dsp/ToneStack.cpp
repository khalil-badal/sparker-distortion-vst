#include "ToneStack.h"

// ===========================================================================
// Lifecycle
// ===========================================================================

void ToneStack::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    lpFilter.prepare (spec);
    updateCoefficients();
}

// ===========================================================================
// Coefficient update — called whenever the TONE knob moves.
//
// The cutoff sweeps exponentially between kFcMin (723 Hz, pot fully in
// circuit) and kFcMax (12 kHz, pot dialed out). The exponential mapping
// stands in for the reverse-log (C-taper) pot of the real pedal.
// ===========================================================================

void ToneStack::updateCoefficients()
{
    const float fc = kFcMin * std::pow (kFcMax / kFcMin, currentTone);

    *lpFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (
                           sampleRate, fc);
}

void ToneStack::setTone (float tone)
{
    if (std::abs (tone - currentTone) > 0.001f)
    {
        currentTone = tone;
        updateCoefficients();
    }
}

// ===========================================================================
// Processing
// ===========================================================================

void ToneStack::process (juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    lpFilter.process (ctx);
}

void ToneStack::reset()
{
    lpFilter.reset();
}
