#include "BurnerCircuit.h"

// ===========================================================================
// Newton-Raphson diode-feedback clipper.
//
// Op-amp inverting stage: input current Vin/Rin balances feedback-branch
// current through Rf in parallel with two antiparallel 1N4148 pairs
// (D2-D5). At the virtual-ground node:
//
//   Vin/Rin + Vout/Rf + Ir*(exp(Vout/nVt) - exp(-Vout/nVt)) = 0
//
// Solved per-sample via Newton-Raphson (this IS the nonlinear feedback
// one-port solve a WDF root-adaptor would produce for this topology).
// Ir is doubled vs. a single 1N4148 pair, since D2-D5 form two antiparallel
// pairs in parallel (see header: BOM-confirmed 4-diode count).
// ===========================================================================
namespace
{
    constexpr float kIs      = 2.52e-9f;             // 1N4148 SPICE-fit saturation current
    constexpr float kN       = 1.752f;               // 1N4148 SPICE-fit ideality factor
    constexpr float kVt      = 0.02585f;              // thermal voltage at room temp
    constexpr float kNVt     = kN * kVt;
    constexpr float kIrPair  = 2.0f * kIs;            // two antiparallel pairs in parallel

    // FREEZE EXCEPTION (Phase 10, narrow numerical-safety fix, re-frozen
    // immediately after): the previous +-20V clamp permitted
    // exp(vout/kNVt) arguments up to ~441 -- exp() overflows float past
    // ~88.7, producing inf, and inf-inf = NaN, which permanently corrupts
    // prevVout (NaN survives the clamp). Found via Blazer's Rin/Rf
    // combination first, then confirmed to be an identical latent bug here
    // under different gain/input conditions during Phase 10 multi-mode
    // switching tests. 4.0f / kNVt ~= 88.3, safely under the overflow
    // ceiling while still far beyond where a real clipped signal should
    // ever sit (diode ceilings here are a fraction of a volt in these
    // normalized DSP units) -- this does not change the audible clipping
    // character, only removes a numerical-overflow failure mode.
    constexpr float kVoutClamp = 4.0f;

    inline float diodeClipperNR (float vin, float rin, float rf, float& prevVout) noexcept
    {
        float vout = juce::jlimit (-kVoutClamp, kVoutClamp, prevVout); // seed from last sample

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

    // Aggression filter placeholder coefficients; real values set in process()
    // once the aggression state is known (avoids recomputing every sample).
    *aggressionPreFilter.state  = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 3000.0f, 0.707f, 1.0f);
    *aggressionPostFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 3000.0f, 0.707f, 1.0f);
    aggressionPreFilter.prepare (spec);
    aggressionPostFilter.prepare (spec);
    lastAggression = -1;

    // Output coupling: C24=220nf against a nominal 470k load -> ~1.5 Hz,
    // essentially DC-block only.
    *outputHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 1.5f);
    outputHpf.prepare (spec);

    bassShelf.prepare (spec);
    trebleShelf.prepare (spec);
    midPeak.prepare (spec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

void BurnerCircuit::reset()
{
    inputHpf.reset();
    aggressionPreFilter.reset();
    aggressionPostFilter.reset();
    outputHpf.reset();
    bassShelf.reset();
    trebleShelf.reset();
    midPeak.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
        clipperPrevVout[ch] = 0.0f;
}

// ===========================================================================
// Processing
// ===========================================================================

void BurnerCircuit::process (juce::AudioBuffer<float>& buffer)
{
    // --- Aggression: recompute only when the switch state changes ---
    // C3=2n2 alone (state 0) vs C3||C4=4n4 (state 1), against R31=4k7
    // (engineering estimate — see header). State 0 (smaller total cap,
    // higher corner) = brighter/more aggressive; state 1 (larger cap,
    // lower corner) = calmer/more compressed — direction is an
    // interpretation, tune by ear once auditioned.
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
        const float cAggr   = (aggression == 0) ? 2.2e-9f : 4.4e-9f;
        const float fc      = 1.0f / (2.0f * juce::MathConstants<float>::pi * 4700.0f * cAggr);
        const float shelfDb = (aggression == 0) ? 5.0f : -4.0f; // brighter vs. tamer

        *aggressionPreFilter.state  = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, fc, 0.707f, juce::Decibels::decibelsToGain (shelfDb));
        *aggressionPostFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, fc, 0.707f, juce::Decibels::decibelsToGain (shelfDb));
        lastAggression = aggression;
    }

    // --- Tone stack: Baxandall bass/treble shelves + active mid peak ---
    const float bassDb   = (bassNorm   - 0.5f) * 24.0f;  // +-12 dB
    const float trebleDb = (trebleNorm - 0.5f) * 24.0f;  // +-12 dB
    const float midDb    = (midNorm    - 0.5f) * 24.0f;  // +-12 dB

    *bassShelf.state   = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 100.0f,  0.707f, juce::Decibels::decibelsToGain (bassDb));
    *trebleShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (trebleDb));
    *midPeak.state     = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 800.0f, 1.0f,   juce::Decibels::decibelsToGain (midDb));

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
    const float aggressionDrive = (aggression == 0) ? 1.35f : 0.85f;

    // Volume (A50k, applied post-clip, pre-tone-stack per the doc's stated
    // "active mid control after volume") and output makeup.
    const float volumeGain = volumeNorm;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    inputHpf.process (ctx);
    aggressionPreFilter.process (ctx);

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        const auto chIdx = juce::jmin (ch, kMaxChannels - 1);

        for (int i = 0; i < numSamples; ++i)
            data[i] = diodeClipperNR (data[i] * aggressionDrive, rin, rf, clipperPrevVout[chIdx]);
    }

    aggressionPostFilter.process (ctx);
    buffer.applyGain (volumeGain);

    bassShelf.process (ctx);
    midPeak.process (ctx);
    trebleShelf.process (ctx);
    outputHpf.process (ctx);
}
