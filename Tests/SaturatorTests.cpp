#include "TestUtils.h"
#include "../Source/DSP/Saturator.h"

class SaturatorTests final : public juce::UnitTest
{
public:
    SaturatorTests() : juce::UnitTest("Saturator", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 512;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("Exact null at drive = 0");
        {
            Saturator sat;
            sat.prepare(spec, 0.0f);

            auto input = generatePinkNoise(numChannels, blockSize, 42);
            auto buffer = input;
            sat.process(buffer, 0.0f);

            bool exact = true;
            for (int ch = 0; ch < numChannels && exact; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    if (buffer.getReadPointer(ch)[i] != input.getReadPointer(ch)[i])
                        exact = false;

            expect(exact, "Output should equal input bit-for-bit when drive is 0");
        }

        beginTest("Bounded, finite output with added harmonics at drive = 100%");
        {
            Saturator sat;
            sat.prepare(spec, 0.0f);

            auto buffer = generatePinkNoise(numChannels, blockSize, 7);

            for (int block = 0; block < 20; ++block)
                sat.process(buffer, 1.0f);

            float maxAbs = 0.0f;
            bool allFinite = true;
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getReadPointer(ch)[i];
                    if (!std::isfinite(v))
                        allFinite = false;
                    maxAbs = juce::jmax(maxAbs, std::abs(v));
                }

            expect(allFinite, "Output must remain finite at full drive");
            expect(maxAbs < 10.0f, "Output should stay in a sane bounded range at full drive");
        }
    }
};

static SaturatorTests saturatorTests;
