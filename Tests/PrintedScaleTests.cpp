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
                expect (worst < 0.05f,
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
            compare ("noise",   Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("failure", Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("mix",     Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size(), even5);
            compare ("output",  Marks::outputDb.data(),        (int) Marks::outputDb.size(),        even5);
            compare ("lp",      Marks::lpKilohertz.data(),     (int) Marks::lpKilohertz.size(),     lpF);
            compare ("hp",      Marks::hpHertz.data(),         (int) Marks::hpHertz.size(),         hpF);
            compare ("ramp",    Marks::rampSeconds.data(),     (int) Marks::rampSeconds.size(),     rampF);
        }

        //== WOW: the one ring that deliberately does NOT match the prototype =
        /*  §3.2 legends WOW as even fifths and the prototype draws them at even ANGLES, sharing a
            `pct` table with NOISE, FAILURE and MIX. Those three are linear and WOW is **skew 0.3**,
            for a reason `Parameters.h` argues at length: WOW's realised deviation is about 5x
            FLUTTER's, so matching exponents would put the same physical condition at different knob
            positions.

            So the shared table is correct for three of the four and wrong for the fourth, and
            nothing at the call site distinguishes them - which is why WOW has its own array and
            this arm exists rather than a comment.

            **The divergence is asserted rather than tolerated.** Left unpinned it would be
            invisible again the moment somebody re-cut the ring; pinned, a re-cut fails here and the
            figures are on screen. `design-asks/taperot-wow-ring.md` asks which five numerals the
            ring should carry now that they are honestly placed.  */
        beginTest ("WOW's ring is drawn from its taper, and the prototype's is not");
        {
            const auto range = rangeFor ("wow");

            struct Expected { float value; float builtAngle; float prototypeAngle; };
            const std::array<Expected, 3> interior { {
                { 25.0f,  43.13f, -67.50f },
                { 50.0f,  84.31f,   0.00f },
                { 75.0f, 112.68f,  67.50f } } };

            for (const auto& e : interior)
            {
                const float built = nf::sweepAngleDegrees (
                    (float) range.convertTo0to1 (e.value), Layout::knobSweepDegrees);

                logMessage ("  WOW " + juce::String ((int) e.value) + "%: built at "
                                + juce::String (built, 2) + " deg, prototype drew it at "
                                + juce::String (e.prototypeAngle, 2) + " deg");

                expectWithinAbsoluteError (built, e.builtAngle, 0.05f,
                                           "WOW's ring has moved off its own taper");

                /*  What the prototype's ring would have MEANT, which is the reason this is a defect
                    rather than a difference: at the printed 50 the pointer would be at 9.92 %.  */
                const float meant = (float) range.convertFrom0to1 (
                    (double) ((e.prototypeAngle + Layout::knobSweepDegrees * 0.5f)
                              / Layout::knobSweepDegrees));

                logMessage ("     the prototype's angle reads " + juce::String (meant, 2)
                                + " % on this range");
                expect (std::abs (meant - e.value) > 1.0f,
                        "the prototype's WOW ring now agrees with the taper - if the range changed, "
                        "this arm and the ring both want re-deriving");
            }

            // The endpoints agree in both, which is exactly why the defect is invisible: a ring
            // wrong only in its interior looks like a ring.
            expectWithinAbsoluteError (nf::sweepAngleDegrees ((float) range.convertTo0to1 (0.0f)),
                                       -135.0f, 0.01f);
            expectWithinAbsoluteError (nf::sweepAngleDegrees ((float) range.convertTo0to1 (100.0f)),
                                       135.0f, 0.01f);
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
