#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Small diskette pictogram button in the preset strip that opens the "Save As" prompt (always
// creates a new user preset - factory presets are never overwritten in place).
class PresetSaveButton final : public juce::Button
{
public:
    PresetSaveButton();

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
