#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Prev/next chevron for the preset strip - a plain custom-painted circle+triangle button (click
// fires onClick like any juce::Button), not momentary like AuxButton.
class PresetArrowButton final : public juce::Button
{
public:
    explicit PresetArrowButton(bool pointsRight);

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    bool pointsRight;
};
