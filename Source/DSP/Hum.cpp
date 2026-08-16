#include "Hum.h"

void Hum::prepare(const juce::dsp::ProcessSpec& spec, bool initiallyEnabled)
{
    sampleRate = spec.sampleRate;
    phaseInc = juce::MathConstants<double>::twoPi * fundamentalHz / sampleRate;
    gainSmoothed.reset(sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue(initiallyEnabled ? 1.0f : 0.0f);
    reset();
}

void Hum::reset()
{
    phase = 0.0;
    gainSmoothed.setCurrentAndTargetValue(gainSmoothed.getTargetValue());
}

void Hum::process(juce::AudioBuffer<float>& buffer, bool humEnabled)
{
    gainSmoothed.setTargetValue(humEnabled ? 1.0f : 0.0f);

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = gainSmoothed.getNextValue();
        const float sample = (float) (std::sin(phase) * fundamentalLevel
                                       + std::sin(phase * 2.0) * secondHarmonicLevel) * gain;

        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer(ch)[i] += sample;

        phase += phaseInc;
        if (phase > juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }
}
