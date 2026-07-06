#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Small trash-can pictogram button in the preset strip, next to PresetSaveButton, that deletes the
// currently-loaded user preset (disabled for factory presets - see PresetStrip). Not a replacement
// for the name plate's right-click "Delete" context menu, which stays as an equally-valid second
// way to do the same thing.
class PresetDeleteButton final : public juce::Button
{
public:
    PresetDeleteButton();

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
