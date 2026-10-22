#include "ClippingStage.h"

// ===========================================================================
// Diode clip modes — the Riot's 3-way CLIP toggle selects three genuinely
// different shunt-diode strings, from the Aion Fusion (traced Suhr Riot)
// parts (D4/D5 red LED, D6–D9 1N34A germanium, D10/D11 1N914 silicon):
//
//   Mode 0 WARM    — 2× 1N34A germanium in series per direction. SYMMETRIC,
//                    lowest knee (~0.65 V): clips earliest and softest, the
//                    warmest/most-compressed and quietest voicing.
//   Mode 1 CLASSIC — ASYMMETRIC silicon: one 1N914 (~0.6 V) on the positive
//                    half, two in series (~1.2 V) on the negative. The unequal
//                    ceilings clip the two half-cycles by different amounts,
//                    which generates EVEN-order harmonics (2nd, 4th) on top of
//                    the odd ones — the tube-like, vintage "grit" that a
//                    symmetric clipper (a Big Muff) fundamentally cannot make.
//   Mode 2 HOT     — 1× red LED per direction (~1.7 V). SYMMETRIC, highest
//                    knee: clips last and least, the most dynamic/open and
//                    loudest voicing — the singing lead mode.
//
// WHY NO PER-MODE PRE-GAIN ANY MORE (was 1.0/2.6/5.2): the real CLIP switch
// only swaps DIODES; it does not change the gain into them. The old pre-gain
// existed solely to force the higher-Vf LED modes to saturate as deeply as
// the germanium mode so all three peaked at the same level — i.e. it was
// cancelling out exactly the level and hardness differences the switch is
// FOR. The drive stage already pushes 100–1000× into this clipper, so every
// mode is deeply saturated on its own; the modes now differ by diode
// character (knee height + asymmetry) and their genuine natural level, as on
// the hardware. `preGain` is kept in the struct (all 1.0) only so the field
// stays available for future per-mode tuning.
//
// The exact diode assignment per switch position is an ENGINEERING ESTIMATE
// (the 3-way toggle's wiring wasn't recoverable from the OCR'd schematic);
// the diode PARTS themselves are from the traced BOM. Tune by ear against the
// real pedal if one is available.
// ===========================================================================

struct DiodeVf
{
    float vfPos;    // total forward voltage, positive half-cycle (V)
    float vfNeg;    // total forward voltage, negative half-cycle (V)
    float preGain;  // per-mode drive boost (now 1.0 for all — see note above)
};

