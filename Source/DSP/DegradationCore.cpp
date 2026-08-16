#include "DegradationCore.h"

namespace
{
    // Large strides so per-stage seed offsets never overlap NoiseSource's/WowFlutter's own
    // per-channel seed spacing.
    constexpr juce::uint64 wowFlutterSeedStride = 0x1000000000ULL;
    constexpr juce::uint64 noiseSeedStride = 0x2000000000ULL;
}

DegradationCore::DegradationCore(int stageIndex)
    : applySaturation(stageIndex > 0),
      wowFlutter((juce::uint64) stageIndex * wowFlutterSeedStride,
                 1.0f + (float) stageIndex * wowFlutterDetunePerStage),
      noiseSource((juce::uint64) stageIndex * noiseSeedStride)
{
}

void DegradationCore::prepare(const juce::dsp::ProcessSpec& spec, int initialModelIndex)
{
    wowFlutter.prepare(spec);
    tapeModelEQ.prepare(spec, initialModelIndex);
    noiseSource.prepare(spec);
}

void DegradationCore::reset()
{
    wowFlutter.reset();
    tapeModelEQ.reset();
    noiseSource.reset();
}

void DegradationCore::process(juce::AudioBuffer<float>& buffer, float wow01, float flutter01, int model, bool clunkMode,
                               float noiseAmount01, int noiseCharacter, float* deviationCentsAccum)
{
    wowFlutter.process(buffer, wow01, flutter01, deviationCentsAccum);
    tapeModelEQ.process(buffer, model, clunkMode);

    if (applySaturation)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        const float normaliser = std::tanh(gentleSaturationDrive);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::tanh(data[i] * gentleSaturationDrive) / normaliser;
        }
    }

    noiseSource.process(buffer, noiseAmount01, noiseCharacter);
}
