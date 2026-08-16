#include "DegradationCore.h"

#include <cmath>

namespace
{
    // Large strides so per-stage seed offsets never overlap NoiseSource's/WowFlutter's own
    // per-channel seed spacing.
    constexpr juce::uint64 wowFlutterSeedStride = 0x1000000000ULL;
    constexpr juce::uint64 noiseSeedStride = 0x2000000000ULL;
}

float DegradationCore::wowRateMultiplierFor(int stageIndex) noexcept
{
    // Deterministic in the stage index, so the plugin stays reproducible; drawn rather than ramped,
    // so the spacing between machines is not itself a regular pattern. See the header.
    juce::Random r((juce::int64) (0x9E3779B97F4A7C15ULL * (juce::uint64) (stageIndex + 1)));

    return 1.0f + wowRateSpread * (r.nextFloat() * 2.0f - 1.0f);
}

float DegradationCore::generationLossCoeffFor(double sampleRate, int modelIndex) noexcept
{
    const auto& model = kTapeModels[(size_t) juce::jlimit(0, (int) kNumTapeModels - 1, modelIndex)];

    if (model.generationLossHz <= 0.0f)
        return 0.0f;   // no transfer loss: a pass-through, not a filter parked at DC

    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * model.generationLossHz
                            / (float) sampleRate);
}

DegradationCore::DegradationCore(int stageIndex)
    : applySaturation(stageIndex > 0),
      wowFlutter((juce::uint64) stageIndex * wowFlutterSeedStride,
                 wowRateMultiplierFor(stageIndex)),
      noiseSource((juce::uint64) stageIndex * noiseSeedStride)
{
}

void DegradationCore::prepare(const juce::dsp::ProcessSpec& spec, int initialModelIndex,
                               float initialNoiseAmount01)
{
    wowFlutter.prepare(spec);
    tapeModelEQ.prepare(spec, initialModelIndex);

    generationLossState.assign((size_t) spec.numChannels, 0.0f);
    generationLossSampleRate = spec.sampleRate;
    noiseSource.prepare(spec, initialNoiseAmount01);
}

void DegradationCore::reset()
{
    std::fill(generationLossState.begin(), generationLossState.end(), 0.0f);
    wowFlutter.reset();
    tapeModelEQ.reset();
    noiseSource.reset();
}

void DegradationCore::process(juce::AudioBuffer<float>& buffer, float wow01, float flutter01, int model, bool clunkMode,
                               float noiseAmount01, int noiseCharacter, float* deviationCentsAccum)
{
    wowFlutter.process(buffer, wow01, flutter01, deviationCentsAccum);
    tapeModelEQ.process(buffer, model, clunkMode);

    /*  **Every generation loses top end, and the loss multiplies.** This is the law GEN is
        supposed to express and the one that was missing entirely — see `generationLossHz`. Applied
        per copy, before the dub saturation, because a transfer band-limits and then the electronics
        colour what is left. */
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), (int) generationLossState.size());

        // Per block, from the model the block is actually running — see TapeModel::generationLossHz.
        const float generationLossCoeff = generationLossCoeffFor(generationLossSampleRate, model);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            auto& state = generationLossState[(size_t) ch];

            if (generationLossCoeff <= 0.0f)
                continue;

            for (int i = 0; i < numSamples; ++i)
            {
                state += generationLossCoeff * (data[i] - state);
                data[i] = state;
            }
        }
    }

    if (applySaturation)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                /*  **`/ drive`, NOT `/ tanh(drive)`, and that one character was +26 dB.**

                    The normalised form is unity only at FULL SCALE; below it the gain is
                    `drive / tanh(drive)` = **1.5445** at drive 1.35. Seven cascaded stages therefore
                    multiplied small signals by 20.97 — **+26.4 dB** — which is where the measured
                    +20.8 dB of noise accumulation across GEN 1..8 came from. Real dubbing is
                    unity-gain per generation, because an engineer sets levels on every copy; what
                    accumulates is the NOISE of eight independent transfers, and independent sources
                    power-sum to 10*log10(8) = +9 dB.

                    `tanh(x*d)/d` is unity for small signals at any drive. **This is the suite's own
                    recorded gain-staging trap**, written down in Elmer's `IronStage`, Gatecrasher's
                    `SlamSaturation` and Fifth Member's history — where it shipped and a feedback
                    loop ran away. TapeRot's own `Saturator` uses the normalised form correctly,
                    because there it is a single stage with an explicit makeup; cascaded eight deep
                    with none, it is a gain of twenty. */
                data[i] = std::tanh(data[i] * gentleSaturationDrive) / gentleSaturationDrive;
            }
        }
    }

    noiseSource.process(buffer, noiseAmount01, noiseCharacter);
}
