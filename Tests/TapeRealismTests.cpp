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
            /*  **256 probes = 10.9 s, about 5.5 wow cycles, and it used to be 64 = 1.4 cycles.**

                One cycle gives one draw. That lesson was learned on the wow cascade, where a 200-block
                window produced an rms ratio of 0.345 against a true 0.984 — but it applies here for a
                second reason as well: wow is 0.7 sine plus 0.3 of a slow random walk, so the
                excursion is not purely periodic and its extreme is not reached every cycle. */
            constexpr int probes = 256;

            const juce::dsp::ProcessSpec spec { fs, (juce::uint32) blockSize, 2 };

            /*  **The SLOWEST stage, not the nominal rate.** Pitch shift is the rate of change of
                delay, so a slower oscillation needs proportionally MORE excursion for the same
                deviation — the slowest transport in the cascade is the worst case and it is what
                sets the floor for any nominal delay. Per-stage rate multipliers are a draw now
                (step 1), so the slowest is found rather than assumed. */
            int slowestStage = 0;
            for (int i = 1; i < 8; ++i)
                if (DegradationCore::wowRateMultiplierFor (i) < DegradationCore::wowRateMultiplierFor (slowestStage))
                    slowestStage = i;

            const auto excursionMs = [&] (float wowDepth, float flutterDepth)
            {
                WowFlutter wf { (juce::uint64) slowestStage,
                                DegradationCore::wowRateMultiplierFor (slowestStage) };
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

            /*  **Only the DOWNWARD excursion binds the centre.** Delay ABOVE the centre needs buffer
                length, which `maxDelayMs` already provides; delay below it is what the centre has to
                be large enough to allow. Conflating the two reported 12.90 ms on the previous run
                where the real constraint was 11.54.

                Stated separately here so the figure that sets `nominalDelayMs` cannot be read off
                the wrong column. */
            const double centreMs = atZero.first;   // measured, not the 25 ms constant transcribed

            logMessage ("  slowest stage " + juce::String (slowestStage) + " at rate x"
                            + juce::String (DegradationCore::wowRateMultiplierFor (slowestStage), 4)
                            + ", window " + juce::String (probes * spacing / fs, 1) + " s ~"
                            + juce::String (probes * spacing / fs * 0.5, 1) + " wow cycles");
            logMessage ("  centre measured " + juce::String (centreMs, 3) + " ms");
            logMessage ("  max wow  -> down " + juce::String (centreMs - atMaxWow.first, 3)
                            + " ms, up " + juce::String (atMaxWow.second - centreMs, 3)
                            + " ms, peak-to-peak " + juce::String (atMaxWow.second - atMaxWow.first, 3) + " ms");
            logMessage ("  max both -> down " + juce::String (centreMs - atMaxBoth.first, 3)
                            + " ms, up " + juce::String (atMaxBoth.second - centreMs, 3)
                            + " ms, peak-to-peak " + juce::String (atMaxBoth.second - atMaxBoth.first, 3) + " ms");
            logMessage ("  => the DOWNWARD figure is what binds a nominal delay; the upward one needs "
                        "buffer length, which maxDelayMs already provides");

            /*  ## THE STEP 3 PREDICTION IS REFUTED, and by a DECISION rather than by an error

                Pre-stated: excursion 0.3-1.7 ms, a 2-3 ms centre, GEN 8 near 20 ms rather than 200.
                Measured: 10.521 ms downward at maximum wow, 12.833 ms with flutter also at maximum.

                **The prediction assumed step 2 would reduce the depth RANGES. It did not.** Step 2
                re-tapered and kept both maxima, on the explicit ruling that 2.75 % wow is a
                warped-record wobble and a legitimate extreme — capping the top to fix the middle
                would have thrown away a sound the plugin can make. At 100 % knob the physical depth
                is still 100 %, so the excursion at maximum is unchanged BY CONSTRUCTION.

                The prediction and the ruling are inconsistent, and the ruling is the later one. This
                is not a measurement that came out differently from expectation; it is a prediction
                written before a decision that invalidated its premise.

                **What the measurement does establish**, and it is not nothing: `centerDelayMs` is 25
                and the binding constraint is 10.52 ms, so the centre carries about 14.5 ms of unused
                headroom at the worst case. Sized to the measurement it could be roughly 12-13 ms —
                halving GEN 8's latency from ~200 ms to ~100 ms, not the tenfold cut the prediction
                assumed.

                **So step 4's ranking does not invert.** At ~100 ms a fixed declared nominal still
                taxes every GEN 1 user with 100 ms, which is the same objection at half the size. The
                held decision stays held on the same reasoning.

                The alternative is sizing the centre for TYPICAL use rather than the maximum — at
                50 % knob the depth is 9.9 % and the excursion about 1 ms, so a 2-3 ms centre is
                reachable if the extreme is clamped. That is a trade between a sound and a latency,
                and it is a decision rather than a measurement. */

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

        beginTest ("THE WOW CASCADE, per stage — 0.91x is below the floor for any accumulation law");
        {
            /*  **Adding sources cannot reduce rms.** Independent ones RMS-sum, correlated ones add,
                and neither gives less than one source alone. WOW at 100 % measured 44.87 cents rms
                at GEN 1 and 41.06 at GEN 8 — a ratio of 0.91, below the floor. So this is not wow
                accumulating differently from flutter; either the later stages contribute nothing or
                the metric is not measuring the chain.

                Driven at exactly one stage at a time, eight runs, the same way the noise weights
                were measured — a measurement that returns the corrected values rather than one that
                rejects a hypothesis.

                  flat contributions  -> the metric reads one stage rather than the chain
                  first stage only    -> wow never reaches the later stages
                  a decaying series   -> something attenuates it per stage

                And the quadrature check beside it: if the eight are independent, all-driven rms
                must equal the root of the sum of their squares. A large shortfall against that says
                they are not merely correlated but CANCELLING, which no accumulation law produces
                either. */
            const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
            constexpr int totalStages = 8;

            const auto deviationRms = [&spec] (int liveStage, int blocks)   // -1 drives every stage
            {
                std::vector<WowFlutter> stages;
                stages.reserve (totalStages);

                for (int i = 0; i < totalStages; ++i)
                    stages.emplace_back (i, DegradationCore::wowRateMultiplierFor (i));

                for (auto& s : stages)
                    s.prepare (spec);

                juce::AudioBuffer<float> buffer (2, 512);
                std::vector<float> accum (512, 0.0f);

                double sumSq = 0.0;
                juce::int64 n = 0;

                for (int block = 0; block < blocks; ++block)
                {
                    buffer.clear();
                    std::fill (accum.begin(), accum.end(), 0.0f);

                    for (int i = 0; i < totalStages; ++i)
                        stages[(size_t) i].process (buffer,
                                                    (liveStage < 0 || i == liveStage) ? 1.0f : 0.0f,
                                                    0.0f, accum.data());

                    if (block < 8)
                        continue;

                    for (float v : accum)
                    {
                        sumSq += (double) v * v;
                        ++n;
                    }
                }

                return n > 0 ? std::sqrt (sumSq / (double) n) : 0.0;
            };

            /*  **Two windows, because the first one cannot answer the question.**

                Wow runs at 0.5 Hz. 200 blocks of 512 at 48 kHz is 2.13 seconds — about ONE wow
                period. Over one period the rms of a sum of eight near-identical-frequency sinusoids
                with independent phases is not its asymptotic value: it is one realisation of a
                random phasor sum, which can land anywhere between zero and eight times a single
                source. Neither 0.91x nor 0.345 is a law; both are one draw.

                **This is the sample-size question, not an accumulation question**, and it is
                answerable from the design rather than from the output: ask whether the window
                contains enough cycles to distinguish the two answers. It does not. Flutter runs at
                ~9.5 Hz and got about twenty cycles in the same window, which is why its 2.18x came
                back clean and looked like a law — the two controls were never measured on comparable
                terms.

                The long window is 3000 blocks, 32 seconds, about 16 wow cycles. */
            for (int blocks : { 200, 3000 })
            {
                logMessage ("  --- window " + juce::String (blocks * 512 / 48000.0, 1) + " s, ~"
                                + juce::String (blocks * 512 / 48000.0 * 0.5, 1) + " wow cycles ---");
                logMessage ("  stage   rate x   deviation rms");

                double quadrature = 0.0;

                for (int k = 0; k < totalStages; ++k)
                {
                    const double r = deviationRms (k, blocks);
                    quadrature += r * r;

                    logMessage ("    " + juce::String (k)
                                    + juce::String (DegradationCore::wowRateMultiplierFor (k), 4).paddedLeft (' ', 10)
                                    + juce::String (r, 3).paddedLeft (' ', 16));
                }

                const double all = deviationRms (-1, blocks);

                logMessage ("  all eight -> " + juce::String (all, 3)
                                + ";  quadrature -> " + juce::String (std::sqrt (quadrature), 3)
                                + ";  ratio " + juce::String (all / juce::jmax (1.0e-9, std::sqrt (quadrature)), 3));

                expect (all > 0.0, "the all-driven arm produced no deviation, so the ratio above is "
                                    "divided by nothing");
            }
        }

        beginTest ("DEPTH RANGES — cents at the knob positions, before any range is touched");
        {
            /*  **Step 2 opens with a measurement, because the ranges may already be right.**

                The original complaint was that wow and flutter over five generations were
                unlistenable, which is what made the factory Programs need rewriting. That was
                ACCUMULATED deviation, and step 1 already cut it from 7.96x to 2.17x rms. The
                single-generation range was never the thing that was wrong.

                **And the taper is doing work a range edit would undo.** Read off the shipping
                parameters rather than transcribed: WOW is 0-100 LINEAR, FLUTTER is 0-100 with skew
                0.2. So half travel is 50 % of range on WOW and 100 * 0.5^5 = 3.1 % on FLUTTER —
                the two controls are not comparable at the same knob position, and only FLUTTER has
                the deliberate crazy-at-the-top shape.

                Judged against real transports, where cents ~ 1731 x fractional deviation:

                  good cassette deck   0.05-0.08 % WRMS   ~0.9-1.4 cents
                  average portable     0.1-0.2 %          ~1.7-3.5
                  worn or cheap        0.3-0.5 %          ~5-9
                  dying transport      past 1 %           17+

                The question is whether the MIDDLE of the travel lands on plausible hardware at
                GEN 1 and stays musical at GEN 8. If both hold, step 2 is a no-op — which is a
                result to report as one rather than a range edit nobody needed. */
            const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };

            TapeRotAudioProcessor ref;   // the shipping ranges, as the declaration they are

            const auto physicalFor = [&ref] (const char* id, float knob)
            {
                if (auto* q = dynamic_cast<juce::RangedAudioParameter*> (ref.apvts.getParameter (id)))
                    return q->getNormalisableRange().convertFrom0to1 (knob);
                return 0.0f;
            };

            const auto deviation = [&spec] (int gen, float wow01, float flutter01)
            {
                std::vector<WowFlutter> stages;
                stages.reserve ((size_t) gen);

                for (int i = 0; i < gen; ++i)
                    stages.emplace_back (i, DegradationCore::wowRateMultiplierFor (i));

                for (auto& s : stages)
                    s.prepare (spec);

                juce::AudioBuffer<float> buffer (2, 512);
                std::vector<float> accum (512, 0.0f);

                double peak = 0.0, sumSq = 0.0;
                juce::int64 n = 0;

                /*  **1500 blocks = 16 s, about EIGHT wow cycles, and 200 was not enough.**

                    Wow runs at 0.5 Hz, so the 200 blocks this used to run was 2.1 seconds — 1.1
                    cycles. Over one cycle the rms of a sum of near-identical-frequency sinusoids
                    with independent phases is one realisation of a random phasor sum, not its
                    asymptotic value, so every wow figure measured that way was a single draw.
                    Flutter at ~9.5 Hz got twenty cycles in the same window and was fine, which is
                    why only the wow rows looked strange. */
                for (int block = 0; block < 1500; ++block)
                {
                    buffer.clear();
                    std::fill (accum.begin(), accum.end(), 0.0f);

                    for (auto& s : stages)
                        s.process (buffer, wow01, flutter01, accum.data());

                    if (block < 8)
                        continue;                    // the delay lines fill

                    for (float v : accum)
                    {
                        peak = juce::jmax (peak, (double) std::abs (v));
                        sumSq += (double) v * v;
                        ++n;
                    }
                }

                return std::pair<double, double> { n > 0 ? std::sqrt (sumSq / (double) n) : 0.0, peak };
            };

            struct Control { const char* id; const char* label; bool isWow; };
            const Control controls[] = { { ParamIDs::wow, "WOW    ", true },
                                         { ParamIDs::flutter, "FLUTTER", false } };

            logMessage ("  control  knob   physical    GEN 1 rms    GEN 1 pk    GEN 8 rms    GEN 8 pk");

            for (const auto& c : controls)
                for (float knob : { 0.25f, 0.50f, 0.75f, 1.00f })
                {
                    const float physical = physicalFor (c.id, knob);
                    const float v = physical / 100.0f;

                    const auto g1 = deviation (1, c.isWow ? v : 0.0f, c.isWow ? 0.0f : v);
                    const auto g8 = deviation (8, c.isWow ? v : 0.0f, c.isWow ? 0.0f : v);

                    logMessage ("  " + juce::String (c.label)
                                    + juce::String ((int) (knob * 100.0f)).paddedLeft (' ', 6) + "%"
                                    + juce::String (physical, 1).paddedLeft (' ', 9) + "%"
                                    + juce::String (g1.first, 2).paddedLeft (' ', 13)
                                    + juce::String (g1.second, 2).paddedLeft (' ', 12)
                                    + juce::String (g8.first, 2).paddedLeft (' ', 13)
                                    + juce::String (g8.second, 2).paddedLeft (' ', 12));
                }

            logMessage ("  (cents. ~1731 x fractional deviation, so 1.7 = 0.1 %, 8.7 = 0.5 %)");

            /*  ## What this measured, recorded because the answer was NOT "no change required"

                **WOW is LINEAR and FLUTTER is skewed, and only one premise survives that.** The
                taper argument — half travel landing in the bottom 3 % of range — is FLUTTER's skew
                of 0.2 and holds: 50 % knob is 3.1 % of range, 7.61 cents, 0.44 %, a worn deck. WOW
                has no skew at all, so 50 % knob is 50 % of range: **22.44 cents rms = 1.30 %**,
                past "dying transport". Its middle does not land on plausible hardware, and even
                25 % knob is 0.65 %.

                A good deck at 0.05-0.08 % needs roughly 2-3 % of WOW's travel — the bottom sliver
                of a linear control, which is precisely the shape the FLUTTER skew exists to avoid.

                **And a second finding that is NOT explained here: WOW does not accumulate across
                generations and FLUTTER does.** WOW 100 % reads 44.87 rms at GEN 1 and 41.06 at
                GEN 8 — a ratio of 0.91, no accumulation. FLUTTER 100 % reads 243.46 and 531.81 —
                2.18x, which is the sqrt(8) the decorrelation work was aiming at. So step 1's 2.17x
                figure, measured with both driven, was FLUTTER's; wow contributed nothing to it.

                Flagged rather than accounted for. Two independent slow oscillators at different
                rates should RMS-sum like anything else, and a ratio slightly below 1.0 is not what
                partial correlation looks like either. It is the next thing to measure, and this
                stage has already had three arms measure something other than what they were named
                after — an explanation offered now would be a fourth. */

            // Reported. A range is a design figure and pinning one here would pin a decision that
            // has not been made — the measurement is what the decision is made from.
            expect (true);
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
