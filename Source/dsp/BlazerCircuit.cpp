#include "BlazerCircuit.h"
#include "DiodeClipperNR.h"

// ===========================================================================
// Lifecycle
// ===========================================================================

void BlazerCircuit::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    // Input coupling: R2=470k, C1=22n -> fc ~ 15.4 Hz.
    *inputHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 15.4f);
    inputHpf.prepare (spec);

    tightHpf.prepare (spec);
    *tightHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 20.0f);

    // Output coupling: C17/C18=220n -> ~1.5 Hz, effectively DC-block only.
    *outputHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 1.5f);
    outputHpf.prepare (spec);

    bassShelf.prepare (spec);
    trebleShelf.prepare (spec);
    presenceShelf.prepare (spec);

    // Force every change-detected coefficient to rebuild on the first block at
    // this (possibly new) sample rate.
    lastTightNorm = lastBassNorm = lastTrebleNorm = lastPresenceNorm
        = std::numeric_limits<float>::quiet_NaN();

    // Fixed 8x oversampling around the NR clipper loop only (post-v2.0
    // exception — see header).
    os8x.initProcessing (static_cast<size_t> (samplesPerBlock));
    os8x.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

void BlazerCircuit::reset()
{
    inputHpf.reset();
    tightHpf.reset();
    outputHpf.reset();
    bassShelf.reset();
    trebleShelf.reset();
    presenceShelf.reset();
    os8x.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

// ===========================================================================
// Processing
// ===========================================================================

void BlazerCircuit::process (juce::AudioBuffer<float>& buffer)
{
    // TIGHT: continuous pre-clip HPF (C100K reverse-audio pot on the real
    // pedal — the taper affects the KNOB-to-normalized mapping in Phase 9's
    // getParameterRanges, not this internal corner sweep). 20 Hz (fully
    // open) to 400 Hz (max tight), matching the range convention already
    // established for Ember's own Tight control.
    // Rebuilt only when the knob moves (makeXxx heap-allocates — see last*Norm).
    if (tightNorm != lastTightNorm)
    {
        const float tightFc = 20.0f + tightNorm * 380.0f;
        *tightHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, tightFc);
        lastTightNorm = tightNorm;
    }

    // 3-band passive-style EQ: Bass (low shelf), Treble (high shelf,
    // ~2kHz), Presence (high shelf, ~5kHz — higher and narrower-feeling
    // than Treble, the classic "sizzle" control distinguishing it from
    // Treble). No active mid stage exists on this circuit. Each rebuilt only
    // when its own knob moves.
    if (bassNorm != lastBassNorm)
    {
        const float bassDb = (bassNorm - 0.5f) * 24.0f;
        *bassShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, 100.0f, 0.707f, juce::Decibels::decibelsToGain (bassDb));
        lastBassNorm = bassNorm;
    }
    if (trebleNorm != lastTrebleNorm)
    {
        const float trebleDb = (trebleNorm - 0.5f) * 24.0f;
        *trebleShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 2000.0f, 0.707f, juce::Decibels::decibelsToGain (trebleDb));
        lastTrebleNorm = trebleNorm;
    }
    if (presenceNorm != lastPresenceNorm)
    {
        const float presenceDb = (presenceNorm - 0.5f) * 24.0f;
        *presenceShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 5000.0f, 0.707f, juce::Decibels::decibelsToGain (presenceDb));
        lastPresenceNorm = presenceNorm;
    }

    // Clipper feedback network: Rin=R4=22k (estimate), Rf = R13=220k base
    // + TRIM (100k, gain-floor offset per the doc's own description) +
    // GAIN pot (B1M, linear taper, 0..1MOhm) in series (all estimates —
    // see header).
    const float rin     = 22000.0f;
    const float rfBase  = 220000.0f + trimNorm * 100000.0f;
    const float rf      = rfBase + gainNorm * 1.0e6f;

    // -----------------------------------------------------------------------
    // Volume (VOLUMEN = A50K, BOM-confirmed) plus the output makeup buffer.
    //
    // WHAT CHANGED (post-Phase-11 level-fidelity revision) AND WHY:
    //   Was  volumeGain = volumeNorm  — linear, and capped at unity. Same two
    //   defects as Burner had: an A50K pot is an AUDIO taper, not linear, and
    //   a real BE-OD is not limited to unity output — it is a famously loud
    //   pedal with plenty of makeup available to push an amp.
    //
    //   Now: audio taper (volumeNorm^2) times a fixed output makeup gain.
    //   Noon is unchanged (0.5^2 * 2.0 = 0.5), so this corrects the taper and
    //   the available headroom without moving the calibrated default level.
    // -----------------------------------------------------------------------
    const float volumeGain = volumeNorm * volumeNorm * makeupGain;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    inputHpf.process (ctx);
    tightHpf.process (ctx);

    // NR clipper runs inside an 8x oversampled region (post-v2.0 exception
    // — see header). Solver math itself is unchanged from before.
    {
        auto upsampledBlock = os8x.processSamplesUp (block);
        const size_t numChUp      = upsampledBlock.getNumChannels();
        const size_t numSamplesUp = upsampledBlock.getNumSamples();

        for (size_t ch = 0; ch < numChUp; ++ch)
        {
            float* data = upsampledBlock.getChannelPointer (ch);
            const auto chIdx = juce::jmin (static_cast<int> (ch), kMaxChannels - 1);

            for (size_t i = 0; i < numSamplesUp; ++i)
                data[i] = diodeclipper::solve (data[i], rin, rf, clipperPrevVout[chIdx]);
        }

        os8x.processSamplesDown (block);
    }

    buffer.applyGain (volumeGain);

    bassShelf.process (ctx);
    trebleShelf.process (ctx);
    presenceShelf.process (ctx);
    outputHpf.process (ctx);
}
