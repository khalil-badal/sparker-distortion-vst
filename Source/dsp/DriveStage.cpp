#include "DriveStage.h"

void DriveStage::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    // Stage 1 shelf corner: R2 (1 kΩ) + C3 (220 nF) in the ground leg.
    // fc = 1/(2π·1k·220n) ≈ 723 Hz. Frequencies above this receive the
    // drive gain; below, the dry path keeps gain at unity.
    *shelfHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 723.0f);
    shelfHpf.prepare (spec);

    // Stage 2 band limits: C4/R3 → HPF 159 Hz;  C5/R4 → LPF 1.6 kHz.
    *stage2Hpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 159.0f);
    stage2Hpf.prepare (spec);
    *stage2Lpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sr, 1592.0f);
    stage2Lpf.prepare (spec);

    dryBuffer.setSize (2, samplesPerBlock);

    // Ramp gain changes over 20 ms to avoid zipper noise.
    gainSmoothed.reset (sr, 0.02);
    gainSmoothed.setCurrentAndTargetValue (50.0f); // drive=0.5 default
}

void DriveStage::setDrive (float drive)
{
    // Stage 1: g1 = (drive · 100k) / R2 = drive · 100.
    // Clamped away from zero because multiplicative smoothing cannot pass
    // through 0 — at drive=0 the shelf contribution is inaudible anyway.
    const float g1 = juce::jmax (0.01f, drive * 100.0f);
    gainSmoothed.setTargetValue (g1);
}

void DriveStage::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ---- Stage 1: dry + g1 · HPF723(in) ------------------------------------
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    {
        juce::dsp::AudioBlock<float>              block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        shelfHpf.process (ctx);
    }

    // Sample-first loop so the smoother advances exactly once per sample.
    if (gainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* out = buffer.getWritePointer (ch);
                out[i] = dryBuffer.getReadPointer (ch)[i] + g * out[i];
            }
        }
    }
    else
    {
        const float g = gainSmoothed.getCurrentValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float*       out = buffer.getWritePointer (ch);
            const float* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                out[i] = dry[i] + g * out[i];
        }
    }

    // ---- Stage 2: fixed gain (kStage2Gain), band-limited 159 Hz – 1.6 kHz ---
    {
        juce::dsp::AudioBlock<float>              block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        stage2Hpf.process (ctx);
        stage2Lpf.process (ctx);
    }
    buffer.applyGain (kStage2Gain);
}

void DriveStage::reset()
{
    shelfHpf.reset();
    stage2Hpf.reset();
    stage2Lpf.reset();
    dryBuffer.clear();
    gainSmoothed.setCurrentAndTargetValue (50.0f);
}