static constexpr DiodeVf kDiodeTable[3] =
{
    { 0.65f, 0.65f, 1.0f },   // Mode 0: WARM    — 2× Ge series, symmetric
    { 0.60f, 1.20f, 1.0f },   // Mode 1: CLASSIC — Si asymmetric (even harmonics)
    { 1.70f, 1.70f, 1.0f },   // Mode 2: HOT     — 1× red LED, symmetric
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
//   f(v)  = k · tanh(v/k)             -- the saturator itself
//   F₁(v) = k² · ln(cosh(v/k))        -- first antiderivative
//
//   y_n = [F₁(x_n) − F₁(x_{n-1})] / (x_n − x_{n-1})
//   Fallback (|Δ| < ε): y_n = f(x_mid), which is the exact Δ→0 limit.
//
// Both stages use FIRST-order ADAA, combined with oversampling.
//
// ---------------------------------------------------------------------------
// WHY NOT SECOND-ORDER ADAA (removed — it was the source of Ember's noise):
//
// This file previously used second-order ADAA on the rail saturator, via the
// second antiderivative F₂ = k³·[u²/2 − u·ln2 + π²/24 + Li₂(−e^{−2u})/2],
// evaluated as  y = 4·[F₂(xₙ) + F₂(xₙ₋₁) − 2·F₂(x_mid)] / Δ².
//
// That expression is CATASTROPHICALLY ILL-CONDITIONED in float32 here. The
// numerator subtracts three nearly-equal values whose magnitude scales as k³
// — and the rail's k is 40, so k³ = 64000. A float carries ~7 significant
// digits, so each F₂ term already has ~1e-2 of absolute rounding error, while
// the true value of the whole bracket is ≈ f·Δ²/4, which for small Δ is FAR
// smaller than that error. Dividing that noise-dominated numerator by Δ² then
// AMPLIFIES it by up to 1/Δ².
//
// Measured effect (offline SNR test, 220 Hz @ −20 dBFS + −70 dBFS noise
// floor, gate off): Ember's output SNR was 11 dB with ADAA₂ and 49.7 dB with
// it bypassed — i.e. ADAA₂ was manufacturing ~39 dB of broadband numerical
// noise on every sample. Because the saturator generates it continuously, it
// was audible DURING playing, not just at idle, and it got WORSE as drive was
// reduced (smaller Δ ⇒ worse conditioning) — which is exactly the behaviour
// reported. The old ε = 1e-4 fallback threshold was ~400× too small to
// protect against this.
//
// F₁ is well-conditioned by comparison: its magnitude scales as k² (not k³)
// and it is divided by Δ only once, not Δ². Anti-aliasing performance is
// maintained by oversampling instead (see kFixedOsOrder in PluginProcessor).
// ===========================================================================
static inline float stableLogCosh (float v) noexcept
{
    const float av = std::abs (v);
    return av + std::log1p (std::exp (-2.0f * av)) - 0.6931472f;
}

static inline float fTanhScaled (float v, float k) noexcept { return k * std::tanh (v / k); }
static inline float FTanhScaled (float v, float k) noexcept { return k * k * stableLogCosh (v / k); }

// Fallback threshold for first-order ADAA, SCALED BY THE SATURATOR'S OWN k.
//
// F₁ scales as k², so its float32 rounding error scales as k² too, while the
// ADAA numerator F₁(x)−F₁(xPrev) ≈ f·Δ scales as k·Δ. Requiring the numerator
// to stay well above the rounding error therefore requires Δ to scale WITH k
// — a single absolute epsilon cannot be right for both the rail (k = 40) and
// the diodes (k ≈ 6.5–17). k·1e-3 keeps the relative error of the difference
// at ~2e-4 for every k used here. Below the threshold the midpoint fallback
// f((x+xPrev)/2) is used, which is the exact Δ→0 limit anyway — and when Δ is
// that small the signal is barely moving, so there is negligible aliasing to
// suppress and nothing is lost.
static inline float adaaEpsilon (float k) noexcept { return k * 1.0e-3f; }

// First-order ADAA for a symmetric scaled-tanh saturator (used for the rail).
static inline float adaa1TanhScaled (float x, float& xPrev, float k) noexcept
{
    const float dx = x - xPrev;
    const float y  = (std::abs (dx) < adaaEpsilon (k))
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
    // k-scaled epsilon (was a fixed 1e-5, which was also too small to keep the
    // F₁ difference above its own rounding error) — see adaaEpsilon().
    const float out = (std::abs (dy) < adaaEpsilon (k))
                        ? fTanhScaled (0.5f * (y + yPrev), k)
                        : (FTanhScaled (y, k) - FTanhScaled (yPrev, k)) / dy;
    yPrev = y;
    return out;
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void ClippingStage::prepare (double sampleRate, int samplesPerBlock)
{
    // -----------------------------------------------------------------------
    // WHAT CHANGED (post-Phase-11 level-fidelity revision) AND WHY:
    //
    // This loop used to compute a per-mode makeup gain
    //   outGain = kGeCeil / maxCeil
    // which divided each mode by its OWN diode ceiling and rescaled it to the
    // germanium (WARM) ceiling — forcing CLASSIC and HOT down by 0.65/1.70 =
    // 0.382 (-8.35 dB) so all three modes peaked at exactly WARM's level.
    //
    // That is not what the pedal does. The clipper is a SHUNT clamp: the
    // output peak IS the conducting diode string's forward voltage. A
    // germanium pair clamps at ~0.65 V; a red LED clamps at ~1.70 V. On the
    // real Riot the LED modes are therefore genuinely ~2.6x (+8.35 dB) LOUDER
    // than the germanium mode — switching to a hotter mode gives you more
    // saturation AND more level, which is exactly the jump players use it for.
    // The old normalisation cancelled that ratio out precisely, leaving mode
    // changes as timbre-only.
    //
    // Now each mode simply keeps its own ceiling. The only remaining scaling
    // is the global kOutRef normalisation, which puts the LOUDEST ceiling
    // (red LED) at 1.0 full scale so the hottest mode uses the available
    // headroom and the quieter modes sit proportionally below it.
    //
    // Clip SHAPE is untouched: cPos/cNeg, preGain, the rail constant, the
    // ADAA math and the oversampling are all exactly as before.
    // -----------------------------------------------------------------------
    for (int m = 0; m < 3; ++m)
    {
        modeCoeffs[m].cPos = kDiodeTable[m].vfPos / Vg;
        modeCoeffs[m].cNeg = kDiodeTable[m].vfNeg / Vg;
        modeCoeffs[m].preGain = kDiodeTable[m].preGain;
    }

    const auto n = static_cast<size_t> (samplesPerBlock);
    os2x.initProcessing (n);  os2x.reset();
    os4x.initProcessing (n);  os4x.reset();
    os8x.initProcessing (n);  os8x.reset();

    // Post-clip DC blocker (base rate — runs after downsampling). CLASSIC's
    // asymmetric ceilings clip the two half-cycles unequally, so the clipped
    // waveform carries a DC offset the symmetric modes don't. On the hardware
    // an output coupling cap removes it; the model had no such cap, so the
    // offset would leak downstream (wasting headroom, biasing later filters).
    // A gentle first-order high-pass at 10 Hz is inaudible on program material
    // and a no-op for the symmetric WARM/HOT modes (their DC is ~0 already).
    juce::dsp::ProcessSpec dcSpec;
    dcSpec.sampleRate       = sampleRate;
    dcSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    dcSpec.numChannels      = 2;
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, 10.0f);
    dcBlocker.prepare (dcSpec);

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

    // Global normalisation only — no per-mode leveling (see prepare()). The
    // red-LED ceiling maps to 1.0 full scale, so WARM's lower germanium
    // ceiling comes out proportionally quieter, as on the real pedal.
    const float outNorm = 1.0f / kOutRef;

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        float* data = upsampledBlock.getChannelPointer (ch);
        const auto chIdx = juce::jmin (static_cast<int> (ch), kMaxChannels - 1);

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float x = c.preGain * data[i];
            const float y = adaa1TanhScaled (x, railXPrev[chIdx], kRail);
            const float o = adaaDiode (y, diodeYPrev[chIdx], c.cPos, c.cNeg);
            data[i] = outNorm * o;
        }
    }

    os.processSamplesDown (inputBlock);

    // Remove any DC the (asymmetric) clip produced — see prepare().
    juce::dsp::ProcessContextReplacing<float> dcCtx (inputBlock);
    dcBlocker.process (dcCtx);
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
    dcBlocker.reset();

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        railXPrev[ch]  = 0.0f;
        diodeYPrev[ch] = 0.0f;
    }
}
