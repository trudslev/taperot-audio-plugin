#pragma once

#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Header oscilloscope: a carrier wave modulated by recent output level, so it visibly reacts to
// the signal. Higher GEN adds jitter for a "dirtier" baseline trace, and STOP flattens/stretches
// the trace toward a flat line as playback speed falls, reopening as it releases.
class Scope final : public juce::Component, private juce::Timer
{
public:
    explicit Scope(TapeRotAudioProcessor& processor);
    ~Scope() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;
    juce::Random jitterRandom;
};
