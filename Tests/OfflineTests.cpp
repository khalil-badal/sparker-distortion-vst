// ===========================================================================
// OfflineTests — Phase 5 integration testing, run against the actual DSP
// classes directly (EmberCircuit / BurnerCircuit / BlazerCircuit), not the
// plugin GUI. Produces objective measurements, not eyeballed judgments.
// Not linked into the plugin — separate console-app CMake target.
// ===========================================================================
#include <JuceHeader.h>
#include "dsp/EmberCircuit.h"
#include "dsp/BurnerCircuit.h"
#include "dsp/BlazerCircuit.h"
#include "BurnerCascade.h"
#include "dsp/ToneStack.h"

namespace
{
    constexpr double kSr          = 48000.0;
    constexpr int    kBlockSize   = 512;
    constexpr int    kNumBlocks   = 20; // ~213 ms of audio, enough to settle filters

    int failures = 0;

    // Burner routing constants — must match PluginProcessor.cpp.
    constexpr float kBurnerDiodeFeed = 0.06f;
    constexpr float kBurnerHotness   = 1.26f;

    // Runs the shared 3-stage cascade (burner::) over a block at BASE rate.
    // Oversampling is omitted here — it changes only aliasing, not the
    // fundamental's RMS, so the loudness measurement stays accurate. State
    // persists across blocks via the caller-owned `st` array.
    inline void applyBurnerCascade (juce::AudioBuffer<float>& buf, float gainNorm,
                                     const burner::Coeffs& coeffs,
                                     burner::CascadeState st[2])
    {
        float drives[burner::kNumStages];
        burner::fillDrives (gainNorm, drives);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            float* d = buf.getWritePointer (ch);
            auto&  s = st[juce::jmin (ch, 1)];
            for (int i = 0; i < buf.getNumSamples(); ++i)
                d[i] = burner::processSample (d[i], drives, coeffs, s);
        }
    }

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  PASS: " << description << "\n";
        }
        else
        {
            std::cout << "  FAIL: " << description << "\n";
            ++failures;
        }
    }

    juce::AudioBuffer<float> makeToneBlock (float freqHz, float amplitude, int numSamples,
                                             double sampleRate, double& phase)
    {
        juce::AudioBuffer<float> buf (2, numSamples);
        const double inc = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = amplitude * (float) std::sin (phase);
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
            phase += inc;
        }
        return buf;
    }

    float rms (const juce::AudioBuffer<float>& buf)
    {
        double sum = 0.0;
        int n = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i) { sum += (double) d[i] * d[i]; ++n; }
        }
        return (float) std::sqrt (sum / juce::jmax (1, n));
    }

    bool hasNonFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i])) return true;
        }
        return false;
    }

    // Band energy via a simple one-pole/two-pole filter applied to a copy of
    // the signal, then RMS of the filtered result — good enough for a
    // relative "did this band change" comparison, not a calibrated analyzer.
    // Hann-windowed single-bin DFT magnitude at one frequency (channel 0).
    // The window suppresses spectral leakage from neighbouring harmonics (the
    // test block is not an integer number of cycles at the fundamental), so a
    // harmonic bin reads its own energy cleanly rather than its neighbours'.
    // (Window coherent-gain 0.5 is compensated so the number reads ~amplitude.)
    float magnitudeAt (const juce::AudioBuffer<float>& buf, double sr, float freqHz)
    {
        const int   n = buf.getNumSamples();
        const float* d = buf.getReadPointer (0);
        double re = 0.0, im = 0.0;
        const double w = 2.0 * juce::MathConstants<double>::pi * (double) freqHz / sr;
        for (int i = 0; i < n; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            re += win * d[i] * std::cos (w * i);
            im += win * d[i] * std::sin (w * i);
        }
        return (float) (2.0 * std::sqrt (re * re + im * im) / (0.5 * (double) n));
    }

    float bandEnergy (juce::AudioBuffer<float> buf, double sr, float loHz, float hiHz)
    {
        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> hp, lp;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) buf.getNumSamples(), (juce::uint32) buf.getNumChannels() };
        hp.prepare (spec);
        lp.prepare (spec);
        *hp.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, loHz);
        *lp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, hiHz);

        juce::dsp::AudioBlock<float> block (buf);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hp.process (ctx);
        lp.process (ctx);
        return rms (buf);
    }

    // Routing-level "character" post-EQ that PluginProcessor.cpp applies AFTER
    // each circuit — must be modelled here so the loudness comps are measured
    // through the real signal path, not the bare circuit. Kept in sync with
    // PluginProcessor::prepareToPlay's fixed-coefficient block.
    struct CharacterEQ
    {
        using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
        IIRDup emberTreble, blazerBody, blazerWarm, blazerTop, blazerPres, midScoop;
        IIRDup bGrowl, bGrind, bBody, bPres, bPowHp, bPowLp;   // Burner character + power-amp
        // Power-amp sag — must match PluginProcessor.cpp. Mirrored at BASE
        // rate here (the plugin runs it at 4x); the time constants are far
        // longer than a sample either way, so the RMS this feeds into the
        // loudness match is accurate.
        static constexpr float kPowerDrive = 2.5f;
        static constexpr float kSagDepth   = 0.35f;
        static constexpr float kSagKnee    = 0.15f;
        float sagEnv = 0.0f, sagEnvBz = 0.0f, sagAtt = 0.0f, sagRel = 0.0f;
        bool  skipPowerAmp = false;   // test hook: isolate the power-amp stage
        bool  driveMode    = false;   // DRIVE voicing: no OT/top rolloff, no sag
        CharacterEQ (double sr, int bs)
        {
            sagAtt = (float) std::exp (-1.0 / (sr * 15.0  * 0.001));
            sagRel = (float) std::exp (-1.0 / (sr * 200.0 * 0.001));
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
            for (auto* f : { &emberTreble, &blazerBody, &blazerWarm, &blazerTop, &blazerPres, &midScoop, &bGrowl, &bGrind, &bBody, &bPres, &bPowHp, &bPowLp })
                f->prepare (spec);
            *emberTreble.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf  (sr, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (4.5f));
            *midScoop.state    = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 400.0f, 1.0f, juce::Decibels::decibelsToGain (-11.0f));
            *blazerBody.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 900.0f,  0.9f,   juce::Decibels::decibelsToGain (3.5f));
            *blazerWarm.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 180.0f,  0.707f, juce::Decibels::decibelsToGain (2.0f));
            *blazerTop.state   = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 6000.0f, 0.707f);
            *blazerPres.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2800.0f, 0.9f, juce::Decibels::decibelsToGain (4.0f));
            // Burner chain — kept in sync with PluginProcessor::prepareToPlay.
            *bGrowl.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 600.0f,  0.7f,  juce::Decibels::decibelsToGain (-1.5f));
            *bGrind.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2200.0f, 1.1f,  juce::Decibels::decibelsToGain ( 2.2f));
            *bBody.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 100.0f,  1.2f,  juce::Decibels::decibelsToGain ( 1.5f));
            *bPres.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3500.0f, 1.2f,   juce::Decibels::decibelsToGain ( 1.0f));
            *bPowHp.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 30.0f);
            *bPowLp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 6200.0f, 0.707f);
        }
        static void run (IIRDup& f, juce::AudioBuffer<float>& b) { juce::dsp::AudioBlock<float> ab (b); juce::dsp::ProcessContextReplacing<float> c (ab); f.process (c); }
        void ember  (juce::AudioBuffer<float>& b) { run (emberTreble, b); }
        void scoop  (juce::AudioBuffer<float>& b) { run (midScoop, b); }   // shared, applied to all circuits
        void blazer (juce::AudioBuffer<float>& b) { run (blazerBody, b); run (blazerWarm, b); run (blazerTop, b);
                                                   if (driveMode) run (blazerPres, b);
                                                   powerAmp (b, sagEnvBz); }
        // Dynamic-sag power amp, mirroring processPowerAmpStage() in the plugin.
        void powerAmp (juce::AudioBuffer<float>& b, float& env)
        {
            if (skipPowerAmp || driveMode) return;
            const float k = kPowerDrive, invK = 1.0f / k;
            const int nCh = juce::jmin (b.getNumChannels(), 2);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float peak = 0.0f;
                for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, std::abs (b.getSample (ch, i)));
                const float c = (peak > env) ? sagAtt : sagRel;
                env = c * env + (1.0f - c) * peak;
                const float h = 1.0f - kSagDepth * (env / (env + kSagKnee));
                const float invH = 1.0f / h;
                for (int ch = 0; ch < nCh; ++ch)
                    b.setSample (ch, i, std::tanh (k * b.getSample (ch, i) * invH) * h * invK);
            }
        }
        void burner (juce::AudioBuffer<float>& b) // full Burner character + power-amp (base-rate; RMS-accurate for loudness)
        {
            run (bGrind, b); run (bBody, b); run (bPres, b);   // (bGrowl removed: superseded by the shared mid scoop)
            run (bPowHp, b);
            powerAmp (b, sagEnv);   // dynamic sag, mirrors processPowerAmpStage()
            if (! driveMode) run (bPowLp, b);
        }
    };
}

