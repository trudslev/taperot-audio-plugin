#pragma once

#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Draws the live GEN value into the counter housing's rightmost digit cell (SectionPanel paints
// the housing and empty cell backgrounds; this is the only thing that ever drew a digit into them).
class GenDigitDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit GenDigitDisplay(TapeRotAudioProcessor& processor);
    ~GenDigitDisplay() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;
    int displayedGen = -1;
};
