#include "DriveStage.h"

void DriveStage::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    // Bass-shaping HPF at 1592 Hz (models C5 100nF + R5 1kΩ in the gain-stage feedback).
    // Matches the Suhr Riot Reloaded (V2) schematic — R5 ≈ 1 kΩ, C5 = 100 nF.
    // Original code used R5 = 10 kΩ → 159 Hz (wrong by 10×), letting far too much
    // low-end through the clipping stage and producing a thicker tone than the real pedal.
    *bassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 1592.0f);
    bassFilter.prepare (spec);

    // Ramp gain changes over 20 ms (= 0.02 s) to avoid zipper noise.
    // The Multiplicative smoother interpolates geometrically, which sounds
    // smooth across the large gain range (4.6× to 470×).
    gainSmoothed.reset (sr, 0.02);
    gainSmoothed.setCurrentAndTargetValue (4.6f); // default: drive=0 → gain≈4.6×
}

void DriveStage::setDrive (float drive)
{
    // Circuit-derived gain formula (see header for derivation):
    //   gain = Rf / (Rin + (1 - drive) × RV1)
    // Values from schematic: Rf = R3 = 470 kΩ, Rin = R2 = 1 kΩ, RV1 = 100 kΩ
    //
    // drive=0 → gain = 470k / (1k + 100k) = 470k / 101k ≈ 4.65×  (+13 dB)
    // drive=1 → gain = 470k / (1k + 0)    = 470k / 1k   ≈ 470×   (+53 dB)
    constexpr float Rf  = 470000.0f;
    constexpr float Rin =   1000.0f;
    constexpr float RV1 = 100000.0f;

    const float gain = Rf / (Rin + (1.0f - drive) * RV1);
    gainSmoothed.setTargetValue (gain);
}

void DriveStage::process (juce::AudioBuffer<float>& buffer)
{
    // Step 1: Bass-shaping filter (cuts low-end before amplification).
    {
        juce::dsp::AudioBlock<float>              block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        bassFilter.process (ctx);
    }

    // Step 2: Variable gain, applied sample-by-sample when the knob is moving
    // so the ramp is identical across all channels.
    //
    // The inner loop order (sample first, then channel) is intentional:
    // getNextValue() must be called exactly once per sample to advance the
    // ramp correctly. If we looped channels first, we'd advance the ramp
    // N×channels times instead of N times.
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (gainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        // Knob not moving — JUCE's vectorised applyGain() is faster.
        buffer.applyGain (gainSmoothed.getCurrentValue());
    }
}

void DriveStage::reset()
{
    bassFilter.reset();
    gainSmoothed.setCurrentAndTargetValue (4.6f);
}