// ===========================================================================
// Check 2: Ember's three internal voicing modes are intact and distinguishable
// ===========================================================================
void testEmberVoicingModes()
{
    std::cout << "\n[Check 2] Ember voicing modes (WARM/CLASSIC/HOT) intact and distinguishable\n";

    float rmsByMode[3];
    float even2ndRatioByMode[3];   // 2nd-harmonic / fundamental (asymmetry proxy)
    bool  finiteByMode[3] = { true, true, true };

    for (int mode = 0; mode < 3; ++mode)
    {
        EmberCircuit ember;
        ember.prepare (kSr, kBlockSize);
        double phase = 0.0;
        juce::AudioBuffer<float> last;

        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (220.0f, 0.5f, kBlockSize, kSr, phase);
            ember.process (block, /*drive*/0.7f, /*tone*/0.5f, /*level*/0.7f, mode,
                           /*inputTrimDb*/0.0f, /*tightFreq*/20.0f, /*outTrimDb*/0.0f,
                           /*osOrder*/2, /*gateThresh*/-70.0f);
            if (hasNonFinite (block)) finiteByMode[mode] = false;
            last = block;
        }
        rmsByMode[mode] = rms (last);

        // 2nd harmonic (440 Hz) relative to fundamental (220 Hz), measured at a
        // MODERATE operating point (low input, low drive). Even-order harmonics
        // from asymmetric clipping are a moderate-drive phenomenon: when the
        // fixed 100x drive stage slams the clipper into a hard square (as at the
        // hot level/distinctness settings above), amplitude asymmetry collapses
        // into pure DC that the DC blocker removes, so ALL modes look symmetric.
        // At a realistic guitar level the two half-cycles clip with genuinely
        // different shapes, and CLASSIC's asymmetry shows as real 2nd harmonic.
        {
            EmberCircuit e2;
            e2.prepare (kSr, kBlockSize);
            double ph = 0.0;
            juce::AudioBuffer<float> lastMod;
            for (int b = 0; b < kNumBlocks; ++b)
            {
                auto blk = makeToneBlock (220.0f, 0.06f, kBlockSize, kSr, ph);
                e2.process (blk, /*drive*/0.15f, /*tone*/0.5f, /*level*/0.7f, mode,
                            0.0f, 20.0f, 0.0f, 2, -70.0f);
                lastMod = blk;
            }
            const float fund = magnitudeAt (lastMod, kSr, 220.0f);
            const float h2   = magnitudeAt (lastMod, kSr, 440.0f);
            even2ndRatioByMode[mode] = h2 / juce::jmax (1.0e-6f, fund);
        }
    }

    check (finiteByMode[0] && finiteByMode[1] && finiteByMode[2],
           "no NaN/Inf produced by any Ember voicing mode");

    // Modes must be distinguishable from each other (not literally identical
    // output), and none should be silent (which would indicate a broken path).
    const float epsilon = 0.001f;
    check (rmsByMode[0] > epsilon && rmsByMode[1] > epsilon && rmsByMode[2] > epsilon,
           "all three modes produce non-silent output");
    check (std::abs (rmsByMode[0] - rmsByMode[1]) > epsilon ||
           std::abs (rmsByMode[1] - rmsByMode[2]) > epsilon ||
           std::abs (rmsByMode[0] - rmsByMode[2]) > epsilon,
           "modes are not all numerically identical (distinguishable)");

    std::cout << "  RMS: WARM=" << rmsByMode[0] << " CLASSIC=" << rmsByMode[1]
               << " HOT=" << rmsByMode[2] << "\n";

    // -----------------------------------------------------------------------
    // Ember CLIP-VOICING FIDELITY revision: the three modes now select three
    // genuinely different diode strings (WARM germanium ~0.65 V, CLASSIC
    // asymmetric silicon ~0.6/1.2 V, HOT red LED ~1.70 V). Because the clipper
    // is a shunt clamp whose output peak IS the conducting diode's forward
    // voltage, loudness must rise monotonically WARM < CLASSIC < HOT, and each
    // mode must be clearly distinct from its neighbours (no two collapsing to
    // the same output the way CLASSIC/HOT did before this revision).
    // -----------------------------------------------------------------------
    check (rmsByMode[0] < rmsByMode[1] && rmsByMode[1] < rmsByMode[2],
           "mode loudness rises monotonically WARM < CLASSIC < HOT (diode knees)");

    const float classicOverWarm = rmsByMode[1] / juce::jmax (1.0e-6f, rmsByMode[0]);
    const float hotOverWarm     = rmsByMode[2] / juce::jmax (1.0e-6f, rmsByMode[0]);
    // Each neighbour separated by a clear margin (>10% RMS) — proves the modes
    // are physically distinct diode strings, not the old "two identical LED
    // modes" situation.
    check (classicOverWarm > 1.10f && (rmsByMode[2] / rmsByMode[1]) > 1.10f,
           "each mode is clearly distinct from its neighbours (>10% RMS apart)");

    // HOT (red LED, 1.70 V) vs WARM (germanium, 0.65 V): the outer ratio
    // should track the diode Vf ratio 1.70/0.65 = 2.615 (+8.35 dB). Generous
    // tolerance — RMS on a saturated signal, not peak. 2.0-3.2x confirms it.
    check (hotOverWarm > 2.0f && hotOverWarm < 3.2f,
           "HOT/WARM loudness ratio tracks the LED/Ge Vf ratio (~2.6x, +8.35 dB)");

    std::cout << "  CLASSIC/WARM = " << classicOverWarm << "x, "
               << "HOT/WARM = " << hotOverWarm
               << "x (" << (20.0f * std::log10 (hotOverWarm)) << " dB), "
               << "LED/Ge Vf ratio = " << (1.70f / 0.65f) << "x\n";

    // -----------------------------------------------------------------------
    // The core of the "less Big Muff" fix: CLASSIC uses ASYMMETRIC diodes, so
    // it must generate a substantially stronger 2nd harmonic than the two
    // SYMMETRIC modes (WARM/HOT). A Big Muff is symmetric — pure odd harmonics
    // — so this even-harmonic content is exactly what differentiates the Riot
    // model from a Muff. Require CLASSIC's 2nd/1st ratio to clearly exceed both
    // symmetric modes'.
    // -----------------------------------------------------------------------
    check (even2ndRatioByMode[1] > 2.0f * even2ndRatioByMode[0] &&
           even2ndRatioByMode[1] > 2.0f * even2ndRatioByMode[2],
           "CLASSIC (asymmetric) generates far more 2nd harmonic than symmetric WARM/HOT");

    std::cout << "  2nd/1st harmonic ratio: WARM=" << even2ndRatioByMode[0]
               << " CLASSIC=" << even2ndRatioByMode[1]
               << " HOT=" << even2ndRatioByMode[2] << "\n";
}

