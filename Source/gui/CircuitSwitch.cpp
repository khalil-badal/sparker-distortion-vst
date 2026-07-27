#include "CircuitSwitch.h"

CircuitSwitch::CircuitSwitch (juce::AudioProcessorValueTreeState& apvts_,
                               PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentCircuit = static_cast<int> (apvts.getRawParameterValue ("circuit")->load());
    apvts.addParameterListener ("circuit", this);
}

CircuitSwitch::~CircuitSwitch()
{
    apvts.removeParameterListener ("circuit", this);
}

void CircuitSwitch::paint (juce::Graphics& g)
{
    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumCircuits;

    const float pad      = 3.0f;
    const float btnH     = h * 0.74f;
    const float labelRowH = h - btnH;

    const auto fMed  = laf.uiFont       (9.0f);
    const auto fSemi = laf.semiBoldFont (10.0f);

    for (int i = 0; i < kNumCircuits; ++i)
    {
        const float bx  = i * segW + pad;
        const float by  = pad;
        const float bw  = segW - 2.0f * pad;
        const float bh  = btnH - 2.0f * pad;
        const bool  sel = (i == currentCircuit);
        const juce::Colour accent (kAccentColours[i]);

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
    g.drawText ("CIRCUIT",
                0, (int) btnH, (int) w, (int) labelRowH,
                juce::Justification::centred, false);
}

void CircuitSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int clicked = (int) (e.position.x / (getWidth() / (float) kNumCircuits));
    const int circuit = juce::jlimit (0, kNumCircuits - 1, clicked);

    if (circuit == currentCircuit)
        return;

    if (auto* param = apvts.getParameter ("circuit"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) circuit));
}

void CircuitSwitch::parameterChanged (const juce::String& /*parameterID*/, float newValue)
{
    currentCircuit = static_cast<int> (newValue);
    repaint();
}
