#include "TestUtils.h"
#include "../Source/GUI/TapeRotTheme.h"
#include "../Source/Parameters.h"
#include "../Source/DSP/TapeModelData.h"

#include <nf/PrintedScale.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Every printed ring on this panel, against the parameter that drives its pointer.

    **BRAND.md makes a printed scale a correctness requirement**, and the failure is specific and
    invisible: a ring legends a taper, so if the parameter's taper moves and the ring does not, the
    numerals point at values the pointer never reaches. The ring still draws, the pointer still
    turns, and nothing looks broken.

    ## Why this file has two different kinds of arm

    `KnobComponent` computes a mark's angle from the parameter's own `NormalisableRange`, so a
    stored-angle-versus-range comparison here would be **a check whose input comes from the thing it
    checks** - it could not fail. Root `CLAUDE.md` lists three members of that family, all of which
    pass and none of which proves anything.

    So the arms are sourced from somewhere the subject cannot reach:

    | Arm | Source | Catches |
    |---|---|---|
    | `nf::printedScaleDefects` | the range | a mark off the range, off the sweep, duplicated, or out of order |
    | the **delivered prototype's own fractions**, transcribed as literals | `design/TapeRot MT-77 Panel.dc.html` | the ring and the design disagreeing about where a numeral goes |

    The second is the one with teeth, and it is what found WOW.
*/
namespace
{
    /*  A throwaway AudioProcessor to host an APVTS, matching the pattern the other parameter tests
        in this folder use. The point is to reach the SHIPPING parameter layout without the plugin
        target's `JucePlugin_*` macros, which the plain console Tests target does not have.

        **Reading the real layout is what makes the arms above worth anything.** A test declaring
        its own ranges would agree with itself and pass while the panel legends something else -
        which is exactly how Fifth Member's Hz ring came to have three copies of one range.  */
    class DummyProcessor final : public juce::AudioProcessor
    {
    public:
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

class PrintedScaleTests final : public juce::UnitTest
{
public:
    PrintedScaleTests() : juce::UnitTest ("Printed scales", "GUI") {}

