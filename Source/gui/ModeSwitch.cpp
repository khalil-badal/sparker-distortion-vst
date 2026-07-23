#include "ModeSwitch.h"

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
// ModeSwitch::paint  (114 × 72 component)
//
// Each button background uses the padded rect; the text label spans the
// full segment width so "CLASSIC" is never clipped.
// ===========================================================================
void ModeSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumModes;

    const float pad      = 3.0f;
    const float btnH     = h * 0.74f;
    const float modeRowH = h - btnH;

    const auto fMed  = laf.uiFont       (9.0f);
    const auto fSemi = laf.semiBoldFont (10.0f);

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

        // Label spans full cell width so "CLASSIC" never gets clipped
        g.setFont (fMed);
        g.setColour (sel ? juce::Colour (0xffccaaff) : juce::Colour (0xff443355));
        g.drawText (kLabels[i],
                    (int) (i * segW), (int) by, (int) segW, (int) bh,
                    juce::Justification::centred, false);
    }

    // "MODE" below the button row
    g.setFont (fSemi);
    g.setColour (juce::Colour (0xff7755aa));
    g.drawText ("MODE",
                0, (int) btnH, (int) w, (int) modeRowH,
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
