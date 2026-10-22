#include "PluginProcessor.h"
#include "PluginEditor.h"

// ===========================================================================
// Loudness compensation — Burner and Blazer output level matched to Ember
// at comparable (12 o'clock default) settings. Measured directly
// (Tests/OfflineTests.cpp, measureLoudnessAtDefaults(), 220Hz test tone,
// each circuit at its corrected 12-o'clock defaults from ParameterRanges.h).
//
// NOTE: an earlier measurement (RMS ratios ~5x) was taken against Volume
// defaults that were themselves wrong — ParameterRanges.h's midpointDefault
// had the JUCE skew formula inverted (pow(0.5,skew) instead of the correct
// pow(0.5,1/skew)), which put Volume's actual default value near its
// maximum rather than its true 12-o'clock position. After that formula fix,
// re-measurement gives the real picture:
//
//   Ember  RMS = 0.0467
//   Burner RMS = 0.0397  (0.851x Ember) -> compensation = 1/0.851 = 1.175
//   Blazer RMS = 0.0428  (0.918x Ember) -> compensation = 1/0.918 = 1.089
//
// (Those figures are historical — see the re-measured values further down,
// taken after the post-Phase-11 level-fidelity revision changed Ember's
// reference level.)
//
// Approach: a simple fixed output-gain multiplier per circuit, applied
// after each circuit's own process() and before Output Trim (so Trim
// remains a normal +-12dB user control on top of the level-matched
// baseline, not fighting the compensation). A limiter was considered but
// rejected: these circuits already have their own diode-clipper ceiling,
// so a further limiter would double-compress and change the dynamic feel;
// a flat gain preserves each circuit's tone exactly, just at a matched
// level. Tone character is unaffected — this is pure output scaling.
// Lives here (not in BurnerCircuit.cpp/BlazerCircuit.cpp) since both are
// frozen; this is routing-level gain staging, not a circuit-model change.
// Re-measure and update these constants if either circuit's frozen state
// changes materials (i.e., a future signed-off exception) or if the
// corrected default values change.
// ===========================================================================
// Pre-clipper drive scaling — Ember's DriveStage has a variable 1-100× stage
// AND a fixed 100× stage, so its clipper sees 100-10000× the raw signal
// depending on the Drive knob. Cranking Ember Drive genuinely drives the
// clipper harder AND opens up the feedback resistor. Burner/Blazer's frozen
// clippers only vary the feedback-resistor ratio (softening clipping) when
// the Gain knob moves — the drive INTO the clipper stayed constant at a
// fixed pre-gain, which is why cranking their Gain knob felt "weak" and
// static compared to Ember HOT.
//
// FIX: pre-gain now scales linearly with the Gain knob position so
// cranking the knob genuinely drives the clipper harder (more saturation,
// more upper harmonics, more sustain from deeper compression), matching
// how cascaded preamp stages in a real high-gain amp behave.
//
// BURNER — CRITICAL ARCHITECTURE NOTE:
//   The frozen BurnerCircuit's diode-in-feedback clipper is a HARD LIMITER.
//   For ANY input above ~0.03 (normalized) it clamps the output to the same
//   ~0.44 square wave — so whatever you feed it, out comes an identical
//   buzzy square. That is why raising the pre-gain did nothing audible: the
//   diode was ERASING all upstream shaping. A hard square, scaled to match
//   Ember's level and with no cabinet smoothing, sounds thin/buzzy/weak —
//   the opposite of a big saturated high-gain amp.
//
//   FIX: feed the diode clipper a TINY signal (kBurnerDiodeFeed) so it stays
//   in its SOFT-KNEE region — it then acts as a gentle makeup/edge stage AND
//   still applies its tone-stack voicing (bass/mid/treble/aggression), while
//   the actual distortion is done by an oversampled, asymmetric cascade
//   upstream that we fully control. This is also the physically correct model
//   of the REVV Generator: a chain of cascaded tube gain stages, not a single
//   hard clip. The Gain knob now drives the cascade (live, wide sweep) AND
//   the diode's feedback-resistor makeup — so cranking it genuinely piles on
//   saturation and level.
//
//   Blazer keeps its knob-scaled diode pre-gain (its clipper is used more
//   conventionally; it was not the subject of the "weak" complaint).
static constexpr float kBurnerDiodeFeed = 0.06f; // keeps the diode in soft-knee

// ---------------------------------------------------------------------------
// Burner power-amp stage — now DYNAMIC (power-supply sag), not a static curve.
//
// WHY: the whole Burner path used to be memoryless — static waveshapers and
// LTI filters, with no time-varying element anywhere. That is the main reason
// it read as "a distortion pedal" rather than "an amp". Two consequences:
//
//   * No give. Stage 1 alone compresses ~3:1 (at Gain=0.5 a 14 dB input range
//     maps to ~4.7 dB out), and the later stages squeeze further, so a 20 dB
//     pick transient arrived at the output as roughly 1 dB. Modern metal tone
//     is defined by a percussive leading edge; a real amp's power section sags
//     AFTER the transient, letting the attack crack through first.
//   * The old `tanh(2x)/2` was near-inert anyway: self-levelling (unity small
//     signal) so it only compresses peaks, but by then the signal has been
//     saturated four times and its crest factor is ~1 — there were no peaks
//     left to compress.
//
// This matters MORE here than in a normal amp model because Burner drives a
// CLEAN, high-headroom amp (Morgan SW50R). That amp's power section is barely
// working, so it supplies no sag and no saturation of its own. If we want the
// cranked-power-section behaviour of a REVV Generator, the pedal has to be the
// one to provide it.
//
// MODEL:  headroom h = 1 - depth * env/(env + knee)   (envelope-tracked)
//         y = tanh(drive * x / h) * h / drive
// Small-signal gain is unity for any h, so this can't run away or need a
// matching makeup; what h controls is the CEILING. As the envelope rises the
// ceiling drops (the supply sags) and the stage compresses harder, then it
// recovers on the release. Attack is fast enough to follow a chug, release
// slow enough to "breathe" between them.
// ---------------------------------------------------------------------------
// Shared by Burner and Blazer (both are pedals into a clean amp, so neither
// gets power-section behaviour from downstream). Per-circuit drive below.
static constexpr float kPowerSagDepth     = 0.35f;  // headroom floor = 1 - depth
static constexpr float kPowerSagKnee      = 0.15f;  // envelope level at which half the sag is reached
static constexpr float kPowerSagAttackMs  = 15.0f;
static constexpr float kPowerSagReleaseMs = 200.0f;

