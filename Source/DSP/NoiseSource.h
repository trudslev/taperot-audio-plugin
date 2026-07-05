#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// TAPE, VCR, and DUST character generators. TAPE's hiss algorithm and gain smoothing must stay
// bit-identical to the pre-refactor NoiseGenerator so switching character at 100% TAPE nulls
// against the previous build. Reusable per-instance so DegradationCore can own one per generation
// stage, each seeded independently via the constructor's seedOffset.
class NoiseSource
{
public:
    enum Character
    {
        tape = 0,
        vcr = 1,
        dust = 2
    };

    explicit NoiseSource(juce::uint64 seedOffset = 0) noexcept : instanceSeedOffset(seedOffset) {}

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float noiseAmount01, int character);

private:
    static constexpr int numCharacters = 3;

    // TAPE - must match the pre-refactor NoiseGenerator exactly (hiss level, highpass freq,
    // gain smoothing time, and per-channel seed formula) for the null test to pass.
    static constexpr float tapeHissLevel = 0.05f;
    static constexpr float tapeHighpassHz = 2000.0f;

    // VCR - darker hiss bed plus a periodic PAL-style head-switch tick.
    static constexpr float vcrHissLevel = 0.119f;
    static constexpr float vcrHighpassHz = 700.0f;
    static constexpr float vcrTickLevel = 0.343f;
    static constexpr float vcrTickRateHz = 25.0f;
    static constexpr float vcrTickFilterHz = 6000.0f;
    static constexpr float vcrTickFilterQ = 3.5f;

    // DUST - sparse bandpassed crackle (with occasional larger pops) over a reduced hiss bed.
    static constexpr float dustHissBedScale = 0.3f;
    static constexpr float dustCrackleRatePerSecAtFull = 45.0f;
    static constexpr float dustBigPopProbability = 0.07f;
    static constexpr float dustBigPopMultiplier = 3.2f;
    static constexpr float dustCrackleImpulseBase = 0.55f;
    static constexpr float dustCrackleFilterHz = 3000.0f;
    static constexpr float dustCrackleFilterQ = 0.7f;

    static constexpr float crossfadeSeconds = 0.03f;

    struct ChannelState
    {
        // Separate RNG streams per character: TAPE's must be drawn from exactly once per sample
        // (matching the pre-refactor NoiseGenerator) so it stays bit-identical regardless of what
        // VCR/DUST consume from their own streams while all three run continuously for crossfades.
        juce::Random tapeRandom;
        juce::Random vcrRandom;
        juce::Random dustRandom;

        float tapeHpfState = 0.0f, tapeHpfPrevInput = 0.0f;
        float vcrHpfState = 0.0f, vcrHpfPrevInput = 0.0f;

        juce::dsp::IIR::Filter<float> vcrTickFilter;
        juce::dsp::IIR::Filter<float> dustCrackleFilter;
    };

    std::vector<ChannelState> channels;

    double sampleRate = 44100.0;
    juce::uint64 instanceSeedOffset;

    float tapeHpfCoeff = 0.0f;
    float vcrHpfCoeff = 0.0f;

    int vcrPeriodSamples = 1;
    int vcrSamplesUntilTick = 1;

    juce::SmoothedValue<float> tapeGainSmoothed{0.0f};
    juce::SmoothedValue<float> noiseAmountSmoothed{0.0f};

    int activeCharacter = Character::tape;
    int pendingCharacter = -1;
    juce::SmoothedValue<float> crossfadeMix{0.0f};
};
