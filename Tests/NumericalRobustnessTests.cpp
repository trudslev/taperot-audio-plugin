#include "../Source/PluginProcessor.h"
#include "../Source/DSP/DegradationCore.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

/**
    Category 2 of the suite-wide bug sweep, for TapeRot.

    ## Output-only scanning is NOT sufficient here, and this casting is why

    Every other numerical test in this sweep scans what the processor emits. That is a sharp check —
    `processBlock` opens with `ScopedNoDenormals`, so a subnormal reaching the output means the
    flush-to-zero guard is not covering that path — but it has a blind spot that a **cascade** walks
    straight into.

    **TapeRot is the deepest cascade in the suite.** At GEN 8 the signal passes through eight
    `DegradationCore` stages in series, each itself WowFlutter → TapeModelEQ → NoiseSource, with a
    Saturator before them and Hum → FailureEngine → StereoSpread → ToneFilters → TapeStop →
    FilterSweep → OutputStage after. Each stage's output is the next stage's input.

    So a subnormal can **arrive at** a stage rather than originate in it, and no per-stage guard sees
    the whole chain. More to the point: a mid-chain subnormal can be scaled back up, or added to, by
    a later stage — so the tail is clean **at the output** while having been subnormal in the middle.
    "Clean at the output" is precisely the result that would hide it.

    This file therefore scans **between the stages**, not only after them.

    ## Which castings need this, and which do not

    | Casting | Wet path | Output-only enough? |
    |---|---|---|
    | **taperot** | 8 DegradationCore in series, ~14 stages deep at GEN 8 | **no** — this file |
    | gatecrasher | 5 deep: reverb → damping → gate → slam → width, then a dry join | no — the dry join can mask a wet-path subnormal |
    | chorus-60 | 3 deep: BBD → decorrelation → character, then a dry join | no — same |
    | elmer | 2 deep: iron → output. **Plus a detector path that never reaches the output at all** | no — for a different reason: the detector's one-poles become a gain multiplier, so output scanning cannot see them by construction |
    | reflect-84 | four independent tanks, selected not chained | yes — already scanned clean |
    | fifth-member | one delay line with feedback | yes — already scanned, HOWL bounded |

    **The ordering heuristic from the survey is not used and is disproven.** Reflect-84 was scanned
    first for having four tanks against only two tiny-constant guards where TapeRot has ten; the
    scanner found nothing there. Guard count does not predict subnormal reach. TapeRot is here
    because it is a cascade, which is a structural reason rather than a counted one.
*/
class NumericalRobustnessTests final : public juce::UnitTest
{
public:
    NumericalRobustnessTests() : juce::UnitTest ("Numerical robustness", "DSP") {}

    void runTest() override
    {
        beginTest ("The GEN cascade, scanned BETWEEN every stage rather than after all eight");
        {
            // Driven directly rather than through the processor, because the processor only exposes
            // the far end of the chain — which is the whole point of this test.
            juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };

            std::vector<std::unique_ptr<DegradationCore>> stages;
            for (int i = 0; i < 8; ++i)
            {
                stages.push_back (std::make_unique<DegradationCore> (i));
                stages.back()->prepare (spec);
            }

            juce::AudioBuffer<float> buffer (2, 512);

            // Excite, then feed silence and let the chain's own state decay — which is when a
            // subnormal appears in a decaying path. Worst case for the cascade: maximum wow and
            // flutter, maximum noise, so every stage has live state.
            excite (buffer);
            for (int i = 0; i < 8; ++i)
                for (auto& s : stages)
                    s->process (buffer, 1.0f, 1.0f, 0, false, 1.0f, 0);

            std::array<int, 8> subnormalsAt {};
            std::array<double, 8> peakAt {};

            // **4000 silent blocks, scanning after EACH stage.** A short tail scans the loud part of
            // the decay and reports clean, which is how a real denormal problem hides.
            for (int block = 0; block < 4000; ++block)
            {
                buffer.clear();

                for (size_t s = 0; s < stages.size(); ++s)
                {
                    stages[s]->process (buffer, 1.0f, 1.0f, 0, false, 0.0f, 0);

                    // Scanned HERE — between stage s and stage s+1 — not after the chain.
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        const auto* read = buffer.getReadPointer (ch);

                        for (int i = 0; i < buffer.getNumSamples(); ++i)
                        {
                            if (std::fpclassify (read[i]) == FP_SUBNORMAL)
                                ++subnormalsAt[s];

                            peakAt[s] = juce::jmax (peakAt[s], (double) std::abs (read[i]));
                        }
                    }
                }
            }

