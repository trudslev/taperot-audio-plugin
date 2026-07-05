#include "TestUtils.h"
#include "../Source/DSP/ToneFilters.h"

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
}

class ToneFiltersTests final : public juce::UnitTest
{
public:
    ToneFiltersTests() : juce::UnitTest("ToneFilters", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 1024;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("Bounded, finite output with LP/HP fully engaged");
        {
            ToneFilters filters;
            filters.prepare(spec);

            auto buffer = generatePinkNoise(numChannels, blockSize, 55);
            for (int block = 0; block < 10; ++block)
                filters.process(buffer, 1000.0f, 2000.0f);

            bool allFinite = true;
            float maxAbs = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getReadPointer(ch)[i];
                    if (!std::isfinite(v)) allFinite = false;
                    maxAbs = juce::jmax(maxAbs, std::abs(v));
                }

            expect(allFinite, "Output must remain finite");
            expect(maxAbs < 5.0f, "Output should stay in a sane bounded range");
        }

        beginTest("LP at low cutoff attenuates a high-frequency sine");
        {
            ToneFilters filtersOff;
            filtersOff.prepare(spec);
            ToneFilters filtersOn;
            filtersOn.prepare(spec);

            auto bufferOff = generateSine(numChannels, blockSize, sampleRate, 12000.0f);
            auto bufferOn = bufferOff;

            for (int block = 0; block < 10; ++block)
            {
                filtersOff.process(bufferOff, 20000.0f, 20.0f); // LP off (wide open)
                filtersOn.process(bufferOn, 1500.0f, 20.0f);    // LP engaged well below the sine
            }

            const float rmsOff = rmsOf(bufferOff);
            const float rmsOn = rmsOf(bufferOn);

            logMessage("LP off RMS: " + juce::String(rmsOff, 4) + ", LP@1.5kHz RMS: " + juce::String(rmsOn, 4));
            expect(rmsOn < rmsOff * 0.3f, "Engaging LP well below the test tone should substantially attenuate it");
        }

        beginTest("HP at high cutoff attenuates a low-frequency sine");
        {
            ToneFilters filtersOff;
            filtersOff.prepare(spec);
            ToneFilters filtersOn;
            filtersOn.prepare(spec);

            auto bufferOff = generateSine(numChannels, blockSize, sampleRate, 80.0f);
            auto bufferOn = bufferOff;

            for (int block = 0; block < 10; ++block)
            {
                filtersOff.process(bufferOff, 20000.0f, 20.0f);   // HP off (fully open)
                filtersOn.process(bufferOn, 20000.0f, 1000.0f);   // HP engaged well above the sine
            }

            const float rmsOff = rmsOf(bufferOff);
            const float rmsOn = rmsOf(bufferOn);

            logMessage("HP off RMS: " + juce::String(rmsOff, 4) + ", HP@1kHz RMS: " + juce::String(rmsOn, 4));
            expect(rmsOn < rmsOff * 0.3f, "Engaging HP well above the test tone should substantially attenuate it");
        }
    }
};

static ToneFiltersTests toneFiltersTests;
