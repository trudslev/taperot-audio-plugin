#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/GUI/TapeRotTheme.h"

#include <nf/HeaderPart.h>
#include <nf/UserProgramDirectory.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    The first tests in this casting that run against the REAL editor.

    **Why this file exists, and why it did not until now.** Reflect-84 shipped the stale-replay
    guard with **zero call sites** for its disarm. The guard was correct, the processor was correct,
    and every suite passed — because the defect was in the editor, which no target a test could run
    compiled. The harness port gave all six castings the ability to construct their shipping editor;
    a port with nothing using it is just a longer build.

    **It was deliberately deferred for five castings until 2026-08-20**, and the deferral is spent:
    a test written the day before its GUI is rebuilt is a test written against the wrong thing, and
    all six editors are now rebuilt. Reflect-84's was the exception because its editor was already
    conformant, and it is the model this file follows.

    **What can honestly be asserted here, and what cannot.** A knob takes the LCD over, and disarms
    the guard, only while it is GENUINELY being dragged — `nf::connectUserEdit` guards on
    `isMouseButtonDown()`, and that state lives in the mouse source rather than in the component. A
    headless test cannot fake it: there is no windowed peer for a synthetic event to arrive through.
    So "a user edit disarms the guard" is **not available from here**, and asserting it would be a
    claim of coverage rather than coverage — which this suite records as worse than no test at all.
    `tools/check_user_edit_wiring.py` covers the call-site question statically and
    `nf::UserEditGate`'s own tests cover the mechanism.

    What IS available is the other half, and it has a rejected design behind it: **with a real
    editor attached and its attachments firing, a host's parameter writes must not disarm the
    guard.** That is precisely what a `ValueTree::Listener` inside core would have broken — the
    extraction plan specified one and it was rejected, because a listener cannot tell a person from
    an automation lane, and a host may write automation on session load before replaying its
    remembered program index. This file is the regression test for that decision.
*/
class EditorWiringTests final : public juce::UnitTest
{
public:
    EditorWiringTests() : juce::UnitTest ("Editor wiring", "GUI") {}

    void runTest() override
    {

        beginTest ("This panel is ON the shared part, and every band figure comes from it");
        {
            /*  **This arm has inverted, and the note it replaces is kept in the commit rather than
                here because it described a panel that no longer exists.** It read: *"Absent by
                construction, not clean. TapeRot does not reference `nf::HeaderGeometry` anywhere:
                it is 1336 x 679 and its band is its own... It becomes possible the moment this
                panel is moved onto the shared part."*

                That moment is this commit. §10 item 1 took the canvas 1336 -> 1340 (call 1, and
                **4 px, the smallest move in the suite**, because the band was already close to the
                part's) and item 5 replaced the header outright.

                So the defect the old note called unreachable is now reachable, and this is the arm
                that reaches it. **Chorus-60's pass aliased its LCD and left SAVE, DELETE and both
                meter wells as literals from the previous canvas - 29 px right and 29 px down -
                invisible for as long as the plate baked their faces.** A literal that happens to
                agree with core is indistinguishable from an alias by reading, so every cell below
                is compared against `nf::HeaderGeometry` rather than against a figure.

                What it CANNOT do is prove provenance: a derivation and a literal are
                indistinguishable at runtime while they agree, so this catches **divergence**, not a
                re-typed constant. That is the whole window in which the two are different at all -
                if the shared figure moves and this casting does not follow, it fires. */
            namespace L = TapeRotTheme::Layout;
            namespace H = TapeRotTheme::Header;

            expectEquals ((int) L::canvasWidth, 1340);
            expectEquals ((int) L::canvasWidth, nf::HeaderGeometry::canvasWidth);
            expectEquals ((int) L::canvasHeight, 790);

            expect (H::lcd()          == nf::HeaderGeometry::lcd().toFloat(),          "LCD cell");
            expect (H::saveButton()   == nf::HeaderGeometry::saveButton().toFloat(),   "SAVE cell");
            expect (H::deleteButton() == nf::HeaderGeometry::deleteButton().toFloat(), "DELETE cell");
            expect (H::inWell()       == nf::HeaderGeometry::inWell().toFloat(),       "IN well");
            expect (H::outWell()      == nf::HeaderGeometry::outWell().toFloat(),      "OUT well");
            expect (H::nameplate()    == nf::HeaderGeometry::nameplate().toFloat(),    "nameplate zone");

            /*  §9's Dymo cut is 694 x 150 at 3x, which is a 230.2 x 44 plate rotated -1.5 deg. That
                arithmetic is what settles the nameplate stack: core records this casting's
                published §4 row as `30 + 38 + 4 = 72`, six short of the anchor, and the delivered
                prototype draws the descriptor at 84, six past it. **The cut says 44, and 30 + 44 + 4
                is 78 exactly.** `Header`'s static_assert pins it; this logs it so a reader who
                meets the three disagreeing figures can see which one was built. */
            logMessage ("  nameplate stack " + juce::String (nf::HeaderGeometry::nameplateY)
                            + " + " + juce::String (H::dymoPlateH, 1)
                            + " + " + juce::String (H::dymoLeading, 1)
                            + " = " + juce::String (nf::HeaderGeometry::descriptorY)
                            + ", the shared descriptor anchor");
        }

        beginTest ("The real editor constructs, lays out and tears down");
        {
            /*  Worth its own case even though it asserts little: until the harness port, nothing
                here executed a line of the editor, so a null dereference or a failed assertion in
                layout would have been found by opening the plugin in a DAW.

                This constructs the SHIPPING processor, which builds its ProgramManager from the
                resolved user-Programs path — it has no injectable override, only ProgramManager
                does. **That path is redirected process-wide** by the
                `nf::ScopedUserProgramDirectoryOverride` in `TestMain`, so nothing here reaches real
                Programs; the case below asserts that is true in this binary rather than trusting
                it. */
            TapeRotAudioProcessor processor;
            auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());

            expect (editor != nullptr, "createEditor returned nothing");

            if (editor != nullptr)
            {
                expectGreaterThan (editor->getWidth(), 0);
                expectGreaterThan (editor->getHeight(), 0);
            }
        }

