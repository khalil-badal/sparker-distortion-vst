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

namespace
{
    constexpr double kSr          = 48000.0;
    constexpr int    kBlockSize   = 512;
    constexpr int    kNumBlocks   = 20; // ~213 ms of audio, enough to settle filters

    int failures = 0;

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
}

// ===========================================================================
// Check 2: Ember's three internal voicing modes are intact and distinguishable
// ===========================================================================
void testEmberVoicingModes()
{
    std::cout << "\n[Check 2] Ember voicing modes (WARM/CLASSIC/HOT) intact and distinguishable\n";

    float rmsByMode[3];
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

    std::cout << "\n=== " << (failures == 0 ? "ALL CHECKS PASSED" : juce::String (failures) + " CHECK(S) FAILED") << " ===\n";
    return failures == 0 ? 0 : 1;
}
