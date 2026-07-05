#include "TestUtils.h"
#include "../Source/DSP/NoiseSource.h"

namespace
{
    // Mirrors the pre-refactor NoiseGenerator's hiss-only algorithm exactly (same seed formula,
    // highpass frequency, hiss level, and gain smoothing time), so TAPE can be null-tested against it.
    struct ReferenceTapeHiss
    {
        struct ChannelState
        {
            juce::Random random;
            float hpfState = 0.0f;
            float hpfPrevInput = 0.0f;
        };

        std::vector<ChannelState> channels;
        juce::SmoothedValue<float> gainSmoothed{0.0f};
        float hpfCoeff = 0.0f;

        void prepare(double sampleRate, int numChannels)
        {
            channels.resize((size_t) numChannels);
            for (int ch = 0; ch < numChannels; ++ch)
                channels[(size_t) ch].random = juce::Random((juce::int64) (0xA5A5A5A5A5A5A5A5ULL * (juce::uint64) (ch + 1)));

            hpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 2000.0f / (float) sampleRate);
            gainSmoothed.reset(sampleRate, 0.02);
        }

        void process(juce::AudioBuffer<float>& buffer, float noiseAmount01)
        {
            gainSmoothed.setTargetValue(noiseAmount01 * 0.05f);
            const int numSamples = buffer.getNumSamples();
            const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());

            for (int i = 0; i < numSamples; ++i)
            {
                const float gain = gainSmoothed.getNextValue();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto& c = channels[(size_t) ch];
                    const float white = c.random.nextFloat() * 2.0f - 1.0f;
                    c.hpfState = hpfCoeff * (c.hpfState + white - c.hpfPrevInput);
                    c.hpfPrevInput = white;
                    buffer.getWritePointer(ch)[i] += c.hpfState * gain;
                }
            }
        }
    };
}

class NoiseSourceTests final : public juce::UnitTest
{
public:
    NoiseSourceTests() : juce::UnitTest("NoiseSource", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 512;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        beginTest("TAPE character nulls against the pre-refactor hiss algorithm");
        {
            NoiseSource source;
            source.prepare(spec);

            ReferenceTapeHiss reference;
            reference.prepare(sampleRate, numChannels);

            juce::AudioBuffer<float> actual(numChannels, blockSize);
            juce::AudioBuffer<float> expected(numChannels, blockSize);

            // Run several blocks so the 20ms gain smoothing on both sides settles to the same
            // steady state before comparing.
            for (int block = 0; block < 10; ++block)
            {
                actual.clear();
                expected.clear();
                source.process(actual, 0.6f, NoiseSource::tape);
                reference.process(expected, 0.6f);
            }

            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError(actual.getReadPointer(ch)[i], expected.getReadPointer(ch)[i], 1.0e-9f);
        }

        beginTest("Bounded, finite output for all three characters at full noise amount");
        {
            for (int character = 0; character < 3; ++character)
            {
                NoiseSource source;
                source.prepare(spec);

                juce::AudioBuffer<float> buffer(numChannels, blockSize);
                bool allFinite = true;
                float maxAbs = 0.0f;

                for (int block = 0; block < 20; ++block)
                {
                    buffer.clear();
                    source.process(buffer, 1.0f, character);

                    for (int ch = 0; ch < numChannels; ++ch)
                        for (int i = 0; i < blockSize; ++i)
                        {
                            const float v = buffer.getReadPointer(ch)[i];
                            if (!std::isfinite(v)) allFinite = false;
                            maxAbs = juce::jmax(maxAbs, std::abs(v));
                        }
                }

                expect(allFinite, "Output must remain finite");
                expect(maxAbs < 5.0f, "Output should stay in a sane bounded range");
            }
        }

        beginTest("Loudness roughly matches across characters at 25/50/75% knob positions");
        {
            for (float amount : { 0.25f, 0.5f, 0.75f })
            {
                float rmsByCharacter[3];

                for (int character = 0; character < 3; ++character)
                {
                    NoiseSource source;
                    source.prepare(spec);

                    juce::AudioBuffer<float> buffer(numChannels, blockSize);
                    double sumSquares = 0.0;
                    juce::int64 count = 0;

                    // Run a few seconds so DUST's sparse crackle density averages out meaningfully.
                    for (int block = 0; block < 400; ++block)
                    {
                        buffer.clear();
                        source.process(buffer, amount, character);

                        if (block >= 10) // skip the crossfade/settle-in period
                        {
                            for (int ch = 0; ch < numChannels; ++ch)
                            {
                                const auto* data = buffer.getReadPointer(ch);
                                for (int i = 0; i < blockSize; ++i)
                                {
                                    sumSquares += (double) data[i] * (double) data[i];
                                    ++count;
                                }
                            }
                        }
                    }

                    rmsByCharacter[character] = (float) std::sqrt(sumSquares / (double) count);
                }

                const float tapeDb = juce::Decibels::gainToDecibels(rmsByCharacter[0]);
                for (int character = 1; character < 3; ++character)
                {
                    const float db = juce::Decibels::gainToDecibels(rmsByCharacter[character]);
                    logMessage("amount=" + juce::String(amount) + " character=" + juce::String(character)
                               + " delta=" + juce::String(db - tapeDb, 2) + " dB");
                    expect(std::abs(db - tapeDb) < 3.0f,
                           "Character " + juce::String(character)
                               + " should be within 3dB of TAPE at amount " + juce::String(amount));
                }
            }
        }
    }
};

static NoiseSourceTests noiseSourceTests;
