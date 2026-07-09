#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// TrimStage — unity-gain ±12 dB trim with click-free smoothing.
// Instantiated twice: once as inputTrim (before drive chain) and once as
// outputTrim (after the level pot), so one class handles both roles.
//
// Uses Multiplicative smoothing so the gain value never passes through zero
// (safe for the whole -12 → +12 dB range, minimum linear value ≈ 0.25).
// ---------------------------------------------------------------------------
class TrimStage
{
public:
    void prepare  (double sampleRate, int samplesPerBlock);
    void setTrimDb (float dB);                        // call every processBlock
    void process  (juce::AudioBuffer<float>& buffer);
    void reset();

private:
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain;
};
