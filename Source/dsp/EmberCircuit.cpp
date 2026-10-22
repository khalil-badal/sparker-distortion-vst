#include "EmberCircuit.h"

// ===========================================================================
// Moved verbatim from SuhrRiotProcessor::prepareToPlay /
// SuhrRiotProcessor::releaseResources / SuhrRiotProcessor::processBlock.
// No DSP logic changed — same stage objects, same order, same parameters.
// ===========================================================================

void EmberCircuit::prepare (double sampleRate, int samplesPerBlock)
{
    inputTrim    .prepare (sampleRate, samplesPerBlock);
    noiseGate    .prepare (sampleRate, samplesPerBlock);
    inputStage   .prepare (sampleRate, samplesPerBlock);
    tightFilter  .prepare (sampleRate, samplesPerBlock);
    driveStage   .prepare (sampleRate, samplesPerBlock);
    clippingStage.prepare (sampleRate, samplesPerBlock);
    toneStack    .prepare (sampleRate, samplesPerBlock);
    outputStage  .prepare (sampleRate, samplesPerBlock);
    outputTrim   .prepare (sampleRate, samplesPerBlock);
}

void EmberCircuit::reset()
{
    inputTrim    .reset();
    noiseGate    .reset();
    inputStage   .reset();
    tightFilter  .reset();
    driveStage   .reset();
    clippingStage.reset();
    toneStack    .reset();
    outputStage  .reset();
    outputTrim   .reset();
}

void EmberCircuit::process (juce::AudioBuffer<float>& buffer,
                             float drive, float tone, float level, int mode,
                             float inputTrimDb, float tightFreq, float outTrimDb,
                             int osOrder, float gateThresh)
{
    inputTrim    .setTrimDb     (inputTrimDb);
    noiseGate    .setThreshold  (gateThresh);
    tightFilter  .setFrequency  (tightFreq);
    driveStage   .setDrive      (drive);
    clippingStage.setMode       (mode);
    clippingStage.setOversamplingOrder (osOrder);
    toneStack    .setTone      (tone);
    outputStage  .setLevel     (level);
    outputTrim   .setTrimDb    (outTrimDb);

    // Signal chain:
    //   Input Trim → Gate → AC coupling → Tight HPF → Drive amp → Clipper → Tone → Level → Output Trim
    inputTrim    .process (buffer);
    noiseGate    .process (buffer);
    inputStage   .process (buffer);
    tightFilter  .process (buffer);
    driveStage   .process (buffer);
    clippingStage.process (buffer);
    toneStack    .process (buffer);
    outputStage  .process (buffer);
    outputTrim   .process (buffer);

    // Re-gate at the OUTPUT with the envelope the gate detected at the INPUT.
    // Ember's preamp adds 40-80 dB, so any noise passing the input gate while
    // it is open is amplified enormously; gating only at the input therefore
    // leaves an audible amplified tail during the gate's release (and on any
    // idle noise that briefly holds the gate open). Applying the same
    // per-sample gate gain here silences that tail directly. During a note the
    // gate is fully open (gain = 1), so this is a no-op on real playing.
    noiseGate    .applyGateToOutput (buffer);
}
