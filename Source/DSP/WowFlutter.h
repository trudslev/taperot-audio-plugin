#pragma once

#include <juce_dsp/juce_dsp.h>

class WowFlutter
{
public:
    // seedOffset decorrelates multiple instances' random modulation (e.g. per GEN cascade stage);
    // rateMultiplier slightly detunes wow/flutter rates so cascaded instances don't phase-lock.
    // Defaults reproduce the exact original single-instance behavior.
    explicit WowFlutter(juce::uint64 seedOffset = 0, float rateMultiplier = 1.0f) noexcept
        : instanceSeedOffset(seedOffset), wowFlutterRateMultiplier(rateMultiplier) {}

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    // deviationCentsAccum, when non-null, receives this instance's realised pitch deviation in
    // cents, ADDED to whatever is already there (numSamples floats, caller-owned). Passing null
    // costs nothing and is what every non-metered call does.
    //
    // Pitch deviation is not the modulation value - it is its rate of change. For a delay line the
    // instantaneous pitch ratio is 1 - d(delay)/dn, so cents = -1200/ln2 * d(delay)/dn. Reading the
    // modulation itself would just redraw the WOW knob; this measures what the audio actually does.
    //
    // Channel 0 only: channel 1 carries channelPhaseOffsetRad, and averaging the two partially
    // cancels the very deviation a scope exists to show.
    void process(juce::AudioBuffer<float>& buffer, float wowDepth01, float flutterDepth01,
                 float* deviationCentsAccum = nullptr);

    // Metering, GUI-thread readable: the rates this instance is actually running at. Both are fixed
    // per instance (flutter is randomised at construction, wow is scaled by rateMultiplier), so a
    // plain read is safe - there is nothing to tear.
    float getWowRateHz() const noexcept { return wowRateHz * wowFlutterRateMultiplier; }
    float getFlutterRateHz() const noexcept { return channels.empty() ? 0.0f : (float) channels[0].flutterRateHz; }

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
        // Metering only: last block's final delay, so the derivative is continuous across
        // block boundaries instead of showing a spurious spike on the first sample.
        float previousDelaySamples = 0.0f;

        /*  **Primed on the first sample after a reset, rather than guessed at reset time.**

            The pitch tap is the DERIVATIVE of the delay line, so it needs a previous value. Reset
            used to seed that with the centre delay, which is right only while the modulation also
            starts at its centre — true when every wow oscillator started at phase 0, and false as
            soon as they start at independent phases. Seeded wrong, the first sample reports a full
            step as a derivative: measured at 797317 cents against a real 400.

            Reset cannot compute the right value, because the delay depends on the wow and flutter
            depths and those arrive as process arguments. So the first sample after a reset takes
            whatever the delay actually is and reports zero deviation for it — which is what a
            derivative with no predecessor honestly is. */
        bool deviationPrimed = false;
        juce::Random random;
    };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine{2000};
    std::vector<ChannelState> channels;
    double sampleRate = 44100.0;
    float maxDelaySamples = 0.0f;
    float centerDelaySamples = 0.0f;

    juce::uint64 instanceSeedOffset;
    float wowFlutterRateMultiplier;
};
