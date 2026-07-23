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
      gateAtt         (p.apvts, "gateThresh",  gateKnob)
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

    // -----------------------------------------------------------------------
    // 2. Parameter labels — Inter Medium 10pt, near-white
    // -----------------------------------------------------------------------
    const auto lblFont = laf.uiFont (10.0f);
    const juce::Colour lblCol (PedalLookAndFeel::kText);

    g.setFont (lblFont);
    g.setColour (lblCol);

    g.drawText ("DRIVE",
                (int) kDistX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                juce::Justification::centred);

    g.drawText ("LEVEL",
                (int) kLevelX, (int) (kTopKnobY - 15.0f), (int) kTopKnobD, 13,
                juce::Justification::centred);

    g.drawText ("TONE",
                (int) kToneX, (int) (kToneY - 15.0f), (int) kToneD, 13,
                juce::Justification::centred);


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
    // 6. Outer-area labels — Inter Medium 10pt
    // -----------------------------------------------------------------------
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

    // -----------------------------------------------------------------------
    // 7. dB tick-mark scales beside the trim knobs (colors unchanged)
    // -----------------------------------------------------------------------
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
}
