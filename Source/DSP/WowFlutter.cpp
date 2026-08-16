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
        c.random = juce::Random((juce::int64) (0x9E3779B97F4A7C15ULL * (juce::uint64) (ch + 1) + instanceSeedOffset));
        c.flutterRateHz = (7.0 + c.random.nextDouble() * 5.0) * (double) wowFlutterRateMultiplier;
    }

    reset();
}

void WowFlutter::reset()
{
    delayLine.reset();
    for (auto& c : channels)
    {
        /*  **An independent starting phase per stage, and it used to be zero for every one.**

            The rate spread is what keeps eight transports apart over time; the starting phase is
            what separates them at t = 0. With every wow oscillator starting at phase 0 and running
            at nearly the same rate, eight generations were phase-coherent from the first sample and
            their deviations added — measured 7.96x at GEN 8 against 8.00x for perfect correlation.

            **Rate alone was not enough, and the measurement said so rather than the reasoning.**
            With a +/-25 % rate spread and a common start, GEN 8 came back at 6.98x: over a two-second
            passage, which is about one wow period, a 25 % rate difference has not yet pulled the
            phases apart. The two mechanisms do different jobs and both are needed.

            Derived from the same per-stage stream as everything else here, so it stays
            deterministic. */
        c.wowPhase = (double) c.random.nextFloat() * juce::MathConstants<double>::twoPi;
        c.flutterPhase = (double) c.random.nextFloat() * juce::MathConstants<double>::twoPi;
        c.wowRandomLpfState = 0.0f;
        c.flutterNoiseLpfState = 0.0f;
        c.flutterNoiseHpfState = 0.0f;
        c.flutterNoisePrevInput = 0.0f;
        // Not seeded to a guess any more — see `deviationPrimed`. The first sample after a reset
        // adopts whatever the delay actually is and reports zero deviation, which is both correct
        // and independent of where the modulation happens to start.
        c.previousDelaySamples = centerDelaySamples;
        c.deviationPrimed = false;
    }
}

void WowFlutter::process(juce::AudioBuffer<float>& buffer, float wowDepth01, float flutterDepth01,
                         float* deviationCentsAccum)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());

    const float wowDepthSamples = wowDepth01 * maxWowMs * 0.001f * (float) sampleRate;
    const float flutterDepthSamples = flutterDepth01 * maxFlutterMs * 0.001f * (float) sampleRate;

    const double wowPhaseInc = juce::MathConstants<double>::twoPi * (wowRateHz * (double) wowFlutterRateMultiplier) / sampleRate;
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

            if (ch == 0)
            {
                // cents = -1200/ln2 * d(delay)/dn. The delay is in samples and dn is one sample, so
                // the derivative is just the difference against the previous value. Negative
                // because a lengthening delay slows playback and pitch falls.
                //
                // previousDelaySamples is updated whether or not anyone is metering, so a stage
                // that starts contributing part-way through (GEN being raised) compares against a
                // current value rather than a stale one and does not emit a spurious spike.
                constexpr float centsPerSampleSlope = -1200.0f / 0.6931472f;   // -1200/ln2

                if (deviationCentsAccum != nullptr)
                    deviationCentsAccum[i] += c.deviationPrimed
                                                ? centsPerSampleSlope * (delaySamples - c.previousDelaySamples)
                                                : 0.0f;

                c.previousDelaySamples = delaySamples;
                c.deviationPrimed = true;
            }

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
