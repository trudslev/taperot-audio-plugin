#include "../Source/PluginProcessor.h"
#include <array>
#include <cmath>
#include "../Source/DSP/TapeModelEQ.h"
#include "../Source/Parameters.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Category 4 — lifecycle. What changes across a prepare that should not?

    ## Why `activeModelIndex` is here rather than in category 3

    Category 3's instrument compares a cold render against a warmed one. **Both arms call
    `prepareToPlay`, so any defect that fires identically on every prepare cancels out of it
    exactly.** That is not a shortcoming of one fixture — it is the whole column being structurally
    unable to see this class, and category 3's four first-run findings are the subset that fires
    ONCE, not the population.

    It surfaced there as a control failure: an isolation attempt on `switchMode` came back with its
    control differing, which is what said the arm could not answer the question rather than that the
    answer was no.

    ## The instrument, and why a SECOND prepare separates what the first could not

    The two candidate causes have opposite lifetimes, and that is the lever:

    | | first prepare | every later prepare |
    |---|---|---|
    | `OutputStage`'s smoothers | target is 0 at construction, so the output **fades in** | target already holds the real value, so `setCurrentAndTargetValue(target)` is a no-op — **no fade** |
    | `TapeModelEQ::activeModelIndex` | set to 0 = NONE (`TapeModelEQ.cpp:82`) | set to 0 = NONE **again** |

    So after a second prepare the fade-in that defeated both category 3 isolations is gone, and
    anything left in that window is the model switch. The parameter default is 5, CASSETTE I
    (`Parameters.h:255`), so the block after every prepare sees `5 != 0` and starts a switch: a
    crossfade in FADE, and in CLUNK a coefficient swap under a mute dip with a thump.

    **This matters more than a first-run defect** precisely because it is not one. A host changing
    sample rate or buffer size re-fires it, every time.

    ## The known case, named before the run

    `model = 0`. Requested equals what prepare set, so no switch can fire and its post-second-prepare
    onset must be flat. If the control dips, the instrument is measuring something else and this
    test answers nothing — which is how both category 3 attempts ended, and why the control is
    stated first rather than checked afterwards.
*/
class LifecycleTests final : public juce::UnitTest
{
public:
    LifecycleTests() : juce::UnitTest ("Lifecycle", "DSP") {}