        beginTest ("A host's parameter writes do not disarm the stale-replay guard");
        {
            TapeRotAudioProcessor processor;
            auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
            expect (editor != nullptr);

            processor.userEdits.armRestore();

            // Every attachment in the editor fires for these, exactly as it does when a host
            // replays automation on session load. None of them is a drag.
            int written = 0;

            for (auto* parameter : processor.getParameters())
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                {
                    const auto original = ranged->getValue();
                    ranged->setValueNotifyingHost (original < 0.5f ? 0.9f : 0.1f);
                    ++written;
                }
            }

            // **The vacuity guard.** An empty parameter list would make the assertion below pass
            // while proving nothing, which is the shape this suite has three recorded cases of.
            expectGreaterThan (written, 0, "no parameters were written, so this asserted nothing");

            expect (processor.userEdits.isRestorePending(),
                    "automation disarmed the stale-replay guard. A ValueTree listener would do "
                    "exactly this - see nf/UserEditGate.h for why the shared model does not use "
                    "one, and do not re-introduce it");
        }
    }
};

static EditorWiringTests editorWiringTests;

/** Proves the process-wide redirect is in force in THIS binary, rather than merely installed in a
    file somebody could delete.

    `run_tests.py` refuses a target whose `TestMain` does not install it, and core's own tests prove
    the mechanism redirects. **Neither establishes that this process is redirected**, which is the
    thing keeping a suite off the user's disk — and the exposure arrived as a side effect of
    unrelated work, since the harness port gave every casting the ability to construct a processor
    that resolves the real path because that is its job.
*/
class ProgramDirectoryRedirectTests final : public juce::UnitTest
{
public:
    ProgramDirectoryRedirectTests() : juce::UnitTest ("Program directory redirect", "programs") {}

    void runTest() override
    {
        beginTest ("The shipping processor cannot reach the user's real Programs directory");
        {
            expect (nf::userProgramDirectoryOverrideRoot() != juce::File(),
                    "no redirect is installed in this process - TestMain must install "
                    "nf::ScopedUserProgramDirectoryOverride before the runner");

            TapeRotAudioProcessor processor;
            const auto used = processor.getUserProgramDirectory();
            // TapeRot resolves the path on the PROCESSOR rather than through a ProgramManager
            // accessor, which is the surface that matters here: it is the call the shipping build
            // makes, and the redirect has to catch it wherever it lives.

            expect (used.isAChildOf (nf::userProgramDirectoryOverrideRoot()),
                    "the processor resolved " + used.getFullPathName()
                        + ", which is outside the redirect root");

            // Named explicitly rather than compared against a rebuilt "real" path: the point is
            // that the application-data root is not on this path at all.
            const auto appData =
                juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

            expect (! used.isAChildOf (appData),
                    "the processor is pointing inside the user's application data");
        }
    }
};

static ProgramDirectoryRedirectTests programDirectoryRedirectTests;
