#include "AuxButton.h"
#include "TapeRotTheme.h"

AuxButton::AuxButton(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(false);
    // The lit sparkle's spike tips deliberately reach a few px past this button's own (tightly-
    // packed, 3px padding) bounds - without this, JUCE's default per-component clip region would
    // crop them to an abrupt flat cut instead of a tapered point.
    setPaintingIsUnclipped(true);
}

void AuxButton::mouseDown(const juce::MouseEvent& e)
{
    setToggleState(true, juce::sendNotification);
    juce::Button::mouseDown(e);
}

void AuxButton::mouseUp(const juce::MouseEvent& e)
{
    setToggleState(false, juce::sendNotification);
    juce::Button::mouseUp(e);
}

void AuxButton::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace TapeRotTheme;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = Layout::auxButtonRadius;

    g.setColour(Colour::dark);
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    if (getToggleState())
    {
        // Hot white-amber core fading to amber at the rim - much brighter than a flat amber fill.
        juce::ColourGradient glow(Colour::specular, centre.x, centre.y,
                                   Colour::amber, centre.x + r, centre.y + r, true);
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - r + 2.0f, centre.y - r + 2.0f, (r - 2.0f) * 2.0f, (r - 2.0f) * 2.0f);

        // Sharp lens-flare sparkle from the hot core - reads as an intensely lit diode rather than
        // a flat bright disc (a soft outward halo was tried here and rejected as looking like a
        // ring rather than a light source).
        drawSparkleHighlight(g, centre, r, Colour::specular, 1.0f, 1.5f);
    }

    g.setColour(Colour::rim);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.75f);
}
