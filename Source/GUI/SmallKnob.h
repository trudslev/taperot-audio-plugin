#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A smaller, simplified rotary knob for utility controls (LP/HP/RAMP) that don't warrant the full
// hero-knob treatment (tick marks etc.) TapeRotLookAndFeel::drawRotarySlider draws at a single
// fixed Layout::knobRadius, so this paints itself directly rather than going through that path.
class SmallKnob final : public juce::Slider
{
public:
    SmallKnob();

private:
    void paint(juce::Graphics&) override;
};