// ===========================================================================
// Check 3: Burner's two Aggression positions produce an audible difference
// ===========================================================================
void testBurnerAggressionStates()
{
    std::cout << "\n[Check 3] Burner Aggression states 0 vs 1 — audible difference\n";

    float rmsByState[2];
    float highBandByState[2];
    bool  finiteByState[2] = { true, true };

    for (int state = 0; state < 2; ++state)
    {
        BurnerCircuit burner;
        burner.prepare (kSr, kBlockSize);
        burner.setGain (0.6f);
        burner.setVolume (0.7f);
        burner.setBass (0.5f);
        burner.setMid (0.5f);
        burner.setTreble (0.5f);
        burner.setAggression (state);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (220.0f, 0.3f, kBlockSize, kSr, phase);
            burner.process (block);
            if (hasNonFinite (block)) finiteByState[state] = false;
            last = block;
        }
        rmsByState[state]      = rms (last);
        highBandByState[state] = bandEnergy (last, kSr, 3000.0f, 20000.0f);
    }

    check (finiteByState[0] && finiteByState[1], "no NaN/Inf in either Aggression state");

    const float relDiff = std::abs (rmsByState[0] - rmsByState[1]) /
                           juce::jmax (1.0e-6f, juce::jmax (rmsByState[0], rmsByState[1]));
    const float highBandRelDiff = std::abs (highBandByState[0] - highBandByState[1]) /
                           juce::jmax (1.0e-6f, juce::jmax (highBandByState[0], highBandByState[1]));

    // "Clearly audible" — require at least a few % relative difference in
    // overall level or high-band content between the two states.
    check (relDiff > 0.02f || highBandRelDiff > 0.02f,
           "Aggression states differ by >2% in RMS or high-band energy (audible)");

    std::cout << "  RMS: state0=" << rmsByState[0] << " state1=" << rmsByState[1]
               << " (rel diff " << (relDiff * 100.0f) << "%)\n";
    std::cout << "  High-band (3kHz+): state0=" << highBandByState[0] << " state1=" << highBandByState[1]
               << " (rel diff " << (highBandRelDiff * 100.0f) << "%)\n";
}

// ===========================================================================
// Check 4: Baxandall Bass/Treble and active Mid respond correctly in Burner
// ===========================================================================
void testBurnerToneStack()
{
    std::cout << "\n[Check 4] Burner Bass/Treble/Mid respond correctly\n";

    auto renderWith = [] (float bass, float mid, float treble, float toneHz) -> juce::AudioBuffer<float>
    {
        BurnerCircuit burner;
        burner.prepare (kSr, kBlockSize);
        burner.setGain (0.3f);   // light drive so tone-shaping dominates over clipping
        burner.setVolume (0.7f);
        burner.setBass (bass);
        burner.setMid (mid);
        burner.setTreble (treble);
        burner.setAggression (0);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (toneHz, 0.2f, kBlockSize, kSr, phase);
            burner.process (block);
            last = block;
        }
        return last;
    };

    // Bass: low-tone (80 Hz) low-band energy should increase from bass=0 to bass=1.
    {
        auto lo = renderWith (0.0f, 0.5f, 0.5f, 80.0f);
        auto hi = renderWith (1.0f, 0.5f, 0.5f, 80.0f);
        const float loBand = bandEnergy (lo, kSr, 20.0f, 200.0f);
        const float hiBand = bandEnergy (hi, kSr, 20.0f, 200.0f);
        check (hiBand > loBand, "Bass knob: low-band energy increases as Bass goes 0 -> 1");
        std::cout << "  Bass low-band: bass=0 -> " << loBand << ", bass=1 -> " << hiBand << "\n";
    }

    // Treble: high-tone (4000 Hz) high-band energy should increase from treble=0 to treble=1.
    {
        auto lo = renderWith (0.5f, 0.5f, 0.0f, 4000.0f);
        auto hi = renderWith (0.5f, 0.5f, 1.0f, 4000.0f);
        const float loBand = bandEnergy (lo, kSr, 2000.0f, 20000.0f);
        const float hiBand = bandEnergy (hi, kSr, 2000.0f, 20000.0f);
        check (hiBand > loBand, "Treble knob: high-band energy increases as Treble goes 0 -> 1");
        std::cout << "  Treble high-band: treble=0 -> " << loBand << ", treble=1 -> " << hiBand << "\n";
    }

    // Mid: mid-tone (800 Hz) should show increased level as Mid goes 0 -> 1.
    {
        auto lo = renderWith (0.5f, 0.0f, 0.5f, 800.0f);
        auto hi = renderWith (0.5f, 1.0f, 0.5f, 800.0f);
        const float loLevel = rms (lo);
        const float hiLevel = rms (hi);
        check (hiLevel > loLevel, "Mid knob: 800 Hz tone level increases as Mid goes 0 -> 1");
        std::cout << "  Mid @800Hz RMS: mid=0 -> " << loLevel << ", mid=1 -> " << hiLevel << "\n";
    }
}

