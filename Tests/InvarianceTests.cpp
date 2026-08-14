#include "../Source/PluginProcessor.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

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

        beginTest ("MODEL resets to NONE on EVERY prepare, and the first block switches away");
        {
            // **Found by survey, because the measurement is structurally blind to it.** The
            // cold-versus-warmed instrument compares two renders that BOTH call prepareToPlay, so a
            // defect that fires identically on every prepare cancels out of it exactly.
            //
            // TapeModelEQ::prepare sets activeModelIndex = 0 unconditionally (TapeModelEQ.cpp:82).
            // 0 is NONE. The parameter's default is 5, CASSETTE I (Parameters.h:255), and a Program
            // can select any of the nine. So the first process block after ANY prepare sees
            // 5 != 0 and starts a model switch — a crossfade in FADE, and in CLUNK a hard
            // coefficient swap under a mute dip with a thump.
            //
            // Not first-run-only, which is what makes it worse than the two ReverbEngine cases:
            // every sample-rate change and every buffer-size change in a host re-fires it.
            //
            // ## The known case, named before the run
            //
            // model = 0 (NONE) must show NOTHING: requested equals what prepare set, so no switch
            // can fire. It is the control, and if it shows a dip the instrument is measuring
            // something other than the switch.
            nf::testing::RenderSpec spec;
            spec.blockSize = 512;
            spec.numBlocks = 16;

            const auto onsetProfile = [&] (const char* label, float modelNormalised)
            {
                TapeRotAudioProcessor p;

                if (auto* m = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::model)))
                    m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 (modelNormalised));

                const auto r = nf::testing::render (p, spec);
                const auto rms = windowedRms (r, spec.sampleRate, 10.0, 10);

                // Each slice against the LAST one, so every row is normalised to its own settled
                // state and the two models are comparable despite sounding different.
                juce::String row;
                for (size_t i = 0; i < rms.size(); ++i)
                    row += juce::String ((rms[i] > 0.0 && rms.back() > 0.0)
                                             ? 20.0 * std::log10 (rms[i] / rms.back()) : -99.0, 1)
                               .paddedLeft (' ', 8);

                logMessage ("  " + juce::String (label).paddedRight (' ', 26) + row);
                return rms;
            };

            logMessage ("  slice (10 ms), vs settled 10     20      30      40      50      60      70      80      90     100");
            const auto none = onsetProfile ("model 0 NONE (control)", 0.0f);
            const auto cassette = onsetProfile ("model 5 CASSETTE I", 5.0f);

            const auto dip = [] (const std::vector<double>& r)
            {
                double worst = 0.0;
                for (size_t i = 0; i < 6; ++i)     // the first 60 ms, which is the switch window
                    if (r[i] > 0.0 && r.back() > 0.0)
                        worst = juce::jmin (worst, 20.0 * std::log10 (r[i] / r.back()));
                return worst;
            };

            logMessage ("  worst dip in the first 60 ms -> NONE " + juce::String (dip (none), 2)
                            + " dB, CASSETTE I " + juce::String (dip (cassette), 2) + " dB");


            // **The control fired too, so the row above does NOT measure the model switch.** NONE
            // came back -66.6 dB against CASSETTE I's -66.0 — that dip is OutputStage's fade-in,
            // already found and measured at -16.6 dB, sitting on top of the window the switch would
            // occupy. An instrument dominated by a larger known defect cannot resolve a smaller one
            // underneath it, and the control is the only reason that is visible rather than assumed.
            //
            // So isolate on something the fade-in cannot touch. switchMode has NO effect except
            // DURING a switch: FADE crossfades the two chains, CLUNK swaps coefficients under a
            // mute dip. If no switch fires on the first block the two renders are bit-identical.
            //
            // Known case, again named first: at model 0 the requested model equals what prepare
            // set, so no switch is possible and FADE must equal CLUNK exactly. If that arm differs,
            // switchMode is doing something outside a switch and this test proves nothing.
            const auto fadeVsClunk = [&] (float modelValue)
            {
                std::vector<std::vector<std::vector<float>>> renders;

                for (float mode : { 0.0f, 1.0f })
                {
                    TapeRotAudioProcessor p;

                    if (auto* m = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::model)))
                        m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 (modelValue));

                    if (auto* s = p.apvts.getParameter (ParamIDs::switchMode))
                        s->setValueNotifyingHost (mode);

                    renders.push_back (nf::testing::render (p, spec));
                }

                return nf::testing::compareRenders (renders[0], renders[1]);
            };

            const auto controlArm = fadeVsClunk (0.0f);
            const auto liveArm    = fadeVsClunk (5.0f);

            logMessage ("  FADE vs CLUNK at model 0 (control) -> " + controlArm.describe());
            logMessage ("  FADE vs CLUNK at model 5           -> " + liveArm.describe());
            logMessage (juce::String ("  => ") + (controlArm.sampleExact && ! liveArm.sampleExact
                            ? "the model switch FIRES on the first block after prepare"
                            : controlArm.sampleExact ? "no switch detected at model 5 either — the "
                                                       "construction is real but its effect is NOT "
                                                       "reproduced, and stays inferred"
                                                     : "the control differed, so this arm proves nothing"));

            expect (true);   // reported; the defect is the reset at TapeModelEQ.cpp:82
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

        beginTest ("Block size — GEN and the transport gate, driven SEPARATELY");
        {
            // Two sites share one signature, so they are driven apart: if both express there are
            // two members and a ruling covers both; if one does, the other stays on the list rather
            // than being closed by association.
            //
            // ## The transport gate cannot be exercised here, and that is a stated limitation
            //
            // `transportGateSmoothed`'s target is `hostIsPlaying ? 1 : 0`, and this harness supplies
            // no play head, so JUCE's fallback makes it permanently true. The target never moves,
            // so the smoother never ramps and the per-block `skip`/flat-apply cannot express. It is
            // therefore NEITHER confirmed nor refuted by anything below — it stays a defect on its
            // own terms (a gain ramp drawn as a staircase on transport start and stop) that this
            // instrument is structurally unable to reach, which is the same shape as category 3's
            // blindness to every-prepare defects.
            //
            // ## GEN can, and the mechanism is specific
            //
            // prepareToPlay sets genSmoothed to the RAW parameter (PluginProcessor.cpp:421) while
            // processBlock sets its target to the ROUNDED one (:509). A non-integral GEN therefore
            // leaves current != target at the first block, so the smoother ramps — and :510-511
            // advance it across the whole block and apply the END value flat. That is block-size
            // dependent, from sample 0, small, and growing with the buffer: the measured signature.
            //
            // KNOWN CASE: an exactly-integral GEN makes current == target, so no ramp exists to be
            // stepped. If the rows go sample-exact there, genSmoothed is the cause. If they do not,
            // both per-block constructions are refuted as the explanation for these rows and the
            // cause is something neither of them touches — a null result with nothing behind it.
            const auto rowsAtGen = [this] (const char* label, float genValue, bool setIt)
            {
                TapeRotAudioProcessor p;

                if (setIt)
                    if (auto* g = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::gen)))
                        g->setValueNotifyingHost (g->getNormalisableRange().convertTo0to1 (genValue));

                logMessage ("  " + juce::String (label) + ", GEN reads \"" 
                                + p.apvts.getParameter (ParamIDs::gen)->getCurrentValueAsText() + "\"");

                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                const auto results = nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 });

                double worst = 0.0;
                bool allExact = true;

                for (const auto& r : results)
                {
                    logMessage ("      " + r.describe());
                    worst = juce::jmax (worst, r.maxAbsDifference);
                    allExact = allExact && r.sampleExact;
                }

                return std::make_pair (allExact, worst);
            };

            const auto atDefault = rowsAtGen ("default", 0.0f, false);
            const auto atInteger = rowsAtGen ("GEN forced to exactly 4", 4.0f, true);

            logMessage (juce::String ("  => ") + (! atDefault.first && atInteger.first
                            ? "CONFIRMED: genSmoothed's raw-versus-rounded mismatch is the cause"
                            : atInteger.first == atDefault.first
                                ? "REFUTED as the explanation for these rows — an integral GEN "
                                  "changes nothing, so neither per-block construction produces them"
                                : "unclassified — reporting and stopping"));


            // **Both named constructions are refuted, so localise by STAGE instead.** DRIVE at 0
            // takes the Saturator's bypass branch (Saturator.cpp:69) and with it the whole
            // oversampled path — the only other place in this chain where block length enters the
            // arithmetic at all. If the rows go exact there, the divergence lives in the
            // oversampler; if they do not, it is downstream of it.
            //
            // The oversampled index map at Saturator.cpp:128 was checked and is NOT it:
            // `i * numSamples / overSamples` with overSamples = numSamples * factor reduces to
            // `i / factor` exactly in integer arithmetic, so numSamples cancels. Recorded because
            // it looks block-dependent and is not, and the next reader will find it too.
            const auto atSilentDrive = rowsAtGen ("DRIVE at 0 (Saturator bypassed)", 0.0f, false);

            {
                TapeRotAudioProcessor p;
                if (auto* d = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::drive)))
                    d->setValueNotifyingHost (0.0f);

                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                bool allExact = true;
                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                {
                    logMessage ("      DRIVE 0 -> " + r.describe());
                    allExact = allExact && r.sampleExact;
                    worst = juce::jmax (worst, r.maxAbsDifference);
                }

                logMessage (juce::String ("  => ") + (allExact
                                ? "the divergence is INSIDE the Saturator's oversampled path"
                                : "the divergence survives a bypassed Saturator — worst "
                                      + juce::String (worst, 9) + ", so it is downstream of it"));

                // **The deltas came back byte-identical to the default arm** — 0.000200260 /
                // 0.001022242 / 0.001926094 to nine digits with DRIVE at 0 and at default. Not
                // merely "still present": the divergent component is wholly independent of the
                // saturator path, which is a stronger exclusion than the bypass alone gives.
                //
                // Remaining candidates, named here so the next pass confirms rather than searches.
                // Both carry the same skip(numSamples)-then-apply-the-end-value-flat shape as the
                // two already refuted, and both are downstream of the Saturator:
                //
                //   ToneFilters.cpp:77-81   lpSmoothed/hpSmoothed advanced across the block, the
                //                           END cutoff used for every sample in it. Guarded at
                //                           prepare, so the question is whether prepare's value and
                //                           process's target agree — if they do not, it ramps.
                //   StereoSpread.cpp:20     amountSmoothed, same shape, and no-op guarded.
                //
                // The refutations so far are worth as much as a confirmation would be: genSmoothed
                // is out (an integral GEN changes nothing, and the default is already integral),
                // the transport gate is structurally unreachable in this harness, the oversampled
                // index map reduces to i/factor, and the Saturator contributes nothing.
            }

            expect (! atDefault.first,
                    "the default arm came back exact, so there was no divergence to explain and "
                    "the integral arm proves nothing");
        }

        beginTest ("The block-size rows — ToneFilters' LP ramp, armed on EVERY prepare");
        {
            // **Both earlier candidates were refuted and this is the third, found by reading the
            // one they pointed at.** ToneFilters::prepare sets lpSmoothed's current value to
            // `nyquistSafeHz` — 23520 at 48 kHz — while the LP PARAMETER defaults to 20 kHz
            // (ToneFilters.cpp:56). So current != target at the first block after every prepare, a
            // ramp exists, and :77-81 advance it across the whole block and apply the END cutoff
            // flat to every sample in it. That is block-size dependent, from sample 0.
            //
            // It is the every-prepare class, which is why warming did not remove it: render() calls
            // prepareToPlay, so each block size re-arms the ramp and steps it differently.
            //
            // ## The discriminator, and it is quantitative
            //
            // If this is the mechanism, the divergence must scale with how far the LP has to
            // travel — |nyquistSafeHz - LP|. At LP 20 kHz that is 3520 Hz; at LP 1 kHz it is
            // 22520 Hz, six times further, and the rows must grow accordingly. A flat response to
            // LP refutes it as surely as the GEN arm refuted genSmoothed.
            const auto worstAtLp = [this] (float lpHz)
            {
                TapeRotAudioProcessor p;

                if (auto* lp = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::lp)))
                    lp->setValueNotifyingHost (lp->getNormalisableRange().convertTo0to1 (lpHz));

                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                    worst = juce::jmax (worst, r.maxAbsDifference);

                logMessage ("  LP " + juce::String (lpHz, 0) + " Hz (travels "
                                + juce::String (23520.0f - lpHz, 0) + " Hz) -> worst |delta| "
                                + juce::String (worst, 9));

                return worst;
            };

            const auto atTop = worstAtLp (20000.0f);
            const auto atMid = worstAtLp (5000.0f);
            const auto atLow = worstAtLp (1000.0f);

            logMessage ("  => " + juce::String (atLow > atTop * 2.0
                            ? "CONFIRMED: the divergence scales with the LP ramp's distance"
                            : "REFUTED: LP's distance does not drive it — a third candidate out"));

            expect (atLow > atTop * 2.0,
                    "the block-size divergence did not grow with the LP smoother's travel, so "
                    "ToneFilters' ramp is not what produces these rows either: 20 kHz "
                        + juce::String (atTop, 9) + ", 1 kHz " + juce::String (atLow, 9));

            juce::ignoreUnused (atMid);
        }

        beginTest ("Block-size rows — bisected by STAGE, not by construction");
        {
            // **Four attempts by construction, four refutations.** genSmoothed (an integral GEN
            // changes nothing, and the default already is integral), the transport gate
            // (structurally unreachable without a play head), ToneFilters' LP ramp (six times the
            // travel gave 1.5x the divergence), and the oversampled index map (reduces to i/factor).
            // Bypassing the Saturator left the deltas byte-identical to nine digits.
            //
            // Every one of those started from "find a skip-then-apply-flat and test it". The one
            // approach that produced a CLEAN exclusion rather than a refutation was bisecting by
            // stage, so this does that deliberately.
            //
            // ## The method, and the known case it is introduced against
            //
            // First drive every parameter to its most neutral value at once. If the divergence
            // survives that, it is in something no parameter reaches — the WowFlutter delay line
            // itself, or the dry-compensation delay, both of which run unconditionally. If it
            // vanishes, stages come back one at a time until it returns, and the one that returns
            // it owns it.
            //
            // KNOWN CASE: the all-neutral arm must still produce OUTPUT. A configuration that
            // silences the plugin would report sample-exact for the trivial reason and look like a
            // result — this sweep has had a comparison pass by being unable to fail three times, so
            // the arm reports its own peak and the peak is checked before the exactness is read.
            const auto rowsWith = [this] (const char* label,
                                          const std::function<void (TapeRotAudioProcessor&)>& configure)
            {
                TapeRotAudioProcessor p;
                configure (p);
                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                    worst = juce::jmax (worst, r.maxAbsDifference);

                // The output check, so an exact row cannot be silence.
                double peak = 0.0;
                for (const auto& ch : nf::testing::render (p, spec))
                    for (float v : ch)
                        peak = juce::jmax (peak, (double) std::abs (v));

                logMessage ("  " + juce::String (label).paddedRight (' ', 30)
                                + "worst |delta| " + juce::String (worst, 9)
                                + "   (peak " + juce::String (peak, 4) + ")");

                return std::make_pair (worst, peak);
            };

            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            const auto neutralise = [&setP] (TapeRotAudioProcessor& p)
            {
                setP (p, ParamIDs::drive, 0.0f);
                setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f);
                setP (p, ParamIDs::noise, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);
                setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);
                setP (p, ParamIDs::model, 0.0f);        // NONE — bypasses the tape-model system
            };

            const auto baseline = rowsWith ("defaults", [] (TapeRotAudioProcessor&) {});
            const auto allOff   = rowsWith ("everything neutral", neutralise);

            expectGreaterThan (allOff.second, 1.0e-4,
                               "the all-neutral arm produced silence, so an exact row there would "
                               "mean nothing — the bisection cannot start from a configuration the "
                               "comparison cannot fail in");

            if (allOff.first < 1.0e-9)
            {
                // Stages back one at a time. The first that returns the divergence owns it.
                struct Stage { const char* id; float on; const char* label; };
                const Stage stages[] = {
                    { ParamIDs::drive,   10.0f, "+ DRIVE" },   { ParamIDs::wow,     100.0f, "+ WOW" },
                    { ParamIDs::flutter, 100.0f, "+ FLUTTER" }, { ParamIDs::noise,   100.0f, "+ NOISE" },
                    { ParamIDs::hum,     100.0f, "+ HUM" },     { ParamIDs::failure, 100.0f, "+ FAILURE" },
                    { ParamIDs::spread,  100.0f, "+ SPREAD" },  { ParamIDs::model,     5.0f, "+ MODEL" },
                };

                for (const auto& s : stages)
                    rowsWith (s.label, [&] (TapeRotAudioProcessor& p)
                    {
                        neutralise (p);
                        setP (p, s.id, s.on);
                    });
            }
            else
            {
                logMessage ("  => the divergence SURVIVES every parameter at neutral, so it is in "
                            "something no parameter reaches — WowFlutter's delay line and the "
                            "dry-compensation delay both run unconditionally.");
            }

            // **LOCALISED: the three GENERATORS own it, and nothing else contributes.** DRIVE,
            // WOW, FLUTTER, SPREAD and MODEL each come back at exactly 0.000000000 — not small,
            // zero — while NOISE gives 0.019, HUM 0.000050 and FAILURE 1.599. The defaults' 0.0019
            // is a mixture of the three at their default settings.
            //
            // That FAILURE figure is the one to read: at full depth, the same sample stream cut
            // into different block sizes produces output differing by 1.6 on a signal peaking at
            // 2.2. Not an artefact — a different performance.
            //
            // **FAILURE's row is a reclassification, not a table entry.** 1.6 against a 2.2 peak
            // means someone bouncing at 2048 and monitoring at 128 gets two different takes, and
            // the plugin's most characterful control behaves differently according to a setting
            // that has nothing to do with it. "Output depends on block size" reads as a −60 dB
            // curiosity; this is at the top of the range and it is a different performance.
            //
            // **CORRECTION: an earlier note here said all three draw from a shared juce::Random.
            // That was inferred, stated as fact, and is false in two ways.** They do not share —
            // FailureEngine has its own seeded instance (FailureEngine.h:109) and NoiseSource has
            // three per-character ones (NoiseSource.h:62-64) — and **Hum has no Random at all**
            // (Hum.h:19-21 is a phase, an increment and a smoother).
            //
            // Two consequences, and both narrow the next pass rather than widen it. There is no
            // shared-stream interference class to close, because there is no sharing. And Hum's
            // divergence cannot be a draw-count mechanism, because Hum is deterministic — so the
            // three rows do not share a cause and may not share a class.
            //
            // What each row is, as far as measurement and construction now say:
            //
            //   HUM      0.000049503, about −93 dB of the peak. Hum's construction is per-sample
            //            correct — getNextValue() per sample, phase += phaseInc per sample, no
            //            skip-then-apply-flat (Hum.cpp:24-33). At this magnitude the row may be
            //            floating-point ordering rather than behaviour, and saying so is not a
            //            dismissal: it is a different investigation from the other two.
            //   NOISE    0.019260951, about −41 dB. Real, and NoiseSource does draw.
            //   FAILURE  1.599813402, about −2.8 dB. Behavioural, at the top of the range.
            //            triggerIfDue is called per sample with a per-sample probability
            //            (FailureEngine.cpp:71-79), which is the correct construction, so the
            //            obvious mechanism is already excluded. The early return at :39 skips a
            //            draw while an event is active — anything making the NUMBER of draws depend
            //            on block boundaries rather than sample count would do it.
            //
            // Recorded as localised rather than explained. Four attempts by construction produced
            // four refutations; one bisection by stage produced five exact zeroes and three
            // culprits, which is the argument for doing it this way first next time.
            juce::ignoreUnused (baseline);
            expect (true);   // locating; the failing assertions live in the block-size test above
        }

        beginTest ("Downstream of the generators — and the known case has to INVERT");
        {
            // **The defect requires a generator running and is invisible without one.** All three
            // generators are bit-identical or event-identical driven alone; five chain stages
            // measured exactly 0.000000000; and the chain-level rows are still real. So the
            // divergence is in how something downstream RESPONDS to generator signal.
            //
            // That is why four construction hypotheses all missed: it is signal-dependent, not
            // construction-dependent, and no amount of reading a per-block-looking line finds it.
            //
            // **And it inverts the known case.** The first bisection was validated by an
            // all-neutral arm that still produced output. That arm would report exactly zero here
            // and mean nothing — it is the configuration in which the defect cannot appear. Every
            // arm below therefore keeps NOISE at 100, and the control is that NOISE-on-only must
            // still diverge, or the run measured nothing.
            //
            // ## The candidate this points at
            //
            // A stage that is active even at neutral settings and whose effect depends on wet
            // differing from dry. `dryCompensationDelay` is exactly that: it delays the dry copy by
            // genValue x perStageDrySamples, computed per block, so MIX blends a delayed dry
            // against the wet. With the generators off and DRIVE at 0 the wet path is nearly the
            // dry path, so any misalignment cancels and is invisible — which is the shape of every
            // observation so far.
            //
            // MIX fully wet removes the dry path entirely. If the divergence vanishes there, the
            // dry/wet alignment owns it.
            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            const auto worstWith = [this, &setP] (const char* label, float mixPercent)
            {
                TapeRotAudioProcessor p;

                setP (p, ParamIDs::drive, 0.0f);
                setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f);
                setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);
                setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);
                setP (p, ParamIDs::model, 0.0f);
                setP (p, ParamIDs::noise, 100.0f);      // the generator stays ON — see above
                setP (p, ParamIDs::mix, mixPercent);

                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                    worst = juce::jmax (worst, r.maxAbsDifference);

                logMessage ("  " + juce::String (label).paddedRight (' ', 28)
                                + "worst |delta| " + juce::String (worst, 9));
                return worst;
            };

            const auto blended = worstWith ("NOISE on, MIX default", 50.0f);
            const auto fullyWet = worstWith ("NOISE on, MIX 100% wet", 100.0f);

            expectGreaterThan (blended, 1.0e-9,
                               "the control arm did not diverge, so this run measured nothing — "
                               "the defect needs a generator and this configuration has one");

            logMessage (juce::String ("  => ") + (fullyWet < 1.0e-9
                            ? "the DRY/WET alignment owns it: removing the dry path removes the "
                              "divergence entirely"
                            : "it survives a fully wet path, so the dry compensation delay is not "
                              "the cause — worst " + juce::String (fullyWet, 9)));

            expect (true);   // locating
        }

        beginTest ("The MIX line — a third point, because two always make a line");
        {
            // **The level arms that used to live here have been WITHDRAWN — their fixture was
            // broken.** They reported a x4 input diverging by 10.1 with no generator running, which
            // was read as refuting the origin claim and establishing level as the trigger. It
            // established neither: the scaled-input fixture fed a DIFFERENT waveform at each block
            // size, so the comparison was between two unrelated noise streams. The rebuilt version
            // is the block below; the origin question is reopened there, not answered here.
            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            const auto neutral = [&setP] (TapeRotAudioProcessor& p, float noisePercent, float mixPercent)
            {
                setP (p, ParamIDs::drive, 0.0f);   setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f); setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);     setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);     setP (p, ParamIDs::model, 0.0f);
                setP (p, ParamIDs::noise, noisePercent);
                setP (p, ParamIDs::mix, mixPercent);
            };

            const auto worstWith = [this, &neutral] (const char* label, float noisePercent,
                                                     float mixPercent)
            {
                TapeRotAudioProcessor p;
                neutral (p, noisePercent, mixPercent);
                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;

                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                    worst = juce::jmax (worst, r.maxAbsDifference);

                logMessage ("  " + juce::String (label).paddedRight (' ', 34)
                                + "worst |delta| " + juce::String (worst, 9));
                return worst;
            };

            // **Three points, because two always make a line.** 50% and 100% came back at exactly
            // 2x and read as linear in wet gain. 25% is what turns that from arithmetic into a
            // claim — and linearity is evidence the divergence is CREATED by something linear and
            // passed through, rather than made by a nonlinear stage, which would rarely scale this
            // cleanly. That is the n=1 check one dimension over: two points cannot distinguish a
            // line from anything else that happens to pass through them.
            logMessage ("  --- the MIX line, third point ---");
            const auto at25  = worstWith ("NOISE 100, MIX 25%",  100.0f,  25.0f);
            const auto at50  = worstWith ("NOISE 100, MIX 50%",  100.0f,  50.0f);
            const auto at100 = worstWith ("NOISE 100, MIX 100%", 100.0f, 100.0f);

            if (at25 > 0.0)
                logMessage ("  ratios against 25% -> 50%: x" + juce::String (at50 / at25, 3)
                                + ", 100%: x" + juce::String (at100 / at25, 3)
                                + "   (linear predicts x2.000 and x4.000)");

            // **Measured exactly x2.000 and x4.000 — and that is MECHANICALLY NECESSARY rather
            // than evidence about mechanism.** MIX is a gain applied downstream of wherever the
            // divergence is created, so it scales whatever arrives at it; a linear creator and a
            // nonlinear one are indistinguishable here. It confirms the location — wet path, before
            // the mix — which was already established, and nothing else. Taking the third point was
            // still right: two points would have supported the stronger reading.
            expect (true);   // locating
        }

        beginTest ("The x4 row was a FIXTURE defect — its input differed between block sizes");
        {
            // **The instrument was wrong before the measurement, and this time it produced a
            // confident finding that has had to be withdrawn.**
            //
            // `render` hands `fillInput` a BLOCK INDEX, not an absolute sample position
            // (ProcessorHarness.cpp:232-235). The withdrawn x4 arm seeded a `juce::Random` with
            // `2024 + blockIndex` and restarted it every block, so the input at 64 samples per block
            // was a different waveform from the input at 2048 and the comparison was between two
            // unrelated noise streams. A worst |delta| of 10.1 on a +/-4 signal is what comparing
            // two unrelated streams looks like; it was read as the plugin being unstable at level.
            //
            // Nothing is wrong with the harness. Its default path is position-determined —
            // `deterministicSample (absolute + i, ch)` with `absolute` accumulating across blocks
            // (:226, :243) — so the default input is identical at every block size by construction.
            //
            // ## The control could not have caught it, and that is the part with reach
            //
            // The x1 arm was written as the control and it passed. It passed because
            // `if (inputGain != 1.0f)` meant it never installed `fillInput` at all: it ran the
            // DEFAULT path, validated the default path, and was silent about the only thing under
            // test. **A control that does not exercise the fixture cannot validate the fixture**,
            // and it is indistinguishable from one that can.
            //
            // Same family as the known case that was known along one axis — there the arm asked the
            // wrong question, here it ran the wrong code path — and both are answerable from the
            // design before the run. The check is: name the line the control shares with the arm it
            // is controlling for. If there is none, it is not a control.
            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            const auto neutral = [&setP] (TapeRotAudioProcessor& p, float noisePercent)
            {
                setP (p, ParamIDs::drive, 0.0f);   setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f); setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);     setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);     setP (p, ParamIDs::model, 0.0f);
                setP (p, ParamIDs::noise, noisePercent);
                setP (p, ParamIDs::mix, 50.0f);
            };

            // `deterministicSample`, transcribed from ProcessorHarness.cpp:42-47 because it is
            // file-local there. **A transcription is checked against its original rather than
            // trusted** — the gain-1 arm below asserts it reproduces the default path byte for byte,
            // which is what makes the louder arms the same signal scaled rather than a second one.
            const auto sampleAt = [] (int absoluteIndex, int channel) noexcept
            {
                uint32_t x = (uint32_t) (absoluteIndex * 2654435761u)
                           ^ (uint32_t) (channel * 40503u) ^ 0x9e3779b9u;
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                return ((float) (x & 0xffffffu) / (float) 0x7fffff) - 1.0f;
            };

            // Absolute position, derived from the block index and the block's own length. `render`
            // uses one fixed block size for a whole run, which is what makes that exact — and it is
            // the single line the withdrawn fixture got wrong.
            const auto scaledInput = [sampleAt] (float gain)
            {
                return [sampleAt, gain] (juce::AudioBuffer<float>& b, int blockIndex)
                {
                    const int absolute = blockIndex * b.getNumSamples();

                    for (int ch = 0; ch < b.getNumChannels(); ++ch)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            b.setSample (ch, i, gain * sampleAt (absolute + i, ch));
                };
            };

            // KNOWN CASE 1 — the transcription reproduces the harness's own generator. Without this
            // the level arms would be a different signal as well as a louder one, which is the
            // confound the whole block exists to remove.
            {
                TapeRotAudioProcessor a, b;
                neutral (a, 0.0f);
                neutral (b, 0.0f);
                warm (a);
                warm (b);

                nf::testing::RenderSpec byDefault;
                byDefault.blockSize = 512;
                byDefault.numBlocks = 8;

                auto byFill = byDefault;
                byFill.fillInput = scaledInput (1.0f);

                const auto viaDefault = nf::testing::render (a, byDefault);
                const auto viaFill    = nf::testing::render (b, byFill);

                double worst = 0.0;

                for (size_t ch = 0; ch < juce::jmin (viaDefault.size(), viaFill.size()); ++ch)
                    for (size_t i = 0; i < juce::jmin (viaDefault[ch].size(), viaFill[ch].size()); ++i)
                        worst = juce::jmax (worst, (double) std::abs (viaDefault[ch][i] - viaFill[ch][i]));

                logMessage ("  transcription check, default against fillInput at gain 1 -> "
                                + juce::String (worst, 9));

                expectEquals (worst, 0.0,
                              "the transcribed input generator does not reproduce the harness's own, "
                              "so a louder arm would be a DIFFERENT signal rather than a scaled one");
            }

            const auto worstAtGain = [this, &neutral, &scaledInput] (const char* label,
                                                                     float noisePercent, float gain)
            {
                TapeRotAudioProcessor p;
                neutral (p, noisePercent);
                warm (p);

                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;
                spec.fillInput = scaledInput (gain);   // ALWAYS, gain 1 included — see the control note

                double worst = 0.0;

                for (const auto& r : nf::testing::blockSizeInvariance (p, spec, { 64, 128, 511, 2048 }))
                    worst = juce::jmax (worst, r.maxAbsDifference);

                double peak = 0.0;
                bool finite = true;

                for (const auto& ch : nf::testing::render (p, spec))
                    for (float v : ch)
                    {
                        finite = finite && std::isfinite (v);
                        peak = juce::jmax (peak, (double) std::abs (v));
                    }

                logMessage ("  " + juce::String (label).paddedRight (' ', 30)
                                + "worst |delta| " + juce::String (worst, 9)
                                + "   (peak " + juce::String (peak, 4)
                                + (finite ? ")" : ", NON-FINITE OUTPUT)"));

                return worst;
            };

            // KNOWN CASE 2 — the fixture must not HIDE divergence either. NOISE at 100 measured
            // 0.0096 through the default path; if it comes back exact through this one, the fixture
            // is suppressing rather than measuring and every zero below means nothing.
            logMessage ("  --- the fixture, both directions ---");
            const auto noiseThroughFill = worstAtGain ("gain 1, NOISE 100", 100.0f, 1.0f);

            expectGreaterThan (noiseThroughFill, 1.0e-9,
                               "the known divergent configuration came back exact through this "
                               "fixture, so the fixture hides divergence and no zero below is "
                               "readable");

            // The level question, asked properly this time. The claim under test is that the chain
            // with no generator is exactly invariant at any level — which is what the origin reading
            // requires, and what the withdrawn arm appeared to refute.
            logMessage ("  --- level, with no generator anywhere ---");
            const auto g1  = worstAtGain ("gain 1,  no generator",   0.0f, 1.0f);
            const auto g15 = worstAtGain ("gain 1.5, no generator",  0.0f, 1.5f);
            const auto g2  = worstAtGain ("gain 2,  no generator",   0.0f, 2.0f);
            const auto g4  = worstAtGain ("gain 4,  no generator",   0.0f, 4.0f);
            const auto g8  = worstAtGain ("gain 8,  no generator",   0.0f, 8.0f);

            const auto loudest = juce::jmax (g15, juce::jmax (g2, juce::jmax (g4, g8)));

            logMessage (juce::String ("  => ") + (loudest < 1.0e-9
                            ? "LEVEL IS EXCLUDED: the generator-free chain is exactly invariant up "
                              "to 8x full scale, so the withdrawn x4 row was entirely its fixture "
                              "and the origin reading stands"
                            : "level does move it: worst across the louder arms "
                                  + juce::String (loudest, 9)));

            expectEquals (g1, 0.0,
                          "the generator-free chain diverged at unity gain through this fixture, "
                          "which the default path measures as exactly zero — the fixture is the "
                          "difference, not the plugin");

            expect (true);   // locating
        }

        beginTest ("Confined to the ramp, or steady? — the partition the withdrawn row owed");
        {
            // **The x4 bisection this was to be cannot run: its case does not exist.** So this is
            // the partition that does — over TIME rather than over stages, which needs no new
            // hypothesis and halves the space whichever way it comes out.
            //
            // The bound that now holds is sharp and worth stating before the run. A generator-free
            // chain is exactly invariant at a peak of 12.4; NOISE at 100 diverges by 0.0096 at a
            // peak of 1.56. So it is neither level nor signal character — eight times the amplitude
            // of the same broadband stream changes nothing, and a quieter internally-generated one
            // changes the output. The distinguishing property really is that the signal ORIGINATES
            // inside the chain.
            //
            // ## What that leaves, and why time splits it
            //
            // `render` calls prepareToPlay and reset() on every arm, so EVERY measurement here is a
            // cold one — and this casting has seven `SmoothedValue::reset (rate, seconds)` sites
            // with no following `setCurrentAndTargetValue`, so those smoothers ramp from zero on the
            // first prepare. The RANKING test measures that as a -16.6 dB first-run fade-in.
            //
            // A smoother advanced with `skip (numSamples)` and applied flat across the block is a
            // staircase whose step IS the block size — so if the divergence lives in the initial
            // ramp, it is that construction and it is confined to the first few tens of ms. If it
            // runs to the end of a two-second render, the ramp is not it and the cause is in steady
            // state. Two very different investigations, separated by one run.
            //
            // KNOWN CASE: the arm must diverge at all, or the profile is a row of zeroes that reads
            // as "confined to nothing". NOISE stays at 100 and the total is asserted non-zero
            // before any slice is read.
            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            TapeRotAudioProcessor p;
            setP (p, ParamIDs::drive, 0.0f);   setP (p, ParamIDs::wow, 0.0f);
            setP (p, ParamIDs::flutter, 0.0f); setP (p, ParamIDs::failure, 0.0f);
            setP (p, ParamIDs::hum, 0.0f);     setP (p, ParamIDs::spread, 0.0f);
            setP (p, ParamIDs::gen, 1.0f);     setP (p, ParamIDs::model, 0.0f);
            setP (p, ParamIDs::noise, 100.0f); setP (p, ParamIDs::mix, 100.0f);

            constexpr int totalSamples = 512 * 192;      // ~2.05 s at 48 kHz

            const auto renderAt = [&p] (int blockSize)
            {
                nf::testing::RenderSpec spec;
                spec.blockSize = blockSize;
                spec.numBlocks = totalSamples / blockSize;
                return nf::testing::render (p, spec);
            };

            const auto small = renderAt (64);
            const auto large = renderAt (2048);

            constexpr int slices = 16;
            const int perSlice = totalSamples / slices;

            double overall = 0.0;
            juce::String row, msRow;

            for (int s = 0; s < slices; ++s)
            {
                double worst = 0.0;

                for (size_t ch = 0; ch < juce::jmin (small.size(), large.size()); ++ch)
                    for (int i = s * perSlice; i < (s + 1) * perSlice; ++i)
                        if ((size_t) i < juce::jmin (small[ch].size(), large[ch].size()))
                            worst = juce::jmax (worst, (double) std::abs (small[ch][(size_t) i]
                                                                        - large[ch][(size_t) i]));

                overall = juce::jmax (overall, worst);
                row += juce::String (worst, 6).paddedLeft (' ', 10);
                msRow += juce::String ((int) (s * perSlice / 48.0)).paddedLeft (' ', 10);
            }

            logMessage ("  slice start (ms)  " + msRow);
            logMessage ("  worst |delta|     " + row);

            expectGreaterThan (overall, 1.0e-9,
                               "the profiled configuration did not diverge at all, so every slice "
                               "is zero for the trivial reason and the profile means nothing");

            // Read by comparing the first slice against the last four: a ramp artefact decays to
            // nothing, a steady-state one does not.
            double lateWorst = 0.0;

            for (int s = slices - 4; s < slices; ++s)
            {
                double worst = 0.0;

                for (size_t ch = 0; ch < juce::jmin (small.size(), large.size()); ++ch)
                    for (int i = s * perSlice; i < (s + 1) * perSlice; ++i)
                        if ((size_t) i < juce::jmin (small[ch].size(), large[ch].size()))
                            worst = juce::jmax (worst, (double) std::abs (small[ch][(size_t) i]
                                                                        - large[ch][(size_t) i]));

                lateWorst = juce::jmax (lateWorst, worst);
            }

            logMessage ("  => worst " + juce::String (overall, 9) + " in the first slice against "
                            + juce::String (lateWorst, 9) + " in the last quarter, a ratio of "
                            + juce::String (lateWorst > 0.0 ? overall / lateWorst : 0.0, 1));

            // **MEASURED: it is BOTH, and that is two findings rather than one.**
            //
            //   slice 0 (0-128 ms)   0.024078
            //   every later slice    0.00023 .. 0.00045, flat to two seconds, no decay
            //
            // So the first-run ramp carries the MAGNITUDE — 0.024 is sixty times the steady figure,
            // and it is what the headline 0.019 over a 0.68 s render was mostly measuring — while a
            // steady-state divergence of ~0.0003 runs underneath it and never decays. A ramp
            // artefact alone would have fallen to nothing; it does not.
            //
            // **Filed as two, for the same reason the two smoother sites were.** They have
            // different exposures: the first-run component fires once per instance and lands under
            // the -16.6 dB fade-in the RANKING test measures, while the steady one is present in
            // every second of every render and is the reason a bounce at 2048 and a monitor at 128
            // are not the same audio. Closing them together would let the smaller and more
            // permanent of the two be closed by association with the larger.
            //
            // The steady component is also what refutes the ramp as a sufficient explanation of the
            // generator rows, which is what this run was for. Next partition is inside the steady
            // component alone — with the first 128 ms EXCLUDED from the comparison, so the ramp
            // cannot dominate the figure being read.
            expect (true);   // locating
        }

        beginTest ("The STEADY component alone — first 128 ms excluded from every arm");
        {
            // **The MIX result is re-derived here rather than carried forward, because it was
            // measured on the composite.** 25 / 50 / 100 came back at exactly x2 and x4 — but the
            // composite is 57:1 ramp over steady, so that line is a property of the RAMP and the
            // steady component merely inherited "wholly in the wet path, scaled by mix" from it.
            //
            // An inherited constraint is the kind that narrows a search to the wrong half. A
            // first-run smoother ramp is plausibly in the wet path; something that never decays
            // could be anywhere, including where the mix does not reach. So the same three points
            // are measured again with the ramp excluded: if the steady component still scales, the
            // location constraint is established for both. If it does not, its location is UNKNOWN
            // rather than known, and the next bisection has to search the whole chain.
            //
            // ## Why 128 ms, and what every arm here excludes
            //
            // The time profile's first slice is 0-128 ms and reads 0.024078 against 0.00023-0.00045
            // everywhere after it. Any arm including that slice reports the ramp's figure and says
            // nothing about the steady one — the same shape as an arm without a generator reporting
            // exactly zero and meaning nothing. **6144 samples at 48 kHz are dropped from every
            // comparison below**, and both constraints are asserted rather than assumed: a
            // generator must be running, and the window must still diverge.
            constexpr int steadySkip  = 6144;              // 128 ms at 48 kHz
            constexpr int totalSamples = 512 * 192;        // ~2.05 s

            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            struct Steady { double worst; double rms; };

            // Renders at 64 and 2048, compares the window AFTER the ramp, and reports the window's
            // own RMS beside the divergence — so "does it scale" is read against measured level
            // rather than against a knob percentage, which is not the same question.
            // Position-determined, gain-scaled input — the corrected construction from the block
            // above, so an input-level arm is the same stream scaled rather than a second stream.
            const auto sampleAt = [] (int absoluteIndex, int channel) noexcept
            {
                uint32_t x = (uint32_t) (absoluteIndex * 2654435761u)
                           ^ (uint32_t) (channel * 40503u) ^ 0x9e3779b9u;
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                return ((float) (x & 0xffffffu) / (float) 0x7fffff) - 1.0f;
            };

            // 1.0 is the harness default (no hook installed), 0.0 is silence, anything else scales.
            const auto steadyWith = [this, &setP, sampleAt] (const char* label,
                                                   const std::function<void (TapeRotAudioProcessor&)>& configure,
                                                   float inputGain = 1.0f)
            {
                TapeRotAudioProcessor p;
                configure (p);

                const auto renderAt = [&p, inputGain, sampleAt] (int blockSize)
                {
                    nf::testing::RenderSpec spec;
                    spec.blockSize = blockSize;
                    spec.numBlocks = totalSamples / blockSize;

                    if (inputGain != 1.0f)
                        spec.fillInput = [inputGain, sampleAt] (juce::AudioBuffer<float>& b, int blockIndex)
                        {
                            const int absolute = blockIndex * b.getNumSamples();

                            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                                for (int i = 0; i < b.getNumSamples(); ++i)
                                    b.setSample (ch, i, inputGain * sampleAt (absolute + i, ch));
                        };

                    return nf::testing::render (p, spec);
                };

                const auto small = renderAt (64);
                const auto large = renderAt (2048);

                double worst = 0.0, sumSq = 0.0;
                int counted = 0;

                for (size_t ch = 0; ch < juce::jmin (small.size(), large.size()); ++ch)
                {
                    const auto n = (int) juce::jmin (small[ch].size(), large[ch].size());

                    for (int i = steadySkip; i < n; ++i)
                    {
                        worst = juce::jmax (worst, (double) std::abs (small[ch][(size_t) i]
                                                                    - large[ch][(size_t) i]));
                        sumSq += (double) small[ch][(size_t) i] * (double) small[ch][(size_t) i];
                        ++counted;
                    }
                }

                const Steady s { worst, counted > 0 ? std::sqrt (sumSq / counted) : 0.0 };

                logMessage ("  " + juce::String (label).paddedRight (' ', 30)
                                + "steady |delta| " + juce::String (s.worst, 9)
                                + "   (rms " + juce::String (s.rms, 6) + ")");
                return s;
            };

            const auto neutral = [&setP] (TapeRotAudioProcessor& p)
            {
                setP (p, ParamIDs::drive, 0.0f);   setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f); setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);     setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);     setP (p, ParamIDs::model, 0.0f);
                setP (p, ParamIDs::noise, 0.0f);   setP (p, ParamIDs::mix, 50.0f);
            };

            // ---- both directions on the window itself, before anything is read from it ----
            logMessage ("  --- the steady window, both directions ---");

            const auto control = steadyWith ("NOISE 100 (must diverge)", [&] (TapeRotAudioProcessor& p)
            {
                neutral (p);
                setP (p, ParamIDs::noise, 100.0f);
            });

            const auto blank = steadyWith ("no generator (must be exact)", neutral);

            expectGreaterThan (control.worst, 1.0e-9,
                               "the steady window does not diverge even with a generator running, "
                               "so every zero below is zero for the trivial reason");

            expectEquals (blank.worst, 0.0,
                          "the generator-free chain diverged in the steady window, which the "
                          "block-size sweep measures as exactly zero — the window is the "
                          "difference, not the plugin");

            // ---- 1 · the MIX line, RE-DERIVED on the steady component ----
            logMessage ("  --- MIX, re-derived without the ramp ---");

            const auto mixAt = [&] (const char* label, float mixPercent)
            {
                return steadyWith (label, [&] (TapeRotAudioProcessor& p)
                {
                    neutral (p);
                    setP (p, ParamIDs::noise, 100.0f);
                    setP (p, ParamIDs::mix, mixPercent);
                }).worst;
            };

            const auto m25  = mixAt ("MIX 25%",  25.0f);
            const auto m50  = mixAt ("MIX 50%",  50.0f);
            const auto m100 = mixAt ("MIX 100%", 100.0f);

            if (m25 > 0.0)
                logMessage ("  ratios against 25% -> 50%: x" + juce::String (m50 / m25, 3)
                                + ", 100%: x" + juce::String (m100 / m25, 3)
                                + "   (linear predicts x2.000 and x4.000)");

            // ---- 2a · WHICH generator. All three were identical driven ALONE, and alone is not
            //           the configuration that diverges, so the question is open per generator.
            logMessage ("  --- which generator, one at a time ---");

            steadyWith ("NOISE 100 only",   [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 100.0f); });
            steadyWith ("HUM 100 only",     [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::hum, 100.0f); });
            steadyWith ("FAILURE 100 only", [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::failure, 100.0f); });

            // ---- 2b · does it scale with GENERATOR amplitude? Proportional means the chain is
            //           responding to the signal; fixed means something accumulates independently
            //           of level. The rms column is what makes that readable, because a knob
            //           percentage is not an amplitude.
            logMessage ("  --- generator amplitude ---");

            const auto n25  = steadyWith ("NOISE 25",  [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 25.0f); });
            const auto n50  = steadyWith ("NOISE 50",  [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 50.0f); });
            const auto n100 = steadyWith ("NOISE 100", [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 100.0f); });

            if (n25.worst > 0.0)
                logMessage ("  delta ratios 25 -> 50: x" + juce::String (n50.worst / n25.worst, 3)
                                + ", 100: x" + juce::String (n100.worst / n25.worst, 3));

            // **The rms column is what makes that row readable, and it reads it DOWN.** NOISE 25
            // against NOISE 100 moves the window's rms by 0.00002 on 0.523 — the noise bed sits
            // about 41 dB under the input — so the manipulation barely moved the quantity it was
            // supposed to vary. A flat delta across it is consistent with "does not scale with
            // generator amplitude" and equally consistent with "the arm could not tell". That is
            // the n=1 question in another unit: ask whether the manipulation moved the thing,
            // before reading the result as an answer.
            //
            // The through-signal is the other half of the same question and it CAN be moved, so
            // that is the arm below rather than a rerun of this one.

            // ---- 3 · does it need the INPUT at all, and does it scale with it? The origin
            //          constraint says a generator is required; these two say whether the input is
            //          required as well, and whether the divergence tracks the signal PASSING
            //          THROUGH. Proportional to the through-signal with a generator merely present
            //          is a modulation shape; fixed is an additive one. Gain 1 installs no hook —
            //          it is the harness default, shown byte-identical to the scaled construction
            //          in the block above.
            logMessage ("  --- the through-signal, generator held on ---");

            const auto withNoise = [&] (TapeRotAudioProcessor& p)
            {
                neutral (p);
                setP (p, ParamIDs::noise, 100.0f);
            };

            const auto i0   = steadyWith ("NOISE 100, silent input",   withNoise, 0.0f);
            const auto i025 = steadyWith ("NOISE 100, input x0.25",    withNoise, 0.25f);
            const auto i1   = steadyWith ("NOISE 100, input x1",       withNoise, 1.0f);
            const auto i4   = steadyWith ("NOISE 100, input x4",       withNoise, 4.0f);

            if (i025.worst > 0.0)
                logMessage ("  delta ratios x0.25 -> x1: x" + juce::String (i1.worst / i025.worst, 3)
                                + ", x4: x" + juce::String (i4.worst / i025.worst, 3)
                                + "   (proportional predicts x4.000 and x16.000)");

            logMessage ("  delta per unit rms: silent " + juce::String (i0.rms > 0.0 ? i0.worst / i0.rms : 0.0, 9)
                            + ", x1 " + juce::String (i1.rms > 0.0 ? i1.worst / i1.rms : 0.0, 9)
                            + ", x4 " + juce::String (i4.rms > 0.0 ? i4.worst / i4.rms : 0.0, 9));

            // ================= MEASURED, and it is four results =================
            //
            // **1 · The MIX line SURVIVES the ramp's removal, so the location constraint is
            // established for the steady component rather than inherited.** 0.000112385 /
            // 0.000224769 / 0.000449538 — x2.000 and x4.000 on the steady window alone. The next
            // bisection can work upstream of the mix without that being a borrowed assumption. It
            // still cannot distinguish a linear creator from a nonlinear one, for the same
            // mechanical reason as before: MIX is a downstream gain and scales whatever arrives.
            //
            // **2 · FAILURE is not one of a set of three. It is 0.914 in STEADY STATE**, against
            // NOISE 0.000225 and HUM 0.000194 — four orders of magnitude, and none of it a ramp
            // artefact. The composite figure of 1.599 was read as "the worst of three generator
            // rows"; it is a different finding that happened to be measured alongside two small
            // ones. At full depth the same sample stream cut into different block sizes is a
            // different performance, permanently, not for the first 128 ms.
            //
            // **3 · The mechanism is MODULATION, not addition — and it took two arms to see,
            // because the first one could not have answered.** Generator amplitude does nothing:
            // NOISE 25 / 50 / 100 gives 0.000218 / 0.000275 / 0.000225, flat and non-monotonic. But
            // that arm barely moved the quantity it was varying — the rms column shows 0.523134
            // against 0.523152, the noise bed sitting ~41 dB under the input — so on its own it is
            // as consistent with "the arm could not tell" as with "it does not scale".
            //
            // The through-signal CAN be moved, and it is what the divergence tracks:
            //
            //   silent input   0.000003379   (rms 0.004597)
            //   input x0.25    0.000051536   (rms 0.130862)      ratio x4.361 against x0.25
            //   input x1       0.000224769   (rms 0.523152)
            //   input x4       0.000872374   (rms 2.092537)      ratio x16.928, proportional says 16
            //
            // Per unit rms that is 0.000430 at x1 and 0.000417 at x4 — constant at about -67 dB of
            // whatever is passing through. So a generator must be RUNNING and its LEVEL is
            // irrelevant, while the divergence scales with the signal it is riding on. That is the
            // signature of something multiplying the through-signal under generator-dependent
            // control, not of a generator's own stream being cut differently — which is also why
            // all three generators measured invariant driven alone.
            //
            // **4 · It is created DOWNSTREAM of the GEN cascade.** NoiseSource injects inside each
            // DegradationCore; Hum injects after the whole cascade. Two different injection points
            // return 0.000225 and 0.000194 — the same magnitude — which a mechanism living in the
            // cascade could not produce. Combined with (1), the creator sits between Hum's
            // injection and the mix: FailureEngine, StereoSpread, ToneFilters, TapeStop,
            // FilterSweep, OutputStage.
            //
            // **The x8 arm is what makes (3) a claim rather than a restatement.** Level alone with
            // no generator is exactly zero at eight times full scale, so this is not the chain
            // responding to loudness; the generator is a gate on the effect and the through-signal
            // is what the effect is applied to. Two separate dependencies, and neither alone
            // produces it.
            expect (true);   // locating
        }

        beginTest ("FAILURE — a SEPARATE finding, not the worst of three generator rows");
        {
            // **This is filed apart from the block-size section deliberately, and the reason is not
            // its delta figure.** 0.914 in steady state against NOISE's 0.000225 and HUM's 0.000194
            // is four orders of magnitude, but severity here comes from what it means musically:
            // FAILURE is this plugin's most characterful control, and at full depth the same sample
            // stream cut into different block sizes is a permanently different performance. Someone
            // bouncing at 2048 and monitoring at 128 gets two different takes — and gets them
            // because of a host setting that has nothing to do with the sound.
            //
            // The composite 1.599 hid that by ranking it as the largest of three rows in one table.
            // A number in a column about invariance reads as a tolerance question. This is not one.
            //
            // ## Everything established for the small component was measured with NOISE
            //
            // Location upstream of the mix, proportionality to the through-signal, independence
            // from generator level, and creation downstream of the GEN cascade — all four came from
            // NOISE arms. **Assuming FAILURE shares them would inherit four constraints it has not
            // earned**, which is exactly what re-deriving the MIX line on the steady window
            // avoided. So the same three cuts are run against FAILURE before anything is bisected.
            //
            // Two hunts, which may converge and may not. FailureEngine is on the six-candidate list
            // for the small component independently of this, so a shared cause is possible — but
            // progress on the small one implies nothing about this one until these arms say so.
            constexpr int steadySkip   = 6144;             // 128 ms at 48 kHz
            constexpr int totalSamples = 512 * 192;        // ~2.05 s

            const auto setP = [] (TapeRotAudioProcessor& p, const char* id, float physical)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
            };

            const auto sampleAt = [] (int absoluteIndex, int channel) noexcept
            {
                uint32_t x = (uint32_t) (absoluteIndex * 2654435761u)
                           ^ (uint32_t) (channel * 40503u) ^ 0x9e3779b9u;
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                return ((float) (x & 0xffffffu) / (float) 0x7fffff) - 1.0f;
            };

            struct Steady { double worst; double rms; };

            // Same window and same construction as the NOISE block, so the two sets are comparable
            // figure for figure rather than by argument.
            const auto renderSteady = [sampleAt] (TapeRotAudioProcessor& p, int blockSize, float inputGain)
            {
                nf::testing::RenderSpec spec;
                spec.blockSize = blockSize;
                spec.numBlocks = totalSamples / blockSize;

                if (inputGain != 1.0f)
                    spec.fillInput = [inputGain, sampleAt] (juce::AudioBuffer<float>& b, int blockIndex)
                    {
                        const int absolute = blockIndex * b.getNumSamples();

                        for (int ch = 0; ch < b.getNumChannels(); ++ch)
                            for (int i = 0; i < b.getNumSamples(); ++i)
                                b.setSample (ch, i, inputGain * sampleAt (absolute + i, ch));
                    };

                return nf::testing::render (p, spec);
            };

            const auto compareSteady = [] (const std::vector<std::vector<float>>& a,
                                           const std::vector<std::vector<float>>& b)
            {
                double worst = 0.0, sumSq = 0.0;
                int counted = 0;

                for (size_t ch = 0; ch < juce::jmin (a.size(), b.size()); ++ch)
                {
                    const auto n = (int) juce::jmin (a[ch].size(), b[ch].size());

                    for (int i = steadySkip; i < n; ++i)
                    {
                        worst = juce::jmax (worst, (double) std::abs (a[ch][(size_t) i] - b[ch][(size_t) i]));
                        sumSq += (double) a[ch][(size_t) i] * (double) a[ch][(size_t) i];
                        ++counted;
                    }
                }

                return Steady { worst, counted > 0 ? std::sqrt (sumSq / counted) : 0.0 };
            };

            // **A SELF-COMPARISON RUNS BESIDE EVERY ARM, and it is not decoration.** The first
            // version of this block rendered at 64 and then at 2048 on one processor and read the
            // difference as block dependence. The 64-against-64 column of cut 4 came back at 0.914
            // for FAILURE and 0.000225 for NOISE — **identical to the block-size figures** — which
            // says those arms were measuring render-to-render non-reproducibility, not block size.
            //
            // Two mechanisms were folded together there: `render` prepares and resets, so the FIRST
            // render of a fresh processor ramps its smoothers up from zero while every later one
            // does not; and anything a `prepare` does not re-seed carries over between renders. So
            // every arm compared a COLD render against a WARM one and attributed the whole
            // difference to the container.
            //
            // The fix is one throwaway render before the reference, so both compared renders are
            // warm — and the self-comparison stays reported, because a driver that cannot be shown
            // to give zero on identical inputs cannot measure anything smaller than its own noise.
            // `blockSizeInvariance` has the same first row and this sweep only ever read the MAX
            // across its sweep, which folded the self-comparison in and hid it.
            const auto steadyWith = [this, &renderSteady, &compareSteady]
                                    (const char* label,
                                     const std::function<void (TapeRotAudioProcessor&)>& configure,
                                     float inputGain = 1.0f)
            {
                TapeRotAudioProcessor p;
                configure (p);

                renderSteady (p, 512, inputGain);          // discarded: spends the first-run ramp

                const auto reference = renderSteady (p, 64, inputGain);
                const auto self      = compareSteady (reference, renderSteady (p, 64, inputGain));
                const auto s         = compareSteady (reference, renderSteady (p, 2048, inputGain));

                logMessage ("  " + juce::String (label).paddedRight (' ', 30)
                                + "steady |delta| " + juce::String (s.worst, 9)
                                + "   (self " + juce::String (self.worst, 9)
                                + ", rms " + juce::String (s.rms, 6) + ")");
                return s;
            };

            const auto neutral = [&setP] (TapeRotAudioProcessor& p)
            {
                setP (p, ParamIDs::drive, 0.0f);   setP (p, ParamIDs::wow, 0.0f);
                setP (p, ParamIDs::flutter, 0.0f); setP (p, ParamIDs::failure, 0.0f);
                setP (p, ParamIDs::hum, 0.0f);     setP (p, ParamIDs::spread, 0.0f);
                setP (p, ParamIDs::gen, 1.0f);     setP (p, ParamIDs::model, 0.0f);
                setP (p, ParamIDs::noise, 0.0f);   setP (p, ParamIDs::mix, 50.0f);
            };

            const auto withFailure = [&] (float amount, float mixPercent)
            {
                return [&, amount, mixPercent] (TapeRotAudioProcessor& p)
                {
                    neutral (p);
                    setP (p, ParamIDs::failure, amount);
                    setP (p, ParamIDs::mix, mixPercent);
                };
            };

            logMessage ("  --- both directions on the window ---");
            const auto control = steadyWith ("FAILURE 100 (must diverge)", withFailure (100.0f, 50.0f));
            const auto blank   = steadyWith ("no generator (must be exact)", neutral);

            expectGreaterThan (control.worst, 1.0e-9,
                               "FAILURE did not diverge in the steady window, so every arm below "
                               "is measuring nothing");

            expectEquals (blank.worst, 0.0,
                          "the generator-free chain diverged in this window, so the window is the "
                          "difference rather than FAILURE");

            // ---- cut 1 · MIX. Upstream of the mix, or not? NOISE said yes, exactly x2 and x4.
            logMessage ("  --- cut 1: MIX ---");
            const auto f25  = steadyWith ("FAILURE 100, MIX 25%",  withFailure (100.0f,  25.0f)).worst;
            const auto f50  = steadyWith ("FAILURE 100, MIX 50%",  withFailure (100.0f,  50.0f)).worst;
            const auto f100 = steadyWith ("FAILURE 100, MIX 100%", withFailure (100.0f, 100.0f)).worst;

            if (f25 > 0.0)
                logMessage ("  ratios against 25% -> 50%: x" + juce::String (f50 / f25, 3)
                                + ", 100%: x" + juce::String (f100 / f25, 3)
                                + "   (linear predicts x2.000 and x4.000)");

            // ---- cut 2 · the through-signal. NOISE tracked it at a constant -67 dB ratio.
            logMessage ("  --- cut 2: the through-signal ---");
            const auto t0   = steadyWith ("FAILURE 100, silent input", withFailure (100.0f, 50.0f), 0.0f);
            const auto t025 = steadyWith ("FAILURE 100, input x0.25",  withFailure (100.0f, 50.0f), 0.25f);
            const auto t1   = steadyWith ("FAILURE 100, input x1",     withFailure (100.0f, 50.0f), 1.0f);
            const auto t4   = steadyWith ("FAILURE 100, input x4",     withFailure (100.0f, 50.0f), 4.0f);

            if (t025.worst > 0.0)
                logMessage ("  delta ratios x0.25 -> x1: x" + juce::String (t1.worst / t025.worst, 3)
                                + ", x4: x" + juce::String (t4.worst / t025.worst, 3)
                                + "   (proportional predicts x4.000 and x16.000)");

            logMessage ("  delta per unit rms: silent " + juce::String (t0.rms > 0.0 ? t0.worst / t0.rms : 0.0, 9)
                            + ", x1 " + juce::String (t1.rms > 0.0 ? t1.worst / t1.rms : 0.0, 9)
                            + ", x4 " + juce::String (t4.rms > 0.0 ? t4.worst / t4.rms : 0.0, 9));

            // ---- cut 3 · its OWN level. NOISE's was unreadable because the knob barely moved the
            //      rms; FAILURE's does move it — dropouts remove energy — so the rms column says
            //      whether this arm can tell before its flatness or steepness is read.
            logMessage ("  --- cut 3: FAILURE's own level ---");
            const auto a25  = steadyWith ("FAILURE 25",  withFailure (25.0f, 50.0f));
            const auto a50  = steadyWith ("FAILURE 50",  withFailure (50.0f, 50.0f));
            const auto a100 = steadyWith ("FAILURE 100", withFailure (100.0f, 50.0f));

            if (a25.worst > 0.0)
                logMessage ("  delta ratios 25 -> 50: x" + juce::String (a50.worst / a25.worst, 3)
                                + ", 100: x" + juce::String (a100.worst / a25.worst, 3)
                                + "   (rms moved " + juce::String (a25.rms, 6) + " -> "
                                + juce::String (a100.rms, 6) + ")");

            // ---- cut 4 · block size, monotonic or not. **This is the one that separates a
            //      per-block-advanced modulator from the other candidates**, and it needs no new
            //      instrument: TapeStop and FilterSweep are time-varying by construction, and this
            //      casting already carries the skip(numSamples)-then-apply-flat shape twice
            //      (genSmoothed, transportGateSmoothed). A modulator stepped per block coarsens
            //      with the buffer, so its divergence should grow monotonically with block size.
            //      Anything event-driven or state-ordering-driven need not.
            //
            //      Filed as a CANDIDATE rather than a hypothesis with a line: nothing here names a
            //      site, and the four refuted construction hypotheses are why.
            logMessage ("  --- cut 4: does it track block size monotonically? ---");

            const auto profile = [&] (const char* label, const std::function<void (TapeRotAudioProcessor&)>& configure)
            {
                TapeRotAudioProcessor p;
                configure (p);

                renderSteady (p, 512, 1.0f);               // discarded, as above

                const auto reference = renderSteady (p, 64, 1.0f);
                juce::String row;

                // 64 leads deliberately: it is the self-comparison, and it must read zero before
                // any figure to its right is a statement about block size.
                for (int bs : { 64, 128, 511, 2048 })
                    row += juce::String (compareSteady (reference, renderSteady (p, bs, 1.0f)).worst, 6)
                               .paddedLeft (' ', 12);

                logMessage ("  " + juce::String (label).paddedRight (' ', 22) + row);
            };

            logMessage ("  against the 64 reference   " + juce::String ("64").paddedLeft (' ', 12)
                            + juce::String ("128").paddedLeft (' ', 12)
                            + juce::String ("511").paddedLeft (' ', 12)
                            + juce::String ("2048").paddedLeft (' ', 12));

            profile ("FAILURE 100", withFailure (100.0f, 50.0f));
            profile ("NOISE 100",   [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 100.0f); });
            profile ("HUM 100",     [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::hum, 100.0f); });
            profile ("defaults",    [] (TapeRotAudioProcessor&) {});
            profile ("defaults, GEN 8", [&] (TapeRotAudioProcessor& p) { setP (p, ParamIDs::gen, 8.0f); });

            // ================= THE ARMS ABOVE OVERTURN THE ROWS THEY WERE RUN TO EXPLAIN =========
            //
            // **The ~0.0002 STEADY component does not exist.** Warmed, NOISE and HUM measure
            // exactly zero after 128 ms at every block size, 64 included. The previous run
            // characterised it as modulation on four measured properties — upstream of the mix,
            // proportional to the through-signal, indifferent to generator level, downstream of the
            // cascade — and every one of those was a property of a COLD render compared against a
            // WARM one rather than of a block-size difference.
            //
            // **NOISE's row itself survives, and cut 5 is what restores it.** Warmed, the
            // self-comparison is exactly 0.000000000 while 2048 differs by 0.009630475 over the
            // whole render. Warm-up length is not a free variable either: 2048 samples and 98 304
            // give the identical figure, and only the unwarmed arm is confounded.
            //
            // **Cut 6 localises it: the first ~20 ms of every render.** 0.009630 / 0.007679 /
            // 0.002036 over the first three 8 ms slices, then exactly zero from 24 ms on. So it is
            // a transient with a ~20 ms decay, re-armed by every `prepareToPlay` — which is the
            // class that is strictly worse than first-run-only, because a host re-fires it on every
            // sample-rate and buffer-size change rather than once per instance.
            //
            // **Cut 7 refutes the obvious candidate for it.** A ~20 ms transient re-armed on every
            // prepare and gated on a generator being audible is exactly the shape
            // `TapeModelEQ::activeModelIndex` has — a stored copy of a selection reset in prepare
            // against a default the Program does not select — and `NoiseSource.cpp:142` carries the
            // same construction. Driven at the constructed character and at two others, all three
            // diverge: 0.009630 / 0.010797 / 0.004283, with every self-comparison exact. A
            // stored-copy re-arm would have been exact at the constructed value. **Fifth refuted
            // construction hypothesis in this hunt**, which is the argument for the stage bisection
            // stated once more.
            //
            // **FAILURE is not block-size dependence at all: it is RENDER-TO-RENDER
            // NON-DETERMINISM.** Its self-comparison — same processor, same block size, same input,
            // warmed, two consecutive renders — is 0.914079, and 128 / 511 / 2048 come back
            // 0.886955 / 0.920244 / 0.914071. The same magnitude everywhere including against
            // itself, which is what a stochastic process that is never re-seeded looks like.
            // `prepareToPlay` and `reset()` do not restore FailureEngine's stream, so replaying a
            // session gives a different performance — the block size was never the variable.
            //
            // It also explains cut 2 exactly: dropouts multiply the through-signal, so the
            // difference between two draws scales with what is passing through (x4.000, x16.000,
            // per-unit-rms constant to six digits at 1.7365). Cut 1's x2/x4 says the same thing MIX
            // always says. Cut 3's 3.279 / 5.451 are noisy because the quantity is stochastic.
            //
            // ---- cut 5 · HOW MUCH warm-up? The earlier arms were not unwarmed — `rowsWith` and
            //      the MIX block both call `warm()`, which is 2048 samples. This block's throwaway
            //      render is 98 304. If those give different answers, the WARM-UP LENGTH is a free
            //      variable nobody controlled, and every figure in the hunt depends on it.
            //
            //      Compared over the WHOLE render, no window skipped, so this is the same
            //      comparison the headline figures were.
            logMessage ("  --- cut 5: warm-up length, whole render compared ---");

            const auto atWarmUp = [&] (const char* label,
                                       const std::function<void (TapeRotAudioProcessor&)>& configure,
                                       int warmUpSamples)
            {
                TapeRotAudioProcessor p;
                configure (p);

                if (warmUpSamples > 0)
                {
                    nf::testing::RenderSpec w;
                    w.blockSize = 512;
                    w.numBlocks = warmUpSamples / 512;
                    nf::testing::render (p, w);
                }

                const auto reference = renderSteady (p, 64, 1.0f);

                const auto wholeRender = [&reference] (const std::vector<std::vector<float>>& other)
                {
                    double worst = 0.0;

                    for (size_t ch = 0; ch < juce::jmin (reference.size(), other.size()); ++ch)
                        for (size_t i = 0; i < juce::jmin (reference[ch].size(), other[ch].size()); ++i)
                            worst = juce::jmax (worst, (double) std::abs (reference[ch][i] - other[ch][i]));

                    return worst;
                };

                logMessage ("  " + juce::String (label).paddedRight (' ', 30)
                                + "self " + juce::String (wholeRender (renderSteady (p, 64, 1.0f)), 9)
                                + "   2048 " + juce::String (wholeRender (renderSteady (p, 2048, 1.0f)), 9));
            };

            const auto noise100 = [&] (TapeRotAudioProcessor& p) { neutral (p); setP (p, ParamIDs::noise, 100.0f); };

            atWarmUp ("NOISE 100, no warm-up",     noise100,     0);
            atWarmUp ("NOISE 100, warm() = 2048",  noise100,  2048);
            atWarmUp ("NOISE 100, 98304 samples",  noise100, 98304);

            // ---- cut 6 · WHERE in the first 128 ms. Cut 5 restores NOISE's row: warmed, the
            //      self-comparison is exactly zero while 2048 differs by 0.009630475 over the whole
            //      render — and cut 4 measured exactly zero for the same configuration after 128 ms.
            //      So it is real block dependence, confined to the head of every render. Sliced at
            //      8 ms so the shape is visible rather than inferred from two endpoints.
            logMessage ("  --- cut 6: where in the first 128 ms, warmed ---");
            {
                TapeRotAudioProcessor p;
                noise100 (p);

                nf::testing::RenderSpec w;
                w.blockSize = 512;
                w.numBlocks = 4;
                nf::testing::render (p, w);                // warm() — cut 5 shows 2048 is enough

                const auto reference = renderSteady (p, 64, 1.0f);
                const auto other     = renderSteady (p, 2048, 1.0f);

                constexpr int sliceSamples = 384;          // 8 ms at 48 kHz
                juce::String row, msRow;

                for (int s = 0; s < 16; ++s)
                {
                    double worst = 0.0;

                    for (size_t ch = 0; ch < juce::jmin (reference.size(), other.size()); ++ch)
                        for (int i = s * sliceSamples; i < (s + 1) * sliceSamples; ++i)
                            if ((size_t) i < juce::jmin (reference[ch].size(), other[ch].size()))
                                worst = juce::jmax (worst, (double) std::abs (reference[ch][(size_t) i]
                                                                           - other[ch][(size_t) i]));

                    row   += juce::String (worst, 6).paddedLeft (' ', 10);
                    msRow += juce::String (s * 8).paddedLeft (' ', 10);
                }

                logMessage ("  slice start (ms)  " + msRow);
                logMessage ("  worst |delta|     " + row);
            }

            // ---- cut 7 · a ~20 ms transient re-armed on EVERY prepare, gated on the generator
            //      being audible, is the shape `TapeModelEQ::activeModelIndex` already has in this
            //      casting — a stored copy of a selection reset in prepare against a default the
            //      Program does not select, whose branch runs a crossfade. NoiseSource carries the
            //      same construction at NoiseSource.cpp:142.
            //
            //      **This is testable the same way that one was, without naming a line: drive the
            //      selection at its CONSTRUCTED value and at another.** If the constructed value is
            //      exact and the others are not, a stored-copy re-arm owns it. If both diverge, it
            //      does not, and four refuted construction hypotheses say not to assume otherwise.
            logMessage ("  --- cut 7: is it a stored-copy re-arm on the noise character? ---");

            for (float character : { 0.0f, 1.0f, 2.0f })
            {
                TapeRotAudioProcessor p;
                neutral (p);
                setP (p, ParamIDs::noise, 100.0f);
                setP (p, ParamIDs::noiseCharacter, character);

                nf::testing::RenderSpec w;
                w.blockSize = 512;
                w.numBlocks = 4;
                nf::testing::render (p, w);

                const auto reference = renderSteady (p, 64, 1.0f);

                const auto whole = [&reference] (const std::vector<std::vector<float>>& other)
                {
                    double worst = 0.0;

                    for (size_t ch = 0; ch < juce::jmin (reference.size(), other.size()); ++ch)
                        for (size_t i = 0; i < juce::jmin (reference[ch].size(), other[ch].size()); ++i)
                            worst = juce::jmax (worst, (double) std::abs (reference[ch][i] - other[ch][i]));

                    return worst;
                };

                logMessage ("  noise character " + juce::String ((int) character)
                                + "               self " + juce::String (whole (renderSteady (p, 64, 1.0f)), 9)
                                + "   2048 " + juce::String (whole (renderSteady (p, 2048, 1.0f)), 9));
            }

            // **What the self-comparison column changes about every earlier figure.**
            // `blockSizeInvariance` renders its reference first and then re-renders the front size,
            // so its first row is render 1 against render 2, and this sweep only ever read the MAX
            // across the sweep — folding the self-comparison in wherever it was non-zero. The arms
            // that called `warm()` first are sound (0.019 at NOISE 100 is 0.0096 here over a longer
            // render); **the arms this session added without warming are not**, which is where the
            // 0.024 head and the 0.0003 tail both came from.
            //
            // The rule the column is worth keeping for: **report a driver's self-comparison beside
            // every result, not once in a premise check.** This file opens with a premise-check
            // block asserting exactly this property — at DEFAULT parameters, where FAILURE is low
            // and the processor is reproducible. Reproducibility is not a property of the
            // processor; it is a property of the processor IN A CONFIGURATION, and the one
            // configuration where it was checked is the one where it holds.
            expect (true);   // locating
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
