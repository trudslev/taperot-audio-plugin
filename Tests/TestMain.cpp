#include <juce_audio_processors/juce_audio_processors.h>
#include <nf/UserProgramDirectory.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

int main()
{
    // **Without a MessageManager, AsyncUpdater::triggerAsyncUpdate() silently clears its own
    // pending flag**, so handleUpdateNowIfNeeded() finds nothing to do and every deferred Program
    // change quietly never happens - tests then pass while proving nothing. The mechanism is
    // juce_AsyncUpdater.cpp:80-83: the message post fails and cancelPendingUpdate() is called.
    //
    // This file was the last one in the suite without it. It went unnoticed because no TapeRot test
    // had exercised the async apply path until ProgramIdentityTests did.
    juce::ScopedJuceInitialiser_GUI juceInit;

    // **Every suite in this process resolves User Programs under a scratch directory, not the
    // user's real one.** The test harness compiles the shipping AudioProcessor, which builds its
    // ProgramManager from the real per-OS path because that is its job — so without this, any test
    // constructing the processor can reach
    // ~/Library/Application Support/<Company>/<Product>/Programs.
    //
    // A comment saying "do not write there" is a convention, and a convention gets broken silently.
    // It is also the one most likely to be broken by someone doing the right thing: verifying the
    // Program list needs several saved Programs, and building that state by hand is the obvious way
    // to get it. A cleanup glob has already destroyed a Program a user had just saved.
    //
    // Installed before the runner for the same reason ScopedJuceInitialiser_GUI is: it has to be in
    // force before the first line of the first test. See nf/UserProgramDirectory.h.
    const nf::ScopedUserProgramDirectoryOverride programRedirect {
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("NeonFoundryTestPrograms")
    };

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    if (failures > 0)
        std::cerr << "\n*** " << failures << " test failure(s)\n";

    return failures > 0 ? 1 : 0;
}
