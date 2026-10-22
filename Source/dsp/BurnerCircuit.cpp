#include "BurnerCircuit.h"
#include "DiodeClipperNR.h"

// ===========================================================================
// Lifecycle
// ===========================================================================

void BurnerCircuit::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    // Input coupling HPF: R2=470k, C1=22n -> fc = 1/(2*pi*470k*22n) ~ 15.4 Hz.
    *inputHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 15.4f);
    inputHpf.prepare (spec);

    // Tight: not schematic-accurate (see header) — 20 Hz (fully open) placeholder.
    *tightHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 20.0f);
    tightHpf.prepare (spec);

    // Aggression filter placeholder coefficients; real values set in process()
    // once the aggression state is known (avoids recomputing every sample).
    *aggressionPreFilter.state  = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 3000.0f, 0.707f, 1.0f);
    *aggressionPostFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 3000.0f, 0.707f, 1.0f);
    aggressionPreFilter.prepare (spec);
    aggressionPostFilter.prepare (spec);
    lastAggression = -1;

    // Force every change-detected coefficient to rebuild on the first block at
    // this (possibly new) sample rate — otherwise a sample-rate change would
    // leave the guarded tone-stack/Tight coefficients stale for the old rate.
    lastTightNorm = lastBassNorm = lastTrebleNorm = lastMidNorm
        = std::numeric_limits<float>::quiet_NaN();

    // Output coupling: C24=220nf against a nominal 470k load -> ~1.5 Hz,
    // essentially DC-block only.
    *outputHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 1.5f);
    outputHpf.prepare (spec);

    bassShelf.prepare (spec);
    trebleShelf.prepare (spec);
    midPeak.prepare (spec);

    // Fixed 8x oversampling around the NR clipper loop only (post-v2.0
    // exception — see header). Linear filters above/below stay at base rate.
    os8x.initProcessing (static_cast<size_t> (samplesPerBlock));
    os8x.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

