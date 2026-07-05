#pragma once

#include <juce_dsp/juce_dsp.h>

class WowFlutter
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float wowDepth01, float flutterDepth01);

    static constexpr float nominalDelayMs = 25.0f;

private:
    static constexpr float centerDelayMs = nominalDelayMs;
    static constexpr float maxDelayMs = 55.0f;
    static constexpr float maxWowMs = 15.0f;
    static constexpr float maxFlutterMs = 3.0f;
    static constexpr float wowRateHz = 0.5f;
    static constexpr float wowRandomLpfHz = 0.2f;
    static constexpr float flutterNoiseLpfHz = 11.0f;
    static constexpr float flutterNoiseHpfHz = 7.0f;
    static constexpr float channelPhaseOffsetRad = juce::MathConstants<float>::halfPi;
    static constexpr int interpolationMarginSamples = 4;

    struct ChannelState
    {
        double wowPhase = 0.0;
        double flutterPhase = 0.0;
        double flutterRateHz = 9.5;
        float wowRandomLpfState = 0.0f;
        float flutterNoiseLpfState = 0.0f;
        float flutterNoiseHpfState = 0.0f;
        float flutterNoisePrevInput = 0.0f;
        juce::Random random;
    };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine{2000};
    std::vector<ChannelState> channels;
    double sampleRate = 44100.0;
    float maxDelaySamples = 0.0f;
    float centerDelaySamples = 0.0f;
};
