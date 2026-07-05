#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Global LP/HP tone shaping applied once after the GEN cascade (not per generation). "Off" at the
// range extremes is achieved by the cutoff reaching an acoustically transparent frequency rather
// than by hard-bypassing the filter, since toggling a stateful IIR filter in/out of the signal
// path mid-stream risks a discontinuity click; letting the coefficients themselves become
// transparent keeps the filter state continuous at all times.
class ToneFilters
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float lpHz, float hpHz);

private:
    static constexpr float smoothingSeconds = 0.02f;
    static constexpr float minFilterHz = 20.0f;

    struct ChannelFilters
    {
        juce::dsp::IIR::Filter<float> lowPass;
        juce::dsp::IIR::Filter<float> highPass;
    };

    std::vector<ChannelFilters> channels;
    double sampleRate = 44100.0;
    float nyquistSafeHz = 20000.0f;

    juce::SmoothedValue<float> lpSmoothed;
    juce::SmoothedValue<float> hpSmoothed;
};
