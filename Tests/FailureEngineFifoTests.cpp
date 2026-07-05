#include "../Source/DSP/FailureEngine.h"
#include <juce_core/juce_core.h>

class FailureEngineFifoTests final : public juce::UnitTest
{
public:
    FailureEngineFifoTests() : juce::UnitTest("FailureEngineFifo", "GUI") {}

    void runTest() override
    {
        beginTest("Pushed events increment the counter and drain in order");
        {
            FailureEngine engine;
            expectEquals((int) engine.getEventCount(), 0);

            expect(engine.pushEvent(FailureEventType::dropout, 0.5f, 1000));
            expect(engine.pushEvent(FailureEventType::snag, 0.8f, 2000));
            expect(engine.pushEvent(FailureEventType::crinkle, 0.3f, 3000));

            expectEquals((int) engine.getEventCount(), 3);

            FailureEvent drained[8];
            const int numRead = engine.popEvents(drained, 8);

            expectEquals(numRead, 3);
            expect(drained[0].type == FailureEventType::dropout);
            expectWithinAbsoluteError(drained[0].intensity, 0.5f, 1.0e-6f);
            expectEquals((int) drained[0].timeSamples, 1000);
            expect(drained[1].type == FailureEventType::snag);
            expect(drained[2].type == FailureEventType::crinkle);

            FailureEvent drainedAgain[8];
            expectEquals(engine.popEvents(drainedAgain, 8), 0);
        }

        beginTest("Reset clears the counter without affecting queued events");
        {
            FailureEngine engine;
            engine.pushEvent(FailureEventType::wobble, 1.0f, 42);
            expectEquals((int) engine.getEventCount(), 1);

            engine.resetEventCount();
            expectEquals((int) engine.getEventCount(), 0);

            FailureEvent drained[4];
            expectEquals(engine.popEvents(drained, 4), 1);
            expect(drained[0].type == FailureEventType::wobble);
        }

        beginTest("Overflow beyond FIFO capacity is dropped without crashing");
        {
            FailureEngine engine;
            int succeeded = 0;
            for (int i = 0; i < 500; ++i)
                if (engine.pushEvent(FailureEventType::crinkle, 1.0f, i))
                    ++succeeded;

            expect(succeeded > 0 && succeeded <= 256);
            expectEquals((int) engine.getEventCount(), succeeded);
        }
    }
};

static FailureEngineFifoTests failureEngineFifoTests;
