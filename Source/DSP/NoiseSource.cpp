#include "NoiseSource.h"

void NoiseSource::prepare(const juce::dsp::ProcessSpec& spec, float initialNoiseAmount01)
{
    sampleRate = spec.sampleRate;
    const int numChannels = (int) spec.numChannels;

    channels.resize((size_t) numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        const auto chMult = (juce::uint64) (ch + 1);
        c.tapeRandom = juce::Random((juce::int64) (0xA5A5A5A5A5A5A5A5ULL * chMult + instanceSeedOffset));
        c.vcrRandom = juce::Random((juce::int64) (0xB5B5B5B5B5B5B5B5ULL * chMult + instanceSeedOffset));
        c.dustRandom = juce::Random((juce::int64) (0xC5C5C5C5C5C5C5C5ULL * chMult + instanceSeedOffset));

        c.vcrTickFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate, vcrTickFilterHz, vcrTickFilterQ);
        c.dustCrackleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate, dustCrackleFilterHz, dustCrackleFilterQ);
    }

    tapeHpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * tapeHighpassHz / (float) sampleRate);
    vcrHpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * vcrHighpassHz / (float) sampleRate);

    vcrPeriodSamples = juce::jmax(1, (int) std::round(sampleRate / vcrTickRateHz));

    tapeGainSmoothed.reset(sampleRate, 0.02);
    noiseAmountSmoothed.reset(sampleRate, 0.02);
    crossfadeMix.reset(sampleRate, crossfadeSeconds);

    tapeGainSmoothed.setCurrentAndTargetValue(initialNoiseAmount01 * tapeHissLevel);
    noiseAmountSmoothed.setCurrentAndTargetValue(initialNoiseAmount01);

    // **A LITERAL, and the only one of the eight that should be.** The other seven track a
    // parameter and take its value; this is a character crossfade, whose settled state is 1.0 —
    // fully arrived at the current character, nothing pending. Snapping it to a stale target is
    // what left it mid-fade on the first block, which is half of the block-size row this casting
    // still carries: "its character crossfade is started per prepare and stepped per block".
    crossfadeMix.setCurrentAndTargetValue(1.0f);

    reset();
}

void NoiseSource::reset()
{
    for (auto& c : channels)
    {
        c.tapeHpfState = 0.0f;
        c.tapeHpfPrevInput = 0.0f;
        c.vcrHpfState = 0.0f;
        c.vcrHpfPrevInput = 0.0f;
        c.vcrTickFilter.reset();
        c.dustCrackleFilter.reset();
    }

    vcrSamplesUntilTick = vcrPeriodSamples;
    tapeGainSmoothed.setCurrentAndTargetValue(tapeGainSmoothed.getTargetValue());
    noiseAmountSmoothed.setCurrentAndTargetValue(noiseAmountSmoothed.getTargetValue());
}

void NoiseSource::process(juce::AudioBuffer<float>& buffer, float noiseAmount01, int character)
{
    character = juce::jlimit(0, numCharacters - 1, character);
    noiseAmount01 = juce::jlimit(0.0f, 1.0f, noiseAmount01);

    if (character != activeCharacter && character != pendingCharacter)
    {
        pendingCharacter = character;
        crossfadeMix.setCurrentAndTargetValue(0.0f);
        crossfadeMix.setTargetValue(1.0f);
    }

    tapeGainSmoothed.setTargetValue(noiseAmount01 * tapeHissLevel);
    noiseAmountSmoothed.setTargetValue(noiseAmount01);

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());

    constexpr int kMaxChannels = 2;
    std::array<float*, kMaxChannels> data{};
    const int activeChannels = juce::jmin(numCh, kMaxChannels);
    for (int ch = 0; ch < activeChannels; ++ch)
        data[(size_t) ch] = buffer.getWritePointer(ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const float tapeGain = tapeGainSmoothed.getNextValue();
        const float amt = noiseAmountSmoothed.getNextValue();

        bool vcrTickNow = false;
        if (--vcrSamplesUntilTick <= 0)
        {
            vcrTickNow = true;
            vcrSamplesUntilTick = vcrPeriodSamples;
        }
        const float vcrTickImpulse = vcrTickNow ? (vcrTickLevel * amt) : 0.0f;

        const bool crossfading = pendingCharacter != -1;
        float cosTheta = 1.0f, sinTheta = 0.0f;
        if (crossfading)
        {
            const float mix = crossfadeMix.getNextValue();
            const float theta = mix * juce::MathConstants<float>::halfPi;
            cosTheta = std::cos(theta);
            sinTheta = std::sin(theta);
        }

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];

            // TAPE
            const float tapeWhite = c.tapeRandom.nextFloat() * 2.0f - 1.0f;
            c.tapeHpfState = tapeHpfCoeff * (c.tapeHpfState + tapeWhite - c.tapeHpfPrevInput);
            c.tapeHpfPrevInput = tapeWhite;
            const float tapeOut = c.tapeHpfState * tapeGain;

            // VCR
            const float vcrWhite = c.vcrRandom.nextFloat() * 2.0f - 1.0f;
            c.vcrHpfState = vcrHpfCoeff * (c.vcrHpfState + vcrWhite - c.vcrHpfPrevInput);
            c.vcrHpfPrevInput = vcrWhite;
            const float vcrHiss = c.vcrHpfState * vcrHissLevel * amt;
            const float tickOut = c.vcrTickFilter.processSample(vcrTickImpulse);
            const float vcrOut = vcrHiss + tickOut;

            // DUST
            float crackleImpulse = 0.0f;
            const float triggerProb = dustCrackleRatePerSecAtFull * amt / (float) sampleRate;
            if (c.dustRandom.nextFloat() < triggerProb)
            {
                const bool bigPop = c.dustRandom.nextFloat() < dustBigPopProbability;
                // Amplitude scales as sqrt(amt) rather than amt: since rate already scales linearly
                // with amt, this keeps crackle power (rate * amplitude^2) tracking TAPE's amt^2
                // power curve instead of outpacing it as the knob approaches 100%.
                const float amp = dustCrackleImpulseBase * std::sqrt(amt) * (bigPop ? dustBigPopMultiplier : 1.0f)
                                   * (0.75f + c.dustRandom.nextFloat() * 0.5f);
                crackleImpulse = c.dustRandom.nextBool() ? amp : -amp;
            }
            const float crackleOut = c.dustCrackleFilter.processSample(crackleImpulse);
            const float dustOut = crackleOut + tapeOut * dustHissBedScale;

            const float outputs[numCharacters] = { tapeOut, vcrOut, dustOut };
            float finalOut = outputs[activeCharacter];
            if (crossfading)
                finalOut = outputs[activeCharacter] * cosTheta + outputs[pendingCharacter] * sinTheta;

            data[(size_t) ch][i] += finalOut;
        }
    }

    if (pendingCharacter != -1 && crossfadeMix.getCurrentValue() >= 1.0f - 1.0e-6f
        && crossfadeMix.getTargetValue() >= 1.0f - 1.0e-6f)
    {
        activeCharacter = pendingCharacter;
        pendingCharacter = -1;
    }
}
