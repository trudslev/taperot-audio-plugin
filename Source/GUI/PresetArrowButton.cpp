#include "PresetArrowButton.h"
#include "TapeRotTheme.h"

PresetArrowButton::PresetArrowButton(bool pointsRightIn)
    : juce::Button(pointsRightIn ? "PresetNext" : "PresetPrev"), pointsRight(pointsRightIn)
{
}

void PresetArrowButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool)
{
    using namespace TapeRotTheme;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = Layout::presetArrowRadius;

    g.setColour(Colour::switchThumb.withAlpha(shouldDrawButtonAsHighlighted ? 1.0f : 0.92f));
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.setColour(Colour::switchThumbStroke);
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.3f);

    juce::Path triangle;
    if (pointsRight)
    {
        triangle.addTriangle(centre.x - 3.5f, centre.y - 5.0f, centre.x - 3.5f, centre.y + 5.0f,
                              centre.x + 5.0f, centre.y);
    }
    else
    {
        triangle.addTriangle(centre.x + 3.5f, centre.y - 5.0f, centre.x + 3.5f, centre.y + 5.0f,
                              centre.x - 5.0f, centre.y);
    }
    g.setColour(Colour::dark);
    g.fillPath(triangle);
}
