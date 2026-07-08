#include "FailureDotToggle.h"
#include "TapeRotTheme.h"

FailureDotToggle::FailureDotToggle(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(true);
    // The lit sparkle's spike tips deliberately reach a few px past this dot's own (tightly-
    // packed, 3px padding) bounds - without this, JUCE's default per-component clip region would
    // crop them to an abrupt flat cut instead of a tapered point.
    setPaintingIsUnclipped(true);
}

void FailureDotToggle::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = Layout::failureDotRadius;

    if (getToggleState())
    {
        juce::ColourGradient glow(Colour::ledRedCore, centre.x, centre.y,
                                   Colour::ledRedEdge, centre.x + r, centre.y + r, true);
        g.setGradientFill(glow);
    }
    else
    {
        g.setColour(Colour::dotOff);
    }
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.setColour(Colour::switchThumbStroke);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);

    if (getToggleState())
    {
        // Sharp lens-flare sparkle from the hot core, same treatment as AuxButton - reads as an
        // intensely lit diode even at this much smaller size.
        drawSparkleHighlight(g, centre, r, Colour::ledRedSparkle, 1.0f, 2.0f);
    }
}
