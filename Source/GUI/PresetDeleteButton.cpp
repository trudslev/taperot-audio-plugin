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

    // No square face or border at all (unlike PresetSaveButton) - just the trash-can bin (light
    // switchThumb) and its two dark ribs, floating directly on the panel behind it, per explicit
    // request to drop the surrounding chip entirely.
    g.setColour(Colour::switchThumb.withAlpha(alpha));
    g.fillRoundedRectangle(bounds.getX() + 9.0f, bounds.getY() + 0.0f, 4.0f, 1.4f, 0.5f);
    g.fillRect(bounds.getX() + 4.0f, bounds.getY() + 1.6f, 14.0f, 3.0f);
    g.fillRoundedRectangle(bounds.getX() + 5.0f, bounds.getY() + 5.0f, 12.0f, 16.5f, 1.0f);

    g.setColour(Colour::dark.withAlpha(alpha));
    g.fillRect(bounds.getX() + 8.7f, bounds.getY() + 8.0f, 1.4f, 11.5f);
    g.fillRect(bounds.getX() + 11.9f, bounds.getY() + 8.0f, 1.4f, 11.5f);
}
