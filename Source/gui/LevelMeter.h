#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// LevelMeter — 6 vertical tick bars driven by REAL signal peak level (not
// a cosmetic function of the trim knob, which was the earlier bug: it lit
// up ticks by default with no actual signal present). At rest (no signal)
// all ticks stay dark. White when a genuine signal is present, red when
// clipping (peak >= 0dBFS). Same behavior across all three circuits — no
// per-circuit theme color, per explicit request.
// ---------------------------------------------------------------------------
class LevelMeter : public juce::Component
{
public:
    // linearPeak: 0..1 = normal headroom, >=1.0 = clipping. From
    // SuhrRiotProcessor::inputPeakLevel/outputPeakLevel (atomic, updated
    // on the audio thread, polled here on the message thread via a timer).
    void setLevel (float linearPeak) noexcept
    {
        level = linearPeak;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        constexpr int kTickCount = 6;
        const float w = (float) getWidth();
        const float h = (float) getHeight();
        const float tickH = 1.5f;
        const float gapY  = (h - kTickCount * tickH) / (float) (kTickCount - 1);

        constexpr float kClipThreshold = 0.98f; // ~-0.2dBFS, matches how real meters flag clip
        constexpr float kSilenceFloor  = 0.02f; // below this reads as "no signal" (all dark)

        const bool  clipping  = level >= kClipThreshold;
        const float normLevel = juce::jlimit (0.0f, 1.0f, level);
        const juce::Colour activeColour = clipping ? juce::Colours::red : juce::Colours::white;

        for (int i = 0; i < kTickCount; ++i)
        {
            const float threshold = (float) (kTickCount - i) / (float) kTickCount;
            const bool  active = (normLevel > kSilenceFloor) && (normLevel >= threshold - 0.12f);
            // Once clipping, light every tick red — a real meter's clip
            // indicator isn't "proportional," it's a flat overload flag.
            const bool  lit = clipping || active;

            const float y = i * (tickH + gapY);
            g.setColour (lit ? activeColour : juce::Colour (0xff272235));
            g.fillRoundedRectangle (0.0f, y, w, tickH, tickH * 0.5f);
        }
    }

private:
    float level = 0.0f;
};
