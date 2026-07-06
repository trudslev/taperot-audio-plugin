#include "PresetDeleteButton.h"
#include "TapeRotTheme.h"

PresetDeleteButton::PresetDeleteButton() : juce::Button("PresetDelete")
{
}

void PresetDeleteButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool)
{
    using namespace TapeRotTheme;

    auto bounds = getLocalBounds().toFloat();
    const float alpha = isEnabled() ? (shouldDrawButtonAsHighlighted ? 1.0f : 0.92f) : 0.35f;

    g.setColour(Colour::switchThumb.withAlpha(alpha));
    g.fillRoundedRectangle(bounds, Layout::presetDeleteRadius);
    g.setColour(Colour::switchThumbStroke.withAlpha(isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle(bounds, Layout::presetDeleteRadius, 1.3f);

    // Trash-can pictogram: handle + lid + body, all the same Colour::dark as PresetSaveButton's
    // floppy disk body, filling nearly the full button height for visual weight parity with it.
    // The two ribs aren't a separate accent colour (that read as a light grey smear at this size) -
    // they're cut through the body in the button's own background colour/alpha, so they read as
    // actual transparent slits rather than a painted-on detail.
    g.setColour(Colour::dark.withAlpha(isEnabled() ? 1.0f : 0.6f));
    g.fillRoundedRectangle(bounds.getX() + 9.0f, bounds.getY() + 0.0f, 4.0f, 1.4f, 0.5f);
    g.fillRect(bounds.getX() + 4.0f, bounds.getY() + 1.6f, 14.0f, 3.0f);
    g.fillRoundedRectangle(bounds.getX() + 5.0f, bounds.getY() + 5.0f, 12.0f, 16.5f, 1.0f);

    g.setColour(Colour::switchThumb.withAlpha(alpha));
    g.fillRect(bounds.getX() + 8.7f, bounds.getY() + 8.0f, 1.4f, 11.5f);
    g.fillRect(bounds.getX() + 11.9f, bounds.getY() + 8.0f, 1.4f, 11.5f);
}
