#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

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

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float failureAmount01,
                 bool dropoutsEnabled, bool snagsEnabled,
                 bool crinklesEnabled, bool imbalanceEnabled);

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

    static constexpr float dropoutRatePerSecAtFull = 1.2f;
    static constexpr float snagRatePerSecAtFull = 1.6f;
    static constexpr float crinkleRatePerSecAtFull = 0.6f;
    static constexpr float wobbleRatePerSecAtFull = 0.35f;

    static constexpr float dropoutMinMs = 8.0f, dropoutMaxMs = 45.0f;
    static constexpr float snagMinMs = 15.0f, snagMaxMs = 55.0f;
    static constexpr float crinkleMinMs = 40.0f, crinkleMaxMs = 150.0f;
    static constexpr float wobbleMinMs = 150.0f, wobbleMaxMs = 500.0f;

    static constexpr float snagFlutterHz = 90.0f;
    static constexpr float crinkleNoiseLevel = 0.18f;
    static constexpr float crinkleHpfHz = 2500.0f;

    struct EventState
    {
        bool active = false;
        int totalSamples = 0;
        int elapsedSamples = 0;
        float intensity = 0.0f;
        int channel = 0;
    };

    // Audio-thread only: rolls the per-sample dice for a new event and pushes a FIFO entry if one starts.
    void triggerIfDue(EventState& state, bool enabled, float ratePerSecAtFull, float minMs, float maxMs,
                       float failureAmount01, FailureEventType type, juce::int64 sampleTime) noexcept;

    juce::AbstractFifo eventFifo;
    std::array<FailureEvent, kFifoCapacity> eventBuffer;
    std::atomic<juce::int64> eventCounter{0};

    EventState dropoutState, snagState, crinkleState, wobbleState;

    // Re-seeded in prepare(), NOT in reset() — the ruling and its figures are beside that line.
    // It was seeded here at construction and nowhere else, which made two renders of the same audio
    // through one instance different performances: a measured self-comparison of 0.914 at FAILURE 100.
    static constexpr juce::int64 generatorSeed = (juce::int64) 0x9E3779B97F4A7C15LL;
    juce::Random random{generatorSeed};
    double sampleRate = 44100.0;
    juce::int64 samplePosition = 0;
    float crinkleHpfCoeff = 0.0f;

    std::vector<float> crinkleHpfState;
    std::vector<float> crinkleHpfPrevInput;
};
