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

    /*  **15 ms, sized against a MEASURED BOUND with a stated margin, and it was 25.**

        The centre exists so the read pointer can wander below it without crossing the write
        pointer — a crossing is an intermittent click nobody would ever trace back. It had 25 ms with
        no recorded derivation.

        Measured over 87 seconds, about **44 wow cycles**, at maximum depth on the SLOWEST stage
        (rate x0.7932, since pitch shift is the rate of change of delay so a slower transport needs
        more excursion for the same deviation):

            wow alone        10.521 ms downward
            wow AND flutter  12.875 ms downward   <- the binding case

        Long enough to be a bound rather than a draw: at 5.5 cycles the same figure read 12.833, so
        it has converged. Wow is 0.7 sine plus 0.3 of a slow random walk, which is why one cycle was
        never going to be enough.

        **Only the DOWNWARD excursion binds this.** Delay above the centre needs buffer length, which
        `maxDelayMs` provides; conflating the two is what once reported 12.90 where the constraint
        was 11.54.

        15.0 = 12.875 measured + 0.083 for `interpolationMarginSamples` + 16 % margin. **An earlier
        estimate of ~13 ms came from the wow-only figure and would have left six samples**, which is
        not a margin — the binding case includes flutter.

        The halving is free: absolute delay offset produces no sound, because pitch modulation is the
        rate of change of delay. A line wandering 2.1-27.9 ms is identical to one wandering
        12.1-37.9 ms. The 10 ms removed was pure latency with nothing attached, and at GEN 8 it is
        200 ms becoming 120. */
    /*  **REVERTED to 25.0 on 2026-08-16, and the reason is block dependence rather than the
        excursion bound.**

        Step 3 sized this to 15.0 against a converged excursion measurement — 12.875 ms downward at
        maximum wow AND flutter over 43.7 cycles, plus the interpolator and 16 % margin — and that
        arithmetic is still correct. 15 ms leaves the read pointer ample room.

        What it also did was take the plugin below a threshold in the DELAY LINE. Against the
        64-block reference, measured at zero depth:

            centre 25 ms   48 kHz 0.000200/0.001022/0.001926   96 kHz 0.000078/0.000393/0.001153
            centre 15 ms   48 kHz 0.029/0.125/0.125            96 kHz 0.068/0.271/0.305

        Consistent at both sample rates, so it is time-domain on the figure that matters. Declared
        latency is compensated by the host; block dependence is compensated by nothing, and the two
        are not commensurable. 120 ms and 200 ms at GEN 8 are both mixing-only, so the halving
        bought no use case.

        **This is choosing to sit above the threshold, not fixing it.** The delay line's sub-15 ms
        block dependence stays open as its own finding — see Tests/InvarianceTests.cpp, where four
        framings for its sub-structure are refuted. */
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
