#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The thumb eases between its two stops (rather than snapping) whenever the toggle state changes,
// however that change happens - a direct click, host automation, or a preset/program load - since
// this polls getToggleState() on a timer rather than hooking a click handler.
class ToggleSwitch final : public juce::Button, private juce::Timer
{
public:
    explicit ToggleSwitch(const juce::String& name);
    ~ToggleSwitch() override;

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void timerCallback() override;

    float thumbPosition01 = 0.0f;
    bool hasPaintedOnce = false;
};
