#pragma once

#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Header lamp that lights up while the FAIL aux effect is engaged (via the button or host
// automation), fading in/out with the same envelope value the DSP itself uses.
class FailLamp final : public juce::Component, private juce::Timer
{
public:
    explicit FailLamp(TapeRotAudioProcessor& processor);
    ~FailLamp() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;
    float displayedLevel = 0.0f;
};
