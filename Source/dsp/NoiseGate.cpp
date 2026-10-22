#include "NoiseGate.h"

static inline float makeCoeff (float ms, double sampleRate) noexcept
{
    return std::exp (-1.0f / (float) (sampleRate * (double) ms * 0.001));
}

void NoiseGate::prepare (double sampleRate, int samplesPerBlock)
{
    sr           = sampleRate;
    envAttack    = makeCoeff (kEnvAttackMs,  sampleRate);
    envRelease   = makeCoeff (kEnvReleaseMs, sampleRate);
    gainOpen     = makeCoeff (kGainOpenMs,   sampleRate);
    gainClose    = makeCoeff (kGainCloseMs,  sampleRate);
    holdSamples  = juce::roundToInt (sampleRate * (double) kHoldMs * 0.001);
    gainTrace.setSize (1, samplesPerBlock, false, false, true);
    gainTrace.clear();
    reset();
}

void NoiseGate::reset()
{
    envelope    = 0.0f;
    gateGain    = 0.0f;
    gateOpen    = false;
    holdCounter = 0;
}

void NoiseGate::process (juce::AudioBuffer<float>& buffer)
{
    const float openThresh  = juce::Decibels::decibelsToGain (thresholdDb);
    // Close threshold sits kHysteresisDb below the open threshold, so a small
    // envelope dip near the boundary doesn't even start the hold/close
    // sequence (see the header's AUDIT notes for how this and Hold combine).
    const float closeThresh = juce::Decibels::decibelsToGain (thresholdDb + kHysteresisDb);
    const int   nSamples    = buffer.getNumSamples();
    const int   nCh         = buffer.getNumChannels();

    auto* const* channelData = buffer.getArrayOfWritePointers();
    float* trace = (nSamples <= gainTrace.getNumSamples()) ? gainTrace.getWritePointer (0) : nullptr;

    float env  = envelope;
    float g    = gateGain;
    bool  open = gateOpen;
    int   hold = holdCounter;

    for (int i = 0; i < nSamples; ++i)
    {
        // STEREO-LINKED peak detection: the loudest channel at this sample
        // drives ONE decision for every channel (see the header's AUDIT
        // FINDING 1) — not an independent decision per channel.
        float x = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            x = juce::jmax (x, std::abs (channelData[ch][i]));

        // Peak envelope follower
        env = (x > env) ? envAttack  * env + (1.0f - envAttack)  * x
                        : envRelease * env + (1.0f - envRelease) * x;

        // Hysteretic gate decision with Hold (see header's AUDIT FINDING 2):
        // once open, a reading below closeThresh doesn't close the gate
        // immediately — it starts/keeps a hold countdown, reset to full any
        // time the envelope is seen above closeThresh again. Only once the
        // envelope has stayed below closeThresh for the whole hold window
        // does the gate actually close.
        if (open)
        {
            if (env >= closeThresh)  hold = holdSamples;
            else if (hold > 0)       --hold;
            else                     open = false;
        }
        else if (env > openThresh)
        {
            open = true;
            hold = holdSamples;
        }

        const float target = open ? 1.0f : 0.0f;
        const float coeff  = (target > g) ? gainOpen : gainClose;
        g = coeff * g + (1.0f - coeff) * target;

        if (trace != nullptr) trace[i] = g;   // remember for applyGateToOutput

        for (int ch = 0; ch < nCh; ++ch)
            channelData[ch][i] *= g;
    }

    envelope    = env;
    gateGain    = g;
    gateOpen    = open;
    holdCounter = hold;
}

// Multiply another buffer (same block, any channel count) by the per-sample
// gate gain computed in the last process() call — gates a downstream/output
// point with the input-detected envelope. See the header for the high-gain
// rationale.
void NoiseGate::applyGateToOutput (juce::AudioBuffer<float>& buffer) const noexcept
{
    const int nSamples = buffer.getNumSamples();
    if (nSamples > gainTrace.getNumSamples())
        return; // block larger than the prepared trace — skip rather than misapply

    const float* trace = gainTrace.getReadPointer (0);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < nSamples; ++i)
            data[i] *= trace[i];
    }
}
