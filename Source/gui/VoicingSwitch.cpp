#include "VoicingSwitch.h"
#include "PluginColors.h"

VoicingSwitch::VoicingSwitch (juce::AudioProcessorValueTreeState& apvts_,
                               PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentVoicing = static_cast<int> (apvts.getRawParameterValue ("voicing")->load());
    apvts.addParameterListener ("voicing", this);
    // The active cell is tinted with the current circuit's theme, so repaint
    // when the circuit (or Burner's aggression, which recolours it) changes.
    apvts.addParameterListener ("circuit", this);
    apvts.addParameterListener ("burnerAggression", this);
}

VoicingSwitch::~VoicingSwitch()
{
    apvts.removeParameterListener ("voicing", this);
    apvts.removeParameterListener ("circuit", this);
    apvts.removeParameterListener ("burnerAggression", this);
}

void VoicingSwitch::paint (juce::Graphics& g)
{
    const int circuit    = static_cast<int> (apvts.getRawParameterValue ("circuit")->load());
    const int aggression = static_cast<int> (apvts.getRawParameterValue ("burnerAggression")->load());
    const auto theme = PluginColors::themeFor (circuit, aggression);

    const float w    = (float) getWidth();
    const float h    = (float) getHeight();
    const float segW = w / (float) kNumStates;
    const float pad  = 3.0f;

    const auto fMed = laf.uiFont (10.0f, true);

    // Panel background + border, matching the other segmented switches.
    g.setColour (juce::Colour (0xff0d0a16));
    g.fillRoundedRectangle (0.0f, 0.0f, w, h, 8.0f);
    g.setColour (juce::Colour (0xff241b38).withAlpha (0.8f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 8.0f, 1.0f);

    for (int i = 0; i < kNumStates; ++i)
    {
        const float bx = i * segW + pad;
        const float by = pad;
        const float bw = segW - 2.0f * pad;
        const float bh = h - 2.0f * pad;
        const bool  sel = (i == currentVoicing);

        if (sel)
        {
            g.setColour (theme.primary.withAlpha (0.35f));
            g.fillRoundedRectangle (bx - 1.5f, by - 1.5f, bw + 3.0f, bh + 3.0f, 6.5f);
            g.setColour (theme.primary.withAlpha (0.2f));
            g.fillRoundedRectangle (bx, by, bw, bh, 5.0f);
            g.setColour (theme.primary);
            g.drawRoundedRectangle (bx, by, bw, bh, 5.0f, 1.5f);
        }

        g.setFont (fMed);
        g.setColour (sel ? theme.accent : juce::Colour (PluginColors::kInactiveTabText));
        g.drawText (kLabels[i], (int) (i * segW), (int) by, (int) segW, (int) bh,
                    juce::Justification::centred, false);
    }
}

void VoicingSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int clicked = (int) (e.position.x / (getWidth() / (float) kNumStates));
    const int state   = juce::jlimit (0, kNumStates - 1, clicked);

    if (state == currentVoicing)
        return;

    if (auto* param = apvts.getParameter ("voicing"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) state));
}

void VoicingSwitch::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "voicing")
        currentVoicing = static_cast<int> (newValue);

    repaint();   // circuit/aggression changes only affect the tint
}
