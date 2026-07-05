#include "TestUtils.h"
#include "../Source/DSP/Saturator.h"
#include "../Source/DSP/WowFlutter.h"
#include "../Source/DSP/TapeModelEQ.h"

class CPUCheckTests final : public juce::UnitTest
{
public:
    CPUCheckTests() : juce::UnitTest("CPUCheck", "Performance") {}

    void runTest() override
    {
        beginTest("Processing stays within real-time budget at 48kHz/64 samples");

        const double sampleRate = 48000.0;
        const int blockSize = 64;
        const int numChannels = 2;
        const int numIterations = 10000;

        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        Saturator sat;
        WowFlutter wow;
        TapeModelEQ eq;
        sat.prepare(spec);
        wow.prepare(spec);
        eq.prepare(spec);

        auto buffer = generatePinkNoise(numChannels, blockSize, 2024);

        const double start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < numIterations; ++i)
        {
            sat.process(buffer, 0.6f);
            wow.process(buffer, 0.5f, 0.5f);
            eq.process(buffer, 2);
        }
        const double end = juce::Time::getMillisecondCounterHiRes();

        const double totalMs = end - start;
        const double avgMsPerBlock = totalMs / (double) numIterations;
        const double realTimeBudgetMs = ((double) blockSize / sampleRate) * 1000.0;

        logMessage("Average block time: " + juce::String(avgMsPerBlock, 4) + " ms (budget: "
                   + juce::String(realTimeBudgetMs, 4) + " ms)");
        logMessage("CPU load: " + juce::String(100.0 * avgMsPerBlock / realTimeBudgetMs, 2) + "%");

        expect(avgMsPerBlock < realTimeBudgetMs, "Average block processing time should be below the real-time budget");
    }
};

static CPUCheckTests cpuCheckTests;
