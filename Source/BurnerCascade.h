#pragma once
#include <cmath>

// ===========================================================================
// BurnerCascade — routing-level, cascaded high-gain "preamp" that gives the
// Burner its REVV-style saturation. This is NOT a frozen circuit model; it's
// the drive engine that feeds the (frozen) BurnerCircuit's tone stack.
//
// CONTEXT: the Sparker Burner is used to drive a CLEAN, high-headroom amp
// (e.g. a Morgan SW50R) plus an IR — not a cranked high-gain amp. That amp
// contributes essentially NO saturation and NO harmonic conversion of its
// own, so this cascade has to supply all of it. The reference target is the
// REVV Generator 120 "Purple" channel, which is a 4-5 gain-stage preamp into
// a hard-driven power section — roughly 6-7 nonlinear stages in total. So
// this is deliberately voiced as an "amp in a box", not as a faithful
// preamp-only pedal model.
//
// ---------------------------------------------------------------------------
// PER-STAGE TOPOLOGY:   DC-block HP  ->  low-shelf tilt  ->  asym sat  ->  LP
//
// 1. DC-BLOCK HP (kDcHpHz, one per stage) — the inter-stage coupling cap.
//    Low corner (25 Hz): its ONLY job is to stop the DC that the asymmetric
//    saturators generate from accumulating down the chain and biasing later
//    stages toward a rail. It is deliberately NOT a tone control.
//
// 2. LOW-SHELF TILT (kShelfHz/kShelfDb) — the "cathode bypass" tightening.
//    REVISION, and the single most important fix in this file: this used to
//    be a HIGH-PASS per stage at 230/185/150 Hz. Three cascaded one-pole
//    high-passes are ~18 dB/oct below 150 Hz, which put a drop-C fundamental
//    (65 Hz) ~29 dB DOWN before it ever reached a clipper:
//        HP230 -11.3 dB | HP185 -9.6 dB | HP150 -8.0 dB  =  -28.9 dB
//    That is the wrong MECHANISM. Real tube-stage tightness comes mostly from
//    a partially-bypassed cathode resistor, which is a low SHELF in gain —
//    finite depth (typ. 6-12 dB), flat below the corner. Bass gets less gain
//    but is still PRESENT and still CLIPS.
//
//    Why that distinction decides whether the plugin sounds heavy: in a real
//    rig the low fundamental's job is to be converted into harmonics by the
//    amp (a clipped 65 Hz makes 130/195/260/325 Hz, a dense stack the ear
//    fuses into "weight"). The cab/IR then removes the fundamental itself but
//    KEEPS those harmonics. High-passing before the distortion skips the
//    conversion entirely, so the harmonics are never created — and no amount
//    of post-clip EQ can put them back, because boosting 100 Hz only lifts a
//    residual fundamental that the cab is about to remove anyway.
//
//    The shelves now total ~-12 dB at 65 Hz instead of ~-29 dB: still ~13 dB
//    of low tilt for intermodulation control (so chords stay articulate), but
//    the fundamental survives into the nonlinearity and does its job.
//
// 3. ASYMMETRIC SATURATION (kBias) — REVISION: the old code applied a single
//    bias of 0.10 to one stage only. With that stage's drive at 3.5-12, 0.10
//    is an input-referred offset of just 0.8%-2.9% of full scale — it shifted
//    the duty cycle by a fraction of a percent and produced almost no even-
//    harmonic content. In practice the cascade was three SYMMETRIC clippers,
//    and symmetric clipping is odd-harmonics-only: flat, solid-state, polite.
//    Real 12AX7 stages are grossly asymmetric (gentle compression toward
//    cutoff, abrupt clip at grid conduction), and that asymmetry is where
//    "thick / vocal / 3D" comes from. Every stage is now meaningfully
//    asymmetric. Signs ALTERNATE because a real common-cathode stage inverts:
//    in a non-inverting cascade like this one, flipping the bias sign each
//    stage is the equivalent of same-direction asymmetry through an inverting
//    chain, and it keeps the stages from simply compounding into one lopsided
//    shape.
//
// 4. MILLER LP (kLpHz) — the per-stage high-frequency rolloff real cascaded
//    tube stages have (Miller capacitance). Keeps the top controlled without
//    smothering the 3-6 kHz "gnarl" that is the Purple channel's signature.
//
// ---------------------------------------------------------------------------
// FOUR stages (was three): stage count directly sets harmonic density and
// compression depth, and the clean amp downstream adds none of its own. The
// four stages are also deliberately DIFFERENT from each other (drive, bias,
// shelf, rolloff) — identical tanh stages just re-square an already-square
// signal and the harmonic content asymptotes, which is why cranking Gain used
// to change the amount of distortion but not its character.
//
// Kept in this shared header so PluginProcessor (oversampled, at the OS rate)
// and the offline loudness test (base rate) run byte-identical math from one
// source of truth — no manual mirroring to drift out of sync.
// ===========================================================================
namespace burner
{
    inline constexpr int kNumStages = 4;

