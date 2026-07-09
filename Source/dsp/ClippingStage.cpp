#include "ClippingStage.h"

// ===========================================================================
// Diode parameters for each mode.
// ---------------------------------------------------------------------------
// Is  = reverse saturation current (A). Smaller Is → higher Vf → harder to
//        forward-bias → diode clips at a higher voltage → more headroom.
// n   = ideality factor. n=1 (ideal), n=2 (recombination-dominated like LEDs).
//        Higher n stretches the exponential curve → softer knee at same Vf.
// ---------------------------------------------------------------------------
// Two 1N34A in series per polarity (Mode 0):
//   Series diodes: total Vf = 2 × Vf_single, Is unchanged, n doubles.
//   Is = 200e-9, n_eff = 2 × 1.0 = 2.0 → soft knee, Vf_pair ≈ 0.50 V
//
// 1N914 silicon (Mode 1):
//   Is = 4e-9, n = 1.5 → sharper knee, Vf ≈ 0.65 V
//
// Red LED (Mode 2):
//   Is = 10e-12, n = 2.0 → very high Vf ≈ 1.9 V, wide dynamic range
// ===========================================================================

struct DiodeSpec { float Is; float n; };

static constexpr DiodeSpec kDiodeTable[3] =
{
    { 200e-9f, 2.0f },   // Mode 0: Germanium 1N34A (2 per polarity)
    {   4e-9f, 1.5f },   // Mode 1: Silicon   1N914
    {  10e-12f, 2.0f },  // Mode 2: LED RED
};

// ===========================================================================
// Waveshaper — the heart of the clipper.
//
// Implements: y = −kOut · arcsinh(x · kIn)
//
// arcsinh(x) = ln(x + √(x²+1))
// std::asinh() handles both large and small arguments stably in C++.
// At large |x|: asinh(x) ≈ ln(2|x|) — no overflow unlike naive exp formulas.
// ===========================================================================
static inline float waveshape (float x, float kIn, float kOut) noexcept
{
    return -kOut * std::asinh (x * kIn);
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void ClippingStage::prepare (double /*sampleRate*/, int samplesPerBlock)
{
    // Pre-compute the two per-mode constants so the audio thread
    // never divides or does expensive math per sample.
    for (int m = 0; m < 3; ++m)
    {
        const float Is = kDiodeTable[m].Is;
        const float n  = kDiodeTable[m].n;

        // kIn: converts normalised DSP sample to the arcsinh argument.
        // Derivation: the physical input voltage is x·Vg.
        //   Vin_phys / (2·Is·Rin)  =  (x·Vg) / (2·Is·Rin)  =  x · (Vg/(2·Is·Rin))
        modeCoeffs[m].kIn  = Vg / (2.0f * Is * Rin);

        // kOut: converts arcsinh result (in physical-volt units) back to DSP.
        // The arcsinh argument is dimensionless; the result is scaled by n·Vt/Vg.
        modeCoeffs[m].kOut = (n * Vt) / Vg;
    }

    oversampler.initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampler.reset();
}

void ClippingStage::process (juce::AudioBuffer<float>& buffer)
{
    const auto& c = modeCoeffs[currentMode];

    // --- 1. Upsample 4× -------------------------------------------------------
    // processSamplesUp() returns a reference to an internal AudioBlock that
    // holds the 4× upsampled data. It's valid until processSamplesDown() is called.
    juce::dsp::AudioBlock<float> inputBlock (buffer);
    auto upsampledBlock = oversampler.processSamplesUp (inputBlock);

    // --- 2. Apply waveshaper at 4× sample rate --------------------------------
    // Each sample is independent (memoryless nonlinearity), so no history needed.
    const size_t numCh      = upsampledBlock.getNumChannels();
    const size_t numSamples = upsampledBlock.getNumSamples();

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        float* data = upsampledBlock.getChannelPointer (ch);
        for (size_t i = 0; i < numSamples; ++i)
            data[i] = waveshape (data[i], c.kIn, c.kOut);
    }

    // --- 3. Downsample back to original rate ----------------------------------
    // processSamplesDown() applies the anti-alias filter and writes the result
    // into the original inputBlock (and therefore into buffer).
    oversampler.processSamplesDown (inputBlock);
}

void ClippingStage::setMode (int mode)
{
    // Clamp to valid range — just in case the DAW sends an out-of-range value.
    currentMode = juce::jlimit (0, 2, mode);
}

void ClippingStage::reset()
{
    oversampler.reset();
}
