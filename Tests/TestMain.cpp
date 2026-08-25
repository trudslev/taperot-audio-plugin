#include <juce_audio_processors/juce_audio_processors.h>
#include <nf/UserProgramDirectory.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

/*  **JUCE's default logger writes to OutputDebugString on Windows, so every line this suite logs
    was invisible in Windows CI.**

    `Logger::outputDebugString` is `std::cerr << text` on POSIX and `OutputDebugString (...)` on
    Windows (`juce_win32_Misc.cpp`), which goes to an attached debugger and nowhere else. The target
    is a console app, so this is not a subsystem problem — it is which sink JUCE picked.

    Measured on Elmer: the Windows step printed **8 lines against macOS's 94**, and the job was
    green. It passes silently and fails loudly — `TestMain` writes its summary to `std::cerr` and
    returns non-zero — so a real failure still goes red. But "green with no evidence" is
    indistinguishable, to a reader, from a step that never ran the binary at all.

    Installing this settled which: all three platforms print 94 suites now, so they were running.

    **RAII rather than an install/clear pair**, because several of these `main`s return early from
    inside a loop, and a trailing clear would be skipped on exactly the path that matters. */
struct ConsoleLogger final : juce::Logger
{
    ConsoleLogger()           { juce::Logger::setCurrentLogger (this); }
    ~ConsoleLogger() override { juce::Logger::setCurrentLogger (nullptr); }

    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main()
{
    ConsoleLogger consoleLogger;

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