// ===========================================================================
// Phase 10 Check 5: Blazer TIGHT produces an audible low-end change
// pre-distortion (measured on the low-band energy of the final output,
// since TIGHT's whole purpose is controlling how much bass reaches the
// clipper — its effect should be visible downstream).
// ===========================================================================
void testBlazerTight()
{
    std::cout << "\n[Check 5] Blazer TIGHT — audible low-end change\n";

    auto renderWith = [] (float tight) -> juce::AudioBuffer<float>
    {
        BlazerCircuit blazer;
        blazer.prepare (kSr, kBlockSize);
        blazer.setGain (0.5f);
        blazer.setVolume (0.7f);
        blazer.setBass (0.5f); blazer.setTreble (0.5f); blazer.setPresence (0.5f);
        blazer.setTight (tight);
        blazer.setTrim (0.5f);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (80.0f, 0.3f, kBlockSize, kSr, phase); // low fundamental
            blazer.process (block);
            last = block;
        }
        return last;
    };

    auto open  = renderWith (0.0f); // fully open — no HPF action
    auto tight = renderWith (1.0f); // max tight — strong pre-clip HPF

    const bool anyNonFinite = hasNonFinite (open) || hasNonFinite (tight);
    check (! anyNonFinite, "no NaN/Inf across Tight sweep");

    const float openLowBand  = bandEnergy (open,  kSr, 20.0f, 150.0f);
    const float tightLowBand = bandEnergy (tight, kSr, 20.0f, 150.0f);
    const float relDiff = std::abs (openLowBand - tightLowBand) /
                          juce::jmax (1.0e-6f, juce::jmax (openLowBand, tightLowBand));

    check (openLowBand > tightLowBand && relDiff > 0.05f,
           "Tight reduces low-band content by >5% going 0 -> 1 (audible)");
    std::cout << "  Low-band (20-150Hz) @80Hz fundamental: tight=0 -> " << openLowBand
               << ", tight=1 -> " << tightLowBand << " (rel diff " << (relDiff * 100.0f) << "%)\n";
}

// ===========================================================================
// Phase 10 Check 6: Blazer Bass/Treble/Presence respond and interact
// ===========================================================================
void testBlazerToneStack()
{
    std::cout << "\n[Check 6] Blazer Bass/Treble/Presence respond correctly\n";

    auto renderWith = [] (float bass, float treble, float presence, float toneHz) -> juce::AudioBuffer<float>
    {
        BlazerCircuit blazer;
        blazer.prepare (kSr, kBlockSize);
        blazer.setGain (0.3f); // light drive so tone-shaping dominates over clipping
        blazer.setVolume (0.7f);
        blazer.setBass (bass); blazer.setTreble (treble); blazer.setPresence (presence);
        blazer.setTight (0.0f);
        blazer.setTrim (0.5f);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (toneHz, 0.2f, kBlockSize, kSr, phase);
            blazer.process (block);
            last = block;
        }
        return last;
    };

    // Bass: low-tone (80 Hz) low-band energy should increase 0 -> 1.
    {
        auto lo = renderWith (0.0f, 0.5f, 0.5f, 80.0f);
        auto hi = renderWith (1.0f, 0.5f, 0.5f, 80.0f);
        const float loBand = bandEnergy (lo, kSr, 20.0f, 200.0f);
        const float hiBand = bandEnergy (hi, kSr, 20.0f, 200.0f);
        check (hiBand > loBand, "Bass knob: low-band energy increases as Bass goes 0 -> 1");
        std::cout << "  Bass low-band: bass=0 -> " << loBand << ", bass=1 -> " << hiBand << "\n";
    }

    // Treble: mid-high tone (2500 Hz, at the Treble shelf corner) should increase 0 -> 1.
    {
        auto lo = renderWith (0.5f, 0.0f, 0.5f, 2500.0f);
        auto hi = renderWith (0.5f, 1.0f, 0.5f, 2500.0f);
        const float loBand = bandEnergy (lo, kSr, 1500.0f, 20000.0f);
        const float hiBand = bandEnergy (hi, kSr, 1500.0f, 20000.0f);
        check (hiBand > loBand, "Treble knob: high-band energy increases as Treble goes 0 -> 1");
        std::cout << "  Treble high-band: treble=0 -> " << loBand << ", treble=1 -> " << hiBand << "\n";
    }

    // Presence: very-high tone (6000 Hz, at the Presence shelf corner,
    // above Treble's 2kHz corner) should increase 0 -> 1, distinct from Treble.
    {
        auto lo = renderWith (0.5f, 0.5f, 0.0f, 6000.0f);
        auto hi = renderWith (0.5f, 0.5f, 1.0f, 6000.0f);
        const float loBand = bandEnergy (lo, kSr, 4000.0f, 20000.0f);
        const float hiBand = bandEnergy (hi, kSr, 4000.0f, 20000.0f);
        check (hiBand > loBand, "Presence knob: very-high-band energy increases as Presence goes 0 -> 1");
        std::cout << "  Presence high-band: pres=0 -> " << loBand << ", pres=1 -> " << hiBand << "\n";
    }

    // Interaction: Presence's effect at 6kHz should be distinguishable from
    // Treble's effect at 6kHz (they shouldn't be the exact same filter).
    {
        auto trebleHi   = renderWith (0.5f, 1.0f, 0.0f, 6000.0f);
        auto presenceHi = renderWith (0.5f, 0.0f, 1.0f, 6000.0f);
        const float trebleBand   = bandEnergy (trebleHi,   kSr, 4000.0f, 20000.0f);
        const float presenceBand = bandEnergy (presenceHi, kSr, 4000.0f, 20000.0f);
        check (std::abs (trebleBand - presenceBand) > 1.0e-4f,
               "Treble and Presence produce distinguishable shaping at 6kHz (not the same filter)");
        std::cout << "  @6kHz: Treble-only=" << trebleBand << ", Presence-only=" << presenceBand << "\n";
    }
}

// ===========================================================================
// Phase 10 Check 7: Blazer internal Trim adjusts the gain floor
// ===========================================================================
void testBlazerTrim()
{
    std::cout << "\n[Check 7] Blazer Trim — adjusts gain floor\n";

    auto renderWith = [] (float trim) -> juce::AudioBuffer<float>
    {
        BlazerCircuit blazer;
        blazer.prepare (kSr, kBlockSize);
        blazer.setGain (0.0f); // minimum gain knob — isolates Trim's floor contribution
        blazer.setVolume (0.7f);
        blazer.setBass (0.5f); blazer.setTreble (0.5f); blazer.setPresence (0.5f);
        blazer.setTight (0.0f);
        blazer.setTrim (trim);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (220.0f, 0.3f, kBlockSize, kSr, phase);
            blazer.process (block);
            last = block;
        }
        return last;
    };

    auto lo = renderWith (0.0f);
    auto hi = renderWith (1.0f);

    check (! hasNonFinite (lo) && ! hasNonFinite (hi), "no NaN/Inf across Trim sweep");

    const float loLevel = rms (lo);
    const float hiLevel = rms (hi);
    check (hiLevel > loLevel, "Trim increases output level as it goes 0 -> 1 (raises the gain floor)");
    std::cout << "  RMS @minimum Gain: trim=0 -> " << loLevel << ", trim=1 -> " << hiLevel << "\n";
}

