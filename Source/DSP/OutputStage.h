#pragma once

#include <juce_dsp/juce_dsp.h>

class OutputStage
{
public:
    void prepare(const juce::dsp::ProcessSpec&) {}
    void reset() {}
    void process(juce::AudioBuffer<float>&, float /*mix01*/, float /*outputDb*/) {}
};
