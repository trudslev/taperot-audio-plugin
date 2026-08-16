#include "../Source/PluginProcessor.h"
#include "../Source/Parameters.h"
#include "../Source/DSP/WowFlutter.h"
#include "../Source/DSP/DegradationCore.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Two read-only measurements. Nothing here changes behaviour; the point is to turn two design
    questions into numbers so they can be decided rather than argued.
*/
class TapeRealismTests final : public juce::UnitTest
{
public:
    TapeRealismTests() : juce::UnitTest ("Tape realism", "DSP") {}

    void runTest() override
    {
        beginTest ("Read-pointer excursion at maximum wow — is the 25 ms centre load-bearing?");
        {
            // `nominalDelayMs = 25` per active GEN stage is what puts GEN 8 at ~200 ms of latency.
            // The question is whether that headroom is USED or was set for safety and never
            // revisited: if maximum wow only moves the pointer a few ms, most of the centre is
            // unused and cutting it would take GEN 8 from ~200 ms to something far smaller without
            // touching GEN's range or the sound.
            //
            // **Wow, not flutter, and this is the worst case by construction.** Pitch shift is the
            // RATE OF CHANGE of delay, so a slow oscillation needs far more excursion than a fast
            // one for the same deviation. Wow here is fixed at 0.5 Hz — there is no slower setting,
            // so this is the floor-setting measurement.
            //
            // **Measured, not read off maxWowMs.** An impulse's arrival offset IS the read pointer's
            // position at that instant, so a train of impulses spaced across a full wow cycle traces
            // the excursion directly. The constant says 15; whether the code reaches it is the
            // question, and a constant is a claim like any other.
            constexpr double fs = 48000.0;
            constexpr int blockSize = 512;
            constexpr int spacing = 2048;                 // impulses per probe
            constexpr int probes = 64;                    // 64 * 2048 = 131072 samples = 2.7 s > one 0.5 Hz cycle

            const juce::dsp::ProcessSpec spec { fs, (juce::uint32) blockSize, 2 };

            const auto excursionMs = [&] (float wowDepth, float flutterDepth)
            {
                WowFlutter wf;
                wf.prepare (spec);

                double lo = 1.0e9, hi = -1.0e9;
                juce::AudioBuffer<float> buffer (2, blockSize);

                // One impulse per `spacing` samples; the arrival offset within the following window
                // is the delay in samples at that moment.
                std::vector<float> stream;
                stream.reserve ((size_t) (spacing * probes));

                int absolute = 0;

                for (int block = 0; block < spacing * probes / blockSize; ++block)
                {
                    buffer.clear();

                    for (int i = 0; i < blockSize; ++i)
                        if (((absolute + i) % spacing) == 0)
                            for (int ch = 0; ch < 2; ++ch)
                                buffer.setSample (ch, i, 1.0f);

                    wf.process (buffer, wowDepth, flutterDepth);

                    for (int i = 0; i < blockSize; ++i)
                        stream.push_back (buffer.getSample (0, i));

                    absolute += blockSize;
                }

                // Each impulse was emitted at k*spacing; find where its energy peaks after that.
                for (int k = 1; k < probes - 1; ++k)
                {
                    const int from = k * spacing;
                    double best = 0.0;
                    int at = -1;

                    for (int i = from; i < juce::jmin ((int) stream.size(), from + spacing); ++i)
                        if (std::abs ((double) stream[(size_t) i]) > best)
                        {
                            best = std::abs ((double) stream[(size_t) i]);
                            at = i - from;
                        }

                    if (at >= 0 && best > 1.0e-3)
                    {
                        lo = juce::jmin (lo, (double) at);
                        hi = juce::jmax (hi, (double) at);
                    }
                }

                return std::make_pair (lo * 1000.0 / fs, hi * 1000.0 / fs);
            };

            const auto atZero = excursionMs (0.0f, 0.0f);
            const auto atMaxWow = excursionMs (1.0f, 0.0f);
            const auto atMaxBoth = excursionMs (1.0f, 1.0f);

            logMessage ("  wow 0%,   flutter 0%   -> delay " + juce::String (atZero.first, 2)
                            + " .. " + juce::String (atZero.second, 2) + " ms");
            logMessage ("  wow 100%, flutter 0%   -> delay " + juce::String (atMaxWow.first, 2)
                            + " .. " + juce::String (atMaxWow.second, 2) + " ms, peak-to-peak "
                            + juce::String (atMaxWow.second - atMaxWow.first, 2) + " ms");
            logMessage ("  wow 100%, flutter 100% -> delay " + juce::String (atMaxBoth.first, 2)
                            + " .. " + juce::String (atMaxBoth.second, 2) + " ms, peak-to-peak "
                            + juce::String (atMaxBoth.second - atMaxBoth.first, 2) + " ms");

            // **Only the DOWNWARD excursion binds the centre**, and the first version of this line
            // took the larger of the two directions, which is wrong. Delay above the centre needs
            // BUFFER (maxDelayMs = 55), not centre; delay below it is what would read from the
            // future if the centre were too small. So the constraint is the minimum delay reached.
            const double bindingMs = atMaxBoth.first;
            logMessage ("  => the binding constraint is the MINIMUM delay, " + juce::String (bindingMs, 2)
                            + " ms. The centre is 25.00, so " + juce::String (bindingMs, 2)
                            + " ms of it is never used — a centre of ~" + juce::String (25.0 - bindingMs, 1)
                            + " ms plus margin would hold this excursion, taking GEN 8 from ~200 ms to ~"
                            + juce::String ((25.0 - bindingMs) * 8.0, 0) + " ms.");

            // The zero-depth arm is the control: with no modulation the delay must sit still at the
            // centre, and a spread there would mean the instrument is measuring something else.
            expectLessThan (atZero.second - atZero.first, 0.1,
                            "the read pointer moved with wow and flutter both at zero, so this "
                            "measurement is not tracing the modulation");
        }

        beginTest ("THE WEIGHT VECTOR, measured per stage — what the accumulation gap actually is");
        {
            /*  **One run that produces the corrected weights rather than rejecting a hypothesis.**

                CASSETTE I accumulates +2.2 dB across GEN 1..8 against a computed weighted power sum
                of +4.56. Three candidates are refuted — peak-against-power (the rms reads LOWER
                than the peak), the +9 dB target itself (withdrawn: these are not eight equal
                independent sources), and the cascade saturation (made transparent, and the column
                came back identical to the printed digit, which `tanh` being linear 40 dB down says
                it had to).

                `noiseAmount01` is a per-CALL argument, so a hand-built cascade can enable hiss at
                exactly one stage and read what arrives. Eight runs give the weight vector directly
                instead of a fourth explanation.

                **The shape of the disagreement names the cause:**

                  - every weight scaled by one factor -> the BASE is wrong: the eight stages do not
                    inject at equal level. `NoiseSource` takes a per-stage seed, and the level comes
                    from one shared `noiseAmount01` — an assumption, never measured.
                  - decays faster than predicted -> the per-stage surviving fraction is
                    over-modelled. Either the hiss spectrum (modelled as a single 2 kHz one-pole
                    from `tapeHighpassHz`) or a per-stage attenuation missing from the model. The
                    leading one is the WOW DELAY LINE: every later stage's fractional-delay
                    interpolator is a lowpass, worst near Nyquist, and the hiss is high-passed at
                    2 kHz so it sits exactly where that bites. Eight stages compound it, and it
                    suppresses the EARLIER generations specifically.
                  - only the eighth is off -> the cascade blends floor/ceil chains at integer GEN,
                    which the stage-1 subdivision makes worth confirming rather than assuming.

                Stage index 7 is the last generation: its hiss passes through nothing after it and
                is 1.000 by construction, so it is the normaliser rather than a measurement. */
            const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
            constexpr int totalStages = 8;
            constexpr int model = 5;                 // CASSETTE I, matching the accumulation table

            const auto powerFromStage = [&spec] (int liveStage, float depth)
            {
                std::vector<std::unique_ptr<DegradationCore>> stages;

                for (int i = 0; i < totalStages; ++i)
                {
                    stages.push_back (std::make_unique<DegradationCore> (i));
                    stages.back()->prepare (spec, model, i == liveStage ? 1.0f : 0.0f);
                }

                juce::AudioBuffer<float> buffer (2, 512);
                double sumSq = 0.0;
                juce::int64 n = 0;

                for (int block = 0; block < 48; ++block)
                {
                    buffer.clear();                  // silence in: the output IS the hiss

                    for (int i = 0; i < totalStages; ++i)
                        stages[(size_t) i]->process (buffer, depth, depth, model, false,
                                                     i == liveStage ? 1.0f : 0.0f, NoiseSource::tape);

                    if (block < 16)
                        continue;                    // let the smoothers and delay lines fill

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 512; ++i)
                        {
                            const double v = buffer.getSample (ch, i);
                            sumSq += v * v;
                            ++n;
                        }
                }

                return n > 0 ? sumSq / (double) n : 0.0;
            };

            const std::array<double, 8> predicted { 0.132, 0.152, 0.178, 0.214, 0.269, 0.363, 0.554, 1.000 };

            /*  **Two depths, and the second is what names the missing term.**

                `WowFlutter::centerDelayMs` is 25 ms, which at 48 kHz is exactly 1200 samples — an
                INTEGER. So at zero wow and flutter the fractional-delay interpolator sits at offset
                zero and is transparent; driven, it sits at a moving fractional offset and is a
                lowpass whose loss is worst near Nyquist. The hiss is high-passed at 2 kHz, so it
                lives exactly where that bites.

                A per-stage attenuation nowhere in the weight model, and it acts only on the
                generations that have later stages to pass through. If the driven and undriven
                weight vectors differ, the interpolator is the term; if they agree, it is not and
                something else is. */
            std::array<double, 8> driven {}, still {};

            for (int k = 0; k < totalStages; ++k)
            {
                driven[(size_t) k] = powerFromStage (k, 1.0f);
                still[(size_t) k]  = powerFromStage (k, 0.0f);
            }

            const double normD = driven[7] > 0.0 ? driven[7] : 1.0;
            const double normS = still[7]  > 0.0 ? still[7]  : 1.0;

            logMessage ("  stage later  driven  ratio    still  ratio   predicted");

            double drivenSum = 0.0, stillSum = 0.0, predictedSum = 0.0;

            for (int k = 0; k < totalStages; ++k)
            {
                const double fd = driven[(size_t) k] / normD;
                const double fs = still[(size_t) k] / normS;
                const double pr = predicted[(size_t) k];

                drivenSum += fd; stillSum += fs; predictedSum += pr;

                logMessage ("    " + juce::String (k)
                                + juce::String (7 - k).paddedLeft (' ', 6)
                                + juce::String (fd, 4).paddedLeft (' ', 9)
                                + juce::String (fd / pr, 3).paddedLeft (' ', 7)
                                + juce::String (fs, 4).paddedLeft (' ', 9)
                                + juce::String (fs / pr, 3).paddedLeft (' ', 7)
                                + juce::String (pr, 4).paddedLeft (' ', 11));
            }

            logMessage ("  sums: driven " + juce::String (drivenSum, 4)
                            + " (" + juce::String (10.0 * std::log10 (juce::jmax (1.0e-9, drivenSum)), 2)
                            + " dB), still " + juce::String (stillSum, 4)
                            + " (" + juce::String (10.0 * std::log10 (juce::jmax (1.0e-9, stillSum)), 2)
                            + " dB), predicted " + juce::String (predictedSum, 4)
                            + " (" + juce::String (10.0 * std::log10 (predictedSum), 2) + " dB)");

            const auto measured = driven;

            // Reported, not asserted: this is a measurement that produces the corrected weights.
            // Pinning any of these figures would be pinning numbers nobody has explained yet.
            expect (measured[7] > 0.0, "the last stage produced no hiss, so every ratio above is "
                                        "divided by nothing and the run says nothing");
        }

