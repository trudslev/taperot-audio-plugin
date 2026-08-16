#include "TestUtils.h"
#include "../Source/DSP/Saturator.h"
#include "../Source/DSP/WowFlutter.h"
#include "../Source/DSP/TapeModelEQ.h"
#include "../Source/DSP/DegradationCore.h"
#include <memory>

class CPUCheckTests final : public juce::UnitTest
{
public:
    CPUCheckTests() : juce::UnitTest("CPUCheck", "Performance") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 64;
        const int numChannels = 2;
        const int numIterations = 10000;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};
        const double realTimeBudgetMs = ((double) blockSize / sampleRate) * 1000.0;

        beginTest("Processing stays within real-time budget at 48kHz/64 samples");
        {
            Saturator sat;
            WowFlutter wow;
            TapeModelEQ eq;
            sat.prepare(spec, 0.0f);
            wow.prepare(spec);
            eq.prepare(spec, 0);

            auto buffer = generatePinkNoise(numChannels, blockSize, 2024);

            const double start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < numIterations; ++i)
            {
                sat.process(buffer, 0.6f);
                wow.process(buffer, 0.5f, 0.5f);
                eq.process(buffer, 2, false);
            }
            const double end = juce::Time::getMillisecondCounterHiRes();

            const double avgMsPerBlock = (end - start) / (double) numIterations;

            logMessage("Average block time: " + juce::String(avgMsPerBlock, 4) + " ms (budget: "
                       + juce::String(realTimeBudgetMs, 4) + " ms)");
            logMessage("CPU load: " + juce::String(100.0 * avgMsPerBlock / realTimeBudgetMs, 2) + "%");

            expect(avgMsPerBlock < realTimeBudgetMs, "Average block processing time should be below the real-time budget");
        }

        beginTest("Naive GEN cascade (1 vs 8 DegradationCore stages) CPU cost at 48kHz/64 samples");
        {
            std::array<std::unique_ptr<DegradationCore>, 8> stages;
            for (int i = 0; i < 8; ++i)
            {
                stages[(size_t) i] = std::make_unique<DegradationCore>(i);
                stages[(size_t) i]->prepare(spec, 0, 0.0f);
            }

            auto buffer = generatePinkNoise(numChannels, blockSize, 4111);

            for (int genCount : { 1, 8 })
            {
                const double start = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < numIterations; ++i)
                    for (int stage = 0; stage < genCount; ++stage)
                        stages[(size_t) stage]->process(buffer, 0.5f, 0.5f, 2, false, 0.4f, NoiseSource::tape);
                const double end = juce::Time::getMillisecondCounterHiRes();

                const double avgMsPerBlock = (end - start) / (double) numIterations;
                const double cpuPercent = 100.0 * avgMsPerBlock / realTimeBudgetMs;

                logMessage("GEN=" + juce::String(genCount) + " (FADE) average block time: "
                           + juce::String(avgMsPerBlock, 4) + " ms, CPU load: "
                           + juce::String(cpuPercent, 2) + "%");

                expect(avgMsPerBlock < realTimeBudgetMs,
                       "GEN=" + juce::String(genCount) + " cascade should stay below the real-time budget");
            }
        }

        beginTest("GEN cascade CPU cost in CLUNK mode (per-sample band loop, not the batched FADE path)");
        {
            std::array<std::unique_ptr<DegradationCore>, 8> stages;
            for (int i = 0; i < 8; ++i)
            {
                stages[(size_t) i] = std::make_unique<DegradationCore>(i);
                stages[(size_t) i]->prepare(spec, 0, 0.0f);
            }

            auto buffer = generatePinkNoise(numChannels, blockSize, 8811);

            for (int genCount : { 1, 8 })
            {
                const double start = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < numIterations; ++i)
                    for (int stage = 0; stage < genCount; ++stage)
                        stages[(size_t) stage]->process(buffer, 0.5f, 0.5f, 2, true, 0.4f, NoiseSource::tape);
                const double end = juce::Time::getMillisecondCounterHiRes();

                const double avgMsPerBlock = (end - start) / (double) numIterations;
                const double cpuPercent = 100.0 * avgMsPerBlock / realTimeBudgetMs;

                logMessage("GEN=" + juce::String(genCount) + " (CLUNK) average block time: "
                           + juce::String(avgMsPerBlock, 4) + " ms, CPU load: "
                           + juce::String(cpuPercent, 2) + "%");

                expect(avgMsPerBlock < realTimeBudgetMs,
                       "GEN=" + juce::String(genCount) + " CLUNK cascade should stay below the real-time budget");
            }
        }
    }
};

static CPUCheckTests cpuCheckTests;