    // Inter-stage coupling cap / DC block. Low corner on purpose — see (1).
    inline constexpr float kDcHpHz = 25.0f;

    // "Cathode bypass" low-shelf tilt: corner and depth per stage — see (2).
    inline constexpr float kShelfHz[kNumStages] = { 220.0f, 190.0f, 170.0f, 150.0f };
    inline constexpr float kShelfDb[kNumStages] = {  -4.0f,  -3.5f,  -3.0f,  -2.5f };

    // Per-stage asymmetry bias, alternating sign — see (3).
    inline constexpr float kBias[kNumStages] = { 0.20f, -0.30f, 0.26f, -0.15f };

    // Per-stage Miller rolloff — see (4).
    //
    // REVERTED to 14/11/9/8k after a bad call. These had been raised to
    // 17/14/12/10k chasing "clarity", but that was the wrong target: the
    // change added +3.3 dB at 8 kHz and only +1.0 dB at 4 kHz, i.e. it mostly
    // boosted the FIZZ band (6-12 kHz) rather than the DEFINITION band
    // (2-5 kHz) that was actually asked for. Measured result was Burner
    // sitting ~9.5 dB hotter than Ember above 8 kHz.
    //
    // That is not a cosmetic problem: this pedal feeds an AMP. Straight into
    // an IR the cab removes everything above ~5-6 kHz so the excess is
    // inaudible — but into an amp (sim or real) that raw top hits the input
    // stage and gets distorted THERE, producing harshness and aliasing
    // downstream of us. The reported symptom was exactly that: Burner sounded
    // fine direct to IR and bad through the amp.
    //
    // Clarity is now delivered where it belongs — the +4 dB @4 kHz presence
    // shelf in PluginProcessor — while these keep the top progressively
    // rolled off the way a real pedal feeding an amp does.
    inline constexpr float kLpHz[kNumStages] = { 14000.0f, 11000.0f, 9000.0f, 8000.0f };

    // Per-stage drive vs the Gain knob (0..1). Stage 2 is the hot one; the
    // final stage stays a touch softer so the output edge is aggressive
    // without turning to pure fizz.
    inline float drive1 (float g) noexcept { return 1.8f + g * 6.2f; }  // 1.8 .. 8.0
    inline float drive2 (float g) noexcept { return 2.2f + g * 8.3f; }  // 2.2 .. 10.5
    inline float drive3 (float g) noexcept { return 1.9f + g * 6.7f; }  // 1.9 .. 8.6
    inline float drive4 (float g) noexcept { return 1.5f + g * 5.1f; }  // 1.5 .. 6.6

    inline void fillDrives (float g, float out[kNumStages]) noexcept
    {
        out[0] = drive1 (g); out[1] = drive2 (g);
        out[2] = drive3 (g); out[3] = drive4 (g);
    }

    // Sample-rate-dependent coefficients. Computed once per prepare() at
    // whatever rate the cascade runs at (the OS rate in the plugin).
    struct Coeffs
    {
        float dcHp   [kNumStages] = {};  // one-pole HP coefficient (DC block)
        float shelfLp[kNumStages] = {};  // one-pole LP coefficient (shelf's LP arm)
        float shelfAmt[kNumStages] = {}; // (1 - linearGain) for the shelf
        float lp     [kNumStages] = {};  // one-pole LP coefficient (Miller)
        float biasOff[kNumStages] = {};  // tanh(bias), precomputed
    };

    inline void computeCoeffs (double sampleRate, Coeffs& c) noexcept
    {
        constexpr float twoPi = 6.283185307179586f;
        const float fs = (float) sampleRate;

        for (int i = 0; i < kNumStages; ++i)
        {
            c.dcHp[i]    = std::exp (-twoPi * kDcHpHz      / fs);
            c.shelfLp[i] = 1.0f - std::exp (-twoPi * kShelfHz[i] / fs);
            c.lp[i]      = 1.0f - std::exp (-twoPi * kLpHz[i]    / fs);

            // Low shelf as  y = x - amt * lowpass(x):
            //   at DC  -> y = x*(1-amt) = x*G     (the shelf depth)
            //   at HF  -> y = x                   (flat)
            const float g = std::pow (10.0f, kShelfDb[i] / 20.0f);
            c.shelfAmt[i] = 1.0f - g;

            c.biasOff[i] = std::tanh (kBias[i]);
        }
    }

