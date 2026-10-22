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

// Matches AI Studio's BurnerPage.tsx AGGRESSION selector exactly: bg
// #0d172a, border #1e293b @80%, each cell uses its own fixed channel color
// (BLUE=#38bdf8, RED=#f43f5e) rather than a shared accent.
void AggressionSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumStates;

    const float pad  = 3.0f;
    const float btnH = h; // "AGGRESSION" label removed — button row now fills the full component

    const juce::Colour cellColours[kNumStates] = { juce::Colour (0xff38bdf8),   // BLUE
                                                    juce::Colour (0xfff43f5e) }; // RED

    const auto fMed = laf.uiFont (10.0f, true); // bumped 9->10 for OpenDyslexic legibility

    g.setColour (juce::Colour (0xff0d172a));
    g.fillRoundedRectangle (0.0f, 0.0f, w, btnH, 8.0f);
    g.setColour (juce::Colour (0xff1e293b).withAlpha (0.8f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, btnH - 1.0f, 8.0f, 1.0f);

    for (int i = 0; i < kNumStates; ++i)
    {
        const float bx  = i * segW + pad;
        const float by  = pad;
        const float bw  = segW - 2.0f * pad;
        const float bh  = btnH - 2.0f * pad;
        const bool  sel = (i == currentState);
        const juce::Colour colour = cellColours[i];

        if (sel)
        {
            g.setColour (colour.withAlpha (0.4f));
            g.fillRoundedRectangle (bx - 1.5f, by - 1.5f, bw + 3.0f, bh + 3.0f, 6.5f);
            g.setColour (colour.withAlpha (0.2f));
            g.fillRoundedRectangle (bx, by, bw, bh, 5.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (bx, by, bw, bh, 5.0f, 1.5f);
        }

        g.setFont (fMed);
        g.setColour (sel ? colour : juce::Colour (0xff64748b));
        g.drawText (kLabels[i],
                    (int) (i * segW), (int) by, (int) segW, (int) bh,
                    juce::Justification::centred, false);
    }
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
