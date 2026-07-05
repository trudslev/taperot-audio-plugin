#include "OutputStage.h"
#include <array>

void OutputStage::prepare(const juce::dsp::ProcessSpec& spec)
{
    mixSmoothed.reset(spec.sampleRate, 0.02);
    gainSmoothed.reset(spec.sampleRate, 0.02);
    reset();
}

void OutputStage::reset()
{
    mixSmoothed.setCurrentAndTargetValue(mixSmoothed.getTargetValue());
    gainSmoothed.setCurrentAndTargetValue(gainSmoothed.getTargetValue());
}

void OutputStage::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& dryBuffer,
                           float mix01, float outputDb)
{
    mixSmoothed.setTargetValue(mix01);
    gainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(outputDb));

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());

    constexpr int kMaxChannels = 2;
    std::array<float*, kMaxChannels> wet{};
    std::array<const float*, kMaxChannels> dry{};
    const int activeChannels = juce::jmin(numCh, kMaxChannels);
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        wet[(size_t) ch] = buffer.getWritePointer(ch);
        dry[(size_t) ch] = dryBuffer.getReadPointer(ch);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();

        for (int ch = 0; ch < activeChannels; ++ch)
            wet[(size_t) ch][i] = (dry[(size_t) ch][i] * (1.0f - mix) + wet[(size_t) ch][i] * mix) * gain;
    }
}
