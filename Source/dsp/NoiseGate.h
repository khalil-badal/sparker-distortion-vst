#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// NoiseGate — peak-tracking envelope follower, STEREO-LINKED, with hysteresis,
// a hold stage, and smoothed gain reduction. Attack -> Hold -> Release, the
// same structure real hardware/commercial gates use (dbx, ISP Decimator,
// Waves TrueGate, etc.) — see the two AUDIT notes below for why each stage
// exists; this is the result of an audit against that reference design.
//
// Placed at the front of the chain (after InputTrim) so it prevents idle
// noise from entering the high-gain drive stage and being amplified + clipped.
//
// Default threshold: -65 dBFS  (covers most guitar pickups' noise floor)
// Attack     :  1 ms  (fast enough to not clip the note attack)
// Env release: 15 ms  (how fast the DETECTOR tracks a level drop — see the
//              kEnvReleaseMs comment; this is deliberately much faster than a
//              gate's audible closing speed, which is set by Hold + the
//              separate gain-close smoothing below, not by this constant)
// Hysteresis : opens at threshold, closes kHysteresisDb below it — a
//              dead-band against chatter from small envelope dips near the
//              threshold.
// Hold       : kHoldMs — once open, the gate ignores the close condition for
//              this long every time the envelope dips back below the close
//              threshold (re-armed for as long as the signal stays live).
//
// AUDIT FINDING 1 — STEREO LINK (fixed here): the previous implementation ran
// an entirely independent envelope + open/close decision PER CHANNEL. Every
// commercial gate links its channels (one detector, one decision, applied to
// every channel identically) precisely because independent per-channel gating
// lets L/R open and close at very slightly different instants the moment the
// channels aren't perfectly bit-identical — collapsing or smearing the stereo
// image every time the gate moves. This plugin currently feeds identical L/R
// (mono guitar, duplicated), so the bug was silent, but it is a genuine defect
// that fires the instant anything upstream ever decorrelates the channels
// even slightly. Fixed by detecting the peak ACROSS channels (a single
// envelope/open/hold state) and applying the resulting gain to every channel.
//
// AUDIT FINDING 2 — NO HOLD TIME (fixed here): with only a static hysteresis
// dead-band and no temporal hold, a decaying or vibrato'd note can cross back
// and forth through the threshold zone several times before it finally dies
// out — each crossing is a full open/close cycle, i.e. audible gate flutter.
// This is the single most common gap between a hobbyist gate and a commercial
// one; real designs always add a Hold stage for exactly this reason. Kept
// deliberately short (25 ms, not the 50-100+ ms some hardware defaults to) to
// limit how much it can lengthen the amplified-noise tail on a high-gain
// downstream chain (Ember) — see the EmberCircuit noise-tail history below.
// ---------------------------------------------------------------------------
class NoiseGate
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    // Re-apply the per-sample gate gain computed by the most recent process()
    // call to ANOTHER buffer (same block, any channel count). Used to gate a
    // circuit's OUTPUT with the envelope detected at its INPUT: in a very
    // high-gain chain (Ember) the preamp amplifies the input noise floor by
    // 40-80 dB whenever the gate is open, so gating only at the input leaves
    // an audible, amplified noise tail during the gate's release/hold.
    // Applying the same gain at the output silences that tail directly.
    // No-op if the destination block is larger than the traced block.
    void applyGateToOutput (juce::AudioBuffer<float>& buffer) const noexcept;

    // threshold: -70 dBFS = essentially off, -30 dBFS = very aggressive
    void setThreshold (float db) noexcept { thresholdDb = db; }

private:
    double sr          = 44100.0;
    float  thresholdDb = -65.0f;

    // STEREO-LINKED state: one envelope, one open/hold/gain decision for the
    // whole buffer (see AUDIT FINDING 1 above) — not per-channel.
    float envelope    = 0.0f;
    float gateGain    = 0.0f;
    bool  gateOpen    = false;
    int   holdCounter = 0;      // samples remaining before a "below threshold" reading is allowed to close the gate

    // Per-sample gain trace from the last process() call (mono — the same
    // gain applies to every channel), so applyGateToOutput can re-apply it to
    // a downstream buffer of any channel count.
    juce::AudioBuffer<float> gainTrace;

    // Smoothing coefficients / sample counts (computed in prepare())
    float envAttack   = 0.0f;
    float envRelease  = 0.0f;
    float gainOpen    = 0.0f;
    float gainClose   = 0.0f;
    int   holdSamples = 0;

    static constexpr float kEnvAttackMs   =   1.0f;
    // Detection-envelope release. The peak follower decays MULTIPLICATIVELY,
    // so from a loud note (~0 dBFS) down to a quiet noise floor (~-60 dBFS) a
    // 100 ms release took ~500 ms to fall below the gate threshold — the gate
    // stayed open that whole time and a high-gain downstream chain (Ember)
    // kept amplifying the noise floor (the audible "it's noisy then the gate
    // kills it" tail). 15 ms tracks the input drop promptly; how audibly fast
    // the gate then actually closes is governed by Hold + the separate
    // gain-close smoothing below, not by this constant.
    static constexpr float kEnvReleaseMs  =  15.0f;
    static constexpr float kGainOpenMs    =   1.0f;
    static constexpr float kGainCloseMs   =  50.0f;

    // Hysteresis: the gate OPENS at the threshold but only starts its
    // close/hold sequence once the envelope falls this many dB below it — a
    // dead-band that stops the gate chattering open/closed while the envelope
    // hovers near the threshold (raking/palm-muting). Kept SMALL (-3 dB): a
    // wide dead-band was found to hold the gate OPEN on a noise floor sitting
    // between the close and open thresholds, so after a note the gate never
    // re-closed and a high-gain downstream chain (Ember) kept amplifying that
    // noise (an audible held-open tail). A 3 dB band still de-chatters while
    // closing tightly on the noise floor; the Hold stage below now carries
    // most of the anti-chatter duty (see AUDIT FINDING 2).
    static constexpr float kHysteresisDb  =  -3.0f;

    // Hold: once open, a reading below the close threshold does not close the
    // gate immediately — it starts this countdown instead, which is reset
    // back to full every time the envelope is seen above the close threshold
    // again. This bridges the brief, repeated threshold-crossings a decaying
    // or vibrato'd note produces into a single clean close instead of
    // flutter (see AUDIT FINDING 2). 25 ms is deliberately short relative to
    // common hardware defaults (many sit at 50-100+ ms) to limit how much it
    // can lengthen the amplified-noise tail on a high-gain downstream chain.
    static constexpr float kHoldMs        =  25.0f;
};
