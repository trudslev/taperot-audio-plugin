#include "../Source/PluginProcessor.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Category 3 — invariance. Does the same audio come out when only the CONTAINER changes?

    ## Why this file leads with premise checks rather than results

    An invariance failure looks like a pass more readily than anything else in this sweep, and it
    also looks like a failure more readily. Reflect-84 produced both in one run: four block-size
    rows all reporting DIFFERS, and every one of them measuring a first-run-only state rather than
    block dependence — because `blockSizeInvariance` compares each size against the FIRST size, so
    its first row is 64 against 64, and that self-comparison differed too.

    So nothing here is believed until the processor is shown to be reproducible against itself, and
    the comparison is shown able to fail. Both are asserted below rather than assumed.
*/
/*  ## The block-size row, traced by SHAPE rather than by bisect

    Reflect-84's LFO and this were 0.025 -> 0.063 -> 0.145 and 0.000200 -> 0.001022 -> 0.001926 —
    same signature, monotonic with buffer, three orders of magnitude apart. So the first move was to
    grep this casting's audio path for the construction rather than bisect for the symptom.

    The category 2 oscillator survey put WowFlutter at per-sample phase and per-sample draws, and it
    still does — but that survey answered "how is the phase advanced", not "is anything else stepped
    per block", and those are different questions.

    **Two sites carry the shape**, both in PluginProcessor.cpp:

      509-511  genSmoothed.setTargetValue (...); genSmoothed.skip (numSamples);
               const float genValue = genSmoothed.getCurrentValue();
      590-592  transportGateSmoothed.setTargetValue (...); transportGateSmoothed.skip (numSamples);
               buffer.applyGain (transportGateSmoothed.getCurrentValue());

    Both advance a smoother across the whole block and then apply its END-OF-BLOCK value to every
    sample in that block. That is the same family as Reflect-84's LFO — a per-sample quantity
    stepped per block — and the second one is a gain ramp drawn as a staircase, which is a zipper on
    transport start and stop that coarsens as the buffer grows.

    **Neither is proved to be the measured cause, and that is stated rather than left implied.**
    Both are LATENT: they only express while the target is moving, and `prepareToPlay` sets both
    smoothers with `setCurrentAndTargetValue` (:421, :431), so a steady render with GEN unautomated
    and the transport constant may never exercise either. The divergence measured below starts at
    sample 0, which a smoother already sitting on its target cannot produce.

    **FILED AS TWO DEFECTS IN THEIR OWN RIGHT, not as candidates for these rows.** A gain ramp
    advanced per block and applied flat across every sample in it is a staircase on transport start
    and stop that coarsens as the buffer grows. That is audible, it does not need the bisect to
    justify itself, and filing it behind "whether it is the cause here is open" would risk it being
    closed along with a finding it may be no part of. `genSmoothed` is the same construction and the
    same argument, with the single difference that GEN moves less often than the transport does.

    **When they are driven, they are driven SEPARATELY** — same reason the three LFO defects were
    filed apart. Two sites share one signature: if both express, there are two members and a ruling
    covers both; if only one does, the other is latent-but-real and stays on the list rather than
    being closed by association.

    Whether either is what these rows measure remains open, and the next step is the
    modulation-depth equivalent rather than a fresh bisect.
*/
class InvarianceTests final : public juce::UnitTest
{
public:
    InvarianceTests() : juce::UnitTest ("Invariance", "DSP") {}

    void runTest() override
    {
        beginTest ("PREMISE CHECK — reproducible against itself, cold and warmed");
        {
            // Three renders, no parameter writes. A vs B and C vs D separate the two shapes:
            //
            //   A != B, C == D   ->  FIRST-RUN-ONLY state: something is in its constructed
            //                        condition for the first render and its steady one after.
            //   A != B, C != D   ->  ONGOING carry across prepareToPlay.
            //   both exact       ->  reproducible; every result below means what it claims.
            //
            // Reflect-84 came back first-run-only, and the cause was a smoother that never got a
            // setCurrentAndTargetValue — its pre-delay glided up from zero on the first run only.
            TapeRotAudioProcessor processor;

            nf::testing::RenderSpec spec;
            spec.blockSize = 512;
            spec.numBlocks = 16;

            const auto ab = nf::testing::compareRenders (nf::testing::render (processor, spec),
                                                         nf::testing::render (processor, spec));
            const auto cd = nf::testing::compareRenders (nf::testing::render (processor, spec),
                                                         nf::testing::render (processor, spec));

            logMessage ("  cold   A vs B -> " + ab.describe());
            logMessage ("  warmed C vs D -> " + cd.describe());
            logMessage (juce::String ("  => ") + (ab.sampleExact ? "reproducible from construction"
                                                : cd.sampleExact ? "FIRST-RUN-ONLY state — see the note below"
                                                                 : "ONGOING carry across prepareToPlay"));

            // The warmed comparison is what every driver below depends on. A cold difference is a
            // finding in its own right and is reported rather than asserted, because the drivers
            // warm before measuring; a warmed difference means no invariance result is readable.
            expect (cd.sampleExact,
                    "this processor is not reproducible even warmed, so NO invariance result below "
                    "means anything: " + cd.describe());

            if (! ab.sampleExact)
                logMessage ("  NOTE: a first-run-only difference is itself a finding — an instance's "
                            "first playback differs from every later one. Reported, not asserted.");
        }

        beginTest ("Block size — sample-exact at 64 / 128 / 511 / 2048");
        {
            TapeRotAudioProcessor processor;
            warm (processor);

            nf::testing::RenderSpec spec;
            spec.blockSize = 512;
            spec.numBlocks = 64;

            const auto results = nf::testing::blockSizeInvariance (processor, spec,
                                                                   { 64, 128, 511, 2048 });

            for (const auto& r : results)
                logMessage ("  " + r.describe());

            // 511 is prime and shares no factor with the others, so it catches any assumption that
            // a block divides evenly into an internal chunk — the failure a 64/128/2048 sweep walks
            // past because all three share factors.
            //
            // The first row is the size compared against itself. It passing is what makes the other
            // three readable; it failing means the run measured non-determinism.
            expect (! results.empty() && results.front().sampleExact,
                    "the self-comparison failed, so the other rows measured non-determinism rather "
                    "than block dependence");

            for (const auto& r : results)
                expect (r.sampleExact,
                        "block-size invariance failed — the same sample stream cut differently "
                        "produced different output: " + r.describe());
        }

        beginTest ("Offline against real-time");
        {
            TapeRotAudioProcessor processor;
            warm (processor);

            const auto r = nf::testing::offlineAgainstRealtime (processor, {});

            logMessage ("  " + r.describe());

            // **Confirm setNonRealtime changed something observable**, or a passing comparison is
            // only evidence that the flag was ignored.
            if (! r.nonRealtimeWasHonoured)
                logMessage ("  NOTE: setNonRealtime changed nothing this processor reports, so this "
                            "row is 'no offline path exists' rather than 'the offline path agrees'.");

            expect (r.sampleExact || ! r.comparisonWasMeaningful,
                    "offline differs from real-time. Not a defect on its face — this casting would "
                    "have to intend it: " + r.describe());
        }
    }

private:
    /** One discarded render, so any first-run-only state is spent before a driver measures. */
    static void warm (TapeRotAudioProcessor& p)
    {
        nf::testing::RenderSpec spec;
        spec.blockSize = 512;
        spec.numBlocks = 4;
        nf::testing::render (p, spec);
    }
};

static InvarianceTests invarianceTests;