// ===========================================================================
// Check 1: switching circuits mid-stream — no NaN/Inf, no large discontinuity.
// Mirrors PluginProcessor's actual fix: reset all circuits on a mode change
// AND apply an 8ms linear fade-in afterward (resetting alone was measured
// insufficient — a cold-zero filter state against a live input caused its
// own startup transient; see Phase 5 findings).
// ===========================================================================
void testCircuitSwitching()
{
    std::cout << "\n[Check 1] Switching between circuits (Ember/Burner/Blazer) — stability + discontinuity check\n";

    EmberCircuit  ember;
    BurnerCircuit burner;
    BlazerCircuit blazer;
    ember.prepare (kSr, kBlockSize);
    burner.prepare (kSr, kBlockSize);
    blazer.prepare (kSr, kBlockSize);
    burner.setGain (0.6f); burner.setVolume (0.7f); burner.setBass (0.5f);
    burner.setMid (0.5f);  burner.setTreble (0.5f); burner.setAggression (0);
    blazer.setGain (0.5f); blazer.setVolume (0.7f); blazer.setBass (0.5f);
    blazer.setTreble (0.5f); blazer.setPresence (0.5f); blazer.setTight (0.0f); blazer.setTrim (0.5f);

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> switchGain;
    switchGain.reset (kSr, 0.008);
    switchGain.setCurrentAndTargetValue (1.0f);

    auto applyFade = [&switchGain] (juce::AudioBuffer<float>& block)
    {
        if (! switchGain.isSmoothing()) return;
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            const float g = switchGain.getNextValue();
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
                block.getWritePointer (ch)[i] *= g;
        }
    };

    double phase = 0.0;
    bool anyNonFinite = false;
    float lastBurnerBlockLastSample = 0.0f;
    float reactivatedFirstSample    = 0.0f;

    for (int b = 0; b < 10; ++b)
    {
        auto block = makeToneBlock (220.0f, 0.4f, kBlockSize, kSr, phase);
        burner.process (block);
        applyFade (block);
        if (hasNonFinite (block)) anyNonFinite = true;
        if (b == 9) lastBurnerBlockLastSample = block.getSample (0, kBlockSize - 1);
    }

    // Switch to Ember — reset + fade, matching PluginProcessor.
    ember.reset(); burner.reset();
    switchGain.setCurrentAndTargetValue (0.0f);
    switchGain.setTargetValue (1.0f);
    for (int b = 0; b < 10; ++b)
    {
        auto block = makeToneBlock (220.0f, 0.4f, kBlockSize, kSr, phase);
        ember.process (block, 0.5f, 0.5f, 0.5f, 0, 0.0f, 20.0f, 0.0f, 2, -70.0f);
        applyFade (block);
        if (hasNonFinite (block)) anyNonFinite = true;
    }

    // Switch to Blazer — reset + fade.
    ember.reset(); burner.reset(); blazer.reset();
    switchGain.setCurrentAndTargetValue (0.0f);
    switchGain.setTargetValue (1.0f);
    for (int b = 0; b < 10; ++b)
    {
        auto block = makeToneBlock (220.0f, 0.4f, kBlockSize, kSr, phase);
        blazer.process (block);
        applyFade (block);
        if (hasNonFinite (block)) anyNonFinite = true;
    }

    // Switch back to Burner — reset + fade again.
    ember.reset(); burner.reset(); blazer.reset();
    switchGain.setCurrentAndTargetValue (0.0f);
    switchGain.setTargetValue (1.0f);
    {
        auto block = makeToneBlock (220.0f, 0.4f, kBlockSize, kSr, phase);
        burner.process (block);
        applyFade (block);
        if (hasNonFinite (block)) anyNonFinite = true;
        reactivatedFirstSample = block.getSample (0, 0);
    }

    check (! anyNonFinite, "no NaN/Inf across Ember->Burner->Blazer->Burner switch sequence");

    // The fade starts at gain 0, so the very first reactivated sample must
    // be at or extremely close to zero — this is what actually masks the
    // click, regardless of the raw circuit output's transient.
    check (std::abs (reactivatedFirstSample) < 0.01f,
           "fade-in masks the switch transient (first sample ~silent)");

    std::cout << "  last Burner sample before switch-away = " << lastBurnerBlockLastSample
               << ", first Burner sample after switch-back (post-reset+fade) = " << reactivatedFirstSample << "\n";
}

// ===========================================================================
// Loudness matching measurement: renders all three circuits at their 12
// o'clock defaults with an identical test tone and reports raw RMS, so a
// fixed output compensation gain for Burner/Blazer can be derived from
// actual measurement rather than guessed. See PluginProcessor.cpp for
// where the resulting gains are applied.
// ===========================================================================
void measureLoudnessAtDefaults()
{
    std::cout << "\n[Loudness measurement] Ember vs Burner vs Blazer at 12 o'clock defaults\n";

    constexpr float testToneHz  = 220.0f;
    constexpr float testAmpl    = 0.3f;

    float emberRms = 0.0f, burnerRms = 0.0f, blazerRms = 0.0f;

    CharacterEQ charEmber (kSr, kBlockSize), charBlazer (kSr, kBlockSize), charBurner (kSr, kBlockSize);

    {
        EmberCircuit ember;
        ember.prepare (kSr, kBlockSize);
        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            // Ember's actual APVTS defaults: drive/tone/level=0.5, mode=0,
            // inputTrim=0dB, tightFreq=20Hz (fully open), outputTrim=0dB,
            // osOrder=2 (8x), gateThresh=-50dB.
            ember.process (block, 0.5f, 0.5f, 0.5f, 0, 0.0f, 20.0f, 0.0f, 2, -50.0f);
            charEmber.ember (block); // routing treble shelf (+3 dB)
            last = block;
        }
        emberRms = rms (last);
    }

    // Pre-gain factors from PluginProcessor.cpp — must stay in sync.
    // Burner uses the tiny kBurnerDiodeFeed (cascade carries the gain);
    // Blazer preGain = 1 + 0.5*14 = 8 at 12 o'clock.
    const float kBlazerPreGain = 1.0f + 0.5f * 14.0f;

    {
        BurnerCircuit burner;
        burner.prepare (kSr, kBlockSize);
        // New defaults (ParameterRanges.h): all linear taper, 0.5 = flat EQ.
        burner.setGain (0.5f);
        burner.setVolume (0.5f);
        burner.setBass (0.5f);
        burner.setMid (0.5f);
        burner.setTreble (0.5f);
        burner.setAggression (1); // Red is now the default

        burner::Coeffs bcoef; burner::computeCoeffs (kSr, bcoef);
        burner::CascadeState cs[2];
        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            applyBurnerCascade (block, 0.5f, bcoef, cs);
            block.applyGain (kBurnerDiodeFeed);
            burner.process (block);
            charBurner.burner (block); // character EQ + power-amp stage
            last = block;
        }
        burnerRms = rms (last);
    }

    {
        BlazerCircuit blazer;
        blazer.prepare (kSr, kBlockSize);
        const float reverseDefault = std::pow (0.5f, 1.0f / 2.857f);
        blazer.setGain (0.5f);
        blazer.setVolume (0.5f);
        blazer.setBass (0.5f);            // NEW default: linear, flat at noon (was reverseDefault +6.8 dB)
        blazer.setTreble (0.5f);
        blazer.setPresence (0.5f);
        blazer.setTight (reverseDefault);
        blazer.setTrim (0.5f);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            block.applyGain (kBlazerPreGain);
            blazer.process (block);
            charBlazer.blazer (block); // routing body peak (+1.5 dB; warmth now unity)
            last = block;
        }
        blazerRms = rms (last);
    }

    std::cout << "  Raw RMS @220Hz, 12 o'clock defaults: Ember=" << emberRms
               << " Burner=" << burnerRms << " Blazer=" << blazerRms << "\n";
    std::cout << "  Burner/Ember ratio = " << (burnerRms / emberRms)
               << " -> needed Burner gain = " << (emberRms / burnerRms) << "\n";
    std::cout << "  Blazer/Ember ratio = " << (blazerRms / emberRms)
               << " -> needed Blazer gain = " << (emberRms / blazerRms) << "\n";
}

