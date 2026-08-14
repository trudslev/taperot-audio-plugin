#include "../Source/PluginProcessor.h"
#include "../Source/Parameters.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

/**
    Category 5 — automation and latency.

    ## The declared-latency check, and the known case it is introduced against

    Five of the six castings declare no latency at all. **A declaration of zero is a claim like any
    other**, and the only one of the six that can validate the instrument is TapeRot: it is the sole
    casting that declares any (`setLatencySamples (saturator.getLatencySamples())`), its Saturator
    oversamples, and category 4 independently measured a ~20 ms dead window at the start of every
    render which is that latency showing up in a level profile.

    So TapeRot is the known case, stated before the set is run: **the instrument must report a
    non-zero figure there and it must agree with `getLatencySamples()`.** If it reports zero for
    TapeRot, a zero anywhere else means nothing — which is the failure mode five "no latency" claims
    would otherwise sail through.

    ## What agreement means

    An impulse must emerge at exactly the declared latency. Emerging LATER than declared is undeclared
    delay — a host aligns by the declaration, so the plugin's output arrives late and every other
    track is early against it. Emerging EARLIER is a declaration that over-compensates, which pulls
    the plugin ahead. Both are reported; neither is assumed to be the interesting one.
*/
/*  ## FINDING — 25 ms per GEN stage, undeclared. TWO problems, and only one is solved.

    Measured 1218 samples at 48 kHz against a declared 4. The source is
    `WowFlutter::nominalDelayMs = 25.0f` — 1200 samples — carried by EVERY active GEN stage, because
    the delay line is the modulation mechanism rather than an optional effect. So it scales 1200 to
    9600 samples as GEN goes 1 to 8.

    **Problem A — the dry path against the wet path. SOLVED.** `dryCompensationDelay` re-times the
    dry copy to match, tracking `genSmoothed` through transitions. MIX blends aligned signals.

    **Problem B — TapeRot against every other track in the session. NOT SOLVED, and nothing
    addresses it.** No declaration reaches the host, so the host aligns by 4 samples and the plugin
    sits up to 200 ms late against everything else.

    **That is why this survived: the failure is inaudible in isolation and obvious only across
    tracks.** Solo the plugin and it is correct. `PluginProcessor.h:292-300` describes the mechanism
    completely and accurately, and sits beside the code that fixes A — which is what makes it read
    as an explanation of a solved problem rather than as a defect nobody declared.

    ## Three options, and the third is only free until release

      1. Declare a fixed 200 ms nominal and internally delay to match at lower GEN — the technique
         `dryCompensationDelay` already implements. Taxes every user at GEN 1 with GEN 8's latency.
      2. Call `setLatencySamples` on every GEN change. What hosts handle badly.
      3. **Make GEN non-automatable.** The whole difficulty is latency moving at runtime under host
         control; if GEN is a setup choice rather than a performance parameter, a latency change
         becomes a graph rebuild while somebody is configuring, which hosts do tolerate. Cheapest of
         the three, a musical question rather than a technical one, and **only free before release**
         — afterwards it breaks saved automation.

    ## Still a prediction, not a result

    Reading says the centre delay is fixed and the models are EQ, so neither the model set nor the
    wow/flutter extremes should move this figure. **That is a prediction and it stays one** until
    GEN 1 / 4 / 8 and the wow/flutter extremes are measured — this sweep has been wrong twice about
    exactly this kind of reading.
*/
class AutomationLatencyTests final : public juce::UnitTest
{
public:
    AutomationLatencyTests() : juce::UnitTest ("Automation and latency", "DSP") {}

