#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// NoiseGate — peak-tracking envelope follower with smoothed gain reduction.
//
// Placed at the front of the chain (after InputTrim) so it prevents idle
// noise from entering the high-gain drive stage and being amplified + clipped.
//
// Default threshold: -65 dBFS  (covers most guitar pickups' noise floor)
// Attack  :   1 ms  (fast enough to not clip the note attack)
// Release : 100 ms  (slow enough to avoid chatter on sustained notes)
//
// The gain reduction itself is additionally smoothed (1 ms / 50 ms) to
// prevent clicks at the open/close boundary.
// ---------------------------------------------------------------------------
class NoiseGate
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // threshold: -70 dBFS = essentially off, -30 dBFS = very aggressive
    void setThreshold (float db) noexcept { thresholdDb = db; }

private:
    double sr          = 44100.0;
    float  thresholdDb = -65.0f;

    // Per-channel envelope + gain state (max stereo)
    float envelope[2] = {};
    float gateGain[2] = {};

    // Smoothing coefficients (computed in prepare())
    float envAttack   = 0.0f;
    float envRelease  = 0.0f;
    float gainOpen    = 0.0f;
    float gainClose   = 0.0f;

    static constexpr float kEnvAttackMs   =   1.0f;
    static constexpr float kEnvReleaseMs  = 100.0f;
    static constexpr float kGainOpenMs    =   1.0f;
    static constexpr float kGainCloseMs   =  50.0f;
};
