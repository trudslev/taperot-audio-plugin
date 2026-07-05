#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

enum class FailureEventType
{
    dropout,
    snag,
    crinkle,
    wobble
};

struct FailureEvent
{
    FailureEventType type = FailureEventType::dropout;
    float intensity = 0.0f;
    juce::int64 timeSamples = 0;
};

class FailureEngine
{
public:
    FailureEngine() : eventFifo(kFifoCapacity) {}

    void prepare(const juce::dsp::ProcessSpec&) {}
    void reset() {}
    void process(juce::AudioBuffer<float>&, float /*failureAmount01*/,
                 bool /*dropoutsEnabled*/, bool /*snagsEnabled*/,
                 bool /*crinklesEnabled*/, bool /*imbalanceEnabled*/) {}

    // Audio-thread only: never blocks, never allocates; drops the event if the FIFO is full.
    bool pushEvent(FailureEventType type, float intensity, juce::int64 timeSamples) noexcept
    {
        int start1, size1, start2, size2;
        eventFifo.prepareToWrite(1, start1, size1, start2, size2);

        if (size1 + size2 < 1)
        {
            eventFifo.finishedWrite(0);
            return false;
        }

        const int index = size1 > 0 ? start1 : start2;
        eventBuffer[(size_t) index] = {type, intensity, timeSamples};
        eventFifo.finishedWrite(1);

        eventCounter.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // GUI-thread only: drains up to maxEvents into destBuffer, returns the number actually read.
    int popEvents(FailureEvent* destBuffer, int maxEvents) noexcept
    {
        int start1, size1, start2, size2;
        eventFifo.prepareToRead(maxEvents, start1, size1, start2, size2);

        int numRead = 0;
        for (int i = 0; i < size1; ++i)
            destBuffer[numRead++] = eventBuffer[(size_t) (start1 + i)];
        for (int i = 0; i < size2; ++i)
            destBuffer[numRead++] = eventBuffer[(size_t) (start2 + i)];

        eventFifo.finishedRead(size1 + size2);
        return numRead;
    }

    juce::int64 getEventCount() const noexcept { return eventCounter.load(std::memory_order_relaxed); }
    void resetEventCount() noexcept { eventCounter.store(0, std::memory_order_relaxed); }

private:
    static constexpr int kFifoCapacity = 256;

    juce::AbstractFifo eventFifo;
    std::array<FailureEvent, kFifoCapacity> eventBuffer;
    std::atomic<juce::int64> eventCounter{0};
};
