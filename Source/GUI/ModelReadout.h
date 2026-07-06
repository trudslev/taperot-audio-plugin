#pragma once

#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Dark readout window under the MODEL knob showing the active model's display name (replaces the
// knob's usual generic label - see SectionPanel::paintKnobLabels, which skips "model"). Polls the
// MODEL parameter so it updates on host automation too, and briefly flashes brighter on change.
class ModelReadout final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit ModelReadout(TapeRotAudioProcessor& processor);
    ~ModelReadout() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;
    int displayedModelIndex = -1;
    float flashLevel = 0.0f;
};