    void runTest() override
    {
        beginTest ("TapeModelEQ directly: does prepare() re-arm a switch the Program did not ask for?");
        {
            // **Three level-based isolations have now failed on this, and the reason is the same
            // each time: a model switch changes EQ, not level.** The onset profile measured
            // OutputStage's fade-in, then the plugin's own reported latency; the FADE/CLUNK arm
            // failed its control. Level cannot see a spectral transition, which is the third
            // instance in this sweep of a metric blind to the axis of the question.
            //
            // So drop the whole plugin chain and drive the class. `TapeModelEQ` is compiled into
            // this target, so the transition is observable directly: process ONE block at model 5
            // straight after prepare, and compare it against the same block processed once the
            // chain has settled at model 5. If prepare left activeModelIndex at 0 = NONE, the first
            // block is a blend and the two differ. If prepare had reconciled it, they are identical.
            //
            // KNOWN CASE, named first: model 0. Requested equals what prepare sets, so no switch is
            // possible and the first block MUST equal the settled block. If it does not, this
            // instrument is measuring something other than the switch — which is exactly how the
            // three previous attempts ended.
            const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };

            /*  **FIXED, and the old defect's own measurement is the positive control now.**
                `TapeModelEQ::prepare` takes the model it is being prepared FOR, so a prepare no
                longer contradicts the configuration. Both arms below prepare at the model they will
                be asked for, which is what the processor does.

                That leaves nothing here able to fail — the same shape as three other metrics this
                stage, where fixing the last failing case took the instrument's only proof that it
                could report one. So the third arm prepares at NONE and requests 5, which is a
                GENUINE model change and must still crossfade. It reproduces what the defect
                measured, because it is the same measurement; the difference is that a real switch
                is supposed to do this and a prepare is not. */
            const auto firstBlockAgainstSettled = [&spec] (int preparedModel, int modelIndex, bool clunk)
            {
                const auto makeInput = [&spec] (juce::AudioBuffer<float>& b)
                {
                    juce::Random r (1234);
                    b.setSize (2, (int) spec.maximumBlockSize);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            b.setSample (ch, i, r.nextFloat() * 2.0f - 1.0f);
                };

                juce::AudioBuffer<float> reference;
                makeInput (reference);

                // Arm A — one block immediately after prepare.
                TapeModelEQ fresh;
                fresh.prepare (spec, preparedModel);
                juce::AudioBuffer<float> a (2, (int) spec.maximumBlockSize);
                a.makeCopyOf (reference);
                fresh.process (a, modelIndex, clunk);

                // Arm B — the same block, after the chain has fully settled at the same model.
                TapeModelEQ settled;
                settled.prepare (spec, preparedModel);
                juce::AudioBuffer<float> scratch (2, (int) spec.maximumBlockSize);

                for (int block = 0; block < 40; ++block)     // 40 x 512 = 427 ms, well past 60 ms
                {
                    scratch.makeCopyOf (reference);
                    settled.process (scratch, modelIndex, clunk);
                }

                juce::AudioBuffer<float> b (2, (int) spec.maximumBlockSize);
                b.makeCopyOf (reference);
                settled.process (b, modelIndex, clunk);

                double worst = 0.0, peak = 0.0;

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < a.getNumSamples(); ++i)
                    {
                        worst = juce::jmax (worst, (double) std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
                        peak  = juce::jmax (peak,  (double) std::abs (b.getSample (ch, i)));
                    }

                return peak > 0.0 ? worst / peak : 0.0;
            };

            std::array<double, 2> liveByMode {}, genuineByMode {};

            for (bool clunk : { false, true })
            {
                const auto control  = firstBlockAgainstSettled (0, 0, clunk);
                const auto live     = firstBlockAgainstSettled (5, 5, clunk);
                const auto genuine  = firstBlockAgainstSettled (0, 5, clunk);

                logMessage (juce::String (clunk ? "  CLUNK" : "  FADE ")
                                + " -> prepared 0/asked 0 (control) " + juce::String (control, 9)
                                + ", prepared 5/asked 5 " + juce::String (live, 9)
                                + ", prepared 0/asked 5 (must switch) " + juce::String (genuine, 9)
                                + "   (relative to the settled block's own peak)");

                expect (control < 1.0e-6,
                        "the control differed after prepare at model 0, where no switch is "
                        "possible — this instrument does not isolate the switch either: "
                            + juce::String (control, 9));

                expect (genuine > 1.0e-3,
                        "**THIS INSTRUMENT CANNOT SEE A MODEL SWITCH.** Prepared at NONE and asked "
                        "for 5 is a genuine change and must cross-fade, so the clean rows above are "
                        "a measurement that only ever reports clean: " + juce::String (genuine, 9));

                liveByMode[clunk ? 1 : 0] = live;
                genuineByMode[clunk ? 1 : 0] = genuine;
            }

            /*  **The residue is NOT the crossfade, and the two modes agreeing is what says so.**

                Fixing `prepare` took FADE from 26.75 % to 13.02 % and CLUNK from **97.55 %** to
                13.04 %. Not zero — and the interesting part is that the two remainders agree to four
                decimal places when the thing they used to measure differed between them by a factor
                of nearly four.

                FADE and CLUNK are completely different transitions: a parallel-chain crossfade
                against a hard coefficient swap under a mute dip. Any cause routed through the switch
                MUST differ between them, and the "prepared 0 / asked 5" arm shows exactly that —
                0.267 against 0.976. A cause that does not differ between them is not going through
                the switch at all.

                What is left is the filters' own startup: a fresh biquad cascade has no history, and
                one block through it differs from one block through the same cascade after 427 ms of
                signal. That is ordinary and unavoidable — a chain cannot be born settled — and it is
                why the model-0 control reads exactly zero, since NONE runs no bands to have history.

                So the assertion is MODE INDEPENDENCE rather than a magnitude. It fails if a switch
                is being re-armed, and it does not pretend a cold filter is a defect. Pinning 0.130
                would be pinning a number nobody had explained. */
            const double modeSpread = std::abs (liveByMode[0] - liveByMode[1]);
            const double genuineSpread = std::abs (genuineByMode[0] - genuineByMode[1]);

            logMessage ("  residue FADE vs CLUNK -> " + juce::String (modeSpread, 9)
                            + "; a genuine switch spreads " + juce::String (genuineSpread, 9));

            expect (modeSpread < 1.0e-3,
                    "the first block after prepare differs between FADE and CLUNK, so what remains "
                    "IS routed through the model switch and prepare is still re-arming one. A cold "
                    "filter cascade cannot know which switch mode it is in: " 
                        + juce::String (modeSpread, 9));

            expect (genuineSpread > 1.0e-2,
                    "**THE MODE-INDEPENDENCE TEST CANNOT FAIL.** A genuine switch must differ "
                    "between a crossfade and a hard swap under a mute dip, and it did not — so the "
                    "row above is not evidence of anything: " + juce::String (genuineSpread, 9));
        }

