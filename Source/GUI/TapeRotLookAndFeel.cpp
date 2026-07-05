#include "TapeRotLookAndFeel.h"

TapeRotLookAndFeel::TapeRotLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, TapeRotTheme::Colour::panelTop);
    setColour(juce::Slider::thumbColourId, TapeRotTheme::Colour::amber);
    setColour(juce::Slider::rotarySliderFillColourId, TapeRotTheme::Colour::amber);
    setColour(juce::Slider::rotarySliderOutlineColourId, TapeRotTheme::Colour::rim);
    setColour(juce::ComboBox::backgroundColourId, TapeRotTheme::Colour::dark);
    setColour(juce::ComboBox::textColourId, TapeRotTheme::Colour::cream);
    setColour(juce::Label::textColourId, TapeRotTheme::Colour::ink);
}

void TapeRotLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float, float,
                                          juce::Slider&)
{
    using namespace TapeRotTheme;

    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const float radius = Layout::knobRadius;

    for (int i = 0; i < Layout::knobNumTicks; ++i)
    {
        const float angle = Layout::knobArcStartDegrees
            + (float) i / (float) (Layout::knobNumTicks - 1)
              * (Layout::knobArcEndDegrees - Layout::knobArcStartDegrees);
        const auto inner = pointOnCircle(centre, Layout::knobTickInnerRadius, angle);
        const auto outer = pointOnCircle(centre, Layout::knobTickOuterRadius, angle);
        g.setColour(Colour::tick);
        g.drawLine({inner, outer}, 1.4f);
    }

    {
        juce::Path facePath;
        facePath.addEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 4, {0, 2});
        shadow.drawForPath(g, facePath);
    }

    juce::ColourGradient faceGradient(Colour::knobFaceTop, centre.x - radius * 0.24f, centre.y - radius * 0.36f,
                                       Colour::knobFaceBottom, centre.x + radius, centre.y + radius, true);
    g.setGradientFill(faceGradient);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(Colour::rim);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.5f);

    g.setColour(Colour::innerRing);
    g.drawEllipse(centre.x - Layout::knobInnerRingRadius, centre.y - Layout::knobInnerRingRadius,
                  Layout::knobInnerRingRadius * 2.0f, Layout::knobInnerRingRadius * 2.0f, 1.0f);

    const float pointerAngle = knobAngleForValue01(sliderPosProportional);
    const auto pointerStart = pointOnCircle(centre, Layout::knobPointerInnerRadius, pointerAngle);
    const auto pointerEnd = pointOnCircle(centre, Layout::knobPointerOuterRadius, pointerAngle);
    g.setColour(Colour::amber);
    g.drawLine({pointerStart, pointerEnd}, 4.0f);
}

std::unique_ptr<juce::Slider> TapeRotLookAndFeel::createKnobSlider(const juce::String& label)
{
    auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                  juce::Slider::NoTextBox);
    slider->setName(label);
    slider->setRotaryParameters(juce::degreesToRadians(TapeRotTheme::Layout::knobArcStartDegrees),
                                 juce::degreesToRadians(TapeRotTheme::Layout::knobArcEndDegrees),
                                 true);
    return slider;
}
