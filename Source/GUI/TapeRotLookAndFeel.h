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

    // Smaller than the LookAndFeel_V2 default so the longest model name ("CASSETTE II") fits the
    // MODEL dropdown's width, which is itself constrained by the MACHINE section's layout.
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    static std::unique_ptr<juce::Slider> createKnobSlider(const juce::String& label);

    // Public (not static-private) so a unit test can assert tickCount == model table size without
    // needing a live Slider/Component.
    static int getTickCountForSlider(const juce::Slider& slider) noexcept;
};
