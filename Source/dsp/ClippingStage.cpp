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
// ===========================================================================
// ADAA (antiderivative anti-aliasing) for a single scaled-tanh saturator.
//
//   f(v) = k * tanh(v/k)              -- the saturator itself
//   F(v) = k^2 * ln(cosh(v/k))        -- its antiderivative
//
// First-order ADAA replaces f(x_n) with the secant slope of F between the
// current and previous sample: (F(x_n) - F(x_n-1)) / (x_n - x_n-1). That
// exactly band-limits the harmonics the nonlinearity generates. When two
// samples are (nearly) equal the secant is ill-conditioned, so it falls
// back to evaluating f at the midpoint.
// ===========================================================================
static inline float stableLogCosh (float v) noexcept
{
    // ln(cosh(v)) = |v| + ln(1 + e^-2|v|) - ln(2), avoids cosh() overflow.
    const float av = std::abs (v);
    return av + std::log1p (std::exp (-2.0f * av)) - 0.6931472f;
}

static inline float fTanhScaled (float v, float k) noexcept { return k * std::tanh (v / k); }
static inline float FTanhScaled (float v, float k) noexcept { return k * k * stableLogCosh (v / k); }

static inline float adaaTanhScaled (float x, float& xPrev, float k) noexcept
{
    const float dx = x - xPrev;
    const float y  = (std::abs (dx) < 1.0e-5f)
                        ? fTanhScaled (0.5f * (x + xPrev), k)
                        : (FTanhScaled (x, k) - FTanhScaled (xPrev, k)) / dx;
    xPrev = x;
    return y;
}

// Diode stage: ceiling is asymmetric (cPos/cNeg). Both F evaluations in a
// step use the ceiling matching the CURRENT sample's sign — F and its slope
// both vanish at 0 regardless of which ceiling is used, so this introduces
// no discontinuity, only a negligible approximation right at a sign flip.
static inline float adaaDiode (float y, float& yPrev, float cPos, float cNeg) noexcept
{
    const float k  = (y >= 0.0f) ? cPos : cNeg;
    const float dy = y - yPrev;
    const float out = (std::abs (dy) < 1.0e-5f)
                        ? fTanhScaled (0.5f * (y + yPrev), k)
                        : (FTanhScaled (y, k) - FTanhScaled (yPrev, k)) / dy;
    yPrev = y;
    return out;
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

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        railXPrev[ch]  = 0.0f;
        diodeYPrev[ch] = 0.0f;
    }
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
        const auto chIdx = juce::jmin (static_cast<int> (ch), kMaxChannels - 1);

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float x = c.preGain * data[i];
            const float y = adaaTanhScaled (x, railXPrev[chIdx], kRail);
            const float o = adaaDiode (y, diodeYPrev[chIdx], c.cPos, c.cNeg);
            data[i] = outNorm * o;
        }
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

    // The oversampled sample stream is discontinuous across an OS switch —
    // flush the ADAA secant history too, same reason as the oversamplers.
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        railXPrev[ch]  = 0.0f;
        diodeYPrev[ch] = 0.0f;
    }
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

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        railXPrev[ch]  = 0.0f;
        diodeYPrev[ch] = 0.0f;
    }
}
