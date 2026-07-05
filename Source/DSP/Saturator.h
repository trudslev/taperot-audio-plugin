#pragma once

#include <juce_dsp/juce_dsp.h>

class Saturator
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float driveTarget01);

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static constexpr float preShelfFreqHz = 3500.0f;
    static constexpr float shelfQ = 0.707f;
    static constexpr float maxShelfBoostDb = 18.0f;
    static constexpr float maxDriveGain = 12.0f;
    static constexpr int numDriveSteps = 41;

    void buildCoefficientCache();

    double sampleRate = 44100.0;
    int numChannels = 2;
    int latencySamples = 0;

    juce::SmoothedValue<float> driveSmoothed{0.0f};

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numDriveSteps> preShelfCoeffCache;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numDriveSteps> postShelfCoeffCache;

    std::vector<juce::dsp::IIR::Filter<float>> preShelfFilters;
    std::vector<juce::dsp::IIR::Filter<float>> postShelfFilters;

    juce::dsp::DelayLine<float> dryCompensationDelay{192};

    juce::AudioBuffer<float> dryBuffer;
    std::vector<float> driveGainScratch;
    bool wasBypassed = true;
};
