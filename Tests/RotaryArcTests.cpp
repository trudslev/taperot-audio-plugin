#include "../Source/GUI/KnobFilmstrip.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

/**
    The rotary arc the Slider reports, against the arc the artwork is cut at.

    **The defect this pins was a SIGN of orientation, not a magnitude.** `setRotaryParameters` was
    given `180 +/- sweep/2`, which is symmetric about SIX o'clock, where JUCE measures rotary angles
    clockwise from twelve. Same span, opposite direction — so any test asserting the sweep width
    would have passed it, and did not exist to.

    **Nothing on the panel showed it either.** `paint()` is fully overridden and blits a baked
    filmstrip frame chosen by `valueToProportionOfLength`, so the Slider's rotary parameters reach no
    pixel in the shipping build. They reach an accessibility client, a look-and-feel, and any JUCE
    default paint path someone later reinstates. A defect that is invisible in the artefact and
    visible only to a consumer nobody has written yet is exactly the kind this suite files under
    "nothing else reads it yet" — a fact about today, not a property of the code.

    `nf::printedScaleDefects` cannot reach it: it checks a ring's printed marks against the
    parameter's own `NormalisableRange`, and an arc is neither a mark nor a range.
*/
class RotaryArcTests final : public juce::UnitTest
{
public:
    RotaryArcTests() : juce::UnitTest ("Rotary arc", "GUI") {}

    void runTest() override
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        constexpr float sweep = TapeRotTheme::Layout::knobSweepDegrees;

        beginTest ("The arc is symmetric about TWELVE o'clock and spans the artwork's sweep");
        {
            KnobFilmstrip knob { TapeRotTheme::Layout::Cap::large };
            const auto p = knob.getRotaryParameters();

            const float span = p.endAngleRadians - p.startAngleRadians;
            const float centre = (p.startAngleRadians + p.endAngleRadians) * 0.5f;

            // Distance from twelve o'clock, wrapped into -pi..+pi so 2*pi and 0 read the same.
            const float offTwelve = std::abs (std::remainder (centre, twoPi));

            logMessage ("  start " + juce::String (juce::radiansToDegrees (p.startAngleRadians), 2)
                            + " deg, end " + juce::String (juce::radiansToDegrees (p.endAngleRadians), 2)
                            + " deg, span " + juce::String (juce::radiansToDegrees (span), 2)
                            + " deg, centre " + juce::String (juce::radiansToDegrees (offTwelve), 4)
                            + " deg off twelve");

            expectWithinAbsoluteError (juce::radiansToDegrees (span), sweep, 0.001f,
                                       "the arc does not span the sweep the filmstrip is cut at");

            // **This is the assertion that catches the defect.** The span passes either way.
            expectWithinAbsoluteError (juce::radiansToDegrees (offTwelve), 0.0f, 0.001f,
                                       "the rotary arc is not centred on twelve o'clock. JUCE "
                                       "measures clockwise from twelve, so `180 +/- sweep/2` points "
                                       "the control at the floor with the correct width");

            expect (p.stopAtEnd, "the arc must stop at its ends rather than wrapping");
        }

        beginTest ("Shown able to fail — the previous construction is off by half a turn");
        {
            /*  Without this the assertion above is indistinguishable from one that cannot fail: a
                centre of zero is also what you get from an arc that was never set. The pre-fix
                construction is evaluated directly and must be caught, which also records the exact
                magnitude of what was wrong — half a turn, not a few degrees. */
            const float oldStart = juce::degreesToRadians (180.0f - sweep * 0.5f);
            const float oldEnd   = juce::degreesToRadians (180.0f + sweep * 0.5f);

            const float oldSpan = juce::radiansToDegrees (oldEnd - oldStart);
            const float oldOff  = juce::radiansToDegrees (
                std::abs (std::remainder ((oldStart + oldEnd) * 0.5f, twoPi)));

            logMessage ("  previous construction: span " + juce::String (oldSpan, 2)
                            + " deg (correct), centre " + juce::String (oldOff, 2) + " deg off twelve");

            expectWithinAbsoluteError (oldSpan, sweep, 0.001f,
                                       "the old construction did not even span the right width, so "
                                       "it is not the defect this test is pinned against");

            expectGreaterThan (oldOff, 179.0f,
                               "the old construction was NOT half a turn out, so either the recorded "
                               "defect is wrong or this check cannot see it");
        }
    }
};

static RotaryArcTests rotaryArcTests;
