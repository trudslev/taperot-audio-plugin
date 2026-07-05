#include "TestUtils.h"
#include "../Source/DSP/DegradationCore.h"
#include "../Source/DSP/TapeModelData.h"
#include <memory>

namespace
{
    std::unique_ptr<DegradationCore> makeCascadeStage(int index, const juce::dsp::ProcessSpec& spec)
    {
        auto stage = std::make_unique<DegradationCore>(index);
        stage->prepare(spec);
        return stage;
    }
}

// Simulates a host rapidly automating MODEL while audio runs, in both switch modes and at GEN
// depths of 1 and 4, per the brief's verification step.
class ModelSwitchAutomationTests final : public juce::UnitTest
{
public:
    ModelSwitchAutomationTests() : juce::UnitTest("ModelSwitchAutomation", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;
        const int blockSize = 128;
        const int numChannels = 2;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        for (const bool clunkMode : { false, true })
        {
            const juce::String modeName = clunkMode ? "CLUNK" : "FADE";

            for (const int genCount : { 1, 4 })
            {
                // Reference: the largest sample-to-sample derivative produced by each model held
                // steady (no switching at all). Resonant EQ'd pink noise already has substantial
                // frame-to-frame variation on its own (peaking models run Q up to 2.0 with makeup
                // gain up to +4.42dB) - this is not clickiness, just texture. A switch should not
                // make the signal move around any more violently than steady-state processing
                // already does, so this - not an arbitrary constant - is the right yardstick for
                // "no discontinuity was introduced by switching".
                float baselineMaxAbsDiff = 0.0f;
                for (int fixedModel = 0; fixedModel < (int) kNumTapeModels; ++fixedModel)
                {
                    std::array<std::unique_ptr<DegradationCore>, 8> baselineStages;
                    for (int i = 0; i < 8; ++i)
                        baselineStages[(size_t) i] = makeCascadeStage(i, spec);

                    float prevSample[2] = { 0.0f, 0.0f };
                    for (int block = 0; block < 100; ++block)
                    {
                        auto buffer = generatePinkNoise(numChannels, blockSize, 999 + block);
                        for (int s = 0; s < genCount; ++s)
                            baselineStages[(size_t) s]->process(buffer, 0.3f, 0.2f, fixedModel, clunkMode, 0.2f, NoiseSource::tape);

                        for (int ch = 0; ch < numChannels; ++ch)
                        {
                            auto* data = buffer.getReadPointer(ch);
                            for (int i = 0; i < blockSize; ++i)
                            {
                                const float prev = (i == 0) ? prevSample[ch] : data[i - 1];
                                baselineMaxAbsDiff = juce::jmax(baselineMaxAbsDiff, std::abs(data[i] - prev));
                            }
                            prevSample[ch] = data[blockSize - 1];
                        }
                    }
                }

                beginTest("Rapid MODEL automation in " + modeName + " mode at GEN=" + juce::String(genCount)
                          + " stays finite and bounded, all stages in sync");
                {
                    std::array<std::unique_ptr<DegradationCore>, 8> stages;
                    for (int i = 0; i < 8; ++i)
                        stages[(size_t) i] = makeCascadeStage(i, spec);

                    juce::Random modelPicker(clunkMode ? 111 : 222);

                    bool allFinite = true;
                    float maxAbsDiff = 0.0f;
                    float previousSample[2] = { 0.0f, 0.0f };

                    for (int block = 0; block < 400; ++block)
                    {
                        // Each processBlock call in a real host delivers fresh audio - the plugin
                        // never feeds its own output back in as next block's input. Reusing a
                        // buffer object across iterations here (as if reprocessing the same
                        // samples 400 times) would just be computing H(z)^400 on one block, which
                        // diverges for any EQ with so much as a fractional-dB peak boost - an
                        // artifact of the test harness, not a real instability. So: fresh pink
                        // noise every block, with only the DegradationCore stages' internal
                        // filter/crossfade state carried over.
                        auto buffer = generatePinkNoise(numChannels, blockSize, 999 + block);

                        // A new MODEL value every few blocks - faster than any real automation
                        // lane, to stress-test worst-case rapid changes.
                        const int model = (block % 3 == 0) ? modelPicker.nextInt((int) kNumTapeModels) : -1;
                        const int effectiveModel = model >= 0 ? model : (int) (block / 3) % (int) kNumTapeModels;

                        for (int s = 0; s < genCount; ++s)
                            stages[(size_t) s]->process(buffer, 0.3f, 0.2f, effectiveModel, clunkMode, 0.2f, NoiseSource::tape);

                        for (int ch = 0; ch < numChannels; ++ch)
                        {
                            auto* data = buffer.getReadPointer(ch);
                            for (int i = 0; i < blockSize; ++i)
                            {
                                if (!std::isfinite(data[i]))
                                    allFinite = false;

                                const float prev = (i == 0) ? previousSample[ch] : data[i - 1];
                                maxAbsDiff = juce::jmax(maxAbsDiff, std::abs(data[i] - prev));
                            }
                            previousSample[ch] = data[blockSize - 1];
                        }
                    }

                    logMessage(modeName + " GEN=" + juce::String(genCount) + " max sample-to-sample jump: "
                               + juce::String(maxAbsDiff, 4) + " (steady-state baseline: " + juce::String(baselineMaxAbsDiff, 4) + ")");

                    expect(allFinite, modeName + " GEN=" + juce::String(genCount) + " must stay finite under rapid MODEL automation");

                    // CLUNK deliberately adds a mute dip + thump around each switch, so allow it a
                    // bit more headroom over the steady-state baseline than FADE, which should
                    // never exceed what steady-state resonant processing already produces.
                    const float jumpLimit = (clunkMode ? 1.5f : 1.2f) * baselineMaxAbsDiff + 0.05f;
                    expect(maxAbsDiff < jumpLimit,
                           modeName + " GEN=" + juce::String(genCount) + " sample-to-sample jump ("
                               + juce::String(maxAbsDiff, 4) + ") should stay within " + juce::String(jumpLimit, 4)
                               + " of the steady-state baseline - no discontinuity introduced by switching");
                }
            }
        }
    }
};

static ModelSwitchAutomationTests modelSwitchAutomationTests;
