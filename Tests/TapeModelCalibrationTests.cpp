#include "TestUtils.h"
#include "../Source/DSP/TapeModelEQ.h"
#include "../Source/DSP/TapeModelData.h"

namespace
{
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
}

// Measures each model's output level against its (unprocessed) input for the same pink noise, so
// TapeModelData.h's makeupGainDb per model can be tuned until all eight are loudness-matched
// (net ~0dB level change from the model's own EQ - only tone should differ between them, not
// volume, per the brief's step 9).
class TapeModelCalibrationTests final : public juce::UnitTest
{
public:
    TapeModelCalibrationTests() : juce::UnitTest("TapeModelCalibration", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 1024;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("All eight models are loudness-matched to within 1dB of their unprocessed input");

        const auto reference = generatePinkNoise(numChannels, blockSize * 30, 606);
        float referenceChunkRms[30];
        for (int block = 0; block < 30; ++block)
        {
            juce::AudioBuffer<float> chunk(numChannels, blockSize);
            for (int ch = 0; ch < numChannels; ++ch)
                chunk.copyFrom(ch, 0, reference, ch, block * blockSize, blockSize);
            referenceChunkRms[block] = rmsOf(chunk);
        }
        double refSumSq = 0.0;
        for (int block = 10; block < 30; ++block) // skip the first 10 blocks (settle-in)
            refSumSq += (double) referenceChunkRms[block] * referenceChunkRms[block];
        const float inputRms = (float) std::sqrt(refSumSq / 20.0);

        for (size_t m = 0; m < kNumTapeModels; ++m)
        {
            TapeModelEQ eq;
            eq.prepare(spec);

            double outSumSq = 0.0;
            for (int block = 0; block < 30; ++block)
            {
                juce::AudioBuffer<float> chunk(numChannels, blockSize);
                for (int ch = 0; ch < numChannels; ++ch)
                    chunk.copyFrom(ch, 0, reference, ch, block * blockSize, blockSize);

                eq.process(chunk, (int) m, false);

                if (block >= 10)
                    outSumSq += (double) rmsOf(chunk) * rmsOf(chunk);
            }
            const float outputRms = (float) std::sqrt(outSumSq / 20.0);

            const float deltaDb = juce::Decibels::gainToDecibels(outputRms) - juce::Decibels::gainToDecibels(inputRms);
            logMessage(juce::String(kTapeModels[m].displayName) + ": " + juce::String(deltaDb, 2)
                       + " dB relative to input (makeupGainDb=" + juce::String(kTapeModels[m].makeupGainDb, 2) + ")");

            expect(std::abs(deltaDb) < 1.0f,
                   juce::String(kTapeModels[m].displayName) + " should be within 1dB of the input level");
        }
    }
};

static TapeModelCalibrationTests tapeModelCalibrationTests;
