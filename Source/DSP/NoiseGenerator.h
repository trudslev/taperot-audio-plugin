#pragma once

#include <juce_dsp/juce_dsp.h>

class NoiseGenerator
{
public:
    void prepare(const juce::dsp::ProcessSpec&) {}
    void reset() {}
    void process(juce::AudioBuffer<float>&, float /*noiseAmount01*/, bool /*humEnabled*/) {}
};
