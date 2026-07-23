#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// DriveStage — the Riot's TWO-stage gain block (IC1B then IC1A).
// Component references and values from the Aion Fusion schematic
// (traced Suhr Riot): R2=1k, C3=220n, Drive=100kB, R3=10k, C4=100n,
// R4=1M, C5=100pF.
//
// STAGE 1 — IC1B, non-inverting, variable gain (DRIVE pot):
//   Gain leg: Drive pot (100kB) in feedback; ground leg R2 (1 kΩ) in series
//   with C3 (220 nF). Below fc = 1/(2π·R2·C3) ≈ 723 Hz the capacitor blocks,
//   so closed-loop gain falls to UNITY — bass passes at unity, it is never
//   removed. Above fc: gain = 1 + (drive·100k)/1k = 1 to ~101×.
//   DSP:  stage1 = dry + g1·HPF723(in),  g1 = drive·100
//
// STAGE 2 — IC1A, inverting, FIXED gain, band-limited:
//   Gain = R4/R3 = 1M/10k = 100× (+40 dB), always on — this is why the Riot
//   distorts even at minimum drive.
//   C4 (100 nF) in series with R3 → HPF at 1/(2π·10k·100n) ≈ 159 Hz
//     (keeps deep bass from over-saturating the clipper).
//   C5 (100 pF) across R4       → LPF at 1/(2π·1M·100p) ≈ 1.6 kHz
//     (the pedal's built-in pre-clip treble rolloff — its anti-fizz).
//   DSP:  out = 100 · LPF1.6k( HPF159( stage1 ) )
//   (The analog inversion is inaudible and not modelled.)
// ---------------------------------------------------------------------------
class DriveStage
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // drive: 0.0 = stage-1 unity (still +40 dB from stage 2), 1.0 = maximum
    void setDrive (float drive);

private:
    using IIRDup = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRDup shelfHpf;    // stage 1: 723 Hz — selects what receives drive gain
    IIRDup stage2Hpf;   // stage 2: 159 Hz (C4/R3)
    IIRDup stage2Lpf;   // stage 2: 1.6 kHz (C5/R4)

    // Pre-allocated dry copy for the stage-1 unity-gain low-frequency path.
    juce::AudioBuffer<float> dryBuffer;

    // Fixed stage-2 gain (R4/R3).
    static constexpr float kStage2Gain = 100.0f;

    // Smoothed stage-1 gain. Multiplicative smoothing interpolates
    // geometrically, which sounds smooth across the 0.01–100× range.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmoothed;

    double sampleRate = 44100.0;
};
