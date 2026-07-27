#include "BlazerCircuit.h"

// ===========================================================================
// Newton-Raphson diode-feedback clipper — same pattern and diode family as
// BurnerCircuit (D2-D5, four 1N4148 as two antiparallel pairs), duplicated
// here rather than shared since BurnerCircuit is frozen (Phase 6) and must
// not be refactored to extract a common helper. See BurnerCircuit.cpp for
// the full derivation comment.
// ===========================================================================
namespace
{
    constexpr float kIs     = 2.52e-9f;
    constexpr float kN      = 1.752f;
    constexpr float kVt     = 0.02585f;
    constexpr float kNVt    = kN * kVt;
    constexpr float kIrPair = 2.0f * kIs;

    // Clamp bound chosen so exp(vout/kNVt) never overflows float (overflows
    // past ~88.7): 4.0f / kNVt ~= 88.3, safely under that ceiling with
    // margin, while still far beyond where a real clipped signal should
    // ever sit (diode ceilings here are a fraction of a volt in these
    // normalized DSP units). BlazerCircuit's Rin/Rf combination (lower Rin,
    // higher Rf floor from Trim than Burner's) drives proportionally more
    // current into the same-amplitude input, which was measured (Phase 10)
    // to push Newton-Raphson's early iterations past a +-20V clamp before
    // convergence, overflowing exp() into inf, then inf-inf into NaN that
    // permanently corrupts clipperPrevVout. Burner shares this same
    // clamp-vs-exp() mismatch as a latent risk under different gain
    // settings, but it's frozen (Phase 6) and this fix is scoped to
    // Blazer's own copy only.
    constexpr float kVoutClamp = 4.0f;

    inline float diodeClipperNR (float vin, float rin, float rf, float& prevVout) noexcept
    {
        float vout = juce::jlimit (-kVoutClamp, kVoutClamp, prevVout);

        for (int iter = 0; iter < 8; ++iter)
        {
            const float e1 = std::exp ( vout / kNVt);
            const float e2 = std::exp (-vout / kNVt);
            const float f  = (vout / rf) + kIrPair * (e1 - e2) + (vin / rin);
            const float fp = (1.0f / rf) + (kIrPair / kNVt) * (e1 + e2);

            const float step = f / fp;
            vout = juce::jlimit (-kVoutClamp, kVoutClamp, vout - step);

            if (std::abs (step) < 1.0e-6f)
                break;
        }

        prevVout = vout;
        return vout;
    }
}

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
    const float tightFc = 20.0f + tightNorm * 380.0f;
    *tightHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, tightFc);

    // 3-band passive-style EQ: Bass (low shelf), Treble (high shelf,
    // ~2kHz), Presence (high shelf, ~5kHz — higher and narrower-feeling
    // than Treble, the classic "sizzle" control distinguishing it from
    // Treble). No active mid stage exists on this circuit.
    const float bassDb     = (bassNorm     - 0.5f) * 24.0f;
    const float trebleDb   = (trebleNorm   - 0.5f) * 24.0f;
    const float presenceDb = (presenceNorm - 0.5f) * 24.0f;

    *bassShelf.state     = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 100.0f,  0.707f, juce::Decibels::decibelsToGain (bassDb));
    *trebleShelf.state   = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 2000.0f, 0.707f, juce::Decibels::decibelsToGain (trebleDb));
    *presenceShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 5000.0f, 0.707f, juce::Decibels::decibelsToGain (presenceDb));

    // Clipper feedback network: Rin=R4=22k (estimate), Rf = R13=220k base
    // + TRIM (100k, gain-floor offset per the doc's own description) +
    // GAIN pot (B1M, linear taper, 0..1MOhm) in series (all estimates —
    // see header).
    const float rin     = 22000.0f;
    const float rfBase  = 220000.0f + trimNorm * 100000.0f;
    const float rf      = rfBase + gainNorm * 1.0e6f;

    const float volumeGain = volumeNorm;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    inputHpf.process (ctx);
    tightHpf.process (ctx);

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        const auto chIdx = juce::jmin (ch, kMaxChannels - 1);

        for (int i = 0; i < numSamples; ++i)
            data[i] = diodeClipperNR (data[i], rin, rf, clipperPrevVout[chIdx]);
    }

    buffer.applyGain (volumeGain);

    bassShelf.process (ctx);
    trebleShelf.process (ctx);
    presenceShelf.process (ctx);
    outputHpf.process (ctx);
}
