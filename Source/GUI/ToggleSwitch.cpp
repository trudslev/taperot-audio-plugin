#include "ToggleSwitch.h"
#include "TapeRotTheme.h"

ToggleSwitch::ToggleSwitch(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(true);
    startTimerHz(60);
}

ToggleSwitch::~ToggleSwitch()
{
    stopTimer();
}

void ToggleSwitch::timerCallback()
{
    const float target = getToggleState() ? 1.0f : 0.0f;
    if (std::abs(target - thumbPosition01) > 0.001f)
    {
        thumbPosition01 += (target - thumbPosition01) * 0.18f;
        repaint();
    }
    else if (thumbPosition01 != target)
    {
        thumbPosition01 = target;
        repaint();
    }
}

void ToggleSwitch::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    if (!hasPaintedOnce)
    {
        // Snap to the real initial state on the very first paint - only changes made after the
        // switch is already on screen should visibly ease between stops.
        thumbPosition01 = getToggleState() ? 1.0f : 0.0f;
        hasPaintedOnce = true;
    }

    const float centreY = (float) getHeight() * 0.5f;
    const juce::Rectangle<float> track(0.0f, centreY - Layout::switchH * 0.5f, Layout::switchW, Layout::switchH);

    g.setColour(Colour::dark);
    g.fillRoundedRectangle(track, Layout::switchRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(track, Layout::switchRadius, 1.0f);

    const float thumbX = Layout::switchThumbRadius
                        + thumbPosition01 * (Layout::switchW - Layout::switchThumbRadius * 2.0f);

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