        beginTest ("HF loss per MODEL across GEN 1..8 — the whole curve, not its endpoint");
        {
            /*  **An endpoint does not say whether a control is usable along its length.** GEN 8 at
                -11 dB of 5 kHz is a defensible destination — eight generations of cassette dubbing
                is murky, and that is the sound people reach for — but the setting that gets used is
                GEN 3 or 4, and nothing about the endpoint predicts those. This is the same question
                that made the factory Programs need rewriting the first time: a range that is right
                at both ends and wrong between them reads as a broken control.

                **Per model, because the loss is per model.** Generation loss is the dimension
                machines differ on most audibly, so a single curve would say nothing about the
                control that actually selects between them. */
            const std::vector<double> hfProbes { 5000.0, 10000.0, 15000.0 };

            for (int model = 1; model < (int) kNumTapeModels; ++model)
            {
                logMessage ("  " + juce::String (kTapeModels[(size_t) model].displayName)
                                + "  (-3 dB per copy at "
                                + juce::String (kTapeModels[(size_t) model].generationLossHz, 0)
                                + " Hz)");
                logMessage ("    GEN     5k       10k       15k     noise rms");

                for (float gen = 1.0f; gen <= 8.0f; gen += 1.0f)
                {
                    TapeRotAudioProcessor p;
                    configureForResponse (p, gen, model);

                    const auto rows = nf::testing::measureProcessorMagnitudeResponse (p, 48000.0, 512, hfProbes);

                    /*  **Noise rms per model, which is what settles the accumulation shortfall.**

                        Step 1 measured +2.6 dB peak across eight generations against a predicted
                        +9, and the rms came back at +2.3 — LOWER — so the peak-against-power
                        hypothesis is refuted rather than confirmed. The remaining candidate is that
                        the prediction counted only the generation loss: an earlier generation's
                        hiss passes through every later stage's MODEL EQ as well, and that was not
                        in the integration.

                        If that is the cause, accumulation must vary BY MODEL and in the direction
                        of each machine's total HF cut. A model-independent figure refutes it. */
                    double noiseSumSq = 0.0;
                    juce::int64 noiseCount = 0;
                    {
                        TapeRotAudioProcessor np;
                        configure (np, gen);
                        if (auto* mp = dynamic_cast<juce::RangedAudioParameter*> (np.apvts.getParameter (ParamIDs::model)))
                            mp->setValueNotifyingHost (mp->getNormalisableRange().convertTo0to1 ((float) model));

                        nf::testing::RenderSpec ns;
                        ns.blockSize = 512;
                        ns.numBlocks = 32;
                        ns.fillInput = [] (juce::AudioBuffer<float>& b, int) { b.clear(); };

                        for (const auto& ch : nf::testing::render (np, ns))
                            for (float v : ch)
                            {
                                noiseSumSq += (double) v * v;
                                ++noiseCount;
                            }
                    }

                    const double nrms = noiseCount > 0 ? std::sqrt (noiseSumSq / (double) noiseCount) : 0.0;

                    logMessage ("     " + juce::String ((int) gen)
                                    + juce::String (rows[0].gainDb, 2).paddedLeft (' ', 9)
                                    + juce::String (rows[1].gainDb, 2).paddedLeft (' ', 10)
                                    + juce::String (rows[2].gainDb, 2).paddedLeft (' ', 10)
                                    + juce::String (20.0 * std::log10 (juce::jmax (1.0e-9, nrms)), 1).paddedLeft (' ', 11));
                }
            }
        }

