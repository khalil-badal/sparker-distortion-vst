#include "ModeSwitch.h"
#include <cmath>

ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& apvts_,
                        PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentMode = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    apvts.addParameterListener ("mode", this);
}

ModeSwitch::~ModeSwitch()
{
    apvts.removeParameterListener ("mode", this);
}

// ===========================================================================
// drawDiodeSymbol
//
// Standard ANSI/IEC diode symbol, centred at (midX, midY):
//
//   anode lead ——▷|—— cathode lead
//
//   WARM    (mode 0) → base symbol only
//   CLASSIC (mode 1) → base symbol only
//   HOT     (mode 2) → base + two 45° emission arrows (LED symbol)
// ===========================================================================
static void drawDiodeSymbol (juce::Graphics& g,
                              float midX, float midY,
                              int   mode,
                              juce::Colour col)
{
    const float tw   = 7.0f;
    const float th   = 7.0f;
    const float lead = 5.0f;

    const float baseX = midX - tw;
    const float tipX  = midX + tw;

    g.setColour (col);

    juce::Path tri;
    tri.startNewSubPath (baseX, midY - th);
    tri.lineTo          (baseX, midY + th);
    tri.lineTo          (tipX,  midY);
    tri.closeSubPath();
    g.fillPath (tri);

    g.drawLine (tipX, midY - th, tipX, midY + th, 1.3f);
    g.drawLine (baseX - lead, midY, baseX, midY, 1.1f);
    g.drawLine (tipX, midY, tipX + lead, midY, 1.1f);

    if (mode == 2)
    {
        const float angle   = -juce::MathConstants<float>::pi * 0.25f;
        const float arrowL  = 7.0f;
        const float headLen = 2.5f;
        const float headSpr = 0.50f;
        const float offset  = th * 0.40f;

        for (int k = 0; k < 2; ++k)
        {
            const float ay = midY - offset + (float) k * 2.0f * offset;
            const float ax = tipX + 2.0f;
            const float bx = ax + std::cos (angle) * arrowL;
            const float by = ay + std::sin (angle) * arrowL;

            g.drawLine (ax, ay, bx, by, 0.9f);

            juce::Path head;
            head.startNewSubPath (bx, by);
            head.lineTo (bx - headLen * std::cos (angle - headSpr),
                         by - headLen * std::sin (angle - headSpr));
            head.startNewSubPath (bx, by);
            head.lineTo (bx - headLen * std::cos (angle + headSpr),
                         by - headLen * std::sin (angle + headSpr));
            g.strokePath (head, juce::PathStrokeType (0.9f));
        }
    }
}

// ===========================================================================
// ModeSwitch::paint
//
// Component bounds = 104 × 110
//
// Vertical structure per button cell:
//   [  GE / SI / LED  ]   ← Inter SemiBold 10pt — top 13 px
//   [  diode symbol   ]   ← centred in remaining button space
//   WARM / CLASSIC / HOT  ← Inter Medium 9pt — sublabel row
//   ◆ MODE ◆              ← Inter SemiBold 10pt — bottom
// ===========================================================================
void ModeSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumModes;

    const float btnH    = h * 0.46f;
    const float pad     = 3.5f;
    const float lblRowH = h * 0.13f;
    const float modeRowH = h * 0.16f;

    const auto fSemi = laf.semiBoldFont (10.0f);  // GE/SI/LED + ◆ MODE ◆
    const auto fMed  = laf.uiFont       (9.0f);   // WARM/CLASSIC/HOT


    for (int i = 0; i < kNumModes; ++i)
    {
        const float bx  = i * segW + pad;
        const float by  = pad;
        const float bw  = segW - 2.0f * pad;
        const float bh  = btnH - 2.0f * pad;
        const bool  sel = (i == currentMode);

        // Button background
        g.setColour (juce::Colour (0xff0e0c18));
        g.fillRoundedRectangle (bx, by, bw, bh, 4.0f);

        if (sel)
        {
            g.setColour (juce::Colour (0xff7733bb));
            g.drawRoundedRectangle (bx, by, bw, bh, 4.0f, 1.4f);
            g.setColour (juce::Colour (0x186633aa));
            g.fillRoundedRectangle (bx + 1.0f, by + 1.0f, bw - 2.0f, bh - 2.0f, 3.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a1428));
            g.drawRoundedRectangle (bx, by, bw, bh, 4.0f, 0.8f);
        }

        // GE / SI / LED — Inter SemiBold 10pt, top of button
        const float idH = 13.0f;
        g.setFont (fSemi);
        g.setColour (sel ? juce::Colour (PedalLookAndFeel::kText)
                         : juce::Colour (0xff5a4470));
        g.drawText (kLabels[i],
                    (int) bx, (int) by, (int) bw, (int) idH,
                    juce::Justification::centred, false);

        // Diode symbol — centred in area below the ID label
        const float diodeAreaTop = by + idH;
        const float diodeAreaH   = bh - idH;
        const float symX = bx + bw * 0.5f;
        const float symY = diodeAreaTop + diodeAreaH * 0.52f;
        const juce::Colour symCol = sel ? juce::Colours::white
                                        : juce::Colour (0xff443355);
        drawDiodeSymbol (g, symX, symY, i, symCol);

        // Sublabel (WARM / CLASSIC / HOT) — Inter Medium 9pt
        g.setFont (fMed);
        g.setColour (sel ? juce::Colour (0xffccaaff) : juce::Colour (0xff443355));
        g.drawText (kSubLabels[i],
                    (int) (i * segW), (int) btnH, (int) segW, (int) lblRowH,
                    juce::Justification::centred, false);
    }

    // "◆ MODE ◆" — Inter SemiBold 10pt
    g.setFont (fSemi);
    g.setColour (juce::Colour (0xff7755aa));
    g.drawText ("MODE",
                0, (int) (h * 0.62f), (int) w, (int) modeRowH,
                juce::Justification::centred, false);
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int clicked = (int) (e.position.x / (getWidth() / (float) kNumModes));
    const int mode    = juce::jlimit (0, kNumModes - 1, clicked);

    if (mode == currentMode)
        return;

    if (auto* param = apvts.getParameter ("mode"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) mode));
}

void ModeSwitch::parameterChanged (const juce::String& /*paramID*/, float newValue)
{
    currentMode = static_cast<int> (newValue);
    repaint();
}
