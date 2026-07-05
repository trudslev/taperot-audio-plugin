#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// 8-segment stepped selector for the GEN (tape generation count, 1-8) parameter. Subclasses
// Slider purely for its click/drag-to-value mapping and SliderAttachment compatibility, same
// pattern as NoiseCharacterSwitch.
class GenSelector final : public juce::Slider
{
public:
    GenSelector();

private:
    void paint(juce::Graphics&) override;
};
