#include "OsSwitch.h"

OsSwitch::OsSwitch (juce::AudioProcessorValueTreeState& apvts_,
                    PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentOrder = static_cast<int> (apvts.getRawParameterValue ("osOrder")->load());
    apvts.addParameterListener ("osOrder", this);
}

OsSwitch::~OsSwitch()
{
    apvts.removeParameterListener ("osOrder", this);
}

void OsSwitch::paint (juce::Graphics& g)
{
    const float w   = (float) getWidth();
    const float h   = (float) getHeight();
    const float pad = 2.0f;
    const float bx  = pad;
    const float by  = pad;
    const float bw  = w - 2.0f * pad;
    const float bh  = h - 2.0f * pad;

    g.setColour (juce::Colour (0xff0e0c18));
    g.fillRoundedRectangle (bx, by, bw, bh, 3.0f);

    g.setColour (juce::Colour (0xff7733bb));
    g.drawRoundedRectangle (bx, by, bw, bh, 3.0f, 1.2f);

    g.setColour (juce::Colour (0x186633aa));
    g.fillRoundedRectangle (bx + 1.0f, by + 1.0f, bw - 2.0f, bh - 2.0f, 2.5f);

    g.setFont (laf.semiBoldFont (9.0f));
    g.setColour (juce::Colour (0xffccaaff));
    g.drawText (kLabels[currentOrder], 0, (int) by, (int) w, (int) bh,
                juce::Justification::centred, false);
}

void OsSwitch::mouseDown (const juce::MouseEvent& /*e*/)
{
    const int nextOrder = (currentOrder + 1) % kNum;
    if (auto* param = apvts.getParameter ("osOrder"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) nextOrder));
}

void OsSwitch::parameterChanged (const juce::String& /*paramID*/, float newValue)
{
    currentOrder = static_cast<int> (newValue);
    repaint();
}
