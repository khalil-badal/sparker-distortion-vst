#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// DriveStage — models the variable-gain amplifier (U1B in the Riot circuit).
//
// Two things happen here that shape the Riot's character:
//
// 1. BASS SHAPING (C5 + R5 in the gain-stage feedback):
//    C5 (100 nF) is in series with R5 (1 kΩ) inside the amplifier's feedback
//    network. Below fc = 1/(2pi x R5 x C5) ~= 1592 Hz, the feedback capacitor
//    looks like a high impedance, which REDUCES the closed-loop gain for bass
//    frequencies. In other words: the op-amp amplifies mids/highs more than
//    bass. This is what keeps the Riot tight and non-muddy even at maximum drive.
//    DSP: modelled as a high-pass filter applied before the gain multiplication.
//    (Original code used R5=10k -> 159 Hz -- wrong by 10x; fixed per schematic traces.)
//
// 2. VARIABLE GAIN (RV1 DRIVE pot):
//    The DRIVE pot (B100k linear taper, RV1) is in the input path of U1B.
//    Higher pot resistance → smaller input signal relative to feedback → less
//    gain. Lower pot resistance → more gain.
//    Circuit-derived formula: gain = Rf / (Rin_fixed + (1 - drive) × RV1)
//      where Rf ≈ 470 kΩ (R3), Rin_fixed = 1 kΩ (R2), RV1 = 100 kΩ
//      At drive=0: gain ≈ 4.6×    (~+13 dB — clean-ish)
//      At drive=1: gain ≈ 470×    (~+53 dB — fully saturating the clippers)
//    Note: exact values are estimates from the schematic; will be verified
//    against a circuit simulation in Stage 3.
// ---------------------------------------------------------------------------
class DriveStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // drive: 0.0 = minimum gain (~4.6×), 1.0 = maximum gain (~470×)
    void setDrive (float drive);

private:
    // HPF that shapes which frequencies get amplified (models C5/R5).
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> bassFilter;

    // Smoothed gain value. Multiplicative smoothing interpolates in log scale
    // (geometrically), which sounds smooth across the 4.6× to 470× range.
    // Using Linear here would cause audible zipper noise on the knob.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmoothed;

    double sampleRate = 44100.0;
};