        beginTest ("The generation accumulation law — noise, pitch and HF across GEN 1..8");
        {
            // Three curves, everything else fixed. What compounds and what should:
            //
            //   NOISE     independent sources power-sum, so eight of them is ~+9 dB, not +20.
            //   PITCH     independent transport instabilities RMS-sum, so eight is sqrt(8) = 2.8x,
            //             not 8x.
            //   HF LOSS   SHOULD compound multiplicatively — a deck 3 dB down at 14 kHz is 24 dB
            //             down after eight passes. This is the one that is supposed to run away.
            //
            // If noise and pitch compound while HF stays shallow, the plugin is spending its
            // generation budget on the two things that should not compound and under-spending on
            // the one that should. Reported, not asserted: whether TapeRot models tape or exceeds
            // it is a design decision, and these numbers are what make it one.
            const std::vector<double> hfProbes { 5000.0, 10000.0, 15000.0 };

            /*  **Peak AND rms, because the law predicts one of them and the column measured the other.**

                Independent transport instabilities RMS-sum, so eight of them is sqrt(8) = 2.83x. A
                PEAK does not obey that: eight sinusoids at similar rates drift in and out of
                alignment, and over a window a peak catches the moments they align, which is why the
                peak column reads 5.48x where the rms reads what the law says. Neither is wrong —
                the peak is what a listener meets at the worst moment and the rms is the quantity
                the accumulation law is about.

                **The metric was the thing that could not express the answer**, not the code. Tuning
                the rate spread until the PEAK read 2.83 would have been fitting a control to a
                number, and would have made the plugin less like eight transports rather than more. */
                logMessage ("  GEN  noise pk  noise rms   pitch cents pk   cents rms    HF 5k      10k       15k");

            for (float gen = 1.0f; gen <= 8.0f; gen += 1.0f)
            {
                // --- noise floor: silence in ------------------------------------------------
                /*  **Peak AND rms, because the LAW is a power sum and the column measured a peak.**

                    Step 1 measured +2.6 dB of noise accumulation across eight generations against a
                    predicted +9. The hiss ordering was confirmed correct by reading — the hiss goes
                    in after its own stage's loss, so generation 8 arrives full-bandwidth and
                    generation 1 passes through the seven losses after it — and integrating the
                    one-pole cascade over the band gives +5.95 dB for white noise, +5.3 weighted by
                    the hiss's own 2 kHz highpass. Still well above what was measured.

                    A peak is not a power. Independent sources power-sum; a peak of a sum of
                    independent noises is a sample of an extreme-value distribution, and cascading
                    low-passes makes the noise progressively more correlated sample-to-sample, which
                    changes the peak-to-rms ratio as well as the power. So the column could not
                    express the quantity the law is about — the third arm in this stage with that
                    shape, after the pitch peak and the HF column reading hiss.

                    Both are reported. The rms is the law's own quantity; the peak is what a limiter
                    downstream would meet.

                    ## The +9 dB target is WITHDRAWN, and what replaces it

                    +9 was the prediction for eight EQUAL, independent, unmodified sources. These are
                    not equal: generation 8's hiss passes through nothing and generation 1's through
                    seven model EQs, seven per-stage makeups and seven generation losses. The number
                    to compare against is a per-model WEIGHTED power sum — each generation's hiss
                    power weighted by the net transfer of the stages after it, integrated over the
                    hiss spectrum.

                    Computed for CASSETTE I (HighShelf 14 k -2.5 dB, Peak 60 Hz +1.5 dB, makeup
                    -0.11 dB, loss 14 k, hiss highpassed at 2 kHz): surviving fractions 1.000, 0.554,
                    0.363, 0.269, 0.214, 0.178, 0.152, 0.132 — a weighted sum of 2.86, or
                    **+4.56 dB**. Measured **+2.2 dB**. A 2.36 dB gap, and it is real rather than a
                    wrong target.

                    ## The saturation candidate is REFUTED, and could not have worked

                    Each generation's hiss is injected after its own stage's soft clipper and passes
                    through every later one, and eight stages of compression on an accumulating floor
                    would suppress it in exactly this direction. Probed by making the cascade
                    saturation transparent (`gentleSaturationDrive` to 0.001, so `tanh(x*d)/d` is
                    linear) and re-running: the noise column came back **identical at every
                    generation to the printed digit** — -40.7 / -39.5 / -39.1 / -38.7 / -38.7 / -38.5
                    / -38.5 / -38.4, unchanged.

                    **And the reason is arithmetic that was available before the run**: `tanh` is
                    linear for small arguments, the hiss sits 40 dB down, so `tanh(0.0135)/1.35`
                    differs from unity by 6 parts in 100000. A soft clipper cannot compress a signal
                    that never leaves its linear region. The candidate fitted the DIRECTION of the
                    gap, which is what made it worth probing and also what makes it a warning: an
                    explanation that merely fits is what produced the /tanh exclusion earlier in this
                    stage.

                    The gap is open. It is not the metric, not the target, and not the saturation. */
                double noisePeak = 0.0, noiseSumSq = 0.0;
                juce::int64 noiseCount = 0;
                {
                    TapeRotAudioProcessor p;
                    configure (p, gen);

                    nf::testing::RenderSpec spec;
                    spec.blockSize = 512;
                    spec.numBlocks = 48;
                    spec.fillInput = [] (juce::AudioBuffer<float>& b, int) { b.clear(); };

                    for (const auto& ch : nf::testing::render (p, spec))
                        for (float v : ch)
                        {
                            noisePeak = juce::jmax (noisePeak, (double) std::abs (v));
                            noiseSumSq += (double) v * v;
                            ++noiseCount;
                        }
                }

                // --- pitch deviation -----------------------------------------------------------
                // **This column was NOT MEASURED on its first run and the numbers said so.** Every
                // stage was default-constructed, so all shared one seed offset, ran identical LFOs
                // and were perfectly correlated — which sums linearly whatever the plugin does. The
                // tell was in the output: 2.00x, 3.00x ... 8.00x to two decimals, and independent
                // sources cannot produce exact integers. A result more regular than its mechanism
                // permits is a result about the fixture.
                //
                // **And "seed each stage" is NOT the fix, which this measurement already shows.**
                // Seeded per stage it reads 7.96x against a correlated 8.00x — barely moved,
                // because WowFlutter::wowRateHz is a static constexpr 0.5f shared by every stage.
                // Two oscillators at exactly the same frequency with different phases do not
                // decorrelate over time: they hold a fixed phase relationship and beat, so their
                // sum still adds rather than RMS-summing.
                //
                // Real transports differ in RATE as well as phase — capstan diameter, motor
                // regulation, belt wear. So the fix is a per-stage rate drawn around a nominal, and
                // only rate variation makes the stages genuinely independent and produces sqrt(N).
                // Recorded here because the obvious remedy is the seeding one and it is measured
                // not to work.
                double cents = 0.0, centsSumSq = 0.0;
                juce::int64 centsCount = 0;
                {
                    const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
                    // **Seeded PER STAGE, as the processor seeds them.** The first version
                    // default-constructed every stage, so all of them shared one seed offset, ran
                    // identical LFOs and summed linearly by construction — see the note above.
                    std::vector<WowFlutter> stages;
                    stages.reserve ((size_t) (int) gen);

                    /*  **`wowRateMultiplierFor (i)`, and this argument was hard-coded to 1.0f.**

                        The fixture rebuilds what `DegradationCore`'s constructor does, and it got
                        one of the two arguments wrong: the seed was corrected once already — the
                        earlier note above records that every stage used to be default-constructed —
                        and the RATE multiplier beside it was left at a literal. So this column
                        measured eight transports running at identical rates, which is a
                        configuration the plugin does not have.

                        It happened to be right while the processor's own spread was 1.3 % per stage,
                        because 1.3 % does not decorrelate a 0.5 Hz oscillation either. It stopped
                        being right the moment the spread became real, and it reported the old figure
                        unchanged to two decimal places — a fixture agreeing with itself.

                        Read from the function the processor calls, never a transcribed value. */
                    for (int i = 0; i < (int) gen; ++i)
                        stages.emplace_back (i, DegradationCore::wowRateMultiplierFor (i));

                    for (auto& s : stages)
                        s.prepare (spec);

                    juce::AudioBuffer<float> buffer (2, 512);
                    std::vector<float> accum (512, 0.0f);

                    for (int block = 0; block < 200; ++block)
                    {
                        buffer.clear();
                        std::fill (accum.begin(), accum.end(), 0.0f);

                        for (auto& s : stages)
                            s.process (buffer, 1.0f, 1.0f, accum.data());

                        for (float v : accum)
                        {
                            cents = juce::jmax (cents, (double) std::abs (v));
                            centsSumSq += (double) v * v;
                            ++centsCount;
                        }
                    }
                }

                // --- HF response --------------------------------------------------------------
                // Generators OFF — see configureForResponse. With them up this column reads the
                // accumulated hiss wherever the chain has pushed the tone beneath it.
                TapeRotAudioProcessor p;
                configureForResponse (p, gen, 5);
                const auto rows = nf::testing::measureProcessorMagnitudeResponse (p, 48000.0, 512, hfProbes);

                const double centsRms = centsCount > 0 ? std::sqrt (centsSumSq / (double) centsCount) : 0.0;
                const double noiseRms = noiseCount > 0 ? std::sqrt (noiseSumSq / (double) noiseCount) : 0.0;

                logMessage ("   " + juce::String ((int) gen)
                                + "   " + juce::String (20.0 * std::log10 (juce::jmax (1.0e-9, noisePeak)), 1).paddedLeft (' ', 7)
                                + juce::String (20.0 * std::log10 (juce::jmax (1.0e-9, noiseRms)), 1).paddedLeft (' ', 9)
                                + "   " + juce::String (cents, 2).paddedLeft (' ', 12)
                                + "   " + juce::String (centsRms, 4).paddedLeft (' ', 10)
                                + "   " + juce::String (rows[0].gainDb, 2).paddedLeft (' ', 8)
                                + juce::String (rows[1].gainDb, 2).paddedLeft (' ', 10)
                                + juce::String (rows[2].gainDb, 2).paddedLeft (' ', 10));
            }

            expect (true);   // reported, not asserted — see the note above
        }
    }

