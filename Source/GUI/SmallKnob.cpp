#include "SmallKnob.h"
#include "TapeRotTheme.h"

SmallKnob::SmallKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters(juce::degreesToRadians(TapeRotTheme::Layout::knobArcStartDegrees),
                         juce::degreesToRadians(TapeRotTheme::Layout::knobArcEndDegrees), true);
}

void SmallKnob::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float radius = Layout::smallKnobRadius;

    juce::ColourGradient faceGradient(Colour::knobFaceTop, centre.x - radius * 0.24f, centre.y - radius * 0.36f,
                                       Colour::knobFaceBottom, centre.x + radius, centre.y + radius, true);
    g.setGradientFill(faceGradient);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(Colour::rim);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.75f);

    const float value01 = (float) valueToProportionOfLength(getValue());
    const float pointerAngle = knobAngleForValue01(value01);
    const auto pointerStart = pointOnCircle(centre, radius * 0.3f, pointerAngle);
    const auto pointerEnd = pointOnCircle(centre, radius - 3.0f, pointerAngle);
    g.setColour(Colour::amber);
    g.drawLine({pointerStart, pointerEnd}, 2.5f);
}
