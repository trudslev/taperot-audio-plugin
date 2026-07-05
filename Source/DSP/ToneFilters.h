#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Global LP/HP tone shaping applied once after the GEN cascade (not per generation). "Off" at the
// range extremes is achieved by the cutoff reaching an acoustically transparent frequency rather
// than by hard-bypassing the filter, since toggling a stateful filter in/out of the signal path
// mid-stream risks a discontinuity click; letting the coefficients themselves become transparent
// keeps the filter state continuous at all times.
//
// Computes its own RBJ-cookbook biquad coefficients as plain floats rather than using
// juce::dsp::IIR::Coefficients: that class's makeLowPass/makeHighPass factories heap-allocate a
// new Coefficients object per call (even though the result is wrapped in a reference-counted
// pointer), which is unsafe to call every block on the audio thread the way this needs to.
class ToneFilters
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float lpHz, float hpHz);

private:
    static constexpr float smoothingSeconds = 0.02f;
    static constexpr float minFilterHz = 20.0f;
    static constexpr float filterQ = 0.70710678f; // Butterworth (maximally flat)

    struct BiquadCoeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };

    struct BiquadState
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    };

    static BiquadCoeffs makeLowPassCoeffs(double sampleRate, float freqHz) noexcept;
    static BiquadCoeffs makeHighPassCoeffs(double sampleRate, float freqHz) noexcept;
    static float processBiquad(BiquadState& state, const BiquadCoeffs& coeffs, float x) noexcept;

    struct ChannelFilters
    {
        BiquadState lowPass, highPass;
    };

    std::vector<ChannelFilters> channels;
    double sampleRate = 44100.0;
    float nyquistSafeHz = 20000.0f;

    juce::SmoothedValue<float> lpSmoothed;
    juce::SmoothedValue<float> hpSmoothed;
};
