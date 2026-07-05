#include "TestUtils.h"
#include "../Source/DSP/TapeModelEQ.h"

class TapeModelEQTests final : public juce::UnitTest
{
public:
    TapeModelEQTests() : juce::UnitTest("TapeModelEQ", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 512;
        const int numChannels = 2;
        const int numBlocks = 20;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("No large discontinuity when switching models mid-stream");
        {
            TapeModelEQ eq;
            eq.prepare(spec);

            const int totalSamples = blockSize * numBlocks;
            auto fullSignal = generatePinkNoise(numChannels, totalSamples, 555);

            float maxAbsDiff = 0.0f;
            bool allFinite = true;
            float previousSample[2] = {0.0f, 0.0f};
            bool havePrevious = false;
            int model = 0;

            for (int block = 0; block < numBlocks; ++block)
            {
                juce::AudioBuffer<float> chunk(numChannels, blockSize);
                for (int ch = 0; ch < numChannels; ++ch)
                    chunk.copyFrom(ch, 0, fullSignal, ch, block * blockSize, blockSize);

                if (block == 10)
                    model = 3;

                eq.process(chunk, model);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = chunk.getReadPointer(ch);
                    for (int i = 0; i < blockSize; ++i)
                    {
                        if (!std::isfinite(data[i]))
                            allFinite = false;

                        if (havePrevious || i > 0)
                        {
                            const float prev = (i == 0) ? previousSample[ch] : data[i - 1];
                            maxAbsDiff = juce::jmax(maxAbsDiff, std::abs(data[i] - prev));
                        }
                    }
                    previousSample[ch] = data[blockSize - 1];
                }
                havePrevious = true;
            }

            expect(allFinite, "Output must remain finite across a model switch");
            expect(maxAbsDiff < 1.0f, "No large sample-to-sample jump around a model switch");
        }
    }
};

static TapeModelEQTests tapeModelEQTests;
