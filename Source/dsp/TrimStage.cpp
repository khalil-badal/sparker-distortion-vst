#include "TrimStage.h"

void TrimStage::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    gain.reset (sampleRate, 0.020);        // 20 ms ramp, same as Drive/Level stages
    gain.setCurrentAndTargetValue (1.0f);  // 0 dB default at startup
}

void TrimStage::setTrimDb (float dB)
{
    gain.setTargetValue (juce::Decibels::decibelsToGain (dB));
}

void TrimStage::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Advance the smoother exactly once per sample (not once per channel),
    // then apply the same gain value to all channels at that sample index.
    // This is the same pattern used in DriveStage and OutputStage.
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

void TrimStage::reset()
{
    // Snap the smoother to its current target so filter memory is cleared
    // cleanly at playback stop (no ramp artifact on next play press).
    gain.setCurrentAndTargetValue (gain.getTargetValue());
}