private:
    /*  **The generators OFF, for any arm measuring a RESPONSE.**

        `configure` drives NOISE, WOW and FLUTTER to 100, which is right for the noise and pitch
        columns and wrong for a magnitude measurement. A swept-tone response reads the output at the
        tone's frequency; once the chain has attenuated the tone below the accumulated hiss, the
        reading is the HISS, and as hiss accumulates with GEN the curve RISES.

        It did, and unmistakably: TOY read -33.47 dB at 15 kHz at GEN 1 and **-6.50 at GEN 8** —
        eight generations of a 4.5 kHz transfer apparently gaining 27 dB of top. DICTAPHONE and
        CAMCORDER did the same. Three arms in this stage have now measured something other than what
        they were named after, each because the metric could not express the question.

        Wow and flutter are off for the same reason from the other side: they modulate the delay
        line, so a steady tone is smeared across bins and its own bin under-reads. */
    static void configureForResponse (TapeRotAudioProcessor& p, float gen, int model)
    {
        const auto set = [&p] (const char* id, float physical)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
        };

        set (ParamIDs::gen, gen);
        set (ParamIDs::noise, 0.0f);
        set (ParamIDs::wow, 0.0f);
        set (ParamIDs::flutter, 0.0f);
        set (ParamIDs::model, (float) model);
    }

    static void configure (TapeRotAudioProcessor& p, float gen)
    {
        const auto set = [&p] (const char* id, float physical)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (physical));
        };

        set (ParamIDs::gen, gen);
        set (ParamIDs::noise, 100.0f);
        set (ParamIDs::wow, 100.0f);
        set (ParamIDs::flutter, 100.0f);
        set (ParamIDs::model, 5.0f);
    }
};

static TapeRealismTests tapeRealismTests;
