#include "AuxButton.h"
#include "TapeRotTheme.h"

AuxButton::AuxButton(const juce::String& name) : juce::Button(name)
{
    setClickingTogglesState(false);
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
        juce::ColourGradient glow(Colour::amberBright, centre.x, centre.y,
                                   Colour::amber, centre.x + r, centre.y + r, true);
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - r + 2.0f, centre.y - r + 2.0f, (r - 2.0f) * 2.0f, (r - 2.0f) * 2.0f);
    }

    g.setColour(Colour::rim);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.75f);
}
