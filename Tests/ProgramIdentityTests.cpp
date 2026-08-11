#include "TestUtils.h"
#include "../Source/PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Program identity, and the host boundary it is deliberately kept away from.

    These drive the REAL processor rather than a stand-in, because the thing under test is an
    ordering between two AudioProcessor entry points - setStateInformation then setCurrentProgram -
    and a harness that reimplemented that ordering would assert against a copy of the code.
*/
class ProgramIdentityTests final : public juce::UnitTest
{
public:
    ProgramIdentityTests() : juce::UnitTest("Program identity", "State") {}

    void runTest() override
    {
        beginTest("The host list is the Factory bank, and its size never changes");
        {
            TapeRotAudioProcessor p;

            expectEquals(p.getNumPrograms(), (int) kNumFactoryPrograms,
                         "the host list must be Factory-only - not INIT, not User Programs");

            // The conformance point. juce_AudioProcessor.h: "must not change over its lifetime."
            const int before = p.getNumPrograms();
            p.saveUserProgram("IDENTITY TEST A");
            expectEquals(p.getNumPrograms(), before, "saving a User Program must not resize the list");

            p.deleteUserProgram({ ProgramBank::user, "IDENTITY TEST A", "IDENTITY TEST A" });
            expectEquals(p.getNumPrograms(), before, "deleting one must not resize it either");
        }

        beginTest("Every factory position round-trips through identity");
        {
            TapeRotAudioProcessor p;

            for (int i = 0; i < p.getNumPrograms(); ++i)
            {
                const auto id = TapeRotAudioProcessor::factoryIdAt(i);
                expectEquals(TapeRotAudioProcessor::factoryPositionOf(id.id), i);
                expect(p.getProgramName(i) == id.displayName);

                // The label the panel paints is 1-based; the host position is 0-based. Host index n
                // IS Factory Program n+1 - the alignment that excluding INIT buys.
                expect(p.displayLabelFor(id).startsWith(juce::String(i + 1).paddedLeft('0', 2)));
            }
        }

        beginTest("A stale host replay right after a restore is ignored");
        {
            TapeRotAudioProcessor p;

            // Put the plugin somewhere distinctive and capture that session.
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(6));
            p.flushPendingProgramChange();
            const auto expectedId = p.getCurrentProgramId();
            const float expectedDrive = driveOf(p);

            juce::MemoryBlock session;
            p.getStateInformation(session);

            // A different Program, then the restore - as reopening a project does.
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(2));
            p.flushPendingProgramChange();
            p.setStateInformation(session.getData(), (int) session.getSize());

            // ...and now the host replays the presetNumber it remembered.
            p.setCurrentProgram(p.getCurrentProgram());
            p.flushPendingProgramChange();