void BurnerCircuit::reset()
{
    inputHpf.reset();
    tightHpf.reset();
    aggressionPreFilter.reset();
    aggressionPostFilter.reset();
    outputHpf.reset();
    bassShelf.reset();
    trebleShelf.reset();
    midPeak.reset();
    os8x.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

// ===========================================================================
// Processing
// ===========================================================================

void BurnerCircuit::process (juce::AudioBuffer<float>& buffer)
{
    // Tight: not schematic-accurate (see header) — continuous pre-clip
    // HPF, 20 Hz (fully open) to 400 Hz (max tight), same range convention
    // as Blazer/Ember's own Tight controls. Rebuilt only when the knob moves
    // (the makeXxx factory heap-allocates — see the last*Norm members).
    if (tightNorm != lastTightNorm)
    {
        const float tightFc = 20.0f + tightNorm * 380.0f;
        *tightHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, tightFc);
        lastTightNorm = tightNorm;
    }

    // --- Aggression: recompute only when the switch state changes ---
    // Two switch states against R31=4k7 (engineering estimate — see header).
    // As implemented below: state 1 uses the SMALLER total cap (C3=2n2 -> a
    // HIGHER corner -> brighter/more aggressive, +5 dB shelf); state 0 uses
    // the LARGER total cap (C3||C4=4n4 -> a LOWER corner -> calmer/more
    // compressed, -4 dB shelf). Direction is an interpretation — tune by ear
    // once auditioned. (Earlier this comment described the two states with
    // the cap sizes swapped; the code below is the source of truth.)
    //
    // Applied as high-shelves BOTH pre-clip (drives more/less HF content
    // into the nonlinearity, consistent with the doc's own "might squeal"
    // note at high gain+aggression — squeal implies more HF gain, not just
    // a static EQ cut) and post-clip (shapes the harmonics clipping
    // actually generates, which is what makes the states audibly
    // distinguishable regardless of the input's fundamental — see Phase 5
    // finding).
    if (aggression != lastAggression)
    {
        const float cAggr   = (aggression == 1) ? 2.2e-9f : 4.4e-9f;
        const float fc      = 1.0f / (2.0f * juce::MathConstants<float>::pi * 4700.0f * cAggr);
        const float shelfDb = (aggression == 1) ? 5.0f : -4.0f; // brighter vs. tamer

        *aggressionPreFilter.state  = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, fc, 0.707f, juce::Decibels::decibelsToGain (shelfDb));
        *aggressionPostFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, fc, 0.707f, juce::Decibels::decibelsToGain (shelfDb));
        lastAggression = aggression;
    }

    // --- Tone stack: Baxandall bass/treble shelves + active mid peak ---
    // Each shelf/peak rebuilt only when its knob moves (makeXxx heap-allocates).
    if (bassNorm != lastBassNorm)
    {
        const float bassDb = (bassNorm - 0.5f) * 24.0f;  // +-12 dB
        *bassShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, 100.0f, 0.707f, juce::Decibels::decibelsToGain (bassDb));
        lastBassNorm = bassNorm;
    }
    if (trebleNorm != lastTrebleNorm)
    {
        const float trebleDb = (trebleNorm - 0.5f) * 24.0f;  // +-12 dB
        *trebleShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (trebleDb));
        lastTrebleNorm = trebleNorm;
    }
    if (midNorm != lastMidNorm)
    {
        const float midDb = (midNorm - 0.5f) * 24.0f;  // +-12 dB
        *midPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 800.0f, 1.0f, juce::Decibels::decibelsToGain (midDb));
        lastMidNorm = midNorm;
    }

    // Clipper feedback network: Rin=R4=47k (estimate), Rf = R5=82k base +
    // GAIN pot (B1M, linear taper, 0..1MOhm) in series (estimate — see header).
    const float rin = 47000.0f;
    const float rf  = 82000.0f + gainNorm * 1.0e6f;

    // Aggression also directly scales how hard the signal drives the
    // clipper (not just an EQ tilt): the doc's own text says the pedal
    // "might squeal on with the aggression switch engage and the gain knob
    // over 75%" — squealing implies materially more drive/gain at high
    // aggression, not a subtle tonal shift. Phase 5 testing showed the
    // shelf-only version was measurably too weak (<2% audible difference)
    // on real signal content, since a low fundamental's harmonic energy in
    // the shelf's band is small in absolute terms even with a large dB
    // swing. This multiplier guarantees a genuinely different clip depth.
    const float aggressionDrive = (aggression == 1) ? 1.35f : 0.85f;

    // -----------------------------------------------------------------------
    // Volume (A50k, applied post-clip, pre-tone-stack per the doc's stated
    // "active mid control after volume") plus IC2's fixed output makeup.
    //
    // WHAT CHANGED (post-Phase-11 level-fidelity revision) AND WHY:
    //   This was  volumeGain = volumeNorm  — a LINEAR map that also capped the
    //   circuit's maximum output at unity. Two problems:
    //     1. VOLUME is an A50k pot (BOM-confirmed), i.e. an AUDIO taper. A
    //        linear map makes the useful range bunch up at the bottom of the
    //        sweep and does not feel like the real control.
    //     2. Capping at unity means the pedal can never put out more than it
    //        takes in. The real G3 has an IC2 makeup buffer after the clipper
    //        (see header) and is emphatically able to drive an amp harder than
    //        unity — that output is part of how the pedal is used.
    //
    //   Now: audio taper (volumeNorm^2) times IC2's fixed makeup. The makeup
    //   was already declared as `makeupGain` but never actually applied — a
    //   dead stub. It is now wired into the real gain calculation.
    //
    //   Note the noon value is unchanged: 0.5^2 * 2.0 = 0.5, exactly what the
    //   old linear map gave at 0.5. So this corrects the TAPER and the
    //   available headroom without moving the calibrated default level.
    // -----------------------------------------------------------------------
    const float volumeGain = volumeNorm * volumeNorm * makeupGain;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    inputHpf.process (ctx);
    tightHpf.process (ctx);
    aggressionPreFilter.process (ctx);

    // NR clipper runs inside an 8x oversampled region (post-v2.0 exception
    // — see header): upsample, run the same per-sample solve at the higher
    // rate, downsample. The solver math itself is unchanged from before.
    {
        auto upsampledBlock = os8x.processSamplesUp (block);
        const size_t numChUp      = upsampledBlock.getNumChannels();
        const size_t numSamplesUp = upsampledBlock.getNumSamples();

        for (size_t ch = 0; ch < numChUp; ++ch)
        {
            float* data = upsampledBlock.getChannelPointer (ch);
            const auto chIdx = juce::jmin (static_cast<int> (ch), kMaxChannels - 1);

            for (size_t i = 0; i < numSamplesUp; ++i)
                data[i] = diodeclipper::solve (data[i] * aggressionDrive, rin, rf, clipperPrevVout[chIdx]);
        }

        os8x.processSamplesDown (block);
    }

    aggressionPostFilter.process (ctx);
    buffer.applyGain (volumeGain);

    bassShelf.process (ctx);
    midPeak.process (ctx);
    trebleShelf.process (ctx);
    outputHpf.process (ctx);
}
