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
        beginTest ("RANKING — is the first-run difference a FADE-IN, or a hair of drift?");
        {
            // **The audible/inaudible split was ranked by reading and this reproduces it.** The
            // plan's rule: an inferred finding is a hypothesis with a line number until the harness
            // reproduces it, and this one is carrying a release-blocker claim.
            //
            // SITE: OutputStage: output gain and wet/dry mix, 20 ms ramp
            //
            // juce::SmoothedValue default-constructs with target 0, and reset(rate, seconds) is
            // setCurrentAndTargetValue(target) — so a constructed smoother starts at ZERO and the
            // first process() call ramps it up to its real value. If what it carries is a gain, the
            // first playback fades in from silence.
            //
            // Reported in dB of the cold render against the warmed one, in 5 ms slices. A fade-in
            // is a monotonic rise from a large negative figure to 0 dB, completing at the ramp
            // length. A hair of drift is a flat row near 0.
            TapeRotAudioProcessor cold;
            TapeRotAudioProcessor warmRef;
            warm (warmRef);

            nf::testing::RenderSpec spec;
            spec.blockSize = 512;
            spec.numBlocks = 16;

            const auto coldRender = nf::testing::render (cold, spec);
            const auto warmRender = nf::testing::render (warmRef, spec);

            logMessage ("  slice (5 ms each)          5      10      15      20      25      30      35      40      45      50");
            reportFirstRunEnvelope ("cold vs warmed", coldRender, warmRender, spec.sampleRate);

            // **The control, and it is what makes the row above readable.** Two warmed renders must
            // be flat at 0 dB across every slice; if they are not, the instrument is reporting
            // something other than the first-run state and the row above means nothing.
            TapeRotAudioProcessor warmB;
            warm (warmB);
            reportFirstRunEnvelope ("warmed vs warmed", nf::testing::render (warmB, spec),
                                    warmRender, spec.sampleRate);

            const auto c = windowedRms (coldRender, spec.sampleRate, 5.0, 10);
            const auto w = windowedRms (warmRender, spec.sampleRate, 5.0, 10);

            const double firstSliceDb = (c[0] > 0.0 && w[0] > 0.0) ? 20.0 * std::log10 (c[0] / w[0])
                                                                   : -99.0;

            logMessage ("  first 5 ms -> " + juce::String (firstSliceDb, 2) + " dB");
            logMessage (juce::String ("  => ") + (firstSliceDb < -6.0
                            ? "AUDIBLE: the first playback is attenuated by more than 6 dB"
                            : "not a fade-in at this magnitude — the audible ranking does NOT hold here"));


            // **A LEVEL-RATIO METRIC IS BLIND TO A TIMING DEFECT, and that is why this second line
            // exists.** The row above ranks ATTENUATION: it answers "does the first playback fade
            // in", and it answers it well. It cannot answer "is the first playback audibly
            // different", because a pre-delay that glides moves WHEN the wet signal arrives and not
            // how loud it is — two renders can differ audibly and have identical RMS per slice.
            //
            // Third instance in this sweep of a metric that reads as a result and cannot rank what
            // it was asked to rank, after gradient-per-pixel and largestResponseDifferenceDb.
            // Caught here only because a casting whose defect is MEASURED at 0.124 came back 0.00 dB.
            //
            // So: the residual, which is blind to nothing. |cold - warmed| at its worst, against the
            // warmed render's own peak, in dB. A timing glide shows here at full size.
            {
                double worst = 0.0, peak = 0.0;

                for (size_t ch = 0; ch < warmRender.size(); ++ch)
                    for (size_t i = 0; i < warmRender[ch].size() && i < coldRender[ch].size(); ++i)
                    {
                        worst = juce::jmax (worst, (double) std::abs (coldRender[ch][i] - warmRender[ch][i]));
                        peak  = juce::jmax (peak,  (double) std::abs (warmRender[ch][i]));
                    }

                const double residualDb = (worst > 0.0 && peak > 0.0) ? 20.0 * std::log10 (worst / peak)
                                                                      : -99.0;

                logMessage ("  residual -> " + juce::String (worst, 9) + " against peak "
                                + juce::String (peak, 6) + " = " + juce::String (residualDb, 1)
                                + " dB below the signal");
                logMessage (juce::String ("  => ") + (residualDb > -40.0
                                ? "the first playback differs AUDIBLY, by whatever mechanism"
                                : "below -40 dB of the signal: not audible on its own"));
            }

            expect (true);   // ranking, not a pass/fail — the defect is asserted elsewhere
        }

    }

private:

    /** Windowed RMS of a render, one figure per `windowMs` slice, channel 0. */
    static std::vector<double> windowedRms (const std::vector<std::vector<float>>& render,
                                            double sampleRate, double windowMs, int windows)
    {
        std::vector<double> out;
        const int n = (int) (windowMs * 0.001 * sampleRate);

        for (int w = 0; w < windows; ++w)
        {
            double sum = 0.0;
            int counted = 0;

            for (int i = w * n; i < (w + 1) * n && i < (int) render[0].size(); ++i)
            {
                sum += (double) render[0][(size_t) i] * render[0][(size_t) i];
                ++counted;
            }

            out.push_back (counted > 0 ? std::sqrt (sum / counted) : 0.0);
        }

        return out;
    }

    /** **The ranking instrument.** A max |delta| says a first playback differs; it does not say
        whether the difference is a fade-in from silence or a hair of drift, and the release-blocker
        claim rests entirely on which. This reports the cold render's level against the warmed one
        in successive slices, in dB, which is the unit the claim is made in.

        A smoother snapping to a constructed zero shows as a monotonic rise from a large negative
        figure to 0 dB, completing at the smoother's own ramp length. Anything else is not that. */
    void reportFirstRunEnvelope (const juce::String& label,
                                 const std::vector<std::vector<float>>& cold,
                                 const std::vector<std::vector<float>>& warm,
                                 double sampleRate)
    {
        constexpr double windowMs = 5.0;
        constexpr int windows = 10;

        const auto c = windowedRms (cold, sampleRate, windowMs, windows);
        const auto w = windowedRms (warm, sampleRate, windowMs, windows);

        juce::String row;

        for (int i = 0; i < windows; ++i)
        {
            const double db = (c[(size_t) i] > 0.0 && w[(size_t) i] > 0.0)
                                  ? 20.0 * std::log10 (c[(size_t) i] / w[(size_t) i])
                                  : (w[(size_t) i] > 0.0 ? -99.0 : 0.0);

            row += juce::String (db, 1).paddedLeft (' ', 8);
        }

        logMessage ("  " + label.paddedRight (' ', 22) + row);
    }

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
