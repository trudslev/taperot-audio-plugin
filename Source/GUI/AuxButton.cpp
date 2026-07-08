#include "AuxButton.h"
#include "TapeRotTheme.h"

AuxButton::AuxButton(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(false);
    // The lit-up glow deliberately bleeds a few px past this button's own (tightly-packed, 3px
    // padding) bounds - without this, JUCE's default per-component clip region would crop it back
    // down to invisible.
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

    if (getToggleState())
    {
        // Soft outward bloom, well outside the button's own footprint, so the panel around it
        // reads as lit rather than just the button itself looking a brighter colour.
        juce::ColourGradient bloom(Colour::amberBright.withAlpha(0.85f), centre.x, centre.y,
                                    Colour::amberBright.withAlpha(0.0f), centre.x + Layout::auxButtonGlowRadius,
                                    centre.y + Layout::auxButtonGlowRadius, true);
        g.setGradientFill(bloom);
        g.fillEllipse(centre.x - Layout::auxButtonGlowRadius, centre.y - Layout::auxButtonGlowRadius,
                      Layout::auxButtonGlowRadius * 2.0f, Layout::auxButtonGlowRadius * 2.0f);
    }

    g.setColour(Colour::dark);
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    if (getToggleState())
    {
        // Hot white-amber core fading to amber at the rim - much brighter than a flat amber fill.
        juce::ColourGradient glow(Colour::specular, centre.x, centre.y,
                                   Colour::amber, centre.x + r, centre.y + r, true);
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - r + 2.0f, centre.y - r + 2.0f, (r - 2.0f) * 2.0f, (r - 2.0f) * 2.0f);
    }

    g.setColour(Colour::rim);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.75f);
}
