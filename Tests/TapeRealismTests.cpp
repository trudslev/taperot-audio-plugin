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
                logMessage ("  GEN   noise dBFS    pitch cents pk   cents rms    HF 5k      10k       15k");

            for (float gen = 1.0f; gen <= 8.0f; gen += 1.0f)
            {
                // --- noise floor: silence in ------------------------------------------------
                double noisePeak = 0.0;
                {
                    TapeRotAudioProcessor p;
                    configure (p, gen);

                    nf::testing::RenderSpec spec;
                    spec.blockSize = 512;
                    spec.numBlocks = 48;
                    spec.fillInput = [] (juce::AudioBuffer<float>& b, int) { b.clear(); };

                    for (const auto& ch : nf::testing::render (p, spec))
                        for (float v : ch)
                            noisePeak = juce::jmax (noisePeak, (double) std::abs (v));
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
                TapeRotAudioProcessor p;
                configure (p, gen);
                const auto rows = nf::testing::measureProcessorMagnitudeResponse (p, 48000.0, 512, hfProbes);

                const double centsRms = centsCount > 0 ? std::sqrt (centsSumSq / (double) centsCount) : 0.0;

                logMessage ("   " + juce::String ((int) gen)
                                + "   " + juce::String (20.0 * std::log10 (juce::jmax (1.0e-9, noisePeak)), 1).paddedLeft (' ', 8)
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
