#pragma once

#include <juce_dsp/juce_dsp.h>

class StereoSpread
{
public:
    void prepare(const juce::dsp::ProcessSpec&) {}
    void reset() {}
    void process(juce::AudioBuffer<float>&, bool /*spreadEnabled*/) {}
};