static constexpr float kBurnerPowerDrive  = 2.5f;   // higher = denser/more saturated
static constexpr float kBlazerPowerDrive  = 2.5f;   // Blazer measured thin/weak — see below

// Runs the dynamic-sag power-amp stage over `buffer`, oversampled via `os`.
// `sagEnv` is the caller's persistent, STEREO-LINKED envelope state.
static void processPowerAmpStage (juce::AudioBuffer<float>& buffer,
                                   juce::dsp::Oversampling<float>& os,
                                   float& sagEnv, float sagAttack, float sagRelease,
                                   float drive)
{
    juce::dsp::AudioBlock<float> blk (buffer);
    auto up = os.processSamplesUp (blk);
    const size_t nCh  = up.getNumChannels();
    const size_t nS   = up.getNumSamples();
    const size_t nUse = juce::jmin (nCh, (size_t) 2);
    const float  invDrive = 1.0f / drive;

    float* chPtr[2] = { nullptr, nullptr };
    for (size_t ch = 0; ch < nUse; ++ch)
        chPtr[ch] = up.getChannelPointer (ch);

    float env = sagEnv;
    for (size_t i = 0; i < nS; ++i)
    {
        // STEREO-LINKED sag detector: one envelope for all channels, so the
        // sag can never shift the stereo image.
        float peak = 0.0f;
        for (size_t ch = 0; ch < nUse; ++ch)
            peak = juce::jmax (peak, std::abs (chPtr[ch][i]));

        const float coef = (peak > env) ? sagAttack : sagRelease;
        env = coef * env + (1.0f - coef) * peak;

        // Supply sags as the envelope rises: the ceiling drops, so the stage
        // compresses harder, then recovers.
        const float h    = 1.0f - kPowerSagDepth * (env / (env + kPowerSagKnee));
        const float invH = 1.0f / h;

        for (size_t ch = 0; ch < nUse; ++ch)
            chPtr[ch][i] = std::tanh (drive * chPtr[ch][i] * invH) * h * invDrive;
    }
    sagEnv = env;

    os.processSamplesDown (blk);
}

static inline float blazerPreGainForKnob (float gainNorm) noexcept
{
    return 1.0f + juce::jlimit (0.0f, 1.0f, gainNorm) * 14.0f;
}

// The Burner drive engine is the 3-stage, inter-stage-filtered cascade in
// BurnerCascade.h (shared with the offline test). See that header for the
// REVV-tightness rationale.

// Intentional "hotter than Ember" bias for Burner — the user wants it to
// read as more aggressive/metal than the Ember. ~+2 dB over the level-matched
// baseline. Applied on top of the measured loudness compensation.
static constexpr float kBurnerHotness = 1.26f;

// Post-circuit loudness compensation — measured via offline test suite
// (Tests/OfflineTests.cpp, measureLoudnessAtDefaults()) at 12 o'clock
// defaults with knob-scaled pre-gain (gainNorm=0.5 → Blazer preGain=8).
//
// RE-MEASURED after the post-Phase-11 level-fidelity revision. That revision
// made Ember +12 dB louder at defaults (U2A is now a real +6 dB recovery
// stage instead of a 0.5x attenuator — see OutputStage.cpp), while Burner and
// Blazer's raw default levels came out BIT-IDENTICAL to before (their volume
// taper fix was deliberately noon-preserving: 0.5^2 * 2.0 = 0.5, exactly the
// old linear map at 0.5). So only Ember's reference moved, and these two
// constants absorb that shift:
//
//   Ember  RMS = 0.1766  (was 0.0441 — 4.0x, the U2A recovery fix)
//   Burner RMS = 0.1189 (0.673x Ember, Red default) -> match comp = 1.485
//   Blazer RMS = 0.2079 (1.177x Ember)              -> comp = 0.849
//
// NOTE these are ROUTING-level product decisions (cross-circuit level
// matching so switching circuits doesn't blast the user), NOT circuit models.
// The circuits' own internal level staging is now physical and unscaled; this
// is the deliberate house-voicing layer on top. Re-measure if any circuit's
// internal staging changes again.
//
// kBurnerLoudnessComp is the level-MATCH value; the deliberate kBurnerHotness
// (+2 dB) is multiplied on top in the process path so Burner reads hotter.
// Re-measured after removing Ember's numerically-broken second-order ADAA
// (see ClippingStage.cpp) and restoring 8x oversampling — that raised Ember's
// reference level slightly (0.1784 -> 0.1879 RMS) because the ~39 dB of
// numerical noise it had been adding is gone. The loudness test also models
// the routing character EQ (Ember +3 dB treble shelf, Blazer +1.5 dB body
// peak) so these reflect the REAL signal path. Measured needs: Burner 1.580,
// Blazer 0.819. (Burner re-measured after the de-box revision — the broad
// −5.5 dB @600 Hz low-mid scoop cuts the 220 Hz test tone's 660 Hz harmonic,
// dropping its measured level, so the match gain rose to 1.963. This is a
// single-tone metric; on broadband material Burner is not 2.5 dB louder than
// before — re-check the cross-circuit switch level by ear.)
// AMP-mode comps. Derived from the K-WEIGHTED (ITU-R BS.1770) perceptual
// loudness of broadband pink noise through the full path — NOT from RMS of a
// single low sine, which is what these used to be and which is why Blazer
// shipped ~1.3 dB quiet while measuring "matched to 0.02%": a 220 Hz tone
// cannot see presence-band differences, and presence band is most of what
// the ear weights. See Check 9 in OfflineTests.
static constexpr float kBurnerLoudnessComp = 1.418f;
static constexpr float kBlazerLoudnessComp = 1.144f;

// DRIVE-mode comps. Bypassing the mid scoop / HF rolloff / sag changes each
// circuit's level by a DIFFERENT amount (Burner and Blazer lose more stages
// than Ember), so the cross-circuit balance has to be re-derived per mode
// rather than reusing the Amp numbers. Measured the same K-weighted way.
//
// Blazer's was 0.744 and is now 0.791. The number was never the real problem:
// integrated loudness alone cannot tell a level deficit from a spectral tilt.
// Blazer sat 2.5-4 dB under both others across 500 Hz-4 kHz while being the
// loudest of the three at 8 kHz, and since K-weighting boosts that region the
// match "passed" on fizz while the musical band stayed starved. The fix was to
// correct the SHAPE (keep the pedal's own 6 kHz rolloff in Drive, add a
// presence peak to replace the contrast the mid scoop used to provide) and
// only then re-derive the scalar. Check 10 now guards the band balance
// directly so a scalar can never paper over a tilt like this again.
static constexpr float kBurnerLoudnessCompDrive = 1.207f;
static constexpr float kBlazerLoudnessCompDrive = 0.791f;

