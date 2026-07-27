#include "PluginEditor.h"
#include <cmath>

// ============================================================================
// Layout constants
// ============================================================================
static constexpr int   kW       = 590;
static constexpr int   kH       = 430;

// Pedal body dimensions (used for control positioning; no visual border drawn)
static constexpr float kPedalX  = 106.0f;
static constexpr float kPedalY  =  12.0f;
static constexpr float kPedalW  = 378.0f;
static constexpr float kPedalCX = kPedalX + kPedalW * 0.5f;          // 295

// Top-row inner knobs (DRIVE / LEVEL)
static constexpr float kTopKnobD = 72.0f;
static constexpr float kTopKnobY = kPedalY + 42.0f;                   // 54
static constexpr float kDistX    = kPedalX + 26.0f;                   // 132
static constexpr float kLevelX   = kPedalX + kPedalW - 26.0f - kTopKnobD;  // 356

// TONE knob (center middle)
static constexpr float kToneD    = 66.0f;
static constexpr float kToneX    = kPedalCX - kToneD * 0.5f;         // 262
static constexpr float kToneY    = kPedalY + 152.0f;                  // 164

// LED dot (painted non-interactive indicator, above footswitch)
static constexpr float kLedCX    = kPedalCX;                          // 295
static constexpr float kLedCY    = kPedalY + 270.0f;                  // 282

// Footswitch (ToggleButton, styled as chrome stomp-switch)
static constexpr float kFootD    = 64.0f;
static constexpr float kFootX    = kPedalCX - kFootD * 0.5f;         // 263
static constexpr float kFootY    = kLedCY + 10.0f;                    // 292

// Outer trim knobs (vertically aligned with the inner top row)
static constexpr float kTrimD    = 68.0f;
static constexpr float kTrimLX   = 12.0f;
static constexpr float kTrimRX   = (float) kW - 12.0f - kTrimD;     // 510
static constexpr float kTrimY    = kTopKnobY - 4.0f;                  // 50

// Tight knob (lower-left, outside pedal)
static constexpr float kTightD   = 52.0f;
static constexpr float kTightX   = 16.0f;
static constexpr float kTightY   = 272.0f;

// Oversampling selector — single cycling button below the Mode panel.
// "OS:" label (24 px) + one button (30 px) aligned to the MODE panel left edge.
static constexpr int   kOsLabelX = 462;   // flush with MODE panel left edge
static constexpr int   kOsX      = 486;   // button starts after "OS:" label
static constexpr int   kOsY      = 352;   // 24 px below MODE panel bottom
static constexpr int   kOsW      = 30;    // single cycling button
static constexpr int   kOsH      = 18;

// Gate threshold knob — lower-left, below the TIGHT knob + its label.
// Same size as the TIGHT knob so the two outer-left controls match visually.
static constexpr float kGateKnobD = 40.0f;
static constexpr float kGateKnobX = 16.0f;
static constexpr float kGateKnobY = 346.0f;

// Mode panel (lower-right, outside pedal) — component bounds
static constexpr int   kModePX   = 462;
static constexpr int   kModePY   = 256;
static constexpr int   kModePW   = 114;
static constexpr int   kModePH   = 72;

// Circuit selector (Ember/Burner/Blazer) — above the MODE panel.
static constexpr int   kCircuitPX = 462;
static constexpr int   kCircuitPY = 168;
static constexpr int   kCircuitPW = 114;
static constexpr int   kCircuitPH = 60;

// Burner-only controls. Gain/Volume reuse the DRIVE/LEVEL knob slots
// (never shown at the same time as Ember's own knobs).
static constexpr float kBurnerRowY  = kToneY;      // Bass/Mid/Treble row
static constexpr float kBurnerKnobD = 58.0f;
static constexpr float kBurnerBassX    = kPedalX + 51.0f;                         // 157
static constexpr float kBurnerMidX     = kPedalCX - kBurnerKnobD * 0.5f;          // 266
static constexpr float kBurnerTrebleX  = kPedalX + kPedalW - 51.0f - kBurnerKnobD; // 375

// Aggression toggle — reuses the TIGHT knob's outer-left slot.
static constexpr int kAggressionX = 16;
static constexpr int kAggressionY = 268;
static constexpr int kAggressionW = 80;
static constexpr int kAggressionH = 52;

