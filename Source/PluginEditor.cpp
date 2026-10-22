#include "PluginEditor.h"
#include "gui/PluginColors.h"
#include <cmath>

// ============================================================================
// Layout constants — reproduces AI Studio's grid layout (see
// AI Studio/extracted/src/components/*.tsx) at fixed pixel positions.
// Window is 4:3 (App.tsx: aspect-[4/3] min-h-[580px]).
// ============================================================================
static constexpr int   kW = 800;
static constexpr int   kH = 600;

// UI size presets — the base 800x600 design scaled up. Small is the default.
static constexpr float kScaleSmall  = 1.0f;
static constexpr float kScaleMedium = 1.25f;
static constexpr float kScaleLarge  = 1.5f;
// Property key under which the chosen scale lives in the APVTS state tree
// (persisted with the project via get/setStateInformation).
static const juce::Identifier kUiScaleProp { "uiScale" };

static constexpr int   kHeaderH = 52;

static constexpr float kPadX = 24.0f;
static constexpr float kContentW = (float) kW - 2.0f * kPadX;

// Top row: INPUT+meter | GAIN | KHYLE AUDIO | LEVEL/VOLUME | OUTPUT+meter
static constexpr float kKnobLargeD = 84.0f;
static constexpr float kTopRowY    = (float) kHeaderH + 32.0f;
static constexpr float kColW       = kContentW / 5.0f;
static float colX (int i) { return kPadX + (float) i * kColW; }
static constexpr float kMeterW = 10.0f;

// Middle row: Ember = 1 knob centered. Burner/Blazer = 3 knobs, gap 48px.
static constexpr float kMidRowY = kTopRowY + kKnobLargeD + 36.0f;
static constexpr float kMidGap  = 48.0f;

// Bottom row — all three groups (GATE/TIGHT, footswitch, MODE/AGGRESSION)
// share a common BOTTOM edge close to the chassis floor (the wordmark sits
// further below, in the remaining margin). This intentionally makes the
// mid-row -> bottom-row gap the largest gap in the layout.
static constexpr float kKnobSmallD  = 56.0f;
static constexpr float kSmallGap    = 56.0f;
static constexpr float kFootD       = 72.0f;
static constexpr float kLedD        = 16.0f; // spec: exactly 16x16px
static constexpr float kLedGap      = 12.0f; // spec: 12px gap (mb-3) between LED and footswitch
static constexpr float kBottomRowBottomY = (float) kH - 100.0f;
static constexpr float kBottomRowY       = kBottomRowBottomY - kKnobSmallD;
static constexpr float kModePanelH       = kKnobSmallD; // vertically aligned with GATE/TIGHT knobs

// ============================================================================
// Letter-spaced text helper (kept from the original implementation)
// ============================================================================
static void drawLetterSpaced (juce::Graphics& g,
                               const juce::String& text,
                               juce::Rectangle<float> bounds,
                               float extraSpacing)
{
    const juce::Font f = g.getCurrentFont();

    float totalW = 0.0f;
    for (int i = 0; i < text.length(); ++i)
    {
        totalW += juce::GlyphArrangement::getStringWidth (f, text.substring (i, i + 1));
        if (i < text.length() - 1)
            totalW += extraSpacing;
    }

    float x = bounds.getCentreX() - totalW * 0.5f;
    const int iy = (int) bounds.getY();
    const int ih = (int) bounds.getHeight();

    for (int i = 0; i < text.length(); ++i)
    {
        const juce::String ch = text.substring (i, i + 1);
        const float cw = juce::GlyphArrangement::getStringWidth (f, ch);
        g.drawText (ch, (int) x, iy, juce::jmax (1, (int) (cw + 1.5f)), ih,
                    juce::Justification::left);
        x += cw + (i < text.length() - 1 ? extraSpacing : 0.0f);
    }
}

