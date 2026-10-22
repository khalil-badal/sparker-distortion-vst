#include "BurnerLookAndFeel.h"
#include "FlatKnobPainter.h"

void BurnerLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    drawFlatRotaryKnob (g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, accent);
}
