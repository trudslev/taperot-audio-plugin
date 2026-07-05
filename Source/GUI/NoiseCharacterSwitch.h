#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// 3-position slide switch (TAPE / VCR / DUST) bound to the noiseCharacter choice parameter.
// Subclasses Slider purely to reuse its click/drag-to-value mapping and SliderAttachment
// compatibility; paint() fully replaces the default look with the switch-track visual. The thumb
// eases between its three stops (see ToggleSwitch, same approach) rather than snapping, whether
// the value changes via a click, a drag, host automation, or a preset/program load.
class NoiseCharacterSwitch final : public juce::Slider, private juce::Timer
{
public:
    NoiseCharacterSwitch();
    ~NoiseCharacterSwitch() override;

private:
    void paint(juce::Graphics&) override;
    void timerCallback() override;

    float displayedT = 0.0f;
    bool hasPaintedOnce = false;
};
