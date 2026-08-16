#include "StereoSpread.h"

void StereoSpread::prepare(const juce::dsp::ProcessSpec& spec, bool initiallyEnabled)
{
    amountSmoothed.reset(spec.sampleRate, 0.03);
    amountSmoothed.setCurrentAndTargetValue(initiallyEnabled ? 1.0f : 0.0f);
    reset();
}

void StereoSpread::reset()
{
    amountSmoothed.setCurrentAndTargetValue(amountSmoothed.getTargetValue());
}

void StereoSpread::process(juce::AudioBuffer<float>& buffer, bool spreadEnabled)
{
    amountSmoothed.setTargetValue(spreadEnabled ? 1.0f : 0.0f);

    if (buffer.getNumChannels() < 2)
    {
        amountSmoothed.skip(buffer.getNumSamples());
        return;
    }

    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = amountSmoothed.getNextValue();
        const float mid = (left[i] + right[i]) * 0.5f;
        const float side = (left[i] - right[i]) * 0.5f;

        const float sideGain = 1.0f + amount * (sideBoost - 1.0f);
        const float midGain = 1.0f - amount * (1.0f - midAttenuation);

        const float newMid = mid * midGain;
        const float newSide = side * sideGain;

        left[i] = newMid + newSide;
        right[i] = newMid - newSide;
    }
}