// ===========================================================================
// Regression check: with the compensation gains applied (matching
// PluginProcessor.cpp's kBurnerLoudnessComp/kBlazerLoudnessComp), Burner
// and Blazer RMS at 12 o'clock defaults should land close to Ember's.
// ===========================================================================
void testLoudnessMatch()
{
    std::cout << "\n[Loudness check] Burner/Blazer match Ember within tolerance after compensation\n";

    constexpr float testToneHz = 220.0f;
    constexpr float testAmpl   = 0.3f;
    const float kBlazerPreGain          = 1.0f + 0.5f * 14.0f;
    constexpr float kBurnerLoudnessComp = 1.418f; // must match PluginProcessor.cpp
    constexpr float kBlazerLoudnessComp = 1.144f; // must match PluginProcessor.cpp

    float emberRms = 0.0f, burnerRms = 0.0f, blazerRms = 0.0f;
    CharacterEQ charEmber (kSr, kBlockSize), charBlazer (kSr, kBlockSize), charBurner (kSr, kBlockSize);

    {
        EmberCircuit ember;
        ember.prepare (kSr, kBlockSize);
        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            ember.process (block, 0.5f, 0.5f, 0.5f, 0, 0.0f, 20.0f, 0.0f, 2, -50.0f);
            charEmber.ember (block);
            last = block;
        }
        emberRms = rms (last);
    }

    {
        BurnerCircuit burner;
        burner.prepare (kSr, kBlockSize);
        burner.setGain (0.5f); burner.setVolume (0.5f); burner.setBass (0.5f);
        burner.setMid (0.5f); burner.setTreble (0.5f); burner.setAggression (1); // Red default

        burner::Coeffs bcoef; burner::computeCoeffs (kSr, bcoef);
        burner::CascadeState cs[2];
        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            applyBurnerCascade (block, 0.5f, bcoef, cs);
            block.applyGain (kBurnerDiodeFeed);
            burner.process (block);
            charBurner.burner (block); // character EQ + power-amp stage
            block.applyGain (kBurnerLoudnessComp * kBurnerHotness);
            last = block;
        }
        burnerRms = rms (last);
    }

    {
        BlazerCircuit blazer;
        blazer.prepare (kSr, kBlockSize);
        const float reverseDefault = std::pow (0.5f, 1.0f / 2.857f);
        blazer.setGain (0.5f); blazer.setVolume (0.5f); blazer.setBass (0.5f); // new flat default
        blazer.setTreble (0.5f); blazer.setPresence (0.5f); blazer.setTight (reverseDefault);
        blazer.setTrim (0.5f);

        double phase = 0.0;
        juce::AudioBuffer<float> last;
        for (int b = 0; b < kNumBlocks; ++b)
        {
            auto block = makeToneBlock (testToneHz, testAmpl, kBlockSize, kSr, phase);
            block.applyGain (kBlazerPreGain);
            blazer.process (block);
            charBlazer.blazer (block);
            block.applyGain (kBlazerLoudnessComp);
            last = block;
        }
        blazerRms = rms (last);
    }

    // Burner is intentionally hotter than Ember (kBurnerHotness ~+2 dB) so it
    // reads as more aggressive/metal — so it's checked against that hotter
    // target, not unity with Ember.
    const float burnerTarget  = emberRms * kBurnerHotness;
    const float burnerRelDiff = std::abs (burnerRms - burnerTarget) / burnerTarget;
    const float blazerRelDiff = std::abs (blazerRms - emberRms) / emberRms;

    // DEMOTED to a gross-breakage guard (was a 15% match assertion). Single-
    // tone RMS is NOT the level-matching criterion any more — see Check 9,
    // which uses K-weighted perceptual loudness. A low sine cannot see the
    // presence-band differences that actually drive perceived loudness, and
    // trusting it is why Blazer shipped ~1.3 dB quiet while measuring
    // "matched to 0.02%". Kept only to catch an order-of-magnitude mistake.
    check (burnerRelDiff < 0.35f, "Burner 220 Hz RMS not grossly off (sanity only)");
    check (blazerRelDiff < 0.35f, "Blazer 220 Hz RMS not grossly off (sanity only)");

    std::cout << "  Compensated RMS: Ember=" << emberRms << " Burner=" << burnerRms
               << " (target " << burnerTarget << ", " << (burnerRelDiff * 100.0f) << "% off) Blazer=" << blazerRms
               << " (" << (blazerRelDiff * 100.0f) << "% off)\n";
}

// ===========================================================================
// Check 8: SIGNAL-TO-NOISE through each circuit.
//
// REGRESSION GUARD. Ember once shipped with a second-order ADAA rail
// saturator whose F₂ expression was catastrophically ill-conditioned in
// float32 (magnitudes scaling as k³ = 64000, differenced then divided by Δ²).
// It manufactured ~39 dB of broadband numerical noise on every sample —
// audible while playing, and WORSE at lower drive — and nothing in the test
// suite caught it, because every existing check measured levels/ratios of a
// loud tone where the noise was masked.
//
// Method: render (signal + noise) and (signal alone) with an IDENTICAL noise
// sequence, then subtract. The difference IS the circuit's noise contribution
// at the output, including any noise the circuit generates itself. The gate is
// disabled so this measures the CIRCUIT, not the gate.
// ===========================================================================
void testSignalToNoise()
{
    std::cout << "\n[Check 8] Signal-to-noise through each circuit (numerical-noise regression guard)\n";

    constexpr int   nB       = 30;
    constexpr float sigAmp   = 0.10f;    // -20 dBFS, realistic guitar level
    constexpr float noiseRms = 0.0003f;  // -70 dBFS floor  => input SNR ~50 dB
    const float inputSnrDb = 20.0f * std::log10 (sigAmp / noiseRms);

    auto render = [&] (int which, bool withNoise, juce::AudioBuffer<float>& out)
    {
        juce::Random rng (999);                     // identical sequence both passes
        EmberCircuit em;  em.prepare (kSr, kBlockSize);
        BurnerCircuit br; br.prepare (kSr, kBlockSize);
        br.setGain(0.5f); br.setVolume(0.5f); br.setBass(0.5f); br.setMid(0.5f);
        br.setTreble(0.5f); br.setAggression(1); br.setTight(0.0f);
        BlazerCircuit bz; bz.prepare (kSr, kBlockSize);
        bz.setGain(0.5f); bz.setVolume(0.5f); bz.setBass(0.5f); bz.setTreble(0.5f);
        bz.setPresence(0.5f); bz.setTight(0.784f); bz.setTrim(0.5f);
        burner::Coeffs bcoef; burner::computeCoeffs (kSr, bcoef);
        burner::CascadeState cs[2];
        double ph = 0.0;

        for (int b = 0; b < nB; ++b)
        {
            auto blk = makeToneBlock (220.0f, sigAmp, kBlockSize, kSr, ph);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = blk.getWritePointer (ch);
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const float n = (rng.nextFloat() * 2.0f - 1.0f) * noiseRms * std::sqrt (3.0f);
                    if (withNoise) d[i] += n;   // rng advances either way => same sequence
                }
            }
            if (which == 0)
                em.process (blk, 0.5f, 0.5f, 0.5f, 0, 0.0f, 20.0f, 0.0f, 2, -70.0f);
            else if (which == 1)
            { applyBurnerCascade (blk, 0.5f, bcoef, cs); blk.applyGain (kBurnerDiodeFeed);
              br.process (blk); blk.applyGain (1.418f * kBurnerHotness); }
            else
            { blk.applyGain (8.0f); bz.process (blk); blk.applyGain (1.144f); }

            if (b == nB - 1) out.makeCopyOf (blk);
        }
    };

    const char* names[3] = { "Ember", "Burner", "Blazer" };
    // Floor chosen well below every circuit's measured value (Ember 42.5,
    // Burner 47.2, Blazer 31.2 dB) but far above the ~11 dB the broken ADAA₂
    // produced, so this fails loudly if that class of bug ever returns.
    constexpr float kMinSnrDb = 25.0f;

    for (int w = 0; w < 3; ++w)
    {
        juce::AudioBuffer<float> withN (2, kBlockSize), noN (2, kBlockSize), diff (2, kBlockSize);
        render (w, true, withN);
        render (w, false, noN);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* a = withN.getReadPointer (ch); auto* b = noN.getReadPointer (ch);
            auto* o = diff.getWritePointer (ch);
            for (int i = 0; i < kBlockSize; ++i) o[i] = a[i] - b[i];
        }
        const float sig = rms (noN), noi = rms (diff);
        const float snr = 20.0f * std::log10 (sig / juce::jmax (1.0e-12f, noi));

        check (snr > kMinSnrDb,
               juce::String (names[w]) + " output SNR above " + juce::String ((int) kMinSnrDb) + " dB");
        std::cout << "  " << names[w] << ": sigOut=" << sig << " noiseOut=" << noi
                   << "  output SNR=" << snr << " dB (input " << inputSnrDb
                   << " dB, degradation " << (inputSnrDb - snr) << " dB)\n";
    }
}

