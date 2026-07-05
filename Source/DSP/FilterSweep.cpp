#include "FilterSweep.h"

void FilterSweep::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance(resonance);
    filter.setCutoffFrequency(openHz);

    envelope.setSampleRate(sampleRate);

    reset();
}

void FilterSweep::reset()
{
    filter.reset();
}

void FilterSweep::process(juce::AudioBuffer<float>& buffer, bool engaged, float rampSeconds)
{
    envelope.setEngaged(engaged);
    envelope.setRampSeconds(rampSeconds);

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float auxValue = envelope.getNextValue();
        const float cutoff = openHz * std::pow(closedHz / openHz, auxValue);
        filter.setCutoffFrequency(cutoff);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[i] = filter.processSample(ch, data[i]);
        }
    }
}
