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
            // **This pins a REASON, not the behaviour.** It is not an endorsement of the
            // amplification and must not be read as "do not change this" — the level question it
            // raises is open (see the silence-in test below, and category 3's ruling).
            //
            // What it pins is the CONDITIONALITY of the zero above: that result holds *because* the
            // cascade amplifies. If someone restages the gain — which the level question might well
            // argue for — this assertion fires, and the correct response is to **re-run the scan**,
            // not to re-tune the assertion. A decaying cascade could reach subnormal territory where
            // an amplifying one cannot.
            expectGreaterThan (peakAt[7], peakAt[0],
                               "the cascade no longer amplifies. The zero-subnormal result above was "
                               "conditional on it doing so, so RE-RUN the per-stage scan — do not "
                               "adjust this assertion to match.");

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

        beginTest ("Silence in at GEN 8 — what the cascade drives the noise bed to");
        {
            // **A level question, raised by the per-stage scan rather than by the ear.** A decaying
            // tail rises 0.056 -> 0.999 through the eight stages, which means that with the input
            // gone the cascade drives whatever remains toward full scale. TapeRot generates hiss and
            // hum deliberately, so what remains is not nothing: it is the noise bed, carried through
            // ~8x of pre-saturation gain.
            //
            // That may be exactly right — eight generations of dubbing should sound like eight
            // generations of dubbing, and a swelling noise floor is the point of the plugin. But it
            // is a number worth being deliberate about rather than discovering, so it is MEASURED
            // here rather than inferred from the tail, and belongs to category 3's
            // silence-in/silence-out ruling: this is the "confirm it generates only where intended"
            // half.
            for (int gen : { 0, 4, 8 })
            {
                TapeRotAudioProcessor processor;

                set (processor, ParamIDs::gen, gen == 0 ? 0.0f : (float) gen / 8.0f);
                set (processor, ParamIDs::noise, 1.0f);

                nf::testing::RenderSpec spec;
                spec.numBlocks = 64;
                spec.fillInput = [] (juce::AudioBuffer<float>& b, int) { b.clear(); };  // silence in

                const auto out = nf::testing::render (processor, spec);

                double peak = 0.0, rms = 0.0;
                size_t n = 0;

                for (const auto& channel : out)
                    for (auto v : channel)
                    {
                        peak = juce::jmax (peak, (double) std::abs (v));
                        rms += (double) v * (double) v;
                        ++n;
                    }

                rms = n > 0 ? std::sqrt (rms / (double) n) : 0.0;

                logMessage ("  GEN " + readBack (processor, ParamIDs::gen)
                                + ", NOISE 100, silence in -> peak " + juce::String (peak, 6)
                                + " (" + juce::String (juce::Decibels::gainToDecibels (peak), 1)
                                + " dB), rms " + juce::String (rms, 6)
                                + " (" + juce::String (juce::Decibels::gainToDecibels (rms), 1) + " dB)");

                // Reported, not judged. Whether the generated floor is at the right LEVEL is a
                // design ruling; what this asserts is only that it is bounded and finite, which is
                // the sweep's business.
                expect (std::isfinite (peak), "non-finite output from silence in at GEN " + juce::String (gen));
                expectLessThan (peak, 4.0,
                                "the generated noise bed at GEN " + juce::String (gen)
                                    + " exceeds 4x full scale, which is a level defect rather than a "
                                    "design choice");
            }
        }

        beginTest ("The denormal guard is ACTIVE — the one line the whole suite rests on");
        {
            // **RULING TAKEN: assert the processor-level guard rather than putting a floor in one
            // filter.** ScopedNoDenormals is one line in one file per casting, and category 2's
            // survey established that no DSP stage in the suite carries its own guard. So every
            // decaying path in this plugin — including the control paths no output scan can reach —
            // is covered by a single statement that, until this test, nothing asserted.
            //
            // Mechanism: feed SUBNORMAL input and see whether it survives. Flush-to-zero also treats
            // subnormal inputs as zero, so a subnormal cannot survive a guarded processBlock while
            // an unguarded one passes it through. This therefore fails if the guard is REMOVED,
            // NARROWED to part of the function, or a path is SCOPED PAST it — the three ways one
            // line stops covering what it appears to.
            //
            // Core's own tests prove the checker can tell guarded from unguarded (1024 in -> 1024
            // out against 1024 in -> 0 out). Without that proof this assertion would be worthless,
            // because "no subnormals survived" is also what a checker that measures nothing reports.
            TapeRotAudioProcessor processor;
            const auto guard = nf::testing::probeDenormalGuard (processor);

            logMessage ("  " + guard.describe());

            expect (guard.guardActive,
                    "ScopedNoDenormals is not covering processBlock. Every decaying path in this "
                    "plugin depends on it, and nothing else guards them: " + guard.describe());
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
