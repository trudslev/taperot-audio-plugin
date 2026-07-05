#include "ToneFilters.h"

ToneFilters::BiquadCoeffs ToneFilters::makeLowPassCoeffs(double sampleRate, float freqHz) noexcept
{
    const float w0 = juce::MathConstants<float>::twoPi * freqHz / (float) sampleRate;
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / (2.0f * filterQ);

    const float a0 = 1.0f + alpha;
    const float b0 = (1.0f - cosw0) * 0.5f;
    const float b1 = 1.0f - cosw0;
    const float b2 = b0;
    const float a1 = -2.0f * cosw0;
    const float a2 = 1.0f - alpha;

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

ToneFilters::BiquadCoeffs ToneFilters::makeHighPassCoeffs(double sampleRate, float freqHz) noexcept
{
    const float w0 = juce::MathConstants<float>::twoPi * freqHz / (float) sampleRate;
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / (2.0f * filterQ);

    const float a0 = 1.0f + alpha;
    const float b0 = (1.0f + cosw0) * 0.5f;
    const float b1 = -(1.0f + cosw0);
    const float b2 = b0;
    const float a1 = -2.0f * cosw0;
    const float a2 = 1.0f - alpha;

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

float ToneFilters::processBiquad(BiquadState& s, const BiquadCoeffs& c, float x) noexcept
{
    const float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
    s.x2 = s.x1;
    s.x1 = x;
    s.y2 = s.y1;
    s.y1 = y;
    return y;
}

void ToneFilters::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    nyquistSafeHz = (float) (sampleRate * 0.49);

    channels.assign((size_t) spec.numChannels, ChannelFilters{});

    lpSmoothed.reset(sampleRate, smoothingSeconds);
    hpSmoothed.reset(sampleRate, smoothingSeconds);
    lpSmoothed.setCurrentAndTargetValue(nyquistSafeHz);
    hpSmoothed.setCurrentAndTargetValue(minFilterHz);

    reset();
}

void ToneFilters::reset()
{
    for (auto& c : channels)
    {
        c.lowPass = BiquadState{};
        c.highPass = BiquadState{};
    }
}

void ToneFilters::process(juce::AudioBuffer<float>& buffer, float lpHz, float hpHz)
{
    lpSmoothed.setTargetValue(juce::jmin(lpHz, nyquistSafeHz));
    hpSmoothed.setTargetValue(juce::jmax(hpHz, minFilterHz));

    const int numSamples = buffer.getNumSamples();
    lpSmoothed.skip(numSamples);
    hpSmoothed.skip(numSamples);

    const float lpCutoff = lpSmoothed.getCurrentValue();
    const float hpCutoff = hpSmoothed.getCurrentValue();

    const BiquadCoeffs lpCoeffs = makeLowPassCoeffs(sampleRate, lpCutoff);
    const BiquadCoeffs hpCoeffs = makeHighPassCoeffs(sampleRate, hpCutoff);

    const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& cf = channels[(size_t) ch];
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = processBiquad(cf.lowPass, lpCoeffs, processBiquad(cf.highPass, hpCoeffs, data[i]));
    }
}
