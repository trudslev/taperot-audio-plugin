#include "TestUtils.h"
#include "../Source/DSP/AuxEnvelope.h"
#include "../Source/DSP/TapeStop.h"
#include "../Source/DSP/FilterSweep.h"

class AuxEffectsTests final : public juce::UnitTest
{
public:
    AuxEffectsTests() : juce::UnitTest("AuxEffects", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 256;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("AuxEnvelope reaches target and retargeting the ramp time mid-fade doesn't jump");
        {
            AuxEnvelope env;
            env.setSampleRate(sampleRate);
            env.setRampSeconds(0.1f);
            env.setEngaged(true);

            float previous = 0.0f;
            float maxStepSeen = 0.0f;
            for (int i = 0; i < (int) (0.2 * sampleRate); ++i)
            {
                if (i == (int) (0.05 * sampleRate))
                    env.setRampSeconds(2.0f); // retarget mid-fade to a much slower ramp

                const float v = env.getNextValue();
                maxStepSeen = juce::jmax(maxStepSeen, std::abs(v - previous));
                previous = v;
            }

            expect(maxStepSeen < 0.01f, "No single-sample value should jump when the ramp time changes mid-fade");
        }

        beginTest("TapeStop is a transparent passthrough when never engaged");
        {
            TapeStop stop;
            stop.prepare(spec);

            auto input = generatePinkNoise(numChannels, blockSize, 909);
            auto buffer = input;

            for (int block = 0; block < 5; ++block)
                stop.process(buffer, false, 0.3f);

            bool exact = true;
            for (int ch = 0; ch < numChannels && exact; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    if (std::abs(buffer.getReadPointer(ch)[i] - input.getReadPointer(ch)[i]) > 1.0e-6f)
                        exact = false;

            expect(exact, "Output should match input when STOP has never been engaged");
        }

        beginTest("TapeStop stays bounded and finite while engaged and after release");
        {
            TapeStop stop;
            stop.prepare(spec);

            auto buffer = generatePinkNoise(numChannels, blockSize, 707);
            bool allFinite = true;
            float maxAbs = 0.0f;

            for (int block = 0; block < 30; ++block)
            {
                const bool engaged = block < 15;
                stop.process(buffer, engaged, 0.2f);
                for (int ch = 0; ch < numChannels; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                    {
                        const float v = buffer.getReadPointer(ch)[i];
                        if (!std::isfinite(v)) allFinite = false;
                        maxAbs = juce::jmax(maxAbs, std::abs(v));
                    }
            }

            expect(allFinite, "Output must remain finite through engage and release");
            expect(maxAbs < 5.0f, "Output should stay in a sane bounded range");
        }

        beginTest("FilterSweep attenuates high frequencies once fully engaged");
        {
            FilterSweep sweepOff;
            sweepOff.prepare(spec);
            FilterSweep sweepOn;
            sweepOn.prepare(spec);

            juce::AudioBuffer<float> bufferOff(numChannels, blockSize);
            juce::AudioBuffer<float> bufferOn(numChannels, blockSize);

            auto fillSine = [&](juce::AudioBuffer<float>& b)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = b.getWritePointer(ch);
                    for (int i = 0; i < blockSize; ++i)
                        data[i] = std::sin(juce::MathConstants<float>::twoPi * 8000.0f * (float) i / (float) sampleRate);
                }
            };

            double sumOff = 0.0, sumOn = 0.0;
            for (int block = 0; block < 40; ++block)
            {
                fillSine(bufferOff);
                fillSine(bufferOn);
                sweepOff.process(bufferOff, false, 0.05f);
                sweepOn.process(bufferOn, true, 0.05f);

                if (block >= 20) // after the ramp has settled
                    for (int ch = 0; ch < numChannels; ++ch)
                        for (int i = 0; i < blockSize; ++i)
                        {
                            sumOff += (double) bufferOff.getReadPointer(ch)[i] * bufferOff.getReadPointer(ch)[i];
                            sumOn += (double) bufferOn.getReadPointer(ch)[i] * bufferOn.getReadPointer(ch)[i];
                        }
            }

            logMessage("FilterSweep off energy: " + juce::String(sumOff, 2) + ", engaged energy: " + juce::String(sumOn, 2));
            expect(sumOn < sumOff * 0.1, "An 8kHz tone should be substantially attenuated once FILTER is fully engaged");
        }
    }
};

static AuxEffectsTests auxEffectsTests;