// Draws a knob's label + numeric readout below it, matching Knob.tsx.
static void drawKnobCaption (juce::Graphics& g, PedalLookAndFeel& laf,
                              float knobX, float knobY, float knobD,
                              const juce::String& label, const juce::String& readout,
                              juce::Colour readoutColour)
{
    // Sizes bumped for legibility: OpenDyslexic renders smaller/thinner than
    // the Inter it replaced at the same pixel height, so the caption label and
    // (especially) the tiny numeric readout were hard to read. The draw boxes
    // are widened/tallened to match so nothing clips.
    const float capY = knobY + knobD + 6.0f;
    g.setFont (laf.uiFont (15.0f, true));
    g.setColour (juce::Colour (PluginColors::kLabelText));
    g.drawText (label, (int) (knobX - 26.0f), (int) capY, (int) (knobD + 52.0f), 18,
                juce::Justification::centred);

    if (readout.isNotEmpty())
    {
        g.setFont (laf.regularFont (13.0f));
        g.setColour (readoutColour);
        g.drawText (readout, (int) (knobX - 26.0f), (int) (capY + 18.0f), (int) (knobD + 52.0f), 16,
                    juce::Justification::centred);
    }
}

// AI Studio's mockup displays every non-dB knob's value on an abstract
// 0-10 scale (see constants.ts DEFAULT_PLUGIN_STATE — drive:6.5, level:7.0,
// etc.), regardless of the parameter's real engineering units underneath.
// This reproduces that exact display convention: raw slider proportion
// (0..1) x10, one decimal — cosmetic only, doesn't change the real range.
static juce::String knobReadout01to10 (juce::Slider& s)
{
    const double proportion = s.valueToProportionOfLength (s.getValue());
    return juce::String (proportion * 10.0, 1);
}

