#include "WowFlutter.h"

void WowFlutter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const int numChannels = (int) spec.numChannels;

    maxDelaySamples = (float) (maxDelayMs * 0.001 * sampleRate);
    centerDelaySamples = (float) (centerDelayMs * 0.001 * sampleRate);

    delayLine.setMaximumDelayInSamples((int) std::ceil(maxDelaySamples) + interpolationMarginSamples * 2);
    delayLine.prepare(spec);

    channels.resize((size_t) numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        c.random = juce::Random((juce::int64) (0x9E3779B97F4A7C15ULL * (juce::uint64) (ch + 1)));
        c.flutterRateHz = 7.0 + c.random.nextDouble() * 5.0;
    }

    reset();
}

void WowFlutter::reset()
{
    delayLine.reset();
    for (auto& c : channels)
    {
        c.wowPhase = 0.0;
        c.flutterPhase = 0.0;
        c.wowRandomLpfState = 0.0f;
        c.flutterNoiseLpfState = 0.0f;
        c.flutterNoiseHpfState = 0.0f;
        c.flutterNoisePrevInput = 0.0f;
    }
}

void WowFlutter::process(juce::AudioBuffer<float>& buffer, float wowDepth01, float flutterDepth01)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());

    const float wowDepthSamples = wowDepth01 * maxWowMs * 0.001f * (float) sampleRate;
    const float flutterDepthSamples = flutterDepth01 * maxFlutterMs * 0.001f * (float) sampleRate;

    const double wowPhaseInc = juce::MathConstants<double>::twoPi * wowRateHz / sampleRate;
    const float wowRandomLpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * wowRandomLpfHz / (float) sampleRate);
    const float flutterLpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * flutterNoiseLpfHz / (float) sampleRate);
    const float flutterHpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * flutterNoiseHpfHz / (float) sampleRate);

    const float lowerBound = (float) interpolationMarginSamples;
    const float upperBound = maxDelaySamples - (float) interpolationMarginSamples;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* data = buffer.getWritePointer(ch);
        const double flutterPhaseInc = juce::MathConstants<double>::twoPi * c.flutterRateHz / sampleRate;
        const float channelPhase = ch == 0 ? 0.0f : channelPhaseOffsetRad;

        for (int i = 0; i < numSamples; ++i)
        {
            const float wowSine = std::sin((float) c.wowPhase + channelPhase);
            const float wowNoiseRaw = c.random.nextFloat() * 2.0f - 1.0f;
            c.wowRandomLpfState += wowRandomLpfCoeff * (wowNoiseRaw - c.wowRandomLpfState);
            const float wowMod = (wowSine * 0.7f + c.wowRandomLpfState * 0.3f) * wowDepthSamples;

            const float flutterSine = std::sin((float) c.flutterPhase + channelPhase);
            const float flutterNoiseRaw = c.random.nextFloat() * 2.0f - 1.0f;
            c.flutterNoiseHpfState = flutterHpfCoeff * (c.flutterNoiseHpfState + flutterNoiseRaw - c.flutterNoisePrevInput);
            c.flutterNoisePrevInput = flutterNoiseRaw;
            c.flutterNoiseLpfState += flutterLpfCoeff * (c.flutterNoiseHpfState - c.flutterNoiseLpfState);
            const float flutterMod = (flutterSine * 0.8f + c.flutterNoiseLpfState * 0.2f) * flutterDepthSamples;

            float delaySamples = centerDelaySamples + wowMod + flutterMod;
            delaySamples = juce::jlimit(lowerBound, upperBound, delaySamples);

            delayLine.pushSample(ch, data[i]);
            delayLine.setDelay(delaySamples);
            data[i] = delayLine.popSample(ch);

            c.wowPhase += wowPhaseInc;
            if (c.wowPhase > juce::MathConstants<double>::twoPi)
                c.wowPhase -= juce::MathConstants<double>::twoPi;

            c.flutterPhase += flutterPhaseInc;
            if (c.flutterPhase > juce::MathConstants<double>::twoPi)
                c.flutterPhase -= juce::MathConstants<double>::twoPi;
        }
    }
}
