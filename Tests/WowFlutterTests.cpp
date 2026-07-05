#include "TestUtils.h"
#include "../Source/DSP/WowFlutter.h"

class WowFlutterTests final : public juce::UnitTest
{
public:
    WowFlutterTests() : juce::UnitTest("WowFlutter", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 512;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("Fixed nominal delay, no modulation, when wow = 0 and flutter = 0");
        {
            WowFlutter wf;
            wf.prepare(spec);

            const int numSamples = blockSize * 4;
            auto input = generatePinkNoise(numChannels, numSamples, 11);
            auto buffer = input;

            wf.process(buffer, 0.0f, 0.0f);

            const int nominalDelaySamples = (int) std::round(sampleRate * 0.001 * WowFlutter::nominalDelayMs);
            const int margin = 8;

            bool closeEnough = true;
            for (int ch = 0; ch < numChannels && closeEnough; ++ch)
                for (int i = nominalDelaySamples + margin; i < numSamples - margin; ++i)
                {
                    const float actual = buffer.getReadPointer(ch)[i];
                    const float expected = input.getReadPointer(ch)[i - nominalDelaySamples];
                    if (std::abs(actual - expected) > 1.0e-3f)
                        closeEnough = false;
                }

            expect(closeEnough, "Output should match input delayed by the fixed nominal delay when depths are zero");
        }

        beginTest("Bounded, finite output when wow and flutter are fully engaged");
        {
            WowFlutter wf;
            wf.prepare(spec);

            const int numSamples = blockSize * 4;
            auto buffer = generatePinkNoise(numChannels, numSamples, 99);

            wf.process(buffer, 1.0f, 1.0f);

            bool allFinite = true;
            float maxAbs = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < numSamples; ++i)
                {
                    const float v = buffer.getReadPointer(ch)[i];
                    if (!std::isfinite(v))
                        allFinite = false;
                    maxAbs = juce::jmax(maxAbs, std::abs(v));
                }

            expect(allFinite, "Output must remain finite under full wow/flutter modulation");
            expect(maxAbs < 10.0f, "Output should stay in a sane bounded range under full modulation");
        }
    }
};

static WowFlutterTests wowFlutterTests;
