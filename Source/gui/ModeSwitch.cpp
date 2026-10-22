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
// ModeSwitch::paint — matches AI Studio's EmberPage.tsx MODE selector
// exactly: bg #130f1e, border #2b223e @60%, active cell = accent text +
// primary-tinted bg/border + glow, inactive = slate-500 text, transparent.
// ===========================================================================
void ModeSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumModes;

    const float pad      = 3.0f;
    const float btnH     = h; // "MODE" label removed — button row now fills the full component

    const juce::Colour primary (0xffd946ef);
    const juce::Colour accent  (0xfff0abfc);
    const juce::Colour glow    = primary.withAlpha (0.35f);

    const auto fMed  = laf.uiFont (10.0f, true); // bumped 9->10 for OpenDyslexic legibility

    // Panel background
    g.setColour (juce::Colour (0xff130f1e));
    g.fillRoundedRectangle (0.0f, 0.0f, w, btnH, 8.0f);
    g.setColour (juce::Colour (0xff2b223e).withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, btnH - 1.0f, 8.0f, 1.0f);

    for (int i = 0; i < kNumModes; ++i)
    {
        const float bx  = i * segW + pad;
        const float by  = pad;
        const float bw  = segW - 2.0f * pad;
        const float bh  = btnH - 2.0f * pad;
        const bool  sel = (i == currentMode);

        if (sel)
        {
            g.setColour (glow);
            g.fillRoundedRectangle (bx - 1.5f, by - 1.5f, bw + 3.0f, bh + 3.0f, 6.5f);
            g.setColour (primary.withAlpha (0.2f));
            g.fillRoundedRectangle (bx, by, bw, bh, 5.0f);
            g.setColour (primary);
            g.drawRoundedRectangle (bx, by, bw, bh, 5.0f, 1.5f);
        }

        g.setFont (fMed);
        g.setColour (sel ? accent : juce::Colour (0xff64748b));
        g.drawText (kLabels[i],
                    (int) (i * segW), (int) by, (int) segW, (int) bh,
                    juce::Justification::centred, false);
    }
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