// Overall trim applied in DRIVE mode so switching Amp<->Drive does not jump
// in level (removing a broad scoop makes the signal measurably louder).
static constexpr float kDriveModeTrim = 0.861f;

// Ember oversampling: back to 8×, matching Burner/Blazer.
//
// It had been dropped to 4× on the rationale that second-order ADAA gave
// |sinc|⁴ suppression and so bought back the missing octave. That ADAA2
// implementation has now been removed — it was numerically catastrophic in
// float32 and was the source of Ember's audible noise (see the long comment
// in ClippingStage.cpp). With both saturators back on well-conditioned
// FIRST-order ADAA, the alias suppression that 4× was leaning on no longer
// exists, so the extra octave of oversampling is restored to compensate.
// setOversamplingOrder still accepts 0=2x, 1=4x, 2=8x.
static constexpr int kFixedOsOrder = 2; // 8x

// ===========================================================================
// Peak metering for the UI's LevelMeter — instant attack, decayed release
// (multiply toward the new peak each block) so it reads like a real meter
// rather than jittering with every block's raw instantaneous value.
// ===========================================================================
static void updatePeakLevel (std::atomic<float>& stored, const juce::AudioBuffer<float>& buffer)
{
    float blockPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        blockPeak = juce::jmax (blockPeak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    const float previous = stored.load (std::memory_order_relaxed);
    const float decayed  = previous * 0.90f; // release
    stored.store (juce::jmax (blockPeak, decayed), std::memory_order_relaxed);
}

// ===========================================================================
// Parameter layout
// ===========================================================================

// This function runs once, during construction, to tell the APVTS which
// parameters exist and what their ranges/defaults are.
// The DAW reads this list to display automation lanes.
juce::AudioProcessorValueTreeState::ParameterLayout
SuhrRiotProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Drive (RV1, B100k linear taper on the real pedal).
    // 0.0 = minimum gain (~10x), 1.0 = maximum gain (~470x).
    // Linear in the parameter, but DriveStage applies a circuit-derived curve.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Tone (10kC reverse-log pot + 22n cap on the real pedal).
    // A simple variable treble cut: 0.0 = dark (LPF at ~723 Hz),
    // 1.0 = bright (LPF at ~12 kHz, effectively open).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Level (RV3, A10k audio taper on the real pedal).
    // OutputStage applies a level^2 curve to simulate the log taper.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "level", 1 }, "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    // Mode — the 3-way slide switch on the pedal, selects clipping diode set.
    // 0 = Germanium (1N34A, soft/warm),  1 = Silicon (1N914, tighter),
    // 2 = LED (red LEDs, high threshold, most dynamic headroom)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Germanium", "Silicon", "LED" }, 0));

    // Circuit — top-level pedal-model selector. 0=Ember (Suhr Riot),
    // 1=Burner (REVV G3, stub), 2=Blazer (Friedman BE-OD, stub).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "circuit", 1 }, "Circuit",
        juce::StringArray { "Ember", "Burner", "Blazer" }, 0));

    // Active (bypass) — true = effect engaged, false = hard bypass (dry pass-through).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "active", 1 }, "Active", true));

    // Oversampling is no longer a user-facing parameter — hardcoded to 8x
    // for all three circuits (see kFixedOsOrder below and BurnerCircuit.h/
    // BlazerCircuit.h's post-v2.0 exceptions for the newly-added Burner/
    // Blazer oversampling).

    // Input noise gate threshold.
    // -70 dBFS = essentially off;  -20 dBFS = very aggressive.
    // Default -50 dBFS sits above typical pickup/cable noise but below soft picks.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateThresh", 1 }, "Gate",
        juce::NormalisableRange<float> (-70.0f, -20.0f), -50.0f));

    // Input Trim — pickup-level matching before the drive chain.
    // 0 dB = unity, negative values attenuate (reduce saturation),
    // positive values push harder into the gain stage.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputTrim", 1 }, "Input Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    // Tight — 2nd-order Butterworth HPF cutoff placed before distortion.
    // Skewed range (skew=0.3) so most of the knob travel covers 20–150 Hz
    // where pickup low-end bloom is musically significant.
    // 20 Hz = fully open (flat response), 500 Hz = maximum tightness.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tightFreq", 1 }, "Tight",
        juce::NormalisableRange<float> (20.0f, 500.0f, 0.0f, 0.3f), 20.0f));

    // Output Trim — post-chain makeup gain.
    // Compensates for level changes caused by the other controls.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputTrim", 1 }, "Output Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    // -----------------------------------------------------------------------
    // Burner (REVV G3) parameters — ranges/tapers from getParameterRanges()
    // (Phase 4). Gain/Mid are linear (B1M/B100k pots feeding stages whose
    // own gain math is already logarithmic); Volume/Bass/Treble get an
    // audio-taper skew (A50k/A100k attenuators/shelves). See
    // ParameterRanges.h for the full rationale.
    // -----------------------------------------------------------------------
    const auto burnerRanges = getParameterRanges (CircuitMode::BURNER);

    auto makeBurnerParam = [&burnerRanges] (const char* paramId, const char* displayName)
    {
        const auto& r = burnerRanges.at (paramId);
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId, 1 }, displayName,
            juce::NormalisableRange<float> (r.min, r.max, 0.0f, r.skew), r.defaultValue);
    };

    params.push_back (makeBurnerParam ("burnerGain",   "Burner Gain"));
    params.push_back (makeBurnerParam ("burnerVolume", "Burner Volume"));
    params.push_back (makeBurnerParam ("burnerBass",   "Burner Bass"));
    params.push_back (makeBurnerParam ("burnerMid",    "Burner Mid"));
    params.push_back (makeBurnerParam ("burnerTreble", "Burner Treble"));
    // Not schematic-accurate — see ParameterRanges.h / BurnerCircuit.h.
    params.push_back (makeBurnerParam ("burnerTight",  "Burner Tight"));

    // Aggression — 2-position switch (SW1, DPDT ON-ON). 0 = one 2n2 cap
    // active, 1 = both 2n2 caps in parallel (4n4). See BurnerCircuit.h.
    // 0 = Blue (cooler/darker, -4 dB shelves, 0.85x drive), 1 = Red (hotter/
    // brighter, +5 dB shelves, 1.35x drive). Defaults to RED — Burner is the
    // high-gain/metal voice, so its out-of-box state should be the aggressive
    // one, not the tame Blue setting.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "burnerAggression", 1 }, "Aggression",
        juce::StringArray { "Blue", "Red" }, 1));

    // -----------------------------------------------------------------------
    // VOICING — global (applies to whichever circuit is active), 2 positions.
    //
    //   AMP   (default) — the shipped "amp in a box" voicing. Carries the
    //                     shaping a real amp would otherwise provide: the
    //                     shared 400 Hz tone-stack scoop, the extra HF
    //                     rolloff, and the power-amp sag stages. Use straight
    //                     into an IR / FRFR.
    //
    //   DRIVE          — pedal voicing, for going into a REAL clean amp.
    //                     Strips that amp-substitution shaping back out, so
    //                     the amp supplies the scoop, the top-end rolloff and
    //                     the power-section feel instead of us doing it twice.
    //                     Without this, running into an amp double-scoops and
    //                     double-rolls-off (the amp's tone stack + ours).
    //
    // Ember's treble shelf stays active in BOTH modes: it compensates Ember's
    // own schematic-accurate 1.6 kHz pre-clip LPF, which is a property of the
    // circuit, not a stand-in for an amp.
    // -----------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "voicing", 1 }, "Voicing",
        juce::StringArray { "Amp", "Drive" }, 0));

    // -----------------------------------------------------------------------
    // Blazer (Friedman BE-OD) parameters — ranges/tapers from
    // getParameterRanges() (Phase 9). Several are C-taper (reverse-audio):
    // Tight/Bass/Presence. See ParameterRanges.h for the full rationale.
    // -----------------------------------------------------------------------
    const auto blazerRanges = getParameterRanges (CircuitMode::BLAZER);

    auto makeBlazerParam = [&blazerRanges] (const char* paramId, const char* displayName)
    {
        const auto& r = blazerRanges.at (paramId);
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId, 1 }, displayName,
            juce::NormalisableRange<float> (r.min, r.max, 0.0f, r.skew), r.defaultValue);
    };

    params.push_back (makeBlazerParam ("blazerGain",     "Blazer Gain"));
    params.push_back (makeBlazerParam ("blazerVolume",   "Blazer Volume"));
    params.push_back (makeBlazerParam ("blazerBass",     "Blazer Bass"));
    params.push_back (makeBlazerParam ("blazerTreble",   "Blazer Treble"));
    params.push_back (makeBlazerParam ("blazerPresence", "Blazer Presence"));
    params.push_back (makeBlazerParam ("blazerTight",    "Blazer Tight"));
    params.push_back (makeBlazerParam ("blazerTrim",     "Blazer Trim"));

    return { params.begin(), params.end() };
}

