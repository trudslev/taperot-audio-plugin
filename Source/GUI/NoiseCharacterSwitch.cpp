#include "NoiseCharacterSwitch.h"
#include "TapeRotTheme.h"

NoiseCharacterSwitch::NoiseCharacterSwitch()
{
    setSliderStyle(juce::Slider::LinearHorizontal);
    setRange(0.0, 2.0, 1.0);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    startTimerHz(60);
}

NoiseCharacterSwitch::~NoiseCharacterSwitch()
{
    stopTimer();
}

void NoiseCharacterSwitch::timerCallback()
{
    const float target = (float) std::round(getValue()) / 2.0f;
    if (std::abs(target - displayedT) > 0.001f)
    {
        displayedT += (target - displayedT) * 0.18f;
        repaint();
    }
    else if (displayedT != target)
    {
        displayedT = target;
        repaint();
    }
}

void NoiseCharacterSwitch::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    if (!hasPaintedOnce)
    {
        // Snap to the real initial value on the very first paint - only changes made after the
        // switch is already on screen should visibly ease between stops.
        displayedT = (float) std::round(getValue()) / 2.0f;
        hasPaintedOnce = true;
    }

    const float centreY = (float) getHeight() * 0.5f;
    const juce::Rectangle<float> track(0.0f, centreY - Layout::switchH * 0.5f, Layout::switchW, Layout::switchH);

    g.setColour(Colour::dark);
    g.fillRoundedRectangle(track, Layout::switchRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(track, Layout::switchRadius, 1.0f);

    const float usableWidth = Layout::switchW - Layout::switchThumbRadius * 2.0f;
    const float thumbX = Layout::switchThumbRadius + displayedT * usableWidth;

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
