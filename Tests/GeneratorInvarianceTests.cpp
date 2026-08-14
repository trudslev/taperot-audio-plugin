#include "../Source/DSP/Hum.h"
#include "../Source/DSP/NoiseSource.h"
#include "../Source/DSP/FailureEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

/**
    The three generators driven DIRECTLY, at class level.

    The chain-level bisection ran the whole plugin with every other stage neutral, so a divergence
    attributed to HUM could equally have been the chain reacting to Hum's output — the Saturator
    still oversamples at DRIVE 0, ToneFilters still filters, OutputStage still applies gain. Driving
    the class alone removes that ambiguity, which is the same step that settled TapeModelEQ and
    CharacterStage.

    **Known case, named before the run:** the all-neutral chain arm measured exactly 0.000000000, so
    a generator that is itself invariant must come back exact here too. A fixture that reports
    divergence for all three would be describing itself.
*/
class GeneratorInvarianceTests final : public juce::UnitTest
{
public:
    GeneratorInvarianceTests() : juce::UnitTest ("Generator invariance", "DSP") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;
        constexpr int totalSamples = 32768;

        // One input stream, cut two ways. Identical samples in both runs by construction.
        std::vector<float> input ((size_t) totalSamples);
        {
            juce::Random r (777);
            for (auto& v : input)
                v = r.nextFloat() * 2.0f - 1.0f;
        }

        const auto runAtBlockSize = [&] (int blockSize,
                                         const std::function<void (juce::AudioBuffer<float>&)>& processBlock,
                                         const std::function<void (int)>& prepareAt)
        {
            prepareAt (blockSize);

            std::vector<float> out;
            out.reserve ((size_t) totalSamples);

            juce::AudioBuffer<float> buffer (2, blockSize);

            for (int pos = 0; pos + blockSize <= totalSamples; pos += blockSize)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                        buffer.setSample (ch, i, input[(size_t) (pos + i)]);

                processBlock (buffer);

                for (int i = 0; i < blockSize; ++i)
                    out.push_back (buffer.getSample (0, i));
            }