// Blazer-only controls. Gain/Volume reuse the DRIVE/LEVEL knob slots (same
// as Burner). Bass/Treble/Presence share Burner's 3-knob row slots exactly
// (Bass/Mid/Treble x-positions), since only one mode's knobs are ever
// visible at once. Tight reuses Ember's own TIGHT knob slot — same control
// concept, same position. Trim is a small "advanced" knob reusing the GATE
// knob's outer-left slot, sized down to read as secondary.
static constexpr float kBlazerTrimD = 30.0f;   // smaller than a main knob
static constexpr float kBlazerTrimX = kGateKnobX + (kGateKnobD - kBlazerTrimD) * 0.5f;
static constexpr float kBlazerTrimY = kGateKnobY + (kGateKnobD - kBlazerTrimD) * 0.5f;

// ============================================================================
// Letter-spaced text helper
// Draws each character individually with extra spacing between them,
// centred horizontally inside `bounds`.
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

// ============================================================================
// Constructor
// ============================================================================
SuhrRiotEditor::SuhrRiotEditor (SuhrRiotProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      modeSwitch   (p.apvts, laf),
      driveAtt        (p.apvts, "drive",      driveKnob),
      toneAtt         (p.apvts, "tone",        toneKnob),
      levelAtt        (p.apvts, "level",       levelKnob),
      inputTrimAtt    (p.apvts, "inputTrim",   inputTrimKnob),
      outputTrimAtt   (p.apvts, "outputTrim",  outputTrimKnob),
      tightAtt        (p.apvts, "tightFreq",   tightKnob),
      footswitchAtt   (p.apvts, "active",      footswitch),
      osSwitch        (p.apvts, laf),
      gateAtt         (p.apvts, "gateThresh",  gateKnob),
      circuitSwitch   (p.apvts, laf),
      aggressionSwitch (p.apvts, laf),
      burnerGainAtt    (p.apvts, "burnerGain",   burnerGainKnob),
      burnerVolumeAtt  (p.apvts, "burnerVolume", burnerVolumeKnob),
      burnerBassAtt    (p.apvts, "burnerBass",   burnerBassKnob),
      burnerMidAtt     (p.apvts, "burnerMid",    burnerMidKnob),
      burnerTrebleAtt  (p.apvts, "burnerTreble", burnerTrebleKnob),
      blazerGainAtt     (p.apvts, "blazerGain",     blazerGainKnob),
      blazerVolumeAtt   (p.apvts, "blazerVolume",   blazerVolumeKnob),
      blazerBassAtt     (p.apvts, "blazerBass",     blazerBassKnob),
      blazerTrebleAtt   (p.apvts, "blazerTreble",   blazerTrebleKnob),
      blazerPresenceAtt (p.apvts, "blazerPresence", blazerPresenceKnob),
      blazerTightAtt    (p.apvts, "blazerTight",    blazerTightKnob),
      blazerTrimAtt     (p.apvts, "blazerTrim",     blazerTrimKnob)
{
    // Rotary knobs
    for (auto* s : { &driveKnob, &toneKnob, &levelKnob,
                     &inputTrimKnob, &outputTrimKnob, &tightKnob, &gateKnob })
    {
        s->setLookAndFeel (&laf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }

    // Footswitch — chrome stomp button
    footswitch.setComponentID ("footswitch");
    footswitch.setLookAndFeel (&laf);
    footswitch.setClickingTogglesState (true);
    addAndMakeVisible (footswitch);

    // dB readout labels — Inter Regular 9.5pt, near-white at 70% alpha
    const auto dbFont = laf.regularFont (9.5f);
    for (auto* lbl : { &inputTrimDbLabel, &outputTrimDbLabel })
    {
        lbl->setFont (dbFont);
        lbl->setColour (juce::Label::textColourId,
                        juce::Colour (PedalLookAndFeel::kText).withAlpha (0.7f));
        lbl->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*lbl);
    }
    inputTrimDbLabel .setText ("0.0 dB", juce::dontSendNotification);
    outputTrimDbLabel.setText ("0.0 dB", juce::dontSendNotification);

    addAndMakeVisible (modeSwitch);
    addAndMakeVisible (osSwitch);

    // Burner-only rotary knobs — use BurnerLookAndFeel (violet accent).
    for (auto* s : { &burnerGainKnob, &burnerVolumeKnob, &burnerBassKnob,
                     &burnerMidKnob, &burnerTrebleKnob })
    {
        s->setLookAndFeel (&burnerLaf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }

    addAndMakeVisible (circuitSwitch);
    addAndMakeVisible (aggressionSwitch);

    // Blazer-only rotary knobs — use BlazerLookAndFeel (gold accent).
    for (auto* s : { &blazerGainKnob, &blazerVolumeKnob, &blazerBassKnob,
                     &blazerTrebleKnob, &blazerPresenceKnob, &blazerTightKnob,
                     &blazerTrimKnob })
    {
        s->setLookAndFeel (&blazerLaf);
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }

    startTimerHz (15);
    setSize (kW, kH);
}

SuhrRiotEditor::~SuhrRiotEditor()
{
    stopTimer();
    for (auto* s : { &driveKnob, &toneKnob, &levelKnob,
                     &inputTrimKnob, &outputTrimKnob, &tightKnob, &gateKnob })
        s->setLookAndFeel (nullptr);
    footswitch.setLookAndFeel (nullptr);

    for (auto* s : { &burnerGainKnob, &burnerVolumeKnob, &burnerBassKnob,
                     &burnerMidKnob, &burnerTrebleKnob })
        s->setLookAndFeel (nullptr);

    for (auto* s : { &blazerGainKnob, &blazerVolumeKnob, &blazerBassKnob,
                     &blazerTrebleKnob, &blazerPresenceKnob, &blazerTightKnob,
                     &blazerTrimKnob })
        s->setLookAndFeel (nullptr);
}

// ============================================================================
// Timer — 15 Hz: updates dB readouts and repaints LED
// ============================================================================
void SuhrRiotEditor::timerCallback()
{
    auto fmtDb = [](float db) -> juce::String
    {
        return juce::String (db, 1) + " dB";
    };

    const float inTrim  = processorRef.apvts.getRawParameterValue ("inputTrim") ->load();
    const float outTrim = processorRef.apvts.getRawParameterValue ("outputTrim")->load();

    inputTrimDbLabel .setText (fmtDb (inTrim),  juce::dontSendNotification);
    outputTrimDbLabel.setText (fmtDb (outTrim), juce::dontSendNotification);

    // -------------------------------------------------------------------
    // Circuit-mode visibility routing. Ember's own components are never
    // modified — only shown/hidden depending on which circuit is active.
    // -------------------------------------------------------------------
    const int circuit = static_cast<int> (processorRef.apvts.getRawParameterValue ("circuit")->load());
    if (circuit != lastEditorCircuit)
    {
        lastEditorCircuit = circuit;
        const bool emberActive  = (circuit == 0);
        const bool burnerActive = (circuit == 1);
        const bool blazerActive = (circuit == 2);

        // Ember-only controls
        for (auto* c : { (juce::Component*) &driveKnob, (juce::Component*) &toneKnob,
                         (juce::Component*) &levelKnob, (juce::Component*) &tightKnob,
                         (juce::Component*) &gateKnob,  (juce::Component*) &modeSwitch,
                         (juce::Component*) &osSwitch,  (juce::Component*) &inputTrimKnob,
                         (juce::Component*) &outputTrimKnob,
                         (juce::Component*) &inputTrimDbLabel,
                         (juce::Component*) &outputTrimDbLabel })
            c->setVisible (emberActive);

        // Burner-only controls
        for (auto* c : { (juce::Component*) &burnerGainKnob, (juce::Component*) &burnerVolumeKnob,
                         (juce::Component*) &burnerBassKnob, (juce::Component*) &burnerMidKnob,
                         (juce::Component*) &burnerTrebleKnob, (juce::Component*) &aggressionSwitch })
            c->setVisible (burnerActive);

        // Blazer-only controls
        for (auto* c : { (juce::Component*) &blazerGainKnob, (juce::Component*) &blazerVolumeKnob,
                         (juce::Component*) &blazerBassKnob, (juce::Component*) &blazerTrebleKnob,
                         (juce::Component*) &blazerPresenceKnob, (juce::Component*) &blazerTightKnob,
                         (juce::Component*) &blazerTrimKnob })
            c->setVisible (blazerActive);

        // Footswitch (bypass) and CircuitSwitch itself stay visible always.
    }

    repaint();
}

// ============================================================================
// Paint
// ============================================================================
void SuhrRiotEditor::paint (juce::Graphics& g)
{
    // -----------------------------------------------------------------------
    // 1. Background — flat dark + purple radial bloom (no patterns, no art)
    // -----------------------------------------------------------------------
    laf.drawPluginBackground (g, getWidth(), getHeight());

    const int circuit      = static_cast<int> (processorRef.apvts.getRawParameterValue ("circuit")->load());
    const bool emberActive  = (circuit == 0);
    const bool burnerActive = (circuit == 1);
    const bool blazerActive = (circuit == 2);

    // Burner/Blazer colorway: subtle accent-tinted bloom overlay on top of
    // the shared black background (does not touch drawPluginBackground
    // itself, which stays Ember's original purple bloom).
    if (burnerActive || blazerActive)
    {
        const uint32_t accent = burnerActive ? BurnerLookAndFeel::kAccent : BlazerLookAndFeel::kAccent;
        const float cx = (float) getWidth() * 0.5f;
        const float cy = (float) getHeight() * 0.48f;
        const float radius = (float) juce::jmax (getWidth(), getHeight()) * 0.65f;
        juce::ColourGradient bloom (
            juce::Colour (accent).withAlpha (0.05f), cx, cy,
            juce::Colour (accent).withAlpha (0.0f), cx, cy + radius,
            true);
        g.setGradientFill (bloom);
        g.fillRect (0, 0, getWidth(), getHeight());
    }

    // -----------------------------------------------------------------------
    // 2. Parameter labels — Inter Medium 10pt, near-white
    // -----------------------------------------------------------------------
    const auto lblFont = laf.uiFont (10.0f);
    const juce::Colour lblCol (PedalLookAndFeel::kText);

    g.setFont (lblFont);
    g.setColour (lblCol);

    if (emberActive)
    {
        g.drawText ("DRIVE",
                    (int) kDistX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("LEVEL",
                    (int) kLevelX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("TONE",
                    (int) kToneX, (int) (kToneY - 15.0f), (int) kToneD, 13,
                    juce::Justification::centred);
    }
    else if (burnerActive)
    {
        g.drawText ("GAIN",
                    (int) kDistX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("VOLUME",
                    (int) kLevelX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("BASS",
                    (int) kBurnerBassX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);
        g.drawText ("MID",
                    (int) kBurnerMidX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);
        g.drawText ("TREBLE",
                    (int) kBurnerTrebleX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);
    }
    else if (blazerActive)
    {
        g.drawText ("GAIN",
                    (int) kDistX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("VOLUME",
                    (int) kLevelX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("BASS",
                    (int) kBurnerBassX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);
        g.drawText ("TREBLE",
                    (int) kBurnerMidX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);
        g.drawText ("PRES",
                    (int) kBurnerTrebleX, (int) (kBurnerRowY - 15.0f), (int) kBurnerKnobD, 13,
                    juce::Justification::centred);

        g.drawText ("TIGHT",
                    (int) kTightX, (int) (kTightY - 15.0f), (int) kTightD, 13,
                    juce::Justification::centred);

        // Trim label — small, dim, "advanced parameter" treatment.
        g.setFont (laf.regularFont (7.5f));
        g.setColour (lblCol.withAlpha (0.5f));
        g.drawText ("TRIM",
                    (int) kBlazerTrimX - 5, (int) (kBlazerTrimY + kBlazerTrimD + 2.0f),
                    (int) kBlazerTrimD + 10, 10,
                    juce::Justification::centred);
        g.setFont (lblFont);
        g.setColour (lblCol);
    }


    // -----------------------------------------------------------------------
    // 3. LED dot — reads "active" parameter; repaints via 15 Hz timer
    // -----------------------------------------------------------------------
    {
        const bool  on = processorRef.apvts.getRawParameterValue ("active")->load() > 0.5f;
        const float r  = 5.0f;

        if (on)
        {
            g.setColour (juce::Colour (0x33aa44ff));
            g.fillEllipse (kLedCX - r - 3.0f, kLedCY - r - 3.0f,
                           (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);

            juce::ColourGradient glow (
                juce::Colour (0xffee99ff), kLedCX, kLedCY,
                juce::Colour (0xff7722cc), kLedCX + r, kLedCY + r, true);
            g.setGradientFill (glow);
            g.fillEllipse (kLedCX - r, kLedCY - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a0828));
            g.fillEllipse (kLedCX - r, kLedCY - r, r * 2.0f, r * 2.0f);
            g.setColour (juce::Colour (0xff2e1042));
            g.drawEllipse (kLedCX - r, kLedCY - r, r * 2.0f, r * 2.0f, 0.7f);
        }
    }

    // -----------------------------------------------------------------------
    // 4. SPARKER nameplate + DISTORTION subtitle (below footswitch)
    // -----------------------------------------------------------------------
    {
        const float nameplateY = kFootY + kFootD + 10.0f;

        // "SPARKER" — Barlow Condensed ExtraBold 24pt, letter-spaced
        g.setFont (laf.brandFont (24.0f));
        g.setColour (juce::Colours::white);
        drawLetterSpaced (g, "SPARKER",
                          { kPedalX, nameplateY, kPedalW, 30.0f }, 4.0f);

        // "DISTORTION" — Barlow Condensed Light 11.5pt, dimmed purple
        g.setFont (laf.barlowLight (11.5f));
        g.setColour (juce::Colour (0xff9977cc));
        drawLetterSpaced (g, "DISTORTION",
                          { kPedalX, nameplateY + 32.0f, kPedalW, 14.0f }, 2.0f);
    }

    // -----------------------------------------------------------------------
    // 5. KHYLE AUDIO — Barlow Condensed ExtraBold 14pt, upper center
    // -----------------------------------------------------------------------
    g.setFont (laf.brandFont (14.0f));
    g.setColour (juce::Colour (PedalLookAndFeel::kText).withAlpha (0.65f));
    g.drawText ("KHYLE AUDIO",
                (int) kPedalCX - 70, (int) (kTopKnobY + kTopKnobD * 0.40f), 140, 16,
                juce::Justification::centred);

    // -----------------------------------------------------------------------
    // 6. Outer-area labels — Inter Medium 10pt (Ember-only: these describe
    //    Ember-only components, which are hidden outside Ember mode)
    // -----------------------------------------------------------------------
    if (emberActive)
    {
        g.setFont (lblFont);
        g.setColour (lblCol);

        g.drawText ("INPUT",
                    (int) kTrimLX, (int) (kTrimY + kTrimD + 6.0f), (int) kTrimD, 12,
                    juce::Justification::centred);

        g.drawText ("OUTPUT",
                    (int) kTrimRX, (int) (kTrimY + kTrimD + 6.0f), (int) kTrimD, 12,
                    juce::Justification::centred);

        g.drawText ("TIGHT",
                    (int) kTightX, (int) (kTightY + kTightD + 6.0f), (int) kTightD, 12,
                    juce::Justification::centred);

        g.setFont (laf.semiBoldFont (9.0f));
        g.setColour (juce::Colour (0xff7755aa));
        g.drawText ("OS:",
                    kOsLabelX, kOsY, 24, kOsH,
                    juce::Justification::centredLeft);

        g.setFont (lblFont);
        g.setColour (lblCol);
        g.drawText ("GATE",
                    (int) kGateKnobX, (int) (kGateKnobY + kGateKnobD + 6.0f),
                    (int) kGateKnobD, 12,
                    juce::Justification::centred);

        // -------------------------------------------------------------------
        // 7. dB tick-mark scales beside the trim knobs (colors unchanged)
        // -------------------------------------------------------------------
        {
            const float scaleX   = kTrimLX + kTrimD + 9.0f;
            const float scaleTop = kTrimY + 5.0f;
            const float scaleBot = kTrimY + kTrimD - 5.0f;
            const float scaleH   = scaleBot - scaleTop;
            const juce::Colour scaleCol (0xff3d2a55);

            g.setColour (scaleCol);
            g.drawLine (scaleX, scaleTop, scaleX, scaleBot, 0.6f);

            const float dbs[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
            for (float db : dbs)
            {
                const float t   = (db + 12.0f) / 24.0f;
                const float ty  = scaleTop + t * scaleH;
                const float len = (db == 0.0f) ? 7.0f : 4.5f;
                g.drawLine (scaleX, ty, scaleX + len, ty, 0.7f);
            }
        }

        {
            const float scaleX   = kTrimRX - 9.0f;
            const float scaleTop = kTrimY + 5.0f;
            const float scaleBot = kTrimY + kTrimD - 5.0f;
            const float scaleH   = scaleBot - scaleTop;
            const juce::Colour scaleCol (0xff3d2a55);

            g.setColour (scaleCol);
            g.drawLine (scaleX, scaleTop, scaleX, scaleBot, 0.6f);

            const float dbs[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
            for (float db : dbs)
            {
                const float t   = (db + 12.0f) / 24.0f;
                const float ty  = scaleTop + t * scaleH;
                const float len = (db == 0.0f) ? 7.0f : 4.5f;
                g.drawLine (scaleX - len, ty, scaleX, ty, 0.7f);
            }
        }
    }
}

// ============================================================================
// Layout
// ============================================================================
void SuhrRiotEditor::resized()
{
    // Inner top-row knobs
    driveKnob .setBounds ((int) kDistX,  (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);
    levelKnob .setBounds ((int) kLevelX, (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);

    // TONE knob
    toneKnob  .setBounds ((int) kToneX,  (int) kToneY,    (int) kToneD,    (int) kToneD);

    // Footswitch
    footswitch.setBounds ((int) kFootX, (int) kFootY, (int) kFootD, (int) kFootD);

    // Outer trim knobs
    inputTrimKnob .setBounds ((int) kTrimLX, (int) kTrimY, (int) kTrimD, (int) kTrimD);
    outputTrimKnob.setBounds ((int) kTrimRX, (int) kTrimY, (int) kTrimD, (int) kTrimD);

    // dB readout labels (below INPUT / OUTPUT text)
    const int dbLblW = (int) kTrimD + 8;
    inputTrimDbLabel .setBounds ((int) kTrimLX - 4, (int) (kTrimY + kTrimD + 20), dbLblW, 12);
    outputTrimDbLabel.setBounds ((int) kTrimRX - 4, (int) (kTrimY + kTrimD + 20), dbLblW, 12);

    // Tight knob
    tightKnob.setBounds ((int) kTightX, (int) kTightY, (int) kTightD, (int) kTightD);

    // Oversampling selector
    osSwitch.setBounds (kOsX, kOsY, kOsW, kOsH);

    // Gate threshold knob
    gateKnob.setBounds ((int) kGateKnobX, (int) kGateKnobY,
                        (int) kGateKnobD, (int) kGateKnobD);

    // Mode panel
    modeSwitch.setBounds (kModePX, kModePY, kModePW, kModePH);

    // Circuit selector
    circuitSwitch.setBounds (kCircuitPX, kCircuitPY, kCircuitPW, kCircuitPH);

    // Burner-only controls — Gain/Volume reuse the DRIVE/LEVEL slots.
    burnerGainKnob  .setBounds ((int) kDistX,  (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);
    burnerVolumeKnob.setBounds ((int) kLevelX, (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);
    burnerBassKnob  .setBounds ((int) kBurnerBassX,   (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);
    burnerMidKnob   .setBounds ((int) kBurnerMidX,    (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);
    burnerTrebleKnob.setBounds ((int) kBurnerTrebleX, (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);

    aggressionSwitch.setBounds (kAggressionX, kAggressionY, kAggressionW, kAggressionH);

    // Blazer-only controls
    blazerGainKnob    .setBounds ((int) kDistX,  (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);
    blazerVolumeKnob  .setBounds ((int) kLevelX, (int) kTopKnobY, (int) kTopKnobD, (int) kTopKnobD);
    blazerBassKnob    .setBounds ((int) kBurnerBassX,   (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);
    blazerTrebleKnob  .setBounds ((int) kBurnerMidX,    (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);
    blazerPresenceKnob.setBounds ((int) kBurnerTrebleX, (int) kBurnerRowY, (int) kBurnerKnobD, (int) kBurnerKnobD);
    blazerTightKnob   .setBounds ((int) kTightX, (int) kTightY, (int) kTightD, (int) kTightD);
    blazerTrimKnob    .setBounds ((int) kBlazerTrimX, (int) kBlazerTrimY, (int) kBlazerTrimD, (int) kBlazerTrimD);
}