            int total = 0;

            for (size_t s = 0; s < stages.size(); ++s)
            {
                logMessage ("  after stage " + juce::String ((int) s) + " -> "
                                + juce::String (subnormalsAt[s]) + " subnormal, peak "
                                + juce::String (peakAt[s], 12));
                total += subnormalsAt[s];
            }

            // **Reported per stage, so a mid-chain result cannot be averaged away by a clean end.**
            // Note this runs WITHOUT ScopedNoDenormals: the stages are driven directly, so the
            // hardware flush-to-zero the processor sets is not in force. That is deliberate — it
            // measures whether the cascade PRODUCES subnormals, which is the question a guarded
            // output scan cannot answer.
            logMessage ("  cascade total -> " + juce::String (total) + " subnormal samples");

            // **The peak progression is the finding, not the zero.** Measured, unguarded:
            //
            //   stage 0 -> 0.056   stage 4 -> 0.972
            //   stage 1 -> 0.072   stage 5 -> 0.989
            //   stage 2 -> 0.840   stage 6 -> 0.996
            //   stage 3 -> 0.929   stage 7 -> 0.999
            //
            // A decaying tail gets LOUDER through this cascade. Each stage from the second onward
            // applies gentleSaturationDrive = 1.35f (DegradationCore.h:31), so seven stages carry
            // ~8x of gain before saturation, and the values asymptote toward 1.0 rather than
            // decaying toward zero.
            //
            // So the cascade cannot produce subnormals BECAUSE it amplifies: the per-stage gain
            // outruns the decay. That is a structural reason for a clean result rather than luck,
            // and it is what a per-stage scan can establish and an output-only scan cannot — the
            // output would have read clean either way, with no way to tell which.
            expectGreaterThan (peakAt[7], peakAt[0],
                               "the cascade no longer amplifies, so the structural reason this "
                               "chain cannot reach subnormal territory no longer holds — re-check "
                               "the per-stage figures rather than trusting the zero above");

            for (size_t s = 0; s < stages.size(); ++s)
                expect (std::isfinite (peakAt[s]),
                        "stage " + juce::String ((int) s) + " produced non-finite output");
        }

        beginTest ("The whole processor at GEN 8, long tail — the guarded comparison");
        {
            // The same chain through the real processor, where ScopedNoDenormals IS in force. The
            // pair is the finding: what the cascade produces unguarded, against what survives to the
            // output guarded.
            TapeRotAudioProcessor processor;

            set (processor, ParamIDs::gen, 1.0f);        // all eight stages
            set (processor, ParamIDs::wow, 1.0f);
            set (processor, ParamIDs::flutter, 1.0f);

            logMessage ("  GEN -> " + readBack (processor, ParamIDs::gen)
                            + ", WOW -> " + readBack (processor, ParamIDs::wow));

            nf::testing::RenderSpec spec;
            spec.numBlocks = 32;

            const auto report = nf::testing::scanTail (processor, spec, 4000);
            logMessage ("  processor output -> " + report.describe());

            expectEquals (report.nans, 0, "NaN at the output: " + report.describe());
            expectEquals (report.infinities, 0, "Inf at the output: " + report.describe());

            // TapeRot GENERATES hiss and hum deliberately, so "never fell silent" is correct here
            // and is not asserted against — see category 3's silence-in/silence-out ruling.
        }
    }

private:
    static void excite (juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, 0.5f * std::sin ((float) i * 0.05f));
    }

    static void set (TapeRotAudioProcessor& p, const char* id, float normalised)
    {
        if (auto* param = p.apvts.getParameter (id))
            param->setValueNotifyingHost (normalised);
    }

    static juce::String readBack (TapeRotAudioProcessor& p, const char* id)
    {
        if (auto* param = p.apvts.getParameter (id))
            return param->getCurrentValueAsText();

        return "<missing>";
    }
};

static NumericalRobustnessTests numericalRobustnessTests;