// ===========================================================================
// Constructor / Destructor
// ===========================================================================

SuhrRiotProcessor::SuhrRiotProcessor()
    // Declare the bus layout: one stereo input, one stereo output.
    // Guitars are mono but most DAWs send stereo by default; we handle both.
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // The APVTS constructor needs: this processor, an undo manager (nullptr =
      // no undo), a state identifier string, and the parameter layout we built.
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

SuhrRiotProcessor::~SuhrRiotProcessor() {}

// ===========================================================================
// Lifecycle
// ===========================================================================

void SuhrRiotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Give each circuit a chance to allocate its internal buffers and
    // compute sample-rate-dependent values (filter coefficients, etc.).
    // All three are prepared regardless of the active mode, so switching
    // circuits mid-session never hits an unprepared object.
    emberCircuit .prepare (sampleRate, samplesPerBlock);
    burnerCircuit.prepare (sampleRate, samplesPerBlock);
    blazerCircuit.prepare (sampleRate, samplesPerBlock);

    circuitSwitchGain.reset (sampleRate, 0.008); // 8 ms fade across a circuit switch
    circuitSwitchGain.setCurrentAndTargetValue (1.0f);
    lastCircuitMode = -1;

    burnerBlazerInputTrim .prepare (sampleRate, samplesPerBlock);
    burnerBlazerOutputTrim.prepare (sampleRate, samplesPerBlock);
    burnerBlazerGate      .prepare (sampleRate, samplesPerBlock);

    // -----------------------------------------------------------------------
    // Character post-EQ filters — prepare all, set coefficients on the
    // fixed ones (see PluginProcessor.h for the reasoning behind each).
    // -----------------------------------------------------------------------
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    emberTrebleShelf.prepare (spec);
    emberToneBoost  .prepare (spec);
    sharedMidScoop  .prepare (spec);
    burnerGrindPeak .prepare (spec);
    burnerBodyPeak  .prepare (spec);
    burnerPresenceShelf.prepare (spec);
    blazerBodyPeak  .prepare (spec);
    blazerTopTame   .prepare (spec);
    blazerPresencePeak.prepare (spec);
    blazerWarmthShelf.prepare (spec);

    // Burner presence/bite: +3 dB high shelf @4.5 kHz. Raised from +1.5 dB —
    // the cascade now keeps more top (post-LPFs at 11/8.5/7 k) and this is the
    // REVV Purple "cut". The power-amp high-cut downstream keeps it from fizzing.
    // Presence/clarity: +4 dB high shelf @ 4 kHz. Raised from +3 dB and moved
    // down from 4.5 kHz so the shelf starts inside the definition/pick-attack
    // band (2-5 kHz) rather than only lifting the air above it — the player
    // reported Burner lacking BOTH clarity and brightness, which are two
    // different bands.
    // Presence/clarity: +4 dB PEAK @ 3.5 kHz, Q 0.9 — deliberately a peak,
    // NOT a shelf.
    //
    // As a high shelf this lifted everything above its corner by the full
    // +4 dB, including 8-16 kHz, which directly fought the Miller and OT
    // rolloffs and left Burner ~9 dB hotter than Ember up top. Clarity lives
    // at 2-5 kHz; a shelf cannot add it without also adding fizz. A peak puts
    // the gain where definition actually is (+3.9 dB @3.5k) and contributes
    // almost nothing at 16 kHz (+0.3 dB), so the top stays rolled off.
    *burnerPresenceShelf.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 3500.0f, 1.2f, juce::Decibels::decibelsToGain (1.0f));

    // Burner grind/gnarl: +3.5 dB peak @2.2 kHz, Q=1.1. This is the band the
    // REVV Purple's aggression/"chainsaw" cut lives in, and nothing was
    // boosting it before. Q kept moderate so it's an aggressive presence, not
    // an ice-pick.
    *burnerGrindPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 2200.0f, 1.1f, juce::Decibels::decibelsToGain (2.2f));

    // Burner power-amp emulation filters (see PluginProcessor.h). Tight HPF for
    // OT/power-section low-end firmness; high-cut for the OT top rolloff.
    // Subsonic/DC guard only — NOT a tone control. This was 90 Hz, which was
    // actively discarding the low end right where a guitar power amp + 4x12
    // has its resonance peak (~85-120 Hz). With a real amp (Morgan SW50R) and
    // an IR downstream, THEY own the final low-end shaping — our job is to
    // generate the content and hand it over intact, not to pre-remove it. We
    // deliberately do NOT add a resonance peak here either: the amp and IR
    // already provide one, and modelling a second would double up and boom.
    *burnerPowerHpf.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, 30.0f);
    // OT-style top rolloff — now SECOND-order at 9 kHz (was first-order, and
    // briefly 14 kHz). A real output transformer + speaker rolls off far
    // faster than 6 dB/oct, and a first-order filter simply cannot get the
    // extreme top down without also dulling 4 kHz. Second-order at 9 kHz
    // leaves 4 kHz almost untouched (−0.2 dB) while taking 16 kHz down
    // −10.4 dB, which is what stops us handing raw fizz to the amp
    // downstream. Measured target: match Ember's rolloff, the one circuit
    // that was reported as sounding right.
    *burnerPowerLpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 6200.0f, 0.707f);
    burnerPowerHpf.prepare (spec);
    burnerPowerLpf.prepare (spec);
    burnerPowerAmpOs.initProcessing (static_cast<size_t> (samplesPerBlock));
    burnerPowerAmpOs.reset();
    blazerPowerAmpOs.initProcessing (static_cast<size_t> (samplesPerBlock));
    blazerPowerAmpOs.reset();

    // Burner cascade oversampler — 8× around the 3 tanh preamp stages.
    burnerDriveOs.initProcessing (static_cast<size_t> (samplesPerBlock));
    burnerDriveOs.reset();

    // Cascade filter coefficients at the OVERSAMPLED rate (8×), and clear the
    // cascade's per-channel filter memory.
    burner::computeCoeffs (sampleRate * 16.0, burnerCascadeCoeffs);
    for (auto& st : burnerCascadeState)
        st.reset();

    // Power-amp sag envelope coefficients — computed at the power-amp stage's
    // OWN oversampled rate (4×), since the follower runs inside that loop.
    {
        const double paSr = sampleRate * 8.0;   // matches burner/blazerPowerAmpOs factor
        powerSagAttack  = std::exp (-1.0 / (paSr * kPowerSagAttackMs  * 0.001));
        powerSagRelease = std::exp (-1.0 / (paSr * kPowerSagReleaseMs * 0.001));
        burnerSagEnv     = 0.0f;
        blazerSagEnv     = 0.0f;
    }

    // Ember top-end recovery: +4.5 dB high shelf at 3 kHz.
    //
    // Ember's DriveStage has a schematic-accurate 1.6 kHz LPF ahead of the
    // clipper, and its tone stack is a LOW-PASS (treble cut only) — so nothing
    // in the circuit can add top back. Measured with pink noise it came out
    // genuinely dark: −2.6 dB at 4 kHz and −7.3 dB at 8 kHz relative to 1 kHz,
    // on top of a broad 500 Hz–2 kHz plateau. That combination (dark top +
    // mid plateau) is the "muddy" half of the complaint, as distinct from the
    // "boxy" half the shared mid scoop addresses.
    //
    // Raised from +3 dB / 2.5 kHz. It had been cut to +3 because it was
    // spotlighting hiss — but that hiss turned out to be the ill-conditioned
    // ADAA2 numerical bug (since removed; Ember's SNR is now 42.5 dB), so the
    // reason for keeping it low no longer applies.
    *emberTrebleShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (4.5f));

    // SHARED tone-stack mid scoop — see PluginProcessor.h for the full
    // rationale. Supersedes the old Burner-only "de-honk" filter, now gone.
    //
    // −11 dB @ 400 Hz, Q=1.0.
    //
    // Centre/width/depth are player-confirmed: an external EQ dip at 400 Hz
    // spanning ~300-500 Hz improved all three circuits.
    //
    // WIDTH IS THE CRITICAL PARAMETER HERE, not depth — this was retuned
    // after the player reported still needing another ~2 dB across 300-500
    // even at −10 dB. At Q=1.5 the notch is only ~0.96 octaves wide, so
    // 400 Hz got the full cut but 300 Hz and 500 Hz got barely half of it
    // (~−5.5 dB) — the honk being heard was the BAND EDGES, not the centre.
    // Q=1.0 widens the notch to ~1.4 octaves (half-depth points near 250 Hz
    // and 650 Hz), which deepens 300 and 500 Hz by ~2.5 dB without having to
    // make the centre absurdly deep. Depth nudged −10 → −11 to keep the
    // centre strong at the wider Q.
    //
    // Reasoning worth keeping for anyone retuning this:
    //   * A LOW SHELF is the wrong tool. "Mud" (200-400 Hz) and "weight"
    //     (80-160 Hz) are adjacent, and a shelf cuts both — it would undo the
    //     low-end fundamental/harmonic restoration in BurnerCascade.h.
    //   * Centring at 500 Hz and going broad drags 1 kHz down with it, so
    //     after level-matching the LOWS come back up relatively — the exact
    //     opposite of the goal.
    // 400 Hz / Q 1.5 keeps 63-125 Hz (weight) and 1 kHz+ (bark) intact.
    *sharedMidScoop.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 400.0f, 1.0f, juce::Decibels::decibelsToGain (-11.0f));

    // Burner chunk: gentle +1.5 dB @ 100 Hz, Q=1.2 — a TIGHT sub/low-mid thump
    // for weight without mud. Pulled back hard from the old +3.5 @150/Q1.1,
    // which (with the eased de-honk) was the source of the muddy/boxy low-mid
    // buildup vs the real amp. Heaviness now comes from the power-amp stage and
    // this tight low thump, not from a broad low-mid hump.
    *burnerBodyPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 100.0f, 1.2f, juce::Decibels::decibelsToGain (1.5f));

    // Blazer body: gentle +1.5 dB peak @250 Hz for palm-mute punch. Q=0.9.
    // Reduced from +3 dB — combined with the old +2 dB warmth shelf and the
    // hidden +6.8 dB bass-knob default it made Blazer "too fat". The real
    // BE-OD is tight/punchy, so this keeps a little low-mid firmness without
    // the woof.
    // Blazer MIDRANGE BARK: +3.5 dB @ 900 Hz, Q=0.9.
    //
    // Repurposed from a +1.5 dB @250 Hz "body" bump, which sat right in the
    // band the shared mid scoop is now cutting (it was fighting it) and did
    // nothing for the thing Blazer actually lacked. Blazer measured as the
    // thinnest and most top-heavy of the three (+5.2 dB @4k, +7.6 dB @8k re
    // 1 kHz) with no mid emphasis anywhere — and the circuit has no Mid
    // control at all (schematic-accurate: the BE-OD is Bass/Treble/Presence
    // only). A real BE-OD is a thick, mid-forward modded-Marshall voice, so
    // the missing bark is a model deficiency, not a faithful omission.
    *blazerBodyPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 900.0f, 0.9f, juce::Decibels::decibelsToGain (3.5f));

    // Blazer warmth shelf REMOVED (was +2 dB @150 Hz). It lifted the sub/low
    // end — the opposite of the BE-OD's tight character — and stacked with the
    // bass knob and body peak into an over-fat low end. blazerWarmthShelf is
    // now a unity pass-through (kept as a no-op to avoid touching the routing
    // structure); see PluginProcessor.h.
    // Blazer WEIGHT: +2 dB low shelf @ 180 Hz. Re-enabled (it had been
    // bypassed to unity when Blazer read as too fat). It is safe to bring
    // back now because the shared mid scoop removes the 300-500 Hz box that
    // made the old +2 dB @150 read as flab — this now adds authority BELOW
    // the scoop rather than thickening the boxy band, which is part of
    // answering "Blazer sounds weak".
    *blazerWarmthShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 180.0f, 0.707f, juce::Decibels::decibelsToGain (2.0f));

    // Blazer TOP TAME: −4 dB high shelf @ 3.5 kHz. Blazer is the only circuit
    // with no high-frequency rolloff anywhere — Burner has per-stage Miller
    // low-passes plus an OT-style cut, Ember has its 1.6 kHz pre-clip LPF and
    // a low-pass tone stack, but Blazer's diode clipper runs straight into the
    // tone stack with nothing taming its upper harmonics. Measured, that left
    // it +5.2 dB @4k / +7.6 dB @8k re 1 kHz: bright and thin, which reads as
    // "weak" and masks the midrange. A real pedal has coupling/Miller
    // capacitance doing this.
    // Blazer top rolloff — SECOND-ORDER LOW-PASS at 6 kHz, replacing a
    // −4 dB high SHELF at 3.5 kHz.
    //
    // The shelf was the wrong shape entirely: it plateaus at −4 dB and then
    // stops, so Blazer measured essentially FLAT out to 16 kHz (−0.5 dB @4k,
    // −1.5 @8k, −1.9 @16k relative to 1 kHz) — a hard diode clipper with no
    // progressive rolloff anywhere, handing full-level 12-16 kHz clipping
    // harmonics to whatever follows. That is the "unpleasant high
    // frequencies", and feeding it to an amp makes the amp alias too.
    //
    // A real pedal has coupling/Miller capacitance rolling the top off
    // continuously, so this runs in BOTH voicings. It had briefly been
    // AMP-only, on the reasoning that "the real amp supplies HF rolloff" —
    // which confused two different things. An amp's rolloff comes from its
    // power section and speaker, DOWNSTREAM of the pedal; this filter stands
    // in for the BE-OD's own coupling capacitance, which is a property of the
    // PEDAL. Ember's intrinsic 1.6 kHz pre-clip LPF was correctly kept in
    // Drive for exactly that reason; bypassing Blazer's was the inconsistency.
    // Measured, bypassing it left Blazer the LOUDEST of the three at 8 kHz
    // (+3.1 dB over Ember) while sitting 2.5-4 dB BELOW both others across
    // 500 Hz-4 kHz — and because K-weighting boosts the 8 kHz region, that
    // fizz surplus is what satisfied the loudness match, paying for a deficit
    // in the whole musical band. That is the "weak, and feels low-passed"
    // report: not actually dark, but starved of 2-4 kHz definition while
    // fizzing above it.
    *blazerTopTame.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate, 6000.0f, 0.707f);

    // Blazer PRESENCE — +4 dB peak @ 2.8 kHz, Q 0.9, DRIVE MODE ONLY.
    //
    // In AMP mode the shared −11 dB @400 Hz scoop is what gives every circuit
    // its mid-to-presence contrast. DRIVE drops that scoop (the real amp's
    // tone stack supplies it), and the three circuits do not survive its
    // removal equally: Ember still has its +4.5 dB @3 kHz shelf and Burner
    // its +2.2 @2.2k / +1.0 @3.5k peaks, but Blazer's entire character EQ is
    // a +3.5 dB @900 Hz bark and a +2 dB @180 Hz shelf — BOTH below the
    // presence band. It was the only circuit with no HF lift to fall back on,
    // so it alone lost all of its definition contrast when the scoop went.
    // Centred at 2.8 kHz to sit in the definition band rather than the fizz
    // region, and below the 6 kHz rolloff above so the two do not fight.
    *blazerPresencePeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 2800.0f, 0.9f, juce::Decibels::decibelsToGain (4.0f));

    // Reset tracker so the first block always initialises the tone-following
    // brightness filter correctly.
    lastEmberTone = -1.0f;
}