    void runTest() override
    {
        beginTest ("Is GEN the only variable? — the prediction, measured");
        {
            // **This was filed as a PREDICTION and is now driven.** Reading says the source is
            // WowFlutter's fixed 25 ms centre delay carried once per active GEN stage, so GEN should
            // scale it and nothing else should touch it: the models are EQ curves, and wow/flutter
            // modulate AROUND the centre without moving it.
            //
            // The prediction is quantitative, which is what makes it falsifiable rather than
            // decorative: 1200 samples per stage at 48 kHz, so GEN 1 / 4 / 8 must come back at
            // roughly 1200 / 4800 / 9600. A figure that is flat across GEN refutes the mechanism
            // entirely; a figure that moves with model or wow/flutter means the centre delay is not
            // fixed and the fix is not a declaration.
            //
            // KNOWN CASE: GEN 1 is already measured at 1218 in the test above, by the same
            // differential method. This sweep must reproduce that figure at GEN 1 or it is not
            // measuring the same thing.
            const auto latencyWith = [this] (const std::function<void (TapeRotAudioProcessor&)>& configure)
            {
                nf::testing::RenderSpec spec;
                spec.blockSize = 512;
                spec.numBlocks = 64;          // 32768 samples — room for GEN 8's ~9600

                const auto renderWith = [&] (bool withImpulse)
                {
                    TapeRotAudioProcessor p;
                    configure (p);

                    nf::testing::RenderSpec warmSpec;
                    warmSpec.blockSize = spec.blockSize;
                    warmSpec.numBlocks = 8;
                    nf::testing::render (p, warmSpec);

                    auto s = spec;
                    s.fillInput = [withImpulse] (juce::AudioBuffer<float>& buffer, int blockIndex)
                    {
                        buffer.clear();

                        if (withImpulse && blockIndex == 0)
                            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                                buffer.setSample (ch, 0, 1.0f);
                    };

                    return nf::testing::render (p, s);
                };

                const auto a = renderWith (true);
                const auto b = renderWith (false);

                for (size_t i = 0; i < a[0].size() && i < b[0].size(); ++i)
                    if (std::abs ((double) a[0][i] - b[0][i]) > 1.0e-4)
                        return (int) i;

                return -1;
            };

            const auto setPhysical = [] (TapeRotAudioProcessor& p, const char* id, float value)
            {
                if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                    param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
            };

            // --- GEN, the predicted variable ------------------------------------------------
            std::vector<int> byGen;

            for (float gen : { 1.0f, 4.0f, 8.0f })
            {
                const auto measured = latencyWith ([&] (TapeRotAudioProcessor& p) { setPhysical (p, ParamIDs::gen, gen); });
                byGen.push_back (measured);

                logMessage ("  GEN " + juce::String (gen, 0) + " -> " + juce::String (measured)
                                + " samples, " + juce::String (measured * 1000.0 / 48000.0, 1) + " ms"
                                + "   (25 ms x GEN predicts " + juce::String ((int) (gen * 1200)) + ")");
            }

            // --- the model set, predicted irrelevant ----------------------------------------
            int modelLo = 1 << 30, modelHi = -1;

            for (float model : { 0.0f, 2.0f, 5.0f, 8.0f })
            {
                const auto measured = latencyWith ([&] (TapeRotAudioProcessor& p) { setPhysical (p, ParamIDs::model, model); });
                modelLo = juce::jmin (modelLo, measured);
                modelHi = juce::jmax (modelHi, measured);
                logMessage ("  model " + juce::String (model, 0) + " -> " + juce::String (measured) + " samples");
            }

            // --- wow and flutter extremes, predicted irrelevant -----------------------------
            int modLo = 1 << 30, modHi = -1;

            for (float depth : { 0.0f, 100.0f })
            {
                const auto measured = latencyWith ([&] (TapeRotAudioProcessor& p)
                {
                    setPhysical (p, ParamIDs::wow, depth);
                    setPhysical (p, ParamIDs::flutter, depth);
                });

                modLo = juce::jmin (modLo, measured);
                modHi = juce::jmax (modHi, measured);
                logMessage ("  wow+flutter " + juce::String (depth, 0) + "% -> "
                                + juce::String (measured) + " samples");
            }

            logMessage ("  spread across the model set -> " + juce::String (modelHi - modelLo)
                            + " samples; across wow/flutter -> " + juce::String (modHi - modLo));

            // **GEN must move it, or the mechanism is wrong.** This is the arm that can refute the
            // whole reading, and it is asserted first for that reason.
            expectGreaterThan (byGen[2] - byGen[0], 6000,
                               "GEN 8 did not carry ~8x GEN 1's latency, so the 25 ms-per-stage "
                               "mechanism is not what produces this figure: GEN 1 "
                                   + juce::String (byGen[0]) + ", GEN 8 " + juce::String (byGen[2]));

            // Reported with a loose bar: the model set changes the EQ, so a few samples of movement
            // in where energy first clears the threshold is expected and is not the centre delay.
            expectLessThan (modelHi - modelLo, 64,
                            "the model set moved the latency by " + juce::String (modelHi - modelLo)
                                + " samples, so the centre delay is not fixed and the fix is not a "
                                  "declaration");

            // **THIS ASSERTION ASKED THE WRONG QUESTION AND THE MEASUREMENT SAID SO.** It was
            // written as "wow/flutter must not move the latency", and wow/flutter moved it by 151
            // samples — 1201 at zero depth, 1352 at full. That is not a defect: a wow/flutter delay
            // line modulating its delay IS the effect, and the impulse's first arrival necessarily
            // rides that modulation.
            //
            // The declarable figure is the CENTRE, and the centre is what zero depth measures. So
            // the bar is on zero depth against the per-stage prediction, not on the spread — and
            // the spread is reported because a host cannot declare a moving number, which is a fact
            // about what the declaration must say rather than a fault to fix.
            logMessage ("  NOTE: the spread across wow/flutter is the modulation itself. The "
                        "declarable figure is the centre, measured at zero depth: "
                            + juce::String (modLo) + " samples.");

            expectWithinAbsoluteError (modLo, 1200, 32,
                                       "at zero modulation depth the latency should be exactly one "
                                       "stage's 25 ms centre delay — 1200 samples at 48 kHz. It is "
                                       + juce::String (modLo) + ", so the centre is not where "
                                       "WowFlutter::nominalDelayMs says it is.");
        }

        beginTest ("Declared latency against an impulse");
        {
            TapeRotAudioProcessor processor;

            nf::testing::RenderSpec spec;
            spec.blockSize = 512;
            spec.numBlocks = 32;

            // An impulse, not the default noise: the question is WHERE energy first emerges.
            // **`measureImpulseLatency` cannot be used on a casting that GENERATES**, and TapeRot
            // is one. Its noise bed and hum are above any sensible detection threshold at every
            // sample, so "the first output above threshold" is sample 0 whatever the latency is —
            // the warmed run reported exactly that, 0 against a declared 4, and it would have read
            // as a 4-sample over-declaration.
            //
            // So measure DIFFERENTIALLY: render the impulse, render silence, subtract. Everything
            // the plugin generates on its own is deterministic and seeded, so it cancels exactly,
            // and what remains is the impulse's own response. On a casting that generates nothing
            // the silent render is zero and this reduces to the original measurement.
            //
            // (This belongs in core beside measureImpulseLatency rather than in six copies. It is
            // here because moving it costs a tag move and six repins mid-category; the six copies
            // are generated from one template, so they are identical by construction rather than by
            // discipline. Recorded so it is moved when the harness is next touched.)
            const auto renderWith = [&] (bool withImpulse)
            {
                TapeRotAudioProcessor p;

                nf::testing::RenderSpec warmSpec;
                warmSpec.blockSize = spec.blockSize;
                warmSpec.numBlocks = 8;
                nf::testing::render (p, warmSpec);      // spend any first-run state — see category 3

                auto s = spec;
                s.fillInput = [withImpulse] (juce::AudioBuffer<float>& buffer, int blockIndex)
                {
                    buffer.clear();

                    if (withImpulse && blockIndex == 0)
                        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                            buffer.setSample (ch, 0, 1.0f);
                };

                return nf::testing::render (p, s);
            };

            const auto withImpulse = renderWith (true);
            const auto silent      = renderWith (false);

            int measured = -1;
            double impulsePeak = 0.0;

            for (size_t i = 0; i < withImpulse[0].size() && i < silent[0].size(); ++i)
            {
                const double d = std::abs ((double) withImpulse[0][i] - silent[0][i]);
                impulsePeak = juce::jmax (impulsePeak, d);

                if (measured < 0 && d > 1.0e-4)
                    measured = (int) i;
            }

            TapeRotAudioProcessor reference;
            reference.setRateAndBufferSizeDetails (spec.sampleRate, spec.blockSize);
            reference.prepareToPlay (spec.sampleRate, spec.blockSize);
            const int declared = reference.getLatencySamples();

            logMessage ("  impulse response peak " + juce::String (impulsePeak, 6)
                            + " — if that is 0 the two renders are identical and nothing was measured");

            logMessage ("  declared " + juce::String (declared) + " samples, impulse emerged at "
                            + juce::String (measured)
                            + (measured < 0 ? "  (NOTHING EMERGED)" : ""));

            if (measured >= 0 && declared >= 0)
                logMessage ("  difference -> " + juce::String (measured - declared)
                                + " samples ("
                                + juce::String ((measured - declared) * 1000.0 / spec.sampleRate, 3)
                                + " ms)");

            expect (impulsePeak > 1.0e-4,
                    "the impulse produced no measurable response at all, so the latency figure "
                    "below is not a measurement of anything");

            expect (measured >= 0,
                    "no impulse emerged at all within " + juce::String (spec.blockSize * spec.numBlocks)
                        + " samples, so this casting produced nothing to measure latency from");

            // **A tolerance, and it is deliberately tight.** Latency is an integer contract with the
            // host; a few samples of disagreement is still a few samples of misalignment on every
            // track in the session. The band exists only for a first output sample that is genuinely
            // tiny rather than exactly zero.
            expectWithinAbsoluteError (measured, declared, 8,
                                       "the impulse did not emerge where the declared latency says "
                                       "it would. A host aligns by the declaration, so this is "
                                       "session-wide misalignment, not a local artefact.");
        }
    }
};

static AutomationLatencyTests automationLatencyTests;
