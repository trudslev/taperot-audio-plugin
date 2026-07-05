#pragma once

#include <juce_dsp/juce_dsp.h>

class StereoSpread
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, bool spreadEnabled);

private:
    static constexpr float sideBoost = 1.6f;
    static constexpr float midAttenuation = 0.9f;

    juce::SmoothedValue<float> amountSmoothed{0.0f};
};
