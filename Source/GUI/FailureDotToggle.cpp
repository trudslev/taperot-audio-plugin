#include "FailureDotToggle.h"
#include "TapeRotTheme.h"

FailureDotToggle::FailureDotToggle(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(true);
    // The lit-up glow deliberately bleeds a few px past this dot's own (tightly-packed, 3px
    // padding) bounds - without this, JUCE's default per-component clip region would crop it back
    // down to invisible.
    setPaintingIsUnclipped(true);
}

void FailureDotToggle::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = Layout::failureDotRadius;

    if (getToggleState())
    {
        juce::ColourGradient bloom(Colour::amberBright.withAlpha(0.8f), centre.x, centre.y,
                                    Colour::amberBright.withAlpha(0.0f), centre.x + Layout::failureDotGlowRadius,
                                    centre.y + Layout::failureDotGlowRadius, true);
        g.setGradientFill(bloom);
        g.fillEllipse(centre.x - Layout::failureDotGlowRadius, centre.y - Layout::failureDotGlowRadius,
                      Layout::failureDotGlowRadius * 2.0f, Layout::failureDotGlowRadius * 2.0f);

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
