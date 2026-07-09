#include "ToneStack.h"

// Component values from the schematic (used in coefficient calculations).
// These are constants — they don't change at runtime.
static constexpr float R10  = 12000.0f;   // 12 kΩ
static constexpr float R11  =  8200.0f;   // 8.2 kΩ
static constexpr float RV2  = 10000.0f;   // 10 kΩ tone pot (total resistance)
static constexpr float C    =  22.0e-9f;  // 22 nF (C8 = C9)

// ===========================================================================
// Lifecycle
// ===========================================================================

void ToneStack::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    lpFilter.prepare (spec);
    hpFilter.prepare (spec);

    // Pre-allocate the HP copy buffer. We size it to the maximum block size
    // declared by the DAW so processBlock never calls new/delete.
    hpBuffer.setSize (2, samplesPerBlock);

    updateCoefficients();
}

// ===========================================================================
// Coefficient update — called whenever the TONE knob moves.
//
// We derive bilinear-transform coefficients from the RC component values.
// ---------------------------------------------------------------------------
// The bilinear transform maps continuous-time s to discrete-time z:
//
//   s → K·(z−1)/(z+1),    where K = 2·fs   (pre-warping factor)
//
// For a first-order LP with time constant τ = R·C:
//   H_LP(s) = 1 / (1 + s·τ)
//
// After bilinear substitution and simplification, the z-domain coefficients are:
//   b0 = 1 / (K·τ + 1),    b1 = b0            (numerator: b0·(1 + z^-1))
//   a1 = (1 − K·τ) / (1 + K·τ)                (denominator: 1 + a1·z^-1)
//
// For a first-order HP with the same τ:
//   H_HP(s) = s·τ / (1 + s·τ)
//
//   b0 =  K·τ / (K·τ + 1),    b1 = −b0       (numerator: b0·(1 − z^-1))
//   a1 = same as LP
//
// JUCE stores biquad coefficients as {b0, b1, b2, a1, a2} (a0=1 implicit).
// For first-order sections, b2=0 and a2=0.
// ===========================================================================

void ToneStack::updateCoefficients()
{
    const float t = currentTone;  // 0..1

    // --- LP branch -----------------------------------------------------------
    // The LP corner frequency decreases as tone increases (more treble cut).
    // R_lo = R10 + (1−t)·RV2 :
    //   t=0 → R_lo = R10+RV2 = 22 kΩ → fc ≈ 330 Hz (wide LP, gentle)
    //   t=1 → R_lo = R10     = 12 kΩ → fc ≈ 603 Hz (narrower LP, more treble cut)
    const float R_lo  = R10 + (1.0f - t) * RV2;
    const float tau_LP = R_lo * C;

    // --- HP branch -----------------------------------------------------------
    // The HP corner frequency increases as tone increases (more bass cut).
    // R_hi = R11 + t·RV2 :
    //   t=0 → R_hi = R11     = 8.2 kΩ → fc ≈ 882 Hz (high HP corner, gentle bass cut)
    //   t=1 → R_hi = R11+RV2 = 18.2 kΩ → fc ≈ 397 Hz (lower HP, more bass cut)
    const float R_hi  = R11 + t * RV2;
    const float tau_HP = R_hi * C;

    // Bilinear pre-warping constant K = 2·fs
    const float K = 2.0f * static_cast<float> (sampleRate);

    // --- LP coefficients (first-order Butterworth LP via bilinear transform) -
    {
        const float KT = K * tau_LP;
        const float b0 =  1.0f / (KT + 1.0f);
        const float b1 =  b0;                   // same as b0 for LP
        const float a1 = (1.0f - KT) / (KT + 1.0f);

        // JUCE IIR::Coefficients takes {b0, b1, b2, a1, a2}.
        // We fill second-order slots with 0 since this is first-order.
        *lpFilter.state = juce::dsp::IIR::Coefficients<float> (b0, b1, 0.0f, 1.0f, a1, 0.0f);
    }

    // --- HP coefficients (first-order Butterworth HP via bilinear transform) -
    {
        const float KT = K * tau_HP;
        const float b0 =  KT / (KT + 1.0f);
        const float b1 = -b0;                   // negated for HP
        const float a1 = (1.0f - KT) / (KT + 1.0f);

        *hpFilter.state = juce::dsp::IIR::Coefficients<float> (b0, b1, 0.0f, 1.0f, a1, 0.0f);
    }
}

void ToneStack::setTone (float tone)
{
    if (std::abs (tone - currentTone) > 0.001f)
    {
        currentTone = tone;
        updateCoefficients();
    }
}

// ===========================================================================
// Processing
// ===========================================================================

void ToneStack::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- Copy input into hpBuffer BEFORE the LP filter touches it -----------
    // (LP filter runs in-place on the main buffer; we need the original signal
    //  for the HP path too, so we save a copy now.)
    for (int ch = 0; ch < numChannels; ++ch)
        hpBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // --- LP path: run in-place on main buffer --------------------------------
    {
        juce::dsp::AudioBlock<float>              lpBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> lpCtx (lpBlock);
        lpFilter.process (lpCtx);
    }

    // --- HP path: run in-place on hpBuffer -----------------------------------
    {
        // Wrap hpBuffer in a block, then sub-slice to the live sample count.
        // AudioBlock(AudioBuffer&) covers the full pre-allocated size, so
        // getSubBlock trims it to exactly what the DAW handed us this callback.
        juce::dsp::AudioBlock<float> fullHpBlock (hpBuffer);
        auto hpBlock = fullHpBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextReplacing<float> hpCtx (hpBlock);
        hpFilter.process (hpCtx);
    }

    // --- Blend: output = (1−t)·LP + t·HP ------------------------------------
    // At t=0: pure LP (bass-heavy).
    // At t=1: pure HP (treble-heavy).
    // At t=0.5: equal blend → partial cancellation at ~450–550 Hz → mid-scoop.
    const float lpGain = 1.0f - currentTone;
    const float hpGain = currentTone;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float*       out = buffer.getWritePointer (ch);
        const float* hp  = hpBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
            out[i] = lpGain * out[i] + hpGain * hp[i];
    }
}

void ToneStack::reset()
{
    lpFilter.reset();
    hpFilter.reset();
    hpBuffer.clear();
}
