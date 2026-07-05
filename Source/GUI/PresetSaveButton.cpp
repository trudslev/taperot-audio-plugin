#include "PresetSaveButton.h"
#include "TapeRotTheme.h"

PresetSaveButton::PresetSaveButton() : juce::Button("PresetSave")
{
}

void PresetSaveButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool)
{
    using namespace TapeRotTheme;

    auto bounds = getLocalBounds().toFloat();

    g.setColour(Colour::switchThumb.withAlpha(shouldDrawButtonAsHighlighted ? 1.0f : 0.92f));
    g.fillRoundedRectangle(bounds, Layout::presetSaveRadius);
    g.setColour(Colour::switchThumbStroke);
    g.drawRoundedRectangle(bounds, Layout::presetSaveRadius, 1.3f);

    // Floppy-disk pictogram: metal shutter (top) + label (bottom), a small window cut into the
    // shutter - drawn relative to the button's own local bounds so it scales with presetSaveW/H.
    g.setColour(Colour::dark);
    g.fillRect(bounds.getX() + 4.5f, bounds.getY(), 13.0f, 8.0f);
    g.setColour(Colour::switchThumb);
    g.fillRect(bounds.getX() + 6.5f, bounds.getY() + 1.7f, 4.0f, 4.5f);
    g.setColour(Colour::dark);
    g.fillRoundedRectangle(bounds.getX() + 4.0f, bounds.getY() + 12.0f, 14.0f, 8.0f, 0.8f);
}
