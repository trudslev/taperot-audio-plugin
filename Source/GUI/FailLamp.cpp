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
        // A plain 2-stop gradient interpolates alpha linearly with radius, which reads as a
        // fairly constant-brightness disc that then cuts off abruptly right near the boundary
        // (the same "ring" look rejected on the aux buttons/failure dots). Extra stops easing the
        // alpha down faster than linear make it actually fade out well before the true edge.
        juce::ColourGradient glow(Colour::ledRedCore.withAlpha(displayedLevel), centre.x, centre.y,
                                   Colour::ledRedCore.withAlpha(0.0f), centre.x + Layout::lampGlowRadius,
                                   centre.y + Layout::lampGlowRadius, true);
        glow.addColour(0.2, Colour::ledRedCore.withAlpha(displayedLevel * 0.62f));
        glow.addColour(0.4, Colour::ledRedCore.withAlpha(displayedLevel * 0.33f));
        glow.addColour(0.6, Colour::ledRedCore.withAlpha(displayedLevel * 0.14f));
        glow.addColour(0.8, Colour::ledRedCore.withAlpha(displayedLevel * 0.03f));
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - Layout::lampGlowRadius, centre.y - Layout::lampGlowRadius,
                      Layout::lampGlowRadius * 2.0f, Layout::lampGlowRadius * 2.0f);
    }

    // Dark saturated red when lit, fading to a neutral gray (unlit diode housing) rather than the
    // amber/white-hot look used elsewhere - matches AuxButton/FailureDotToggle's red diode colour.
    const auto bulbColour = Colour::ledRedCore.interpolatedWith(Colour::lampUnlit, 1.0f - displayedLevel);
    g.setColour(bulbColour);
    g.fillEllipse(centre.x - Layout::lampRadius, centre.y - Layout::lampRadius,
                  Layout::lampRadius * 2.0f, Layout::lampRadius * 2.0f);
    g.setColour(Colour::lampUnlit);
    g.drawEllipse(centre.x - Layout::lampRadius, centre.y - Layout::lampRadius,
                  Layout::lampRadius * 2.0f, Layout::lampRadius * 2.0f, 1.5f);

    g.setColour(Colour::specular.withAlpha(0.7f));
    g.fillEllipse(Layout::lampSpecularX - Layout::lampSpecularRadius, Layout::lampSpecularY - Layout::lampSpecularRadius,
                  Layout::lampSpecularRadius * 2.0f, Layout::lampSpecularRadius * 2.0f);

    // Sharp lens-flare sparkle from the hot core, same treatment as AuxButton/FailureDotToggle -
    // scales with the same envelope that drives the glow, so it fades in/out with it.
    drawSparkleHighlight(g, centre, Layout::lampRadius, Colour::ledRedSparkle, displayedLevel);
}
