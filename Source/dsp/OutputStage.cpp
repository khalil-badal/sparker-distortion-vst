#include "OutputStage.h"

void OutputStage::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    // 20 ms ramp — long enough to be inaudible as a click, short enough
    // that the knob still feels responsive.
    gainSmoothed.reset (sr, 0.02);

    // Default: level=0.5 → gain = 0.5^2 * 0.5 = 0.125 (−18 dB)
    gainSmoothed.setCurrentAndTargetValue (0.125f);

    (void) samplesPerBlock; // OutputStage doesn't need buffer-size info
}

void OutputStage::setLevel (float level)
{
    // A10k (audio-taper) approximation: gain = level^2.
    //
    // Why squared? An audio-taper pot has a logarithmic response because the
    // human ear perceives loudness logarithmically (every doubling of amplitude
    // sounds like the same perceptual step). Squaring maps the 0..1 linear
    // knob range to a curve that feels like a proper volume control:
    //   level=0.0 → gain=0.00  (−∞ dB, silence)
    //   level=0.5 → gain=0.25  (−12 dB, feels like halfway)
    //   level=1.0 → gain=1.00  (  0 dB, full output)
    // The clipper output is normalised to ≤ 1.0 (red-LED ceiling), so a
    // 0.5 constant gives sensible pedal levels: level=0.5 → peaks ≈ −18 dBFS,
    // level=1.0 → −6 dBFS. Comfortable headroom for amp-sim inputs.
    const float gain = level * level * 0.5f;
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
    gainSmoothed.setCurrentAndTargetValue (0.125f);
}
