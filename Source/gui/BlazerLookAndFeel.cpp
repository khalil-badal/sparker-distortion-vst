#include "BlazerLookAndFeel.h"
#include "FlatKnobPainter.h"

void BlazerLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    drawFlatRotaryKnob (g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle,
                        juce::Colour (kAccent));
}
