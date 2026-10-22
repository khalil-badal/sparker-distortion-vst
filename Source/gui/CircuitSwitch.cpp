#include "CircuitSwitch.h"
#include "PluginColors.h"

CircuitSwitch::CircuitSwitch (juce::AudioProcessorValueTreeState& apvts_,
                               PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
    currentCircuit = static_cast<int> (apvts.getRawParameterValue ("circuit")->load());
    apvts.addParameterListener ("circuit", this);
    apvts.addParameterListener ("burnerAggression", this); // badge color depends on aggression when Burner is active
}

CircuitSwitch::~CircuitSwitch()
{
    apvts.removeParameterListener ("circuit", this);
    apvts.removeParameterListener ("burnerAggression", this);
}

void CircuitSwitch::paint (juce::Graphics& g)
{
    const int aggression = static_cast<int> (apvts.getRawParameterValue ("burnerAggression")->load());
    const auto theme = PluginColors::themeFor (currentCircuit, aggression);

    // The pill is inset within the component's own bounds (not just
    // reduced by 1px) so the glow below has room to expand and fade
    // WITHOUT hitting the component's edge — JUCE hard-clips all drawing
    // at a component's boundary, so a glow that expands past
    // getLocalBounds() gets sliced off in a straight line instead of
    // fading, which is what read as a "boxy" artifact around the badge.
    constexpr float kGlowMargin = 6.0f;
    auto bounds = getLocalBounds().toFloat().reduced (kGlowMargin);
    const float corner = bounds.getHeight() * 0.32f;

    // Soft glow halo behind the badge (mockup: box-shadow 0 0 14px
    // primaryGlow), sized to stay within kGlowMargin so it never reaches
    // the component's clip edge.
    for (int i = 6; i >= 1; --i)
    {
        const float t = (float) i / 6.0f;
        g.setColour (theme.primary.withAlpha (0.06f * t));
        g.fillRoundedRectangle (bounds.expanded (t * (kGlowMargin - 1.0f)), corner + t * (kGlowMargin - 1.0f) * 0.75f);
    }

    // Fill: primary at ~13% alpha ("22" hex suffix in the mockup)
    g.setColour (theme.primary.withAlpha (0.13f));
    g.fillRoundedRectangle (bounds, corner);

    // Border: primary at ~38% alpha ("60" hex suffix)
    g.setColour (theme.primary.withAlpha (0.38f));
    g.drawRoundedRectangle (bounds, corner, 1.4f);

    g.setFont (laf.uiFont (11.0f, true));
    g.setColour (theme.accent);
    g.drawText (kLabels[currentCircuit], getLocalBounds(), juce::Justification::centred, false);
}

void CircuitSwitch::mouseDown (const juce::MouseEvent&)
{
    // Cycles forward through EMBER -> BURNER -> BLAZER -> EMBER, matching
    // PluginHeader.tsx's handleToggleCircuit.
    const int next = (currentCircuit + 1) % kNumCircuits;

    if (auto* param = apvts.getParameter ("circuit"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
}

void CircuitSwitch::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "circuit")
        currentCircuit = static_cast<int> (newValue);
    repaint();
}
