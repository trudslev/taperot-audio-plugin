#include "FailureDotToggle.h"
#include "TapeRotTheme.h"

FailureDotToggle::FailureDotToggle(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(true);
}

void FailureDotToggle::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = Layout::failureDotRadius;

    if (getToggleState())
    {
        juce::ColourGradient glow(Colour::specular, centre.x, centre.y,
                                   Colour::amber, centre.x + r, centre.y + r, true);
        g.setGradientFill(glow);
    }
    else
    {
        g.setColour(Colour::dotOff);
    }
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.setColour(Colour::switchThumbStroke);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
}