// ============================================================================
// Constructor
// ============================================================================
SuhrRiotEditor::SuhrRiotEditor (SuhrRiotProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      header          (p.apvts, laf),
      modeSwitch      (p.apvts, laf),
      driveAtt        (p.apvts, "drive",      driveKnob),
      toneAtt         (p.apvts, "tone",        toneKnob),
      levelAtt        (p.apvts, "level",       levelKnob),
      inputTrimAtt    (p.apvts, "inputTrim",   inputTrimKnob),
      outputTrimAtt   (p.apvts, "outputTrim",  outputTrimKnob),
      tightAtt        (p.apvts, "tightFreq",   tightKnob),
      footswitchAtt   (p.apvts, "active",      footswitch),
      gateAtt         (p.apvts, "gateThresh",  gateKnob),
      aggressionSwitch (p.apvts, laf),
      voicingSwitch    (p.apvts, laf),
      burnerGainAtt    (p.apvts, "burnerGain",   burnerGainKnob),
      burnerVolumeAtt  (p.apvts, "burnerVolume", burnerVolumeKnob),
      burnerBassAtt    (p.apvts, "burnerBass",   burnerBassKnob),
      burnerMidAtt     (p.apvts, "burnerMid",    burnerMidKnob),
      burnerTrebleAtt  (p.apvts, "burnerTreble", burnerTrebleKnob),
      burnerTightAtt   (p.apvts, "burnerTight",  burnerTightKnob),
      blazerGainAtt     (p.apvts, "blazerGain",     blazerGainKnob),
      blazerVolumeAtt   (p.apvts, "blazerVolume",   blazerVolumeKnob),
      blazerBassAtt     (p.apvts, "blazerBass",     blazerBassKnob),
      blazerTrebleAtt   (p.apvts, "blazerTreble",   blazerTrebleKnob),
      blazerPresenceAtt (p.apvts, "blazerPresence", blazerPresenceKnob),
      blazerTightAtt    (p.apvts, "blazerTight",    blazerTightKnob),
      blazerTrimAtt     (p.apvts, "blazerTrim",     blazerTrimKnob)
{
    // The root fills the editor and is the parent of every control; it also
    // paints the chassis. Everything is added to `root` (not the editor) so a
    // single transform on `root` scales the whole UI uniformly.
    addAndMakeVisible (root);

    // Ember rotary knobs
    for (auto* s : { &driveKnob, &toneKnob, &levelKnob,
                     &inputTrimKnob, &outputTrimKnob, &tightKnob, &gateKnob })
    {
        s->setLookAndFeel (&laf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        root.addAndMakeVisible (*s);
    }

    footswitch.setComponentID ("footswitch");
    footswitch.setLookAndFeel (&laf);
    footswitch.setClickingTogglesState (true);
    root.addAndMakeVisible (footswitch);

    root.addAndMakeVisible (modeSwitch);
    root.addAndMakeVisible (aggressionSwitch);
    root.addAndMakeVisible (voicingSwitch);
    root.addAndMakeVisible (inputMeter);
    root.addAndMakeVisible (outputMeter);

    for (auto* s : { &burnerGainKnob, &burnerVolumeKnob, &burnerBassKnob,
                     &burnerMidKnob, &burnerTrebleKnob, &burnerTightKnob })
    {
        s->setLookAndFeel (&burnerLaf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        root.addAndMakeVisible (*s);
    }

    for (auto* s : { &blazerGainKnob, &blazerVolumeKnob, &blazerBassKnob,
                     &blazerTrebleKnob, &blazerPresenceKnob, &blazerTightKnob,
                     &blazerTrimKnob })
    {
        s->setLookAndFeel (&blazerLaf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        root.addAndMakeVisible (*s);
    }

    header.onSelectPreset = [this] (const PluginHeader::Entry& entry)
    {
        if (entry.isUserFile)
        {
            // Full state replace — real user-saved presets capture every
            // parameter, not just a hand-picked subset.
            PresetManager::loadPreset (processorRef.apvts, entry.file);
            return;
        }

        // Factory preset: switch circuit first if needed, then apply the
        // hand-picked param subset (PresetData.h).
        jassert (entry.factory != nullptr);
        if (auto* circuitParam = processorRef.apvts.getParameter ("circuit"))
            circuitParam->setValueNotifyingHost (circuitParam->convertTo0to1 ((float) entry.factory->circuit));

        for (auto& v : entry.factory->values)
            if (auto* param = processorRef.apvts.getParameter (v.paramId))
                param->setValueNotifyingHost (param->convertTo0to1 (v.value));
    };
    root.addAndMakeVisible (header);

    startTimerHz (15);

    // Restore the persisted UI size (defaults to Small on first open). This
    // calls setSize internally, so no separate setSize is needed here.
    const float savedScale = (float) processorRef.apvts.state.getProperty (kUiScaleProp, (double) kScaleSmall);
    applyUiScale (savedScale, /*persist*/ false);
}

SuhrRiotEditor::~SuhrRiotEditor()
{
    stopTimer();
    for (auto* s : { &driveKnob, &toneKnob, &levelKnob,
                     &inputTrimKnob, &outputTrimKnob, &tightKnob, &gateKnob })
        s->setLookAndFeel (nullptr);
    footswitch.setLookAndFeel (nullptr);

    for (auto* s : { &burnerGainKnob, &burnerVolumeKnob, &burnerBassKnob,
                     &burnerMidKnob, &burnerTrebleKnob, &burnerTightKnob })
        s->setLookAndFeel (nullptr);

    for (auto* s : { &blazerGainKnob, &blazerVolumeKnob, &blazerBassKnob,
                     &blazerTrebleKnob, &blazerPresenceKnob, &blazerTightKnob,
                     &blazerTrimKnob })
        s->setLookAndFeel (nullptr);
}

// ============================================================================
// Timer — 15 Hz: updates meters/visibility and repaints
// ============================================================================
void SuhrRiotEditor::timerCallback()
{
    const int circuit = static_cast<int> (processorRef.apvts.getRawParameterValue ("circuit")->load());
    const int aggression = static_cast<int> (processorRef.apvts.getRawParameterValue ("burnerAggression")->load());
    const auto theme = PluginColors::themeFor (circuit, aggression);

    // The voicing switch sits lower in Blazer (see layOutVoicingSwitch), and
    // the circuit can change without a resize, so re-place it here. setBounds
    // is a no-op when the bounds are unchanged, which is the common case.
    layOutVoicingSwitch();

    // Real signal peak, not a cosmetic function of the trim knob — dark at
    // rest, white with signal, red when clipping. Same for all circuits,
    // no theme tint (see LevelMeter.h).
    inputMeter .setLevel (processorRef.inputPeakLevel.load  (std::memory_order_relaxed));
    outputMeter.setLevel (processorRef.outputPeakLevel.load (std::memory_order_relaxed));

    burnerLaf.setAccent (theme.primary);

    // Stash the current accent as an ARGB int on the footswitch so
    // PedalLookAndFeel::drawToggleButton can read it without needing
    // direct access to circuit state.
    footswitch.getProperties().set ("circuitColorArgb", (int) theme.text.getARGB());

    if (circuit != lastEditorCircuit)
    {
        lastEditorCircuit = circuit;
        const bool emberActive  = (circuit == 0);
        const bool burnerActive = (circuit == 1);
        const bool blazerActive = (circuit == 2);

        // Input/Output Trim and Gate are universal (shown in every mode),
        // so their LookAndFeel must switch with the circuit too — they were
        // stuck on the base (Ember/magenta) laf regardless of mode.
        auto* universalLaf = emberActive ? static_cast<PedalLookAndFeel*> (&laf)
                            : burnerActive ? static_cast<PedalLookAndFeel*> (&burnerLaf)
                                           : static_cast<PedalLookAndFeel*> (&blazerLaf);
        for (auto* s : { &inputTrimKnob, &outputTrimKnob, &gateKnob })
            s->setLookAndFeel (universalLaf);

        for (auto* c : { (juce::Component*) &driveKnob, (juce::Component*) &toneKnob,
                         (juce::Component*) &levelKnob, (juce::Component*) &tightKnob,
                         (juce::Component*) &modeSwitch })
            c->setVisible (emberActive);

        for (auto* c : { (juce::Component*) &burnerGainKnob, (juce::Component*) &burnerVolumeKnob,
                         (juce::Component*) &burnerBassKnob, (juce::Component*) &burnerMidKnob,
                         (juce::Component*) &burnerTrebleKnob, (juce::Component*) &burnerTightKnob,
                         (juce::Component*) &aggressionSwitch })
            c->setVisible (burnerActive);

        for (auto* c : { (juce::Component*) &blazerGainKnob, (juce::Component*) &blazerVolumeKnob,
                         (juce::Component*) &blazerBassKnob, (juce::Component*) &blazerTrebleKnob,
                         (juce::Component*) &blazerPresenceKnob, (juce::Component*) &blazerTightKnob,
                         (juce::Component*) &blazerTrimKnob })
            c->setVisible (blazerActive);

        // Universal: Input/Output Trim, their meters, GATE, footswitch, header.
    }

    repaint();
}

// ============================================================================
// Paint
// ============================================================================
void SuhrRiotEditor::paintChassis (juce::Graphics& g)
{
    // Drawn in the fixed 800x600 base coordinate space onto `root`, which
    // carries the UI-size transform — so this scales with everything else.
    const int circuit     = static_cast<int> (processorRef.apvts.getRawParameterValue ("circuit")->load());
    const int aggression  = static_cast<int> (processorRef.apvts.getRawParameterValue ("burnerAggression")->load());
    const bool emberActive  = (circuit == 0);
    const bool burnerActive = (circuit == 1);
    const bool blazerActive = (circuit == 2);
    const auto theme = PluginColors::themeFor (circuit, aggression);

    // -----------------------------------------------------------------------
    // 1. Background — per-circuit radial gradient, matches App.tsx exactly.
    // -----------------------------------------------------------------------
    if (emberActive)
    {
        laf.drawPluginBackground (g, kW, kH);
    }
    else
    {
        // Burner is "at 50% 35%" (offset toward top); Blazer is "at
        // center" — these were incorrectly sharing the same 35% offset.
        // Radius approximates CSS's "farthest-corner" default sizing (was
        // spread too far at 0.85x, reading as flat color instead of
        // fading to near-black at the edges).
        const float cx = (float) kW * 0.5f;
        const float cy = burnerActive ? (float) kH * 0.35f : (float) kH * 0.5f;
        const float radius = std::sqrt (juce::square ((float) kW * 0.5f) + juce::square ((float) kH * 0.5f));

        juce::Colour c0, c1, c2, c3;
        if (burnerActive)
        {
            if (aggression == 1) { c0 = juce::Colour (0xff440c26); c1 = juce::Colour (0xff290833); c2 = juce::Colour (0xff150522); c3 = juce::Colour (0xff080212); }
            else                 { c0 = juce::Colour (0xff0d2c4e); c1 = juce::Colour (0xff1f0b38); c2 = juce::Colour (0xff130522); c3 = juce::Colour (0xff080212); }
        }
        else // Blazer
        {
            c0 = juce::Colour (0xff20160b); c1 = juce::Colour (0xff20160b); c2 = juce::Colour (0xff140d06); c3 = juce::Colour (0xff0a0603);
        }

        juce::ColourGradient grad (c0, cx, cy, c3, cx, cy + radius, true);
        grad.addColour (0.45, c1);
        grad.addColour (0.75, c2);
        g.setGradientFill (grad);
        g.fillRect (0, 0, kW, kH);
    }

    // Chassis border with a soft outer glow bloom (mockup: box-shadow glow
    // around the whole panel) — approximated with several expanding,
    // fading strokes since JUCE has no native CSS-style blur.
    for (int i = 5; i >= 1; --i)
    {
        const float t = (float) i / 5.0f;
        g.setColour (theme.primary.withAlpha (0.05f * t));
        g.drawRoundedRectangle (1.0f - (float) i * 1.2f, 1.0f - (float) i * 1.2f,
                                (float) kW - 2.0f + (float) i * 2.4f,
                                (float) kH - 2.0f + (float) i * 2.4f,
                                14.0f + (float) i, 2.0f);
    }
    g.setColour (theme.border);
    g.drawRoundedRectangle (1.0f, 1.0f, (float) kW - 2.0f, (float) kH - 2.0f, 14.0f, 1.5f);

    const auto lblFont = laf.uiFont (12.0f, true);
    const juce::Colour lblCol (PluginColors::kLabelText);

    // -----------------------------------------------------------------------
    // 2. Top row captions + KHYLE AUDIO logo
    // -----------------------------------------------------------------------
    const float inputKnobX  = colX (0) + 18.0f;
    const float gainKnobX   = colX (1) + (kColW - kKnobLargeD) * 0.5f;
    const float levelKnobX  = colX (3) + (kColW - kKnobLargeD) * 0.5f;
    const float outputKnobX = colX (4) + kColW - kKnobLargeD - 18.0f;

    auto fmtDb = [] (float db) { return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB"; };
    const float inTrim  = processorRef.apvts.getRawParameterValue ("inputTrim") ->load();
    const float outTrim = processorRef.apvts.getRawParameterValue ("outputTrim")->load();

    drawKnobCaption (g, laf, inputKnobX,  kTopRowY, kKnobLargeD, "INPUT",  fmtDb (inTrim),  juce::Colours::white);
    drawKnobCaption (g, laf, outputKnobX, kTopRowY, kKnobLargeD, "OUTPUT", fmtDb (outTrim), juce::Colours::white);

    g.setFont (laf.uiFont (14.0f, true));
    g.setColour (juce::Colour (0xffcbd5e1).withAlpha (0.8f));
    drawLetterSpaced (g, "KHYLE AUDIO",
                      { colX (2), kTopRowY + kKnobLargeD * 0.4f, kColW, 16.0f }, 1.5f);

    if (emberActive)
    {
        drawKnobCaption (g, laf, gainKnobX,  kTopRowY, kKnobLargeD, "GAIN",  knobReadout01to10 (driveKnob), juce::Colours::white);
        drawKnobCaption (g, laf, levelKnobX, kTopRowY, kKnobLargeD, "LEVEL", knobReadout01to10 (levelKnob), juce::Colours::white);

        drawKnobCaption (g, laf, (float) kW * 0.5f - kKnobLargeD * 0.5f, kMidRowY, kKnobLargeD,
                          "TONE", knobReadout01to10 (toneKnob), juce::Colours::white);
    }
    else if (burnerActive)
    {
        drawKnobCaption (g, laf, gainKnobX,  kTopRowY, kKnobLargeD, "GAIN",   knobReadout01to10 (burnerGainKnob),   juce::Colours::white);
        drawKnobCaption (g, laf, levelKnobX, kTopRowY, kKnobLargeD, "LEVEL", knobReadout01to10 (burnerVolumeKnob), juce::Colours::white);

        const float startX = ((float) kW - (3.0f * kKnobLargeD + 2.0f * kMidGap)) * 0.5f;
        drawKnobCaption (g, laf, startX,                              kMidRowY, kKnobLargeD, "BASS",   knobReadout01to10 (burnerBassKnob),   juce::Colours::white);
        drawKnobCaption (g, laf, startX + kKnobLargeD + kMidGap,      kMidRowY, kKnobLargeD, "MID",    knobReadout01to10 (burnerMidKnob),    juce::Colours::white);
        drawKnobCaption (g, laf, startX + 2.0f * (kKnobLargeD + kMidGap), kMidRowY, kKnobLargeD, "TREBLE", knobReadout01to10 (burnerTrebleKnob), juce::Colours::white);
    }
    else // Blazer
    {
        drawKnobCaption (g, laf, gainKnobX,  kTopRowY, kKnobLargeD, "GAIN",   knobReadout01to10 (blazerGainKnob),   juce::Colours::white);
        drawKnobCaption (g, laf, levelKnobX, kTopRowY, kKnobLargeD, "LEVEL", knobReadout01to10 (blazerVolumeKnob), juce::Colours::white);

        const float startX = ((float) kW - (3.0f * kKnobLargeD + 2.0f * kMidGap)) * 0.5f;
        drawKnobCaption (g, laf, startX,                              kMidRowY, kKnobLargeD, "BASS",   knobReadout01to10 (blazerBassKnob),     juce::Colours::white);
        drawKnobCaption (g, laf, startX + kKnobLargeD + kMidGap,      kMidRowY, kKnobLargeD, "TREBLE", knobReadout01to10 (blazerTrebleKnob),   juce::Colours::white);
        drawKnobCaption (g, laf, startX + 2.0f * (kKnobLargeD + kMidGap), kMidRowY, kKnobLargeD, "PRES", knobReadout01to10 (blazerPresenceKnob), juce::Colours::white);
    }

    // -----------------------------------------------------------------------
    // 3. Bottom-left: GATE + mode-specific "tight" knob captions
    // -----------------------------------------------------------------------
    const float gateX  = kPadX + 16.0f;
    const float tightX = gateX + kKnobSmallD + kSmallGap;
    drawKnobCaption (g, laf, gateX, kBottomRowY, kKnobSmallD, "GATE", knobReadout01to10 (gateKnob), juce::Colours::white);

    if (emberActive)
        drawKnobCaption (g, laf, tightX, kBottomRowY, kKnobSmallD, "TIGHT", knobReadout01to10 (tightKnob), juce::Colours::white);
    else if (burnerActive)
        drawKnobCaption (g, laf, tightX, kBottomRowY, kKnobSmallD, "TIGHT", knobReadout01to10 (burnerTightKnob), juce::Colours::white);
    else
        drawKnobCaption (g, laf, tightX, kBottomRowY, kKnobSmallD, "TIGHT", knobReadout01to10 (blazerTightKnob), juce::Colours::white);

    if (blazerActive)
    {
        const float trimX = (float) kW - kPadX - 60.0f - kKnobSmallD;
        drawKnobCaption (g, laf, trimX, kBottomRowY, kKnobSmallD, "TRIM", knobReadout01to10 (blazerTrimKnob), juce::Colours::white);
    }

    // -----------------------------------------------------------------------
    // 4. LED dot above the footswitch
    // -----------------------------------------------------------------------
    {
        const bool on = processorRef.apvts.getRawParameterValue ("active")->load() > 0.5f;
        const float cx = (float) kW * 0.5f;
        const float footY = kBottomRowBottomY - kFootD;
        const float cy = footY - kLedGap - kLedD * 0.5f;
        const float r  = kLedD * 0.5f;

        if (on)
        {
            // Glow derives from ledColor (spec: "ledShadow" is ledColor's
            // own glow), not the theme's primary — they happen to match
            // for Ember but differ for Burner/Blazer.
            g.setColour (theme.ledColor.withAlpha (0.5f));
            g.fillEllipse (cx - r - 4.0f, cy - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f);
            g.setColour (theme.ledColor);
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff2a2638));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        g.setColour (on ? theme.ledColor : juce::Colour (0xff4a4360));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    // -----------------------------------------------------------------------
    // 5. SPARKER / DISTORTION wordmark below the footswitch
    // -----------------------------------------------------------------------
    {
        const float footY = kBottomRowBottomY - kFootD;
        const float nameplateY = footY + kFootD + 10.0f;
        const float cx = (float) kW * 0.5f;

        g.setFont (laf.brandFont (24.0f));
        g.setColour (juce::Colours::white);
        drawLetterSpaced (g, "SPARKER", { cx - 140.0f, nameplateY, 280.0f, 28.0f }, 5.3f);

        g.setFont (laf.uiFont (12.0f, true));
        g.setColour (theme.text);
        drawLetterSpaced (g, "DISTORTION", { cx - 140.0f, nameplateY + 28.0f, 280.0f, 16.0f }, 2.6f);
    }
}

// ============================================================================
// Layout
// ============================================================================
// Global AMP/DRIVE voicing — bottom right, BELOW the mode/aggression panel
// and sharing its right edge, so that corner reads as one column. (It used to
// sit in the header beside the circuit badge, which crowded that corner.)
//
// Blazer needs it lower than the other two. Ember and Burner fill the slot
// above with a mode panel whose bottom edge is kBottomRowBottomY, but Blazer
// has no mode switch and instead puts its TRIM knob in that corner — and a
// knob brings a caption with it. drawKnobCaption() lays the label out at
// knob bottom + 6 and the numeric readout 18 below that, running to
// kBottomRowBottomY + 40, which is what the switch was landing on top of.
//
// Both offsets are derived from that caption geometry rather than hardcoded,
// so they stay correct if the knob size or caption spacing is ever changed.
void SuhrRiotEditor::layOutVoicingSwitch()
{
    const int circuit = static_cast<int> (processorRef.apvts.getRawParameterValue ("circuit")->load());
    const bool blazerActive = (circuit == 2);

    constexpr float voicingW = 132.0f;
    constexpr float voicingH = 28.0f;

    // Caption block under a bottom-row knob: 6 px gap, 18 px label, 16 px
    // readout (see drawKnobCaption). Blazer clears all of it; the others
    // only need to clear the mode panel's bottom edge.
    constexpr float kCaptionBlockH = 6.0f + 18.0f + 16.0f;
    const float gap = blazerActive ? kCaptionBlockH + 8.0f : 12.0f;

    voicingSwitch.setBounds ((int) ((float) kW - kPadX - voicingW),
                             (int) (kBottomRowBottomY + gap),
                             (int) voicingW, (int) voicingH);
}

void SuhrRiotEditor::resized()
{
    // `root` is always the base 800x600 canvas; the UI-size transform scales
    // it (and every control + all chassis painting) up to fill the editor.
    root.setBounds (0, 0, kW, kH);
    root.setTransform (uiScale == 1.0f ? juce::AffineTransform()
                                       : juce::AffineTransform::scale (uiScale));

    header.setBounds (0, 0, kW, kHeaderH);

    // --- Top row ---
    const float inputKnobX  = colX (0) + 18.0f;
    const float gainKnobX   = colX (1) + (kColW - kKnobLargeD) * 0.5f;
    const float levelKnobX  = colX (3) + (kColW - kKnobLargeD) * 0.5f;
    const float outputKnobX = colX (4) + kColW - kKnobLargeD - 18.0f;

    inputTrimKnob .setBounds ((int) inputKnobX,  (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    outputTrimKnob.setBounds ((int) outputKnobX, (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    inputMeter .setBounds ((int) (inputKnobX - kMeterW - 4.0f),      (int) (kTopRowY + 6.0f), (int) kMeterW, (int) (kKnobLargeD - 12.0f));
    outputMeter.setBounds ((int) (outputKnobX + kKnobLargeD + 4.0f), (int) (kTopRowY + 6.0f), (int) kMeterW, (int) (kKnobLargeD - 12.0f));

    driveKnob       .setBounds ((int) gainKnobX,  (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    levelKnob       .setBounds ((int) levelKnobX, (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    burnerGainKnob  .setBounds ((int) gainKnobX,  (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    burnerVolumeKnob.setBounds ((int) levelKnobX, (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    blazerGainKnob  .setBounds ((int) gainKnobX,  (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    blazerVolumeKnob.setBounds ((int) levelKnobX, (int) kTopRowY, (int) kKnobLargeD, (int) kKnobLargeD);

    // --- Middle row ---
    toneKnob.setBounds ((int) ((float) kW * 0.5f - kKnobLargeD * 0.5f), (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);

    const float midStartX = ((float) kW - (3.0f * kKnobLargeD + 2.0f * kMidGap)) * 0.5f;
    burnerBassKnob  .setBounds ((int) midStartX,                                  (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    burnerMidKnob   .setBounds ((int) (midStartX + kKnobLargeD + kMidGap),        (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    burnerTrebleKnob.setBounds ((int) (midStartX + 2.0f * (kKnobLargeD + kMidGap)), (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);

    blazerBassKnob    .setBounds ((int) midStartX,                                  (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    blazerTrebleKnob  .setBounds ((int) (midStartX + kKnobLargeD + kMidGap),        (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);
    blazerPresenceKnob.setBounds ((int) (midStartX + 2.0f * (kKnobLargeD + kMidGap)), (int) kMidRowY, (int) kKnobLargeD, (int) kKnobLargeD);

    // --- Bottom row ---
    const float gateX  = kPadX + 16.0f;
    const float tightX = gateX + kKnobSmallD + kSmallGap;
    gateKnob.setBounds ((int) gateX, (int) kBottomRowY, (int) kKnobSmallD, (int) kKnobSmallD);

    tightKnob      .setBounds ((int) tightX, (int) kBottomRowY, (int) kKnobSmallD, (int) kKnobSmallD);
    burnerTightKnob.setBounds ((int) tightX, (int) kBottomRowY, (int) kKnobSmallD, (int) kKnobSmallD);
    blazerTightKnob.setBounds ((int) tightX, (int) kBottomRowY, (int) kKnobSmallD, (int) kKnobSmallD);

    const float trimX = (float) kW - kPadX - 60.0f - kKnobSmallD;
    blazerTrimKnob.setBounds ((int) trimX, (int) kBottomRowY, (int) kKnobSmallD, (int) kKnobSmallD);

    const float footY = kBottomRowBottomY - kFootD;
    const float footX  = (float) kW * 0.5f - kFootD * 0.5f;
    footswitch.setBounds ((int) footX, (int) footY, (int) kFootD, (int) kFootD);

    // Right-side mode-specific panels — bottom-aligned to the same
    // kBottomRowBottomY baseline as the footswitch and GATE/TIGHT knobs.
    const float modePanelW = 200.0f;
    modeSwitch.setBounds ((int) ((float) kW - kPadX - modePanelW), (int) (kBottomRowBottomY - kModePanelH),
                          (int) modePanelW, (int) kModePanelH);

    const float aggrPanelW = 170.0f;
    aggressionSwitch.setBounds ((int) ((float) kW - kPadX - aggrPanelW), (int) (kBottomRowBottomY - kModePanelH),
                                (int) aggrPanelW, (int) kModePanelH);

    layOutVoicingSwitch();
    // No per-child scaling here: every control above is a child of `root`,
    // and `root`'s transform (set at the top) scales them all at once.
}

// ============================================================================
// UI size (Small / Medium / Large) — right-click menu
// ============================================================================
void SuhrRiotEditor::applyUiScale (float newScale, bool persist)
{
    uiScale = newScale;

    if (persist)
        processorRef.apvts.state.setProperty (kUiScaleProp, (double) newScale, nullptr);

    // Resize the window to the scaled base dimensions. setSize triggers
    // resized(), which re-lays-out children and applies the scale to them.
    setSize (juce::roundToInt ((float) kW * uiScale),
             juce::roundToInt ((float) kH * uiScale));
    repaint();
}

void SuhrRiotEditor::showSizeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Plugin Size");
    m.addItem (1, "Small",  true, uiScale == kScaleSmall);
    m.addItem (2, "Medium", true, uiScale == kScaleMedium);
    m.addItem (3, "Large",  true, uiScale == kScaleLarge);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this] (int result)
                     {
                         if (result == 1) applyUiScale (kScaleSmall,  true);
                         else if (result == 2) applyUiScale (kScaleMedium, true);
                         else if (result == 3) applyUiScale (kScaleLarge,  true);
                     });
}

void SuhrRiotEditor::rootMouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showSizeMenu();
}