            return out;
        };

        const auto compare = [this] (const char* label,
                                     const std::vector<float>& a, const std::vector<float>& b)
        {
            double worst = 0.0;
            int firstAt = -1;

            for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            {
                const double d = std::abs ((double) a[i] - b[i]);

                if (d > worst) worst = d;
                if (firstAt < 0 && d > 0.0) firstAt = (int) i;
            }

            logMessage ("  " + juce::String (label).paddedRight (' ', 16)
                            + (worst == 0.0 ? juce::String ("BIT-IDENTICAL across 64 and 512")
                                            : "differs, worst " + juce::String (worst, 9)
                                                  + ", first at sample " + juce::String (firstAt)));
            return worst;
        };

        beginTest ("Hum — deterministic, so a divergence here cannot be a draw count");
        {
            // Hum holds a phase, an increment and a smoother (Hum.h:19-21) and no juce::Random at
            // all. Its process advances both per sample (Hum.cpp:24-33). If it is bit-identical
            // here, the chain-level 0.000049503 was the CHAIN reacting to Hum's signal and not
            // Hum's own — which closes the row as not-a-defect for a stated reason rather than a
            // likely one.
            // **A FRESH INSTANCE PER BLOCK SIZE, and the first version of this fixture had one.**
            // It declared a single Hum and drove it at 64 and then at 512, so the second run
            // inherited the first's phase and smoother state and reported 0.0157 of "block-size
            // dependence" that was entirely carry-over. That is the same defect core's render() was
            // fixed for — and it came back because this driver is hand-rolled and bypasses it.
            const auto runFresh = [&] (int blockSize)
            {
                Hum hum;
                return runAtBlockSize (blockSize,
                    [&hum] (juce::AudioBuffer<float>& b) { hum.process (b, true); },
                    [&hum] (int bs) { hum.prepare ({ fs, (juce::uint32) bs, 2 }); });
            };

            const auto at64 = runFresh (64);
            const auto at512 = runFresh (512);

            const auto worst = compare ("Hum", at64, at512);

            expectEquals (worst, 0.0,
                          "Hum diverges with block size despite being deterministic and per-sample "
                          "throughout — which would be a more interesting finding than either of "
                          "the other two generators");
        }

        beginTest ("NoiseSource — is its draw count sample-determined or state-determined?");
        {
            const auto runFresh = [&] (int blockSize)
            {
                NoiseSource noise;      // fresh per block size — see the note in the Hum arm
                return runAtBlockSize (blockSize,
                    [&noise] (juce::AudioBuffer<float>& b) { noise.process (b, 1.0f, 0); },
                    [&noise] (int bs) { noise.prepare ({ fs, (juce::uint32) bs, 2 }); });
            };

            const auto at64 = runFresh (64);
            const auto at512 = runFresh (512);

            const auto worst = compare ("NoiseSource", at64, at512);

            expectEquals (worst, 0.0,
                          "NoiseSource's output depends on block size, so its draw count is "
                          "state-determined rather than sample-determined");

            // **CHARACTER 0 WAS THE ONE VALUE THAT COULD NOT FIRE THE CROSSFADE.** NoiseSource
            // carries a stored-copy character switch (NoiseSource.cpp:142) — the same construction
            // catalogued in root CLAUDE.md, where a member holding a copy of a selection is
            // compared per block to detect a change. Driving it at 0, its constructed value, means
            // requested == stored and no crossfade ever starts. That is a probe chosen at the one
            // value that cannot distinguish the hypothesis, which this sweep has already done once
            // with a pre-delay at 0 and 0.5.
            for (int character : { 0, 1, 2 })
            {
                const auto runAt = [&] (int blockSize)
                {
                    NoiseSource fresh;
                    return runAtBlockSize (blockSize,
                        [&fresh, character] (juce::AudioBuffer<float>& b) { fresh.process (b, 1.0f, character); },
                        [&fresh] (int bs) { fresh.prepare ({ fs, (juce::uint32) bs, 2 }); });
                };

                const auto w = compare ((juce::String ("NoiseSource ch") + juce::String (character)).toRawUTF8(),
                                        runAt (64), runAt (512));

                expectEquals (w, 0.0,
                              "NoiseSource diverges with block size at character "
                                  + juce::String (character) + " — its character crossfade is "
                                  "started per prepare and stepped per block");
            }
        }

        beginTest ("FailureEngine — the EVENT COUNT, which is the quantity that matters");
        {
            // **Comparing audio would answer the wrong question here.** A dropout starting one
            // sample earlier produces an enormous sample difference while the failure BEHAVIOUR is
            // unchanged. What a user experiences is how many events happen per unit time, and this
            // class counts them itself — so the count is the measurement, and the audio is only
            // corroboration.
            //
            // Same input, same duration, two block sizes. The event count must match: the rate is
            // specified per second (ratePerSecAtFull) and nothing about a buffer boundary should
            // change how many fire in 0.68 s of audio.
            const auto countAt = [&] (int blockSize)
            {
                FailureEngine fe;
                fe.prepare ({ fs, (juce::uint32) blockSize, 2 });
                fe.resetEventCount();

                // **60 seconds, not 0.68.** The first run counted ONE event at each block size and
                // read as agreement — three identical figures that a single event cannot support.
                // A rate specified per second needs enough seconds to be a rate at all; at n=1,
                // "identical" and "coincidence" are the same observation.
                const int longRun = (int) (fs * 60.0);
                juce::AudioBuffer<float> buffer (2, blockSize);

                for (int pos = 0; pos + blockSize <= longRun; pos += blockSize)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < blockSize; ++i)
                            buffer.setSample (ch, i, input[(size_t) ((pos + i) % totalSamples)]);

                    fe.process (buffer, 1.0f, true, true, true, true);
                }

                return fe.getEventCount();
            };

            const auto c64 = countAt (64);
            const auto c512 = countAt (512);
            const auto c2048 = countAt (2048);

            logMessage (juce::String ("  events in 60 s at FAILURE 100% -> ")
                            + "block 64: " + juce::String (c64)
                            + ", block 512: " + juce::String (c512)
                            + ", block 2048: " + juce::String (c2048));

            expect (c64 == c512 && c512 == c2048,
                    "the number of failure events in the same audio depends on the host's buffer "
                    "size: 64 -> " + juce::String (c64) + ", 512 -> " + juce::String (c512)
                        + ", 2048 -> " + juce::String (c2048)
                        + ". The rate is specified per SECOND, so this is the plugin's most "
                          "characterful control behaving differently according to a setting that "
                          "has nothing to do with it.");
        }
    }
};

static GeneratorInvarianceTests generatorInvarianceTests;
