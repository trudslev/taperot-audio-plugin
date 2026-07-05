#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FailureDotToggle final : public juce::Button
{
public:
    explicit FailureDotToggle(const juce::String& name);

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
