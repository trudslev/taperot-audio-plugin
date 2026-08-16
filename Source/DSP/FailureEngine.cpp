#include "FailureEngine.h"
#include <algorithm>
#include <cmath>

namespace
{
    float raisedCosineWindow(float x) noexcept
    {
        return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * x);
    }
}

void FailureEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const int numChannels = (int) spec.numChannels;

    crinkleHpfState.assign((size_t) numChannels, 0.0f);
    crinkleHpfPrevInput.assign((size_t) numChannels, 0.0f);
    crinkleHpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * crinkleHpfHz / (float) sampleRate);

    /*  **HERE, and not in `reset()`, and that placement is a ruling rather than a preference.**

        `random` was seeded once at construction and nowhere else, which made this the only generator
        in the suite that failed even the *prepare* premise: two renders of the same audio through
        one instance were different performances, measured at a self-comparison of **0.914** with
        FAILURE at 100. Every block-size figure taken in that configuration was that number wearing a
        different name.

        Nothing was affected while FAILURE defaulted to 0 and no audio test applied a Program - and
        both halves of that stop being true at the same moment, because `FactoryPrograms.h:77` carries
        a non-zero `failurePercent` and the realism work ends in rewriting the bank. The first audio
        test written against the rewritten Programs would have read as non-deterministic for a reason
        nobody would connect to a generator seed, and the symptom would have looked like the Program
        rewrite breaking something. That is why it lands ahead of it.

        **A `reset()` owes a cleared tail, not a rewound generator** - it is a transport event rather
        than an instantiation, and a stream rewound by it replays identical crinkle on every lap of a
        loop. `nf::testing::reproducibleAcrossReset` measures the difference across all six castings;
        this engine now matches the other four rather than Chorus-60's former superset.

        An earlier note in the root CLAUDE.md said to seed `FailureEngine::reset()`. That predates the
        driver and is superseded by it.
    */
    random = juce::Random(generatorSeed);

    reset();
}

void FailureEngine::reset()
{
    dropoutState = {};
    snagState = {};
    crinkleState = {};
    wobbleState = {};
    samplePosition = 0;
    std::fill(crinkleHpfState.begin(), crinkleHpfState.end(), 0.0f);
    std::fill(crinkleHpfPrevInput.begin(), crinkleHpfPrevInput.end(), 0.0f);
}

void FailureEngine::triggerIfDue(EventState& state, bool enabled, float ratePerSecAtFull, float minMs, float maxMs,
                                  float failureAmount01, FailureEventType type, juce::int64 sampleTime) noexcept
{
    if (state.active || !enabled || failureAmount01 <= 0.0f)
        return;

    const float probabilityThisSample = ratePerSecAtFull * failureAmount01 / (float) sampleRate;
    if (random.nextFloat() >= probabilityThisSample)
        return;

    const float durationMs = minMs + random.nextFloat() * (maxMs - minMs);
    state.active = true;
    state.elapsedSamples = 0;
    state.totalSamples = juce::jmax(1, (int) std::round(durationMs * 0.001f * (float) sampleRate));
    state.intensity = failureAmount01 * (0.5f + random.nextFloat() * 0.5f);
    state.channel = random.nextBool() ? 0 : 1;

    pushEvent(type, state.intensity, sampleTime);
}

void FailureEngine::process(juce::AudioBuffer<float>& buffer, float failureAmount01,
                             bool dropoutsEnabled, bool snagsEnabled,
                             bool crinklesEnabled, bool imbalanceEnabled)
{
    failureAmount01 = juce::jlimit(0.0f, 1.0f, failureAmount01);

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    constexpr int kMaxChannels = 2;
    std::array<float*, kMaxChannels> data{};
    const int activeChannels = juce::jmin(numCh, kMaxChannels);
    for (int ch = 0; ch < activeChannels; ++ch)
        data[(size_t) ch] = buffer.getWritePointer(ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const juce::int64 sampleTime = samplePosition + i;

        triggerIfDue(dropoutState, dropoutsEnabled, dropoutRatePerSecAtFull, dropoutMinMs, dropoutMaxMs,
                     failureAmount01, FailureEventType::dropout, sampleTime);
        triggerIfDue(snagState, snagsEnabled, snagRatePerSecAtFull, snagMinMs, snagMaxMs,
                     failureAmount01, FailureEventType::snag, sampleTime);
        triggerIfDue(crinkleState, crinklesEnabled, crinkleRatePerSecAtFull, crinkleMinMs, crinkleMaxMs,
                     failureAmount01, FailureEventType::crinkle, sampleTime);
        triggerIfDue(wobbleState, imbalanceEnabled, wobbleRatePerSecAtFull, wobbleMinMs, wobbleMaxMs,
                     failureAmount01, FailureEventType::wobble, sampleTime);

        if (dropoutState.active)
        {
            const float gain = 1.0f - raisedCosineWindow((float) dropoutState.elapsedSamples
                                                           / (float) dropoutState.totalSamples) * dropoutState.intensity;
            for (int ch = 0; ch < activeChannels; ++ch)
                data[(size_t) ch][i] *= gain;
            if (++dropoutState.elapsedSamples >= dropoutState.totalSamples)
                dropoutState.active = false;
        }

        if (snagState.active)
        {
            const float envelope = raisedCosineWindow((float) snagState.elapsedSamples / (float) snagState.totalSamples);
            const float flutter = std::abs(std::sin(juce::MathConstants<float>::twoPi * snagFlutterHz
                                                      * (float) snagState.elapsedSamples / (float) sampleRate));
            const float gain = 1.0f - envelope * flutter * snagState.intensity;
            for (int ch = 0; ch < activeChannels; ++ch)
                data[(size_t) ch][i] *= gain;
            if (++snagState.elapsedSamples >= snagState.totalSamples)
                snagState.active = false;
        }

        if (crinkleState.active)
        {
            const float envelope = raisedCosineWindow((float) crinkleState.elapsedSamples
                                                        / (float) crinkleState.totalSamples) * crinkleState.intensity;
            for (int ch = 0; ch < activeChannels; ++ch)
            {
                const float white = random.nextFloat() * 2.0f - 1.0f;
                auto& hpfState = crinkleHpfState[(size_t) ch];
                auto& prevInput = crinkleHpfPrevInput[(size_t) ch];
                hpfState = crinkleHpfCoeff * (hpfState + white - prevInput);
                prevInput = white;
                data[(size_t) ch][i] += hpfState * envelope * crinkleNoiseLevel;
            }
            if (++crinkleState.elapsedSamples >= crinkleState.totalSamples)
                crinkleState.active = false;
        }

        if (wobbleState.active)
        {
            const float depth = raisedCosineWindow((float) wobbleState.elapsedSamples
                                                     / (float) wobbleState.totalSamples) * wobbleState.intensity;
            const float gain = 1.0f - depth;
            const int targetChannel = juce::jlimit(0, activeChannels - 1, wobbleState.channel);
            data[(size_t) targetChannel][i] *= gain;
            if (++wobbleState.elapsedSamples >= wobbleState.totalSamples)
                wobbleState.active = false;
        }
    }

    samplePosition += numSamples;
}