void SuhrRiotProcessor::releaseResources()
{
    // Flush all internal filter states and delay lines.
    // Called when playback stops or the plugin is unloaded.
    emberCircuit .reset();
    burnerCircuit.reset();
    blazerCircuit.reset();

    burnerBlazerInputTrim .reset();
    burnerBlazerOutputTrim.reset();
    burnerBlazerGate      .reset();

    emberTrebleShelf .reset();
    emberToneBoost   .reset();
    sharedMidScoop   .reset();
    burnerGrindPeak  .reset();
    burnerBodyPeak   .reset();
    burnerPresenceShelf.reset();
    burnerPowerHpf   .reset();
    burnerPowerLpf   .reset();
    burnerPowerAmpOs .reset();
    blazerBodyPeak   .reset();
    blazerWarmthShelf.reset();
    blazerTopTame    .reset();
    blazerPresencePeak.reset();
    blazerPowerAmpOs .reset();
    burnerDriveOs    .reset();
    for (auto& st : burnerCascadeState)
        st.reset();
    burnerSagEnv = 0.0f;
    blazerSagEnv = 0.0f;
}

// ===========================================================================
// Audio processing — the hot path, called by the DAW ~every ~5-20 ms.
// No memory allocation, no locks, no UI calls allowed here.
// ===========================================================================

void SuhrRiotProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& /*midiMessages*/)
{
    // Prevents denormal floats (values extremely close to zero) from causing
    // CPU spikes in some processors. Good practice in any audio plugin.
    juce::ScopedNoDenormals noDenormals;

    // Hard bypass: when inactive, leave the buffer untouched (dry pass-through).
    if (apvts.getRawParameterValue ("active")->load() < 0.5f)
    {
        inputPeakLevel.store (0.0f, std::memory_order_relaxed);
        outputPeakLevel.store (0.0f, std::memory_order_relaxed);
        return;
    }

    updatePeakLevel (inputPeakLevel, buffer);

    // Read current parameter values from the APVTS.
    // getRawParameterValue returns a pointer to an atomic<float>, which is
    // safe to read on the audio thread without locking.
    const float drive       = apvts.getRawParameterValue ("drive")      ->load();
    const float tone        = apvts.getRawParameterValue ("tone")       ->load();
    const float level       = apvts.getRawParameterValue ("level")      ->load();
    const int   mode        = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const float inputTrimDb = apvts.getRawParameterValue ("inputTrim")  ->load();
    const float tightFreq   = apvts.getRawParameterValue ("tightFreq")  ->load();
    const float outTrimDb   = apvts.getRawParameterValue ("outputTrim") ->load();
    const float gateThresh  = apvts.getRawParameterValue ("gateThresh")->load();
    // Global voicing: 0 = Amp (amp-in-a-box, ships as-is), 1 = Drive (pedal
    // into a real amp — strips the amp-substitution shaping). See the
    // "voicing" parameter registration for the full rationale.
    const bool  driveMode   = apvts.getRawParameterValue ("voicing")->load() > 0.5f;
    const auto  circuitMode = static_cast<CircuitMode> (
                                   static_cast<int> (apvts.getRawParameterValue ("circuit")->load()));

    // Reset all three circuits on a mode switch — an inactive circuit's
    // filters otherwise freeze mid-state while un-called and resume from
    // stale state on reactivation, causing an audible discontinuity
    // (Phase 5 finding). This is a discrete, rare event (user clicking the
    // switch), not a per-block cost.
    if (static_cast<int> (circuitMode) != lastCircuitMode)
    {
        emberCircuit.reset();
        burnerCircuit.reset();
        blazerCircuit.reset();
        burnerBlazerInputTrim.reset();
        burnerBlazerOutputTrim.reset();
        burnerBlazerGate.reset();

        // Character filters have IIR state — flush on switch so we don't
        // resume a filter chain mid-decay against a freshly-reset circuit.
        emberTrebleShelf .reset();
        emberToneBoost   .reset();
        sharedMidScoop   .reset();
        burnerGrindPeak  .reset();
        burnerBodyPeak   .reset();
        burnerPresenceShelf.reset();
        burnerPowerHpf   .reset();
        burnerPowerLpf   .reset();
        burnerPowerAmpOs .reset();
        blazerBodyPeak   .reset();
        blazerWarmthShelf.reset();
        blazerTopTame    .reset();
        blazerPresencePeak.reset();
        blazerPowerAmpOs .reset();
        burnerDriveOs    .reset();
        for (auto& st : burnerCascadeState)
            st.reset();
        burnerSagEnv = 0.0f;
        blazerSagEnv = 0.0f;

        lastCircuitMode = static_cast<int> (circuitMode);

        // Fade the output in over ~8ms to mask the filter-startup transient
        // a cold reset causes against an already-live input signal.
        circuitSwitchGain.setCurrentAndTargetValue (0.0f);
        circuitSwitchGain.setTargetValue (1.0f);
    }

    // Route to the active circuit only — the other two sit idle this block.
    juce::dsp::AudioBlock<float> characterBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> characterCtx (characterBlock);

    switch (circuitMode)
    {
        case CircuitMode::EMBER:
        {
            emberCircuit.process (buffer, drive, tone, level, mode,
                                   inputTrimDb, tightFreq, outTrimDb,
                                   kFixedOsOrder, gateThresh);

            // Fixed treble compensation (always on). Restores the top-end
            // the frozen DriveStage's 1.6 kHz LPF strips before clipping.
            emberTrebleShelf.process (characterCtx);

            // Tone-following extra brightness: additive, only above tone=0.5.
            // Real Riot's tone control is a variable LPF that only cuts, so
            // "0.5 = neutral, 1.0 = extra bright" mirrors user expectation
            // without changing behavior below noon.
            const float toneAboveNoon = juce::jmax (0.0f, tone - 0.5f) * 2.0f; // 0..1
            if (std::abs (tone - lastEmberTone) > 0.001f)
            {
                const float boostDb = toneAboveNoon * 6.0f; // up to +6 dB at max
                *emberToneBoost.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    getSampleRate(), 4000.0f, 0.707f, juce::Decibels::decibelsToGain (boostDb));
                lastEmberTone = tone;
            }
            if (toneAboveNoon > 0.001f)
                emberToneBoost.process (characterCtx);

            // NOTE: the mode-dependent "character" post-EQ that used to sit
            // here was retired in the Ember fidelity revision. It existed to
            // FAKE a difference between CLASSIC and HOT, which were identical
            // in the old clipper (both red-LED, differing only by an invented
            // pre-gain). ClippingStage now selects three genuinely different
            // diode strings per mode (WARM germanium / CLASSIC asymmetric
            // silicon / HOT red LED), so the voicing difference — including
            // CLASSIC's even-harmonic asymmetry and each mode's natural
            // loudness — comes from the physics, not a cosmetic shelf.
            break;
        }

        case CircuitMode::BURNER:
        {
            burnerBlazerInputTrim.setTrimDb (inputTrimDb);
            burnerBlazerInputTrim.process (buffer);
            burnerBlazerGate.setThreshold (gateThresh);
            burnerBlazerGate.process (buffer);

            const float burnerGainKnob = apvts.getRawParameterValue ("burnerGain")->load();

            // Cascaded high-gain preamp (oversampled 8×) — the primary gain
            // engine. Three tanh stages with progressive inter-stage high-pass
            // filtering (see BurnerCascade.h) give the tight, articulate,
            // dense REVV saturation a lone diode clamp can't. Drive scales
            // with the Gain knob so the whole sweep is alive.
            {
                float drives[burner::kNumStages];
                burner::fillDrives (burnerGainKnob, drives);
                juce::dsp::AudioBlock<float> driveBlock (buffer);
                auto up = burnerDriveOs.processSamplesUp (driveBlock);
                const size_t nCh = up.getNumChannels();
                const size_t nS  = up.getNumSamples();
                for (size_t ch = 0; ch < nCh; ++ch)
                {
                    float* d = up.getChannelPointer (ch);
                    auto&  st = burnerCascadeState[juce::jmin (static_cast<int> (ch), 1)];
                    for (size_t i = 0; i < nS; ++i)
                        d[i] = burner::processSample (d[i], drives, burnerCascadeCoeffs, st);
                }
                burnerDriveOs.processSamplesDown (driveBlock);
            }

            // Tiny diode feed — keeps the frozen diode clipper in its soft-knee
            // region so the cascade's character survives instead of being
            // flattened into the diode's hard square. The diode still applies
            // its tone-stack voicing and adds gentle edge.
            buffer.applyGain (kBurnerDiodeFeed);

            burnerCircuit.setGain       (burnerGainKnob);
            burnerCircuit.setVolume     (apvts.getRawParameterValue ("burnerVolume")    ->load());
            burnerCircuit.setBass       (apvts.getRawParameterValue ("burnerBass")      ->load());
            burnerCircuit.setMid        (apvts.getRawParameterValue ("burnerMid")       ->load());
            burnerCircuit.setTreble     (apvts.getRawParameterValue ("burnerTreble")    ->load());
            burnerCircuit.setAggression (static_cast<int> (apvts.getRawParameterValue ("burnerAggression")->load()));
            burnerCircuit.setTight      (apvts.getRawParameterValue ("burnerTight")     ->load());
            burnerCircuit.process (buffer);

            // Post-circuit character EQ: de-honk (800 Hz) -> grind/gnarl
            // (2.2 kHz) -> palm-mute thump (150 Hz) -> presence/bite (4.5 kHz).
            burnerGrindPeak    .process (characterCtx);
            burnerBodyPeak     .process (characterCtx);
            burnerPresenceShelf.process (characterCtx);

            // Power-amp emulation (see header): tight HPF -> gentle self-
            // levelling tanh saturation (oversampled 4×) -> OT high-cut. Stands
            // in for the REVV Generator's power section, which the clean amp
            // downstream doesn't supply — adds density, compression and thump.
            // Power-amp stage + OT rolloff are AMP-mode only: in Drive the
            // real amp downstream supplies its own power section and top-end
            // rolloff, and doing both double-compresses and double-dulls.
            // The subsonic HPF stays in both (it is a DC guard, not voicing).
            burnerPowerHpf.process (characterCtx);
            if (! driveMode)
            {
                processPowerAmpStage (buffer, burnerPowerAmpOs, burnerSagEnv,
                                      powerSagAttack, powerSagRelease, kBurnerPowerDrive);
                burnerPowerLpf.process (characterCtx);
            }

            buffer.applyGain ((driveMode ? kBurnerLoudnessCompDrive : kBurnerLoudnessComp)
                              * kBurnerHotness);

            burnerBlazerOutputTrim.setTrimDb (outTrimDb);
            burnerBlazerOutputTrim.process (buffer);
            break;
        }

        case CircuitMode::BLAZER:
        {
            burnerBlazerInputTrim.setTrimDb (inputTrimDb);
            burnerBlazerInputTrim.process (buffer);
            burnerBlazerGate.setThreshold (gateThresh);
            burnerBlazerGate.process (buffer);

            const float blazerGainKnob = apvts.getRawParameterValue ("blazerGain")->load();
            buffer.applyGain (blazerPreGainForKnob (blazerGainKnob));

            blazerCircuit.setGain     (blazerGainKnob);
            blazerCircuit.setVolume   (apvts.getRawParameterValue ("blazerVolume")  ->load());
            blazerCircuit.setBass     (apvts.getRawParameterValue ("blazerBass")    ->load());
            blazerCircuit.setTreble   (apvts.getRawParameterValue ("blazerTreble")  ->load());
            blazerCircuit.setPresence (apvts.getRawParameterValue ("blazerPresence")->load());
            blazerCircuit.setTight    (apvts.getRawParameterValue ("blazerTight")   ->load());
            blazerCircuit.setTrim     (apvts.getRawParameterValue ("blazerTrim")    ->load());
            blazerCircuit.process (buffer);

            // Character: midrange bark (900 Hz) -> weight (180 Hz shelf).
            // These are the BE-OD's own voicing and stay in both modes.
            blazerBodyPeak   .process (characterCtx);
            blazerWarmthShelf.process (characterCtx);

            // Top rolloff is PEDAL-intrinsic (coupling capacitance) so it runs
            // in BOTH voicings — see prepareToPlay for why bypassing it in
            // Drive was what made Blazer read weak and dull.
            blazerTopTame.process (characterCtx);

            if (driveMode)
            {
                // No shared mid scoop in Drive, so Blazer needs its own
                // presence contrast — it is the one circuit whose character EQ
                // has no HF lift. See prepareToPlay.
                blazerPresencePeak.process (characterCtx);
            }
            else
            {
                // Power-amp sag is AMP-mode only: in Drive the real amp
                // supplies its own power section (see Burner above).
                processPowerAmpStage (buffer, blazerPowerAmpOs, blazerSagEnv,
                                      powerSagAttack, powerSagRelease, kBlazerPowerDrive);
            }

            buffer.applyGain (driveMode ? kBlazerLoudnessCompDrive : kBlazerLoudnessComp);

            burnerBlazerOutputTrim.setTrimDb (outTrimDb);
            burnerBlazerOutputTrim.process (buffer);
            break;
        }
    }

    // SHARED tone-stack mid scoop — applied to every circuit, after the
    // switch, so all three get identical treatment (the boxiness was
    // identical on all three) and so the cross-circuit loudness compensation
    // stays valid: a shared EQ cancels in the circuit-to-circuit ratio.
    // See PluginProcessor.h for why this is needed at all.
    // AMP mode only: this is the stand-in for an amp tone stack's inherent
    // 400 Hz notch. In DRIVE the real amp supplies it, and applying both
    // double-scoops (the reported "scooped af" through a clean amp).
    if (! driveMode)
        sharedMidScoop.process (characterCtx);
    else
        buffer.applyGain (kDriveModeTrim);   // keep Amp<->Drive level-matched

    // Apply the switch fade (a no-op multiply-by-1 once settled).
    if (circuitSwitchGain.isSmoothing())
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = circuitSwitchGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    updatePeakLevel (outputPeakLevel, buffer);
}

// ===========================================================================
// Preset save / load
// ===========================================================================

void SuhrRiotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialise the entire APVTS state to XML, then to a binary blob that
    // the DAW embeds in the project file.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SuhrRiotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Reverse of getStateInformation: deserialise the blob and restore state.
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ===========================================================================
// Editor factory
// ===========================================================================

juce::AudioProcessorEditor* SuhrRiotProcessor::createEditor()
{
    return new SuhrRiotEditor (*this);
}

// ===========================================================================
// JUCE entry point — tells the DAW how to instantiate this plugin.
// Must be in exactly one .cpp file.
// ===========================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuhrRiotProcessor();
}
