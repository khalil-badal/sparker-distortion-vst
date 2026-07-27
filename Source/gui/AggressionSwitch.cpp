#include "AggressionSwitch.h"

AggressionSwitch::AggressionSwitch (juce::AudioProcessorValueTreeState& apvts_,
                                     PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentState = static_cast<int> (apvts.getRawParameterValue ("burnerAggression")->load());
    apvts.addParameterListener ("burnerAggression", this);
}

AggressionSwitch::~AggressionSwitch()
{
    apvts.removeParameterListener ("burnerAggression", this);
}

void AggressionSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumStates;

    const float pad      = 3.0f;
    const float btnH     = h * 0.74f;
    const float labelRowH = h - btnH;
    const juce::Colour accent (0xffaa33ff); // Burner electric violet

    const auto fMed  = laf.uiFont       (9.0f);
    const auto fSemi = laf.semiBoldFont (9.0f);

    for (int i = 0; i < kNumStates; ++i)
    {
        const float bx  = i * segW + pad;
        const float by  = pad;
        const float bw  = segW - 2.0f * pad;
        const float bh  = btnH - 2.0f * pad;
        const bool  sel = (i == currentState);

        g.setColour (juce::Colour (0xff0e0c18));
        g.fillRoundedRectangle (bx, by, bw, bh, 4.0f);

        if (sel)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (bx, by, bw, bh, 4.0f, 1.4f);
            g.setColour (accent.withAlpha (0.15f));
            g.fillRoundedRectangle (bx + 1.0f, by + 1.0f, bw - 2.0f, bh - 2.0f, 3.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a1428));
            g.drawRoundedRectangle (bx, by, bw, bh, 4.0f, 0.8f);
        }

        g.setFont (fMed);
        g.setColour (sel ? accent.brighter (0.5f) : juce::Colour (0xff443355));
        g.drawText (kLabels[i],
                    (int) (i * segW), (int) by, (int) segW, (int) bh,
                    juce::Justification::centred, false);
    }

    g.setFont (fSemi);
    g.setColour (juce::Colour (0xff7755aa));
    g.drawText ("AGGR",
                0, (int) btnH, (int) w, (int) labelRowH,
                juce::Justification::centred, false);
}

void AggressionSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int clicked = (int) (e.position.x / (getWidth() / (float) kNumStates));
    const int state    = juce::jlimit (0, kNumStates - 1, clicked);

    if (state == currentState)
        return;

    if (auto* param = apvts.getParameter ("burnerAggression"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) state));
}

void AggressionSwitch::parameterChanged (const juce::String& /*parameterID*/, float newValue)
{
    currentState = static_cast<int> (newValue);
    repaint();
}