            expect(p.getCurrentProgramId() == expectedId, "the restored Program must survive the replay");
            expectWithinAbsoluteError(driveOf(p), expectedDrive, 1.0e-4f);
        }

        beginTest("A genuine change immediately after a restore is honoured");
        {
            TapeRotAudioProcessor p;

            juce::MemoryBlock session;
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(6));
            p.flushPendingProgramChange();
            p.getStateInformation(session);
            p.setStateInformation(session.getData(), (int) session.getSize());

            // A DIFFERENT position is not a replay, so the guard must not swallow it.
            p.setCurrentProgram(3);
            p.flushPendingProgramChange();
            expect(p.getCurrentProgramId() == TapeRotAudioProcessor::factoryIdAt(3));
        }

        beginTest("The guard expires - a matching change later in the session is honoured");
        {
            TapeRotAudioProcessor p;

            juce::MemoryBlock session;
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(6));
            p.flushPendingProgramChange();
            p.getStateInformation(session);
            p.setStateInformation(session.getData(), (int) session.getSize());

            // First call disarms, whether or not it was honoured.
            p.setCurrentProgram(p.getCurrentProgram());
            p.flushPendingProgramChange();

            // Now go elsewhere and come back to the same position. This is the case that motivated
            // bounding the flag: a user reverting an edited Program by re-selecting it.
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(1));
            p.flushPendingProgramChange();
            p.setCurrentProgram(6);
            p.flushPendingProgramChange();
            expect(p.getCurrentProgramId() == TapeRotAudioProcessor::factoryIdAt(6),
                   "the guard must not persist beyond the first call after a restore");
        }

        beginTest("A user edit disarms the guard, so a later matching change still applies");
        {
            TapeRotAudioProcessor p;

            juce::MemoryBlock session;
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(6));
            p.flushPendingProgramChange();
            p.getStateInformation(session);
            p.setStateInformation(session.getData(), (int) session.getSize());

            p.noteUserEdit();               // what the editor calls on a real knob drag

            p.setCurrentProgram(p.getCurrentProgram());
            p.flushPendingProgramChange();
            expect(p.getCurrentProgramId() == TapeRotAudioProcessor::factoryIdAt(6));
        }

        beginTest("A restored User Program survives the replay, which reports position 0");
        {
            TapeRotAudioProcessor p;

            p.saveUserProgram("IDENTITY TEST B");
            p.flushPendingProgramChange();
            const auto userId = p.getCurrentProgramId();
            expect(userId.bank == ProgramBank::user, "save must select the Program it just wrote");

            // The accepted divergence: with User Programs off the host list, this answers 0.
            expectEquals(p.getCurrentProgram(), 0);

            juce::MemoryBlock session;
            p.getStateInformation(session);
            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(4));
            p.flushPendingProgramChange();
            p.setStateInformation(session.getData(), (int) session.getSize());

            // The host replays 0 - which resolves to Factory 01, a DIFFERENT identity. This is
            // exactly why the guard compares positions rather than identities.
            p.setCurrentProgram(0);
            p.flushPendingProgramChange();
            expect(p.getCurrentProgramId() == userId, "the user's Program must survive the replay");

            p.deleteUserProgram(userId);
            p.flushPendingProgramChange();
        }

        beginTest("An unresolved identifier keeps the values and says it does not know the name");
        {
            TapeRotAudioProcessor p;

            p.requestProgramChange(TapeRotAudioProcessor::factoryIdAt(9));
            p.flushPendingProgramChange();
            const float expectedDrive = driveOf(p);

            juce::MemoryBlock session;
            p.getStateInformation(session);

            // Rewrite the session as one naming a Program this build no longer has.
            std::unique_ptr<juce::XmlElement> xml(
                juce::AudioProcessor::getXmlFromBinary(session.getData(), (int) session.getSize()));
            expect(xml != nullptr);
            xml->setAttribute("taperotProgramId", "a-program-from-the-future");
            xml->setAttribute("taperotProgramName", "SOME FUTURE SOUND");

            juce::MemoryBlock rewritten;
            juce::AudioProcessor::copyXmlToBinary(*xml, rewritten);
            p.setStateInformation(rewritten.getData(), (int) rewritten.getSize());

            const auto id = p.getCurrentProgramId();
            expect(id.bank == ProgramBank::unresolved);
            expect(id.displayName == "SOME FUTURE SOUND",
                   "the panel needs a presentable name - a slug would read as a rendering fault");
            expectWithinAbsoluteError(driveOf(p), expectedDrive, 1.0e-4f,
                                      "the values are what the session was saved for; they must not move");
        }

        beginTest("User Programs sort by displayed name, case-insensitively");
        {
            TapeRotAudioProcessor p;

            // "AB C" vs "AB" is the pair that exposes sorting on the filename WITH its extension:
            // a space (0x20) precedes the dot (0x2E), so "AB C" used to sort first.
            for (const auto* n : { "ZEBRA", "apple", "AB C", "AB" })
                p.saveUserProgram(n);

            juce::StringArray shown;

            for (const auto& id : p.listPrograms())
                if (id.bank == ProgramBank::user)
                    shown.add(id.displayName);

            const auto indexOf = [&shown](const juce::String& s) { return shown.indexOf(s); };

            expect(indexOf("AB") >= 0 && indexOf("AB C") >= 0);
            expect(indexOf("AB") < indexOf("AB C"), "\"AB\" must precede \"AB C\"");
            expect(indexOf("apple") < indexOf("ZEBRA"), "case must not decide the order");

            for (const auto& id : p.listPrograms())
                if (id.bank == ProgramBank::user)
                    p.deleteUserProgram(id);
        }
    }

private:
    static float driveOf(TapeRotAudioProcessor& p)
    {
        return p.apvts.getRawParameterValue(ParamIDs::drive)->load();
    }
};

static ProgramIdentityTests programIdentityTests;