int main()
{
    std::cout << "=== Sparker Distortion — Phase 10 Offline Integration Tests (Ember/Burner/Blazer) ===\n";

    testCircuitSwitching();
    testEmberVoicingModes();
    testBurnerAggressionStates();
    testBurnerToneStack();
    testBlazerTight();
    testBlazerToneStack();
    testBlazerTrim();
    testSignalToNoise();
    measureLoudnessAtDefaults();
    testLoudnessMatch();

    // ===== Check 9: PERCEPTUAL (K-weighted, ITU-R BS.1770) loudness match =====
    // THIS is the authoritative cross-circuit level criterion, replacing the
    // old RMS-of-a-220-Hz-sine match. That measure ignores how the ear weights
    // frequency, so it reported all three matched to 0.02% while the player
    // heard Blazer as clearly quieter — Blazer has the least presence-band
    // energy plus a 6 kHz low-pass, which a single low tone cannot see.
    // K-weighting (head high-shelf + RLB high-pass, on broadband pink noise)
    // is what broadcast and commercial level matching actually use.
    {
        const double sr = kSr; const int bs = kBlockSize;
        struct Pink { float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
            float next (juce::Random& r){ const float w=r.nextFloat()*2-1;
                b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f;
                b2=0.96900f*b2+w*0.1538520f; b3=0.86650f*b3+w*0.3104856f;
                b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
                const float p=b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6=w*0.115926f; return p*0.12f; } };

        auto kWeighted = [&](int which, bool drive = false)->double
        {
            juce::Random rng (7); Pink pk;
            CharacterEQ chr (sr, bs);
            chr.driveMode = drive;
            EmberCircuit em;  em.prepare (sr, bs);
            BurnerCircuit br; br.prepare (sr, bs);
            br.setGain(0.5f);br.setVolume(0.5f);br.setBass(0.5f);br.setMid(0.5f);
            br.setTreble(0.5f);br.setAggression(1);br.setTight(0.0f);
            BlazerCircuit bz; bz.prepare (sr, bs);
            bz.setGain(0.5f);bz.setVolume(0.5f);bz.setBass(0.5f);bz.setTreble(0.5f);
            bz.setPresence(0.5f);bz.setTight(0.784f);bz.setTrim(0.5f);
            burner::Coeffs bc; burner::computeCoeffs (sr, bc);
            burner::CascadeState cs[2];
            // K-weighting filters
            using D = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
            D shelf, hp; juce::dsp::ProcessSpec sp { sr, (juce::uint32) bs, 2 };
            shelf.prepare (sp); hp.prepare (sp);
            *shelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1681.97, 0.7071, juce::Decibels::decibelsToGain (3.999));
            *hp.state    = *juce::dsp::IIR::Coefficients<float>::makeHighPass  (sr, 38.13, 0.5);
            double sum = 0.0; int n = 0;
            for (int b = 0; b < 60; ++b)
            {
                juce::AudioBuffer<float> blk (2, bs);
                for (int i=0;i<bs;++i){ const float s=pk.next(rng); blk.setSample(0,i,s); blk.setSample(1,i,s); }
                if (which==0){ em.process (blk,0.5f,0.5f,0.5f,0,0.0f,20.0f,0.0f,2,-70.0f); chr.ember(blk); }
                else if (which==1){ applyBurnerCascade(blk,0.5f,bc,cs); blk.applyGain(kBurnerDiodeFeed);
                                    br.process(blk); chr.burner(blk); blk.applyGain((drive?1.207f:1.418f)*kBurnerHotness); }
                else { blk.applyGain(8.0f); bz.process(blk); chr.blazer(blk); blk.applyGain(drive?0.791f:1.144f); }
                if (! drive) chr.scoop (blk); else blk.applyGain (0.861f);  // DRIVE: no scoop, level trim
                if (b < 12) continue;
                juce::dsp::AudioBlock<float> ab (blk); juce::dsp::ProcessContextReplacing<float> cx (ab);
                shelf.process (cx); hp.process (cx);
                const float* d = blk.getReadPointer (0);
                for (int i=0;i<bs;++i){ sum += (double)d[i]*d[i]; ++n; }
            }
            return 10.0 * std::log10 (sum / juce::jmax (1, n));
        };

        const double e = kWeighted(0), bu = kWeighted(1), bl = kWeighted(2);
        const double de = kWeighted(0,true), dbu = kWeighted(1,true), dbl = kWeighted(2,true);
        // DRIVE mode must hold the same balance AND the same absolute level as
        // AMP, so toggling voicing is a tonal change only, never a level jump.
        check (std::abs (de - e)       < 0.5, "DRIVE Ember matches AMP Ember in level");
        check (std::abs (dbu - e - 2.0) < 0.75, "DRIVE Burner holds its ~+2 dB offset");
        check (std::abs (dbl - e)      < 0.75, "DRIVE Blazer is level with Ember");
        std::cout << "  DRIVE mode, dB re AMP Ember:  Ember " << juce::String(de-e,2)
                  << "   Burner " << juce::String(dbu-e,2)
                  << "   Blazer " << juce::String(dbl-e,2) << "\n";
        std::cout << "\n[Check 9] Perceptual (K-weighted) loudness match, pink noise, dB re Ember\n";
        // Burner is DELIBERATELY ~+2 dB hotter (kBurnerHotness); Blazer should
        // sit level with Ember.
        check (std::abs (bu - e - 2.0) < 0.75, "Burner sits ~+2 dB over Ember (its intended hotness)");
        check (std::abs (bl - e)       < 0.75, "Blazer is perceptually level with Ember");
        std::cout << "  Ember 0.00 dB (ref)   Burner " << juce::String (bu - e, 2)
                   << " dB   Blazer " << juce::String (bl - e, 2) << " dB\n";
    }


    // ===== Check 10: OCTAVE-BAND balance across circuits, in BOTH voicings =====
    // Check 9 matches INTEGRATED loudness, which is necessary but not
    // sufficient: a single scalar cannot distinguish "quieter" from "tilted".
    // DRIVE Blazer passed Check 9 while sitting 2.5-4 dB under both other
    // circuits across 500 Hz-4 kHz and 3.1 dB OVER them at 8 kHz — the
    // K-weighting high-shelf let a fizz surplus pay for a deficit in the whole
    // musical band, which is what the player heard as "weak, and it feels like
    // it was low-passed". This check pins the band balance directly, so that
    // failure mode is caught by measurement rather than by ear.
    {
        const double sr = kSr; const int bs = kBlockSize;
        struct PinkB { float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
            float next (juce::Random& r){ const float w=r.nextFloat()*2-1;
                b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f;
                b2=0.96900f*b2+w*0.1538520f; b3=0.86650f*b3+w*0.3104856f;
                b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
                const float p=b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6=w*0.115926f; return p*0.12f; } };

        auto capture = [&](int which, bool drive)->juce::AudioBuffer<float>
        {
            juce::Random rng (7); PinkB pk;
            CharacterEQ chr (sr, bs); chr.driveMode = drive;
            EmberCircuit em;  em.prepare (sr, bs);
            BurnerCircuit br; br.prepare (sr, bs);
            br.setGain(0.5f);br.setVolume(0.5f);br.setBass(0.5f);br.setMid(0.5f);
            br.setTreble(0.5f);br.setAggression(1);br.setTight(0.0f);
            BlazerCircuit bz; bz.prepare (sr, bs);
            bz.setGain(0.5f);bz.setVolume(0.5f);bz.setBass(0.5f);bz.setTreble(0.5f);
            bz.setPresence(0.5f);bz.setTight(0.784f);bz.setTrim(0.5f);
            burner::Coeffs bc; burner::computeCoeffs (sr, bc);
            burner::CascadeState cs[2];
            const int keep = 48;
            juce::AudioBuffer<float> out (2, keep * bs); out.clear();
            int w = 0;
            for (int b = 0; b < 60; ++b)
            {
                juce::AudioBuffer<float> blk (2, bs);
                for (int i=0;i<bs;++i){ const float s=pk.next(rng); blk.setSample(0,i,s); blk.setSample(1,i,s); }
                if (which==0){ em.process (blk,0.5f,0.5f,0.5f,0,0.0f,20.0f,0.0f,2,-70.0f); chr.ember(blk); }
                else if (which==1){ applyBurnerCascade(blk,0.5f,bc,cs); blk.applyGain(kBurnerDiodeFeed);
                                    br.process(blk); chr.burner(blk); blk.applyGain((drive?1.207f:1.418f)*kBurnerHotness); }
                else { blk.applyGain(8.0f); bz.process(blk); chr.blazer(blk); blk.applyGain(drive?0.791f:1.144f); }
                if (! drive) chr.scoop (blk); else blk.applyGain (0.861f);
                if (b < 12) continue;
                for (int ch=0;ch<2;++ch) out.copyFrom (ch, w*bs, blk, ch, 0, bs);
                ++w;
            }
            return out;
        };

        const double centres[] = { 125, 250, 500, 1000, 2000, 4000, 8000 };
        const char*  names[]   = { "Ember ", "Burner", "Blazer" };
        // Burner is deliberately ~+2 dB hotter overall (kBurnerHotness), so
        // its bands are compared against that offset rather than against 0.
        const double expected[3] = { 0.0, 2.0, 0.0 };

        for (int drive = 0; drive <= 1; ++drive)
        {
            std::cout << "\n[Check 10] " << (drive ? "DRIVE" : "AMP")
                      << " octave-band balance, dB re Ember's same band\n";
            std::cout << "              125    250    500     1k     2k     4k     8k   | crest\n";
            double ref[7] = {};
            for (int c = 0; c < 3; ++c)
            {
                auto full = capture (c, drive != 0);
                double bandDb[7];
                for (int k = 0; k < 7; ++k)
                {
                    juce::AudioBuffer<float> cp; cp.makeCopyOf (full);
                    const float lo = (float) (centres[k] / 1.4142), hi = (float) (centres[k] * 1.4142);
                    bandDb[k] = juce::Decibels::gainToDecibels (bandEnergy (cp, sr, lo, hi));
                }
                if (c == 0) { for (int k = 0; k < 7; ++k) ref[k] = bandDb[k]; }

                const float peakMag = full.getMagnitude (0, full.getNumSamples());
                const double crest  = juce::Decibels::gainToDecibels (peakMag / juce::jmax (1.0e-9f, rms (full)));

                std::cout << "  " << names[c] << "  ";
                for (int k = 0; k < 7; ++k)
                    std::cout << juce::String (bandDb[k] - ref[k], 1).paddedLeft (' ', 7);
                std::cout << "   | " << juce::String (crest, 1) << "\n";

                // The DEFINITION band (1-4 kHz) is what carries perceived
                // strength and presence on a distorted guitar. Its MEAN is the
                // right statistic, not each band: the circuits are deliberately
                // voiced differently from each other (Burner is mid-forward at
                // 2.2 kHz, Ember shelves from 3 kHz), so per-band tracking
                // would fail on intended character. What must not happen is a
                // circuit sitting uniformly UNDER across the whole band while
                // an integrated-loudness match still reports it level — which
                // is exactly what DRIVE Blazer did at -2.2 dB mean.
                if (c > 0)
                {
                    const double defMean = ((bandDb[3] - ref[3]) + (bandDb[4] - ref[4])
                                            + (bandDb[5] - ref[5])) / 3.0;
                    check (std::abs (defMean - expected[c]) < 2.0,
                           juce::String (drive ? "DRIVE " : "AMP ") + names[c]
                             + " definition band (1-4 kHz) mean tracks Ember ("
                             + juce::String (defMean, 2) + " dB)");
                }
                // 8 kHz must not run away from Ember's.
                if (c > 0)
                    check ((bandDb[6] - ref[6]) - expected[c] < 2.5,
                           juce::String (drive ? "DRIVE " : "AMP ") + names[c]
                             + " has no 8 kHz fizz surplus");
            }
        }
    }

    std::cout << "\n=== " << (failures == 0 ? "ALL CHECKS PASSED" : juce::String (failures) + " CHECK(S) FAILED") << " ===\n";
    return failures == 0 ? 0 : 1;
}
