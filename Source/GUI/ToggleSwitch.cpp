#include "ToggleSwitch.h"
#include "TapeRotTheme.h"

ToggleSwitch::ToggleSwitch(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(true);
}

void ToggleSwitch::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    const float centreY = (float) getHeight() * 0.5f;
    const juce::Rectangle<float> track(0.0f, centreY - Layout::switchH * 0.5f, Layout::switchW, Layout::switchH);

    g.setColour(Colour::dark);
    g.fillRoundedRectangle(track, Layout::switchRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(track, Layout::switchRadius, 1.0f);

    const float thumbX = getToggleState() ? (Layout::switchW - Layout::switchThumbRadius) : Layout::switchThumbRadius;

    g.setColour(Colour::switchThumb);
    g.fillEllipse(thumbX - Layout::switchThumbRadius, centreY - Layout::switchThumbRadius,
                  Layout::switchThumbRadius * 2.0f, Layout::switchThumbRadius * 2.0f);
    g.setColour(Colour::switchThumbStroke);
    g.drawEllipse(thumbX - Layout::switchThumbRadius, centreY - Layout::switchThumbRadius,
                  Layout::switchThumbRadius * 2.0f, Layout::switchThumbRadius * 2.0f, 1.5f);

    g.setColour(Colour::amber);
    g.fillEllipse(thumbX - Layout::switchThumbDotRadius, centreY - Layout::switchThumbDotRadius,
                  Layout::switchThumbDotRadius * 2.0f, Layout::switchThumbDotRadius * 2.0f);
}
