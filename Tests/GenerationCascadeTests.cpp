#include "TestUtils.h"
#include "../Source/DSP/DegradationCore.h"
#include "../Source/DSP/WowFlutter.h"
#include "../Source/DSP/TapeModelEQ.h"
#include "../Source/DSP/NoiseSource.h"
#include <memory>

namespace
{
    juce::AudioBuffer<float> generateSine(int numChannels, int numSamples, double sampleRate, float freqHz)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::sin(juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
        }
        return buffer;
    }

    float rmsOf(const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        juce::int64 count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                sum += (double) data[i] * (double) data[i];
                ++count;
            }
        }
        return (float) std::sqrt(sum / (double) count);
    }

    std::unique_ptr<DegradationCore> makeCascadeStage(int index, const juce::dsp::ProcessSpec& spec)
    {
        auto stage = std::make_unique<DegradationCore>(index);
        stage->prepare(spec, 0);
        return stage;
    }
}

class GenerationCascadeTests final : public juce::UnitTest
{
public:
    GenerationCascadeTests() : juce::UnitTest("GenerationCascade", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 1024;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("GEN=1 (stage 0 alone) matches a direct WowFlutter -> TapeModelEQ -> NoiseSource(TAPE) pass");
        {
            auto stage0 = makeCascadeStage(0, spec);

            WowFlutter wow;
            wow.prepare(spec);
            TapeModelEQ eq;
            eq.prepare(spec, 0);
            NoiseSource noise;
            noise.prepare(spec);

            auto input = generatePinkNoise(numChannels, blockSize, 3141);
            auto actual = input;
            auto expected = input;

            for (int block = 0; block < 5; ++block)
            {
                stage0->process(actual, 0.5f, 0.3f, 2, false, 0.4f, NoiseSource::tape);

                wow.process(expected, 0.5f, 0.3f);
                eq.process(expected, 2, false);
                noise.process(expected, 0.4f, NoiseSource::tape);
            }

            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError(actual.getReadPointer(ch)[i], expected.getReadPointer(ch)[i], 1.0e-6f);
        }

        beginTest("GEN=6 shows measurably reduced high-frequency bandwidth versus GEN=1");
        {
            constexpr int model = 3; // VCR LP: rolloff from 8kHz, compounds each pass (NONE is index 0)
            constexpr float testToneHz = 15000.0f;

            std::array<std::unique_ptr<DegradationCore>, 8> stages;
            for (int i = 0; i < 8; ++i)
                stages[(size_t) i] = makeCascadeStage(i, spec);

            auto gen1 = generateSine(numChannels, blockSize, sampleRate, testToneHz);
            auto gen6 = gen1;

            for (int block = 0; block < 20; ++block)
            {
                stages[0]->process(gen1, 0.0f, 0.0f, model, false, 0.0f, NoiseSource::tape);
                for (int s = 0; s < 6; ++s)
                    stages[(size_t) s]->process(gen6, 0.0f, 0.0f, model, false, 0.0f, NoiseSource::tape);
            }

            const float rms1 = rmsOf(gen1);
            const float rms6 = rmsOf(gen6);
            logMessage("15kHz tone RMS - GEN=1: " + juce::String(rms1, 5) + ", GEN=6: " + juce::String(rms6, 5));
            expect(rms6 < rms1 * 0.8f, "GEN=6 should attenuate a 15kHz tone noticeably more than GEN=1 (bandwidth shrinks)");
        }

        beginTest("GEN=6 shows measurably raised noise floor versus GEN=1");
        {
            std::array<std::unique_ptr<DegradationCore>, 8> stages;
            for (int i = 0; i < 8; ++i)
                stages[(size_t) i] = makeCascadeStage(i, spec);

            juce::AudioBuffer<float> gen1(numChannels, blockSize);
            juce::AudioBuffer<float> gen6(numChannels, blockSize);

            double sum1 = 0.0, sum6 = 0.0;
            juce::int64 count = 0;

            for (int block = 0; block < 40; ++block)
            {
                gen1.clear();
                gen6.clear();

                stages[0]->process(gen1, 0.0f, 0.0f, 0, false, 0.5f, NoiseSource::tape);
                for (int s = 0; s < 6; ++s)
                    stages[(size_t) s]->process(gen6, 0.0f, 0.0f, 0, false, 0.5f, NoiseSource::tape);

                if (block >= 5) // past the gain-smoothing settle-in
                {
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        const auto* d1 = gen1.getReadPointer(ch);
                        const auto* d6 = gen6.getReadPointer(ch);
                        for (int i = 0; i < blockSize; ++i)
                        {
                            sum1 += (double) d1[i] * d1[i];
                            sum6 += (double) d6[i] * d6[i];
                        }
                        count += blockSize;
                    }
                }
            }

            const float rms1 = (float) std::sqrt(sum1 / (double) count);
            const float rms6 = (float) std::sqrt(sum6 / (double) count);
            logMessage("Noise floor (silence in) - GEN=1: " + juce::String(rms1, 5) + ", GEN=6: " + juce::String(rms6, 5));
            expect(rms6 > rms1 * 1.5f, "GEN=6 should have a noticeably higher noise floor than GEN=1 (independent per-stage noise sums)");
        }
    }
};

static GenerationCascadeTests generationCascadeTests;