    // Per-channel filter memory: a DC-block HP, a shelf LP and a Miller LP
    // for each stage.
    struct CascadeState
    {
        float hp      [kNumStages][2] = {};  // [stage][{xPrev, yPrev}]
        float shelfLp [kNumStages]    = {};
        float lp      [kNumStages]    = {};
        float satXPrev[kNumStages]    = {};  // ADAA: previous saturator INPUT per stage
        void reset() noexcept
        {
            for (auto& f : hp) { f[0] = 0.0f; f[1] = 0.0f; }
            for (auto& v : shelfLp)  v = 0.0f;
            for (auto& v : lp)       v = 0.0f;
            for (auto& v : satXPrev) v = 0.0f;
        }
    };

    // Asymmetric saturator. Subtracting tanh(bias) keeps f(0) = 0 so silence
    // stays silent; the DC the asymmetry generates on real signal is removed
    // by the NEXT stage's coupling-cap high-pass (and, for the last stage, by
    // BurnerCircuit's own 15.4 Hz input high-pass).
    inline float asymSat (float x, float drive, float bias, float biasOffset) noexcept
    {
        return std::tanh (drive * x + bias) - biasOffset;
    }

    // ---------------------------------------------------------------------
    // ANTIDERIVATIVE ANTI-ALIASING (ADAA).
    //
    // Four cascaded saturators running on plain oversampling alone measured
    // −17.9 dB of non-harmonic (aliasing) energy at 48 kHz — versus −54 dB for
    // Ember, which ADAAs its saturators. A 2 kHz tone at high gain was clearly
    // sample-rate dependent (−28.4 dB at 96 kHz, a 10.5 dB improvement), which
    // is the signature of real aliasing rather than noise. Brute-force
    // oversampling is the wrong lever here: each doubling buys only ~10 dB, so
    // reaching a clean floor would need 64-128x. ADAA buys ~40 dB at 8x.
    //
    //   f(x) = tanh(g·x + b) − tanh(b)
    //   F(x) = ln(cosh(g·x + b))/g − tanh(b)·x        (verified: F' = f)
    //   y    = [F(xₙ) − F(xₙ₋₁)] / (xₙ − xₙ₋₁)
    //
    // F is O(|x|) here (ln(cosh z) → |z| − ln2), so unlike Ember's old
    // second-order ADAA this expression is well conditioned in float32 — there
    // is no k³-scaled cancellation to blow up. The ε below still guards the
    // small-Δ case, where the midpoint fallback is the exact limit anyway.
    // ---------------------------------------------------------------------
    inline constexpr float kAdaaEps = 1.0e-3f;

    inline float asymSatAntideriv (float x, float drive, float bias, float biasOffset) noexcept
    {
        const float z  = drive * x + bias;
        const float az = std::abs (z);
        // numerically stable ln(cosh z)
        const float lnCosh = az + std::log1p (std::exp (-2.0f * az)) - 0.6931472f;
        return lnCosh / drive - biasOffset * x;
    }

    inline float asymSatADAA (float x, float& xPrev, float drive, float bias, float biasOffset) noexcept
    {
        const float dx = x - xPrev;
        const float y  = (std::abs (dx) < kAdaaEps)
                           ? asymSat (0.5f * (x + xPrev), drive, bias, biasOffset)
                           : (asymSatAntideriv (x,     drive, bias, biasOffset)
                            - asymSatAntideriv (xPrev, drive, bias, biasOffset)) / dx;
        xPrev = x;
        return y;
    }

    // One sample through all four stages.
    inline float processSample (float x, const float drives[kNumStages],
                                 const Coeffs& c, CascadeState& st) noexcept
    {
        auto hp = [] (float in, float coef, float* s) noexcept
        {
            const float y = coef * (s[1] + in - s[0]);
            s[0] = in;
            s[1] = y;
            return y;
        };
        auto lp = [] (float in, float coef, float& s) noexcept
        {
            s += coef * (in - s);
            return s;
        };

        float v = x;
        for (int i = 0; i < kNumStages; ++i)
        {
            v = hp (v, c.dcHp[i], st.hp[i]);                       // coupling cap / DC block
            v -= c.shelfAmt[i] * lp (v, c.shelfLp[i], st.shelfLp[i]); // cathode-bypass low shelf
            v  = asymSatADAA (v, st.satXPrev[i], drives[i],        // asymmetric clip, ADAA'd
                              kBias[i], c.biasOff[i]);
            v  = lp (v, c.lp[i], st.lp[i]);                        // Miller rolloff
        }
        return v;
    }
}
