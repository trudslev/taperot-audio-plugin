#include "FailLamp.h"
#include "TapeRotTheme.h"

FailLamp::FailLamp(TapeRotAudioProcessor& processor) : processorRef(processor)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

FailLamp::~FailLamp()
{
    stopTimer();
}

void FailLamp::timerCallback()
{
    const float level = processorRef.getFailAuxDisplay();
    if (std::abs(level - displayedLevel) > 0.002f)
    {
        displayedLevel = level;
        repaint();
    }
}

void FailLamp::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    const juce::Point<float> centre(Layout::lampX, Layout::lampY);

    if (displayedLevel > 0.01f)
    {
        juce::ColourGradient glow(Colour::lampGlowInner.withAlpha(displayedLevel * 0.9f), centre.x, centre.y,
                                   Colour::lampGlowInner.withAlpha(0.0f), centre.x + Layout::lampGlowRadius,
                                   centre.y + Layout::lampGlowRadius, true);
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - Layout::lampGlowRadius, centre.y - Layout::lampGlowRadius,
                      Layout::lampGlowRadius * 2.0f, Layout::lampGlowRadius * 2.0f);
    }

    const auto bulbColour = Colour::lamp.interpolatedWith(Colour::lampRing, 1.0f - displayedLevel);
    g.setColour(bulbColour);
    g.fillEllipse(centre.x - Layout::lampRadius, centre.y - Layout::lampRadius,
                  Layout::lampRadius * 2.0f, Layout::lampRadius * 2.0f);
    g.setColour(Colour::lampRing);
    g.drawEllipse(centre.x - Layout::lampRadius, centre.y - Layout::lampRadius,
                  Layout::lampRadius * 2.0f, Layout::lampRadius * 2.0f, 1.5f);

    g.setColour(Colour::specular.withAlpha(0.7f));
    g.fillEllipse(Layout::lampSpecularX - Layout::lampSpecularRadius, Layout::lampSpecularY - Layout::lampSpecularRadius,
                  Layout::lampSpecularRadius * 2.0f, Layout::lampSpecularRadius * 2.0f);
}
