#include <juce_audio_processors/juce_audio_processors.h>
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
