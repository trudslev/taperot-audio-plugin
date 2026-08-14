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
            // **What is NOT yet named is the line.** All three draw from a shared juce::Random, and
            // FailureEngine's triggerIfDue is called per sample with a per-sample probability
            // (FailureEngine.cpp:71-79), which is the correct construction — so the mechanism is
            // not the obvious one and naming it needs the same treatment inside these three that
            // this bisection just gave the chain. The early return at :39, which skips a draw while
            // an event is active, is where to start: anything that makes the NUMBER of draws depend
            // on block boundaries rather than on sample count would do it.
            //
            // Recorded as localised rather than explained. Four attempts by construction produced
            // four refutations; one bisection by stage produced five exact zeroes and three
            // culprits, which is the argument for doing it this way first next time.
            juce::ignoreUnused (baseline);
            expect (true);   // locating; the failing assertions live in the block-size test above
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