    void runTest() override
    {
        using namespace TapeRotTheme;

        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts { proc, nullptr, "PARAMETERS",
                                                   createTapeRotParameterLayout() };

        const auto rangeFor = [&] (const char* id)
        {
            auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
            jassert (param != nullptr);
            return param->getNormalisableRange();
        };

        //== Arm 1: the structural defects, against each parameter's range ====
        beginTest ("No ring carries a mark the parameter cannot reach");
        {
            const auto check = [&] (const char* id, const Marks::Mark* marks, int count)
            {
                const auto range = rangeFor (id);
                std::vector<nf::PrintedMark> printed;

                for (int i = 0; i < count; ++i)
                    printed.push_back ({ marks[i].value,
                                         nf::sweepAngleDegrees ((float) range.convertTo0to1 (marks[i].value),
                                                                Layout::knobSweepDegrees) });

                const auto defects = nf::printedScaleDefects (range, printed, Layout::knobSweepDegrees);

                for (const auto& d : defects)
                    logMessage (juce::String ("  ") + id + ": " + d);

                expectEquals (defects.size(), 0, juce::String (id) + "'s printed scale has defects");
            };

            check ("drive",   Marks::driveAndFlutter.data(),   (int) Marks::driveAndFlutter.size());
            check ("flutter", Marks::driveAndFlutter.data(),   (int) Marks::driveAndFlutter.size());
            check ("wow",     Marks::wowPercent.data(),        (int) Marks::wowPercent.size());
            check ("noise",   Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size());
            check ("failure", Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size());
            check ("mix",     Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size());
            check ("output",  Marks::outputDb.data(),          (int) Marks::outputDb.size());
            check ("lp",      Marks::lpKilohertz.data(),       (int) Marks::lpKilohertz.size());
            check ("hp",      Marks::hpHertz.data(),           (int) Marks::hpHertz.size());
            check ("ramp",    Marks::rampSeconds.data(),       (int) Marks::rampSeconds.size());
        }

        //== Arm 2: against the delivered prototype ===========================
        /*  The fractions below are transcribed from `design/TapeRot MT-77 Panel.dc.html`'s own
            `knobSpec()`, which is where the designer authored them. They are a genuinely
            independent source: they came from the design side, and the angles they are compared
            against are computed here from the build's `NormalisableRange`.

            **The prototype stores fractions and this file stores values, and that asymmetry is
            deliberate.** BRAND.md's rule is that code stores the value so a taper change moves the
            ring with the pointer; a prototype has no range to read, so it publishes derived output.
            Converting all 60 fractions back through the ranges returns clean round values - 1000,
            1200, 1500, 2000, 3000... for LP - which is the evidence that they were authored as
            values in the first place.  */
        beginTest ("Every ring lands where the delivered prototype puts it");
        {
            const auto compare = [&] (const char* id, const Marks::Mark* marks, int count,
                                      const std::vector<float>& prototypeFractions)
            {
                expectEquals ((int) prototypeFractions.size(), count,
                              juce::String (id) + ": the prototype and the build disagree on how "
                                                  "many marks the ring carries");
                if ((int) prototypeFractions.size() != count)
                    return;

                const auto range = rangeFor (id);
                float worst = 0.0f;

                for (int i = 0; i < count; ++i)
                {
                    const float built = nf::sweepAngleDegrees (
                        (float) range.convertTo0to1 (marks[i].value), Layout::knobSweepDegrees);
                    const float drawn = nf::sweepAngleDegrees (prototypeFractions[(size_t) i],
                                                               Layout::knobSweepDegrees);
                    worst = juce::jmax (worst, std::abs (built - drawn));
                }

                logMessage (juce::String ("  ") + id + ": worst disagreement "
                                + juce::String (worst, 4) + " deg");
                /*  0.1 deg, not 0.05. The prototype publishes DERIVED fractions and one of
                    WOW's is 1.8e-4 out — 0.759836 where (0.4)^0.3 is 0.759658, which is 0.048 deg.
                    §3.2's own stated angle for that mark is +70.11, which is what the correct
                    fraction gives, so the spec is right and the prototype's transcription is a
                    rounding away from it. Harmless here because the build stores VALUES and asks
                    the range for the angle — but a 0.05 bound would sit 0.002 deg from failing on
                    somebody else's rounding, which is a bound that reports the transcription rather
                    than the ring.  */
                expect (worst < 0.1f,
                        juce::String (id) + " disagrees with the delivered prototype by "
                            + juce::String (worst, 4) + " deg");
            };

            const std::vector<float> driveF { 0.0f, 0.2513f, 0.3466f, 0.3981f, 0.4573f, 0.5493f,
                                              0.6310f, 0.7579f, 0.8706f, 0.9441f, 1.0f };
            const std::vector<float> even5  { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            const std::vector<float> lpF    { 0.0f, 0.255084f, 0.335788f, 0.413403f, 0.508959f,
                                              0.574791f, 0.626602f, 0.707651f, 0.799184f,
                                              0.912457f, 1.0f };
            const std::vector<float> hpF    { 0.0f, 0.204645f, 0.251947f, 0.284536f, 0.331659f,
                                              0.381880f, 0.441757f, 0.487060f, 0.556094f,
                                              0.653691f, 0.725693f, 0.809782f, 0.916481f, 1.0f };
            const std::vector<float> rampF  { 0.0f, 0.120718f, 0.174160f, 0.229806f, 0.270270f,
                                              0.331541f, 0.419417f, 0.485877f, 0.565523f,
                                              0.669743f, 0.754006f, 0.889795f, 1.0f };

            compare ("drive",   Marks::driveAndFlutter.data(), (int) Marks::driveAndFlutter.size(), driveF);
            compare ("flutter", Marks::driveAndFlutter.data(), (int) Marks::driveAndFlutter.size(), driveF);
            const std::vector<float> wowF { 0.0f, 0.251189f, 0.501187f, 0.759836f, 1.0f };

            compare ("wow",     Marks::wowPercent.data(),        (int) Marks::wowPercent.size(),        wowF);
            compare ("noise",   Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("failure", Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("mix",     Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("output",  Marks::outputDb.data(),        (int) Marks::outputDb.size(),        even5);
            compare ("lp",      Marks::lpKilohertz.data(),     (int) Marks::lpKilohertz.size(),     lpF);
            compare ("hp",      Marks::hpHertz.data(),         (int) Marks::hpHertz.size(),         hpF);
            compare ("ramp",    Marks::rampSeconds.data(),     (int) Marks::rampSeconds.size(),     rampF);
        }

        //== WOW: the ring the round re-cut, and the guard that keeps it off ====
        /*  **This arm asserted the DIVERGENCE until 2026-08-21, and inverting it is not relaxing
            it.** It used to pin the gap between a build drawing WOW from its own taper and a
            prototype drawing it at even angles — with the figures, so a re-cut could not happen
            silently. §3.2 re-cut the ring and the gap is gone, so the old arm now asserts the
            absence of a defect that no longer exists, which is a vacuity guard encoding the symptom.

            What replaces it is the property the ruling actually bought: **WOW must not read the
            shared even-fifths table.** That can still fail — one edit reaching for the obvious
            shared constant puts back a ring printing 25 / 50 / 75 at 0.98 / 9.92 / 38.33 %, with
            both endpoints agreeing so nothing shows. The old figures stay in the log line, because
            what they characterise is what this guard is for.  */
        beginTest ("WOW does not read the shared even-fifths table");
        {
            const auto range = rangeFor ("wow");

            expect (Marks::wowPercent.data() != Marks::evenFifthsPercent.data(),
                    "WOW is back on the shared percent table — its skew of 0.3 makes that ring "
                    "print values the pointer never reaches");

            expect (! juce::approximatelyEqual ((float) range.skew, 1.0f),
                    "WOW's range is linear again. §3.2's decade series was chosen FOR skew 0.3; if "
                    "the taper really has changed, the ring wants re-deriving, not this assertion "
                    "removing");

            // What the retired ring would have read on this range, kept as the reason for the guard.
            for (const float v : { 25.0f, 50.0f, 75.0f })
            {
                const float f = (v + 0.0f) / 100.0f;
                const float atEvenAngle = (float) range.convertFrom0to1 ((double) f);
                logMessage ("  an even-fifths " + juce::String ((int) v) + " would sit at "
                                + juce::String (atEvenAngle, 2) + " % on this range");
            }

            // And the ring that shipped instead, at its own honest angles.
            for (const auto& m : Marks::wowPercent)
                logMessage ("  " + juce::String (m.numeral) + " at "
                                + juce::String (nf::sweepAngleDegrees (
                                      (float) range.convertTo0to1 (m.value),
                                      Layout::knobSweepDegrees), 2) + " deg");
        }

        //== §3.3's detents ===================================================
        /*  Re-aimed from `FilmstripConformanceTests`, which asserted the MODEL filmstrip had one
            frame per tape model. The strip is gone; the claim it protected is not - add a model and
            forget the ring and the ninth machine silently shares the eighth's detent.  */
        beginTest ("MODEL has exactly one detent per tape model");
        {
            expectEquals (Marks::modelDetentCount, (int) kNumTapeModels,
                          "§3.3's nine detents and the model table disagree");
        }
    }

};

static PrintedScaleTests printedScaleTests;
