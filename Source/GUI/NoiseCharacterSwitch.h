#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// 3-position slide switch (TAPE / VCR / DUST) bound to the noiseCharacter choice parameter.
// Subclasses Slider purely to reuse its click/drag-to-value mapping and SliderAttachment
// compatibility; paint() fully replaces the default look with the switch-track visual.
class NoiseCharacterSwitch final : public juce::Slider
{
public:
    NoiseCharacterSwitch();

private:
    void paint(juce::Graphics&) override;
};