        beginTest ("A second prepare re-fires the MODEL switch, with no fade-in to hide it");
        {
            constexpr double fs = 48000.0;
            constexpr int blockSize = 512;

            const auto onsetAfterSecondPrepare = [&] (const char* label, float modelValue)
            {
                TapeRotAudioProcessor p;

                if (auto* m = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (ParamIDs::model)))
                    m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 (modelValue));

                nf::testing::RenderSpec spec;
                spec.sampleRate = fs;
                spec.blockSize = blockSize;
                spec.numBlocks = 32;

                // First render: spends the fade-in AND completes the first model switch.
                nf::testing::render (p, spec);

                // Second render re-prepares, which is the whole point — see the table above.
                const auto second = nf::testing::render (p, spec);

                const auto rms = windowedRms (second, fs, 10.0, 10);

                juce::String row;
                for (size_t i = 0; i < rms.size(); ++i)
                    row += juce::String ((rms[i] > 0.0 && rms.back() > 0.0)
                                             ? 20.0 * std::log10 (rms[i] / rms.back()) : -99.0, 1)
                               .paddedLeft (' ', 8);

                logMessage ("  " + juce::String (label).paddedRight (' ', 26) + row);

                double worst = 0.0;
                for (size_t i = 0; i < 6; ++i)
                    if (rms[i] > 0.0 && rms.back() > 0.0)
                        worst = juce::jmin (worst, 20.0 * std::log10 (rms[i] / rms.back()));

                return worst;
            };

            logMessage ("  slice (10 ms), vs settled 10     20      30      40      50      60      70      80      90     100");

            const auto control = onsetAfterSecondPrepare ("model 0 NONE (control)", 0.0f);
            const auto live    = onsetAfterSecondPrepare ("model 5 CASSETTE I", 5.0f);

            logMessage ("  worst dip in the first 60 ms -> control " + juce::String (control, 2)
                            + " dB, model 5 " + juce::String (live, 2) + " dB");

            // **Reported against the control, never in absolute terms.** The two models genuinely
            // sound different, so an absolute dip says nothing; what says something is one of them
            // dipping where the other does not.
            logMessage (juce::String ("  => ") + (control > -1.0 && live < -3.0
                            ? "the MODEL switch re-fires on every prepare, and it is audible"
                            : control <= -1.0 ? "the CONTROL dipped — the instrument is measuring "
                                                "something other than the switch and proves nothing"
                                              : "no switch detected — the construction is real but "
                                                "its effect stays INFERRED"));

            // **INSTRUMENT REJECTED, and kept because the rejection is the record.** The control
            // dips ~56 dB after a second prepare, where no model switch is possible — that is
            // TapeRot's own reported latency (the Saturator oversamples), present in every render
            // and occupying most of the window a 60 ms switch would occupy. Nothing is asserted
            // from this arm; the class-level test above is the one that isolates, and its control
            // returns exactly 0.000000000.
            //
            // It stays in the fixture because a rejected instrument that is deleted gets rebuilt by
            // the next person from the same reasoning. Four attempts were made at this finding and
            // three were rejected by their own controls — that count is the useful artefact.
            logMessage ("  INSTRUMENT REJECTED: the control dips " + juce::String (control, 2)
                            + " dB, which is the plugin's own latency, not the switch. Nothing is "
                              "asserted here — see the class-level test above.");
        }
    }

private:
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
};

static LifecycleTests lifecycleTests;
