#pragma once

#include <juce_dsp/juce_dsp.h>

// Mains hum overlay - independent of noise character, applied once globally regardless of GEN.
class Hum
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, bool humEnabled);

private:
    static constexpr float fundamentalHz = 60.0f;
    static constexpr float fundamentalLevel = 0.015f;
    static constexpr float secondHarmonicLevel = 0.006f;

    double sampleRate = 44100.0;
    double phase = 0.0;
    double phaseInc = 0.0;
    juce::SmoothedValue<float> gainSmoothed{0.0f};
};
