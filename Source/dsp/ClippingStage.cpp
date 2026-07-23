#include "ClippingStage.h"

// ===========================================================================
// Diode forward voltages — from the Aion Fusion (traced Suhr Riot) parts:
//   D6–D9: 1N34A germanium, two in series per direction → ≈ 0.65 V total
//   D10/D11: 1N914 silicon, two in series (positive)     → ≈ 1.3 V total
//   D12: blue LED (negative)                              → ≈ 2.5 V
//   D4/D5: red LED, one per direction                     → ≈ 1.7 V
// ===========================================================================

struct DiodeVf
{
    float vfPos;    // total forward voltage, positive half-cycle (V)
    float vfNeg;    // total forward voltage, negative half-cycle (V)
    float preGain;  // drive boost into the clipper (see HOT note below)
};

static constexpr DiodeVf kDiodeTable[3] =
{
    { 0.65f, 0.65f, 1.0f },   // Mode 0: WARM — 2× Ge series, symmetric

    // Mode 1: CLASSIC — red LED each way with 2.6× (+8.3 dB) pre-gain.
    // The pre-gain drives the LEDs as deep past their 1.7 V knee as WARM's
    // Ge pair gets (1.7/0.65), so saturation holds through the note decay.
    // Peaks are still ceiling-clamped, so loudness matches the other modes.
    { 1.70f, 1.70f, 2.6f },

    // Mode 2: HOT — same red-LED voicing as CLASSIC but driven twice as
    // hard (5.2×, +14.3 dB into the clipper). Deepest saturation and the
    // longest singing sustain; the lead mode.
    { 1.70f, 1.70f, 5.2f },
};

// ===========================================================================
// Waveshaper — rail saturation followed by the shunt-diode clamp.
//
// Small-signal behaviour is UNITY (tanh slope = 1 at zero): below the diode
// knee the signal — and the noise floor — passes through unchanged. This is
// the physically correct behaviour of a shunt clipper, and it is what keeps
// the plugin quiet between notes.
// ===========================================================================
static inline float waveshape (float x, float rail,
                                float cPos, float cNeg) noexcept
{
    // Op-amp rail saturation (symmetric)
    x = rail * std::tanh (x / rail);

    // Shunt diode clamp (asymmetric ceilings)
    if (x >= 0.0f)
        return cPos * std::tanh (x / cPos);
    else
        return cNeg * std::tanh (x / cNeg);
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void ClippingStage::prepare (double /*sampleRate*/, int samplesPerBlock)
{
    for (int m = 0; m < 3; ++m)
    {
        modeCoeffs[m].cPos = kDiodeTable[m].vfPos / Vg;
        modeCoeffs[m].cNeg = kDiodeTable[m].vfNeg / Vg;

        // Per-mode makeup gain: divide by this mode's own (larger) ceiling
        // and scale to the Ge ceiling, so every mode peaks exactly where
        // WARM does. (WARM: outGain = 1.)
        const float maxCeil  = juce::jmax (modeCoeffs[m].cPos, modeCoeffs[m].cNeg);
        modeCoeffs[m].outGain = kGeCeil / maxCeil;
        modeCoeffs[m].preGain = kDiodeTable[m].preGain;
    }

    const auto n = static_cast<size_t> (samplesPerBlock);
    os2x.initProcessing (n);  os2x.reset();
    os4x.initProcessing (n);  os4x.reset();
    os8x.initProcessing (n);  os8x.reset();
}

void ClippingStage::process (juce::AudioBuffer<float>& buffer)
{
    const auto& c = modeCoeffs[currentMode];

    auto& os = (currentOsOrder == 0) ? os2x
             : (currentOsOrder == 1) ? os4x
             :                         os8x;

    juce::dsp::AudioBlock<float> inputBlock (buffer);
    auto upsampledBlock = os.processSamplesUp (inputBlock);

    const size_t numCh      = upsampledBlock.getNumChannels();
    const size_t numSamples = upsampledBlock.getNumSamples();

    // Per-mode makeup times global norm: every mode peaks where WARM does.
    const float outNorm = c.outGain / kOutRef;

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        float* data = upsampledBlock.getChannelPointer (ch);
        for (size_t i = 0; i < numSamples; ++i)
            data[i] = outNorm * waveshape (c.preGain * data[i], kRail, c.cPos, c.cNeg);
    }

    os.processSamplesDown (inputBlock);
}

void ClippingStage::setOversamplingOrder (int order)
{
    order = juce::jlimit (0, 2, order);
    if (order == currentOsOrder) return;
    currentOsOrder = order;
    // Flush all three to avoid a click when switching
    os2x.reset();
    os4x.reset();
    os8x.reset();
}

void ClippingStage::setMode (int mode)
{
    currentMode = juce::jlimit (0, 2, mode);
}

void ClippingStage::reset()
{
    os2x.reset();
    os4x.reset();
    os8x.reset();
}
