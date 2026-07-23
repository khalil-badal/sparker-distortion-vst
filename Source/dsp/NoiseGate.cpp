#include "NoiseGate.h"

static inline float makeCoeff (float ms, double sampleRate) noexcept
{
    return std::exp (-1.0f / (float) (sampleRate * (double) ms * 0.001));
}

void NoiseGate::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr           = sampleRate;
    envAttack    = makeCoeff (kEnvAttackMs,  sampleRate);
    envRelease   = makeCoeff (kEnvReleaseMs, sampleRate);
    gainOpen     = makeCoeff (kGainOpenMs,   sampleRate);
    gainClose    = makeCoeff (kGainCloseMs,  sampleRate);
    reset();
}

void NoiseGate::reset()
{
    envelope[0] = envelope[1] = 0.0f;
    gateGain[0] = gateGain[1] = 0.0f;
}

void NoiseGate::process (juce::AudioBuffer<float>& buffer)
{
    const float threshold = juce::Decibels::decibelsToGain (thresholdDb);
    const int   nSamples  = buffer.getNumSamples();
    const int   nCh       = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float  env  = envelope[ch];
        float  g    = gateGain[ch];

        for (int i = 0; i < nSamples; ++i)
        {
            // Peak envelope follower
            const float x = std::abs (data[i]);
            env = (x > env) ? envAttack  * env + (1.0f - envAttack)  * x
                            : envRelease * env + (1.0f - envRelease) * x;

            // Binary gate decision, then smooth the transition
            const float target = (env >= threshold) ? 1.0f : 0.0f;
            const float coeff  = (target > g) ? gainOpen : gainClose;
            g = coeff * g + (1.0f - coeff) * target;

            data[i] *= g;
        }

        envelope[ch] = env;
        gateGain[ch] = g;
    }
}
