#pragma once

#include "TapeRotTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

class TapeRotLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TapeRotLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    static std::unique_ptr<juce::Slider> createKnobSlider(const juce::String& label);
};
