#pragma once

#include <juce_dsp/juce_dsp.h>

class OutputStage
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // dryBuffer must hold the untouched input captured before the rest of the signal chain ran.
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& dryBuffer,
                 float mix01, float outputDb);

private:
    juce::SmoothedValue<float> mixSmoothed{1.0f};
    juce::SmoothedValue<float> gainSmoothed{1.0f};
};
