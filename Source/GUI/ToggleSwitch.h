#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class ToggleSwitch final : public juce::Button
{
public:
    explicit ToggleSwitch(const juce::String& name);

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
