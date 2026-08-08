#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

/**
    Carries the realised pitch deviation from the audio thread to the scope.

    Single producer (audio), single consumer (GUI), lock-free, non-allocating, and it **drops rather
    than blocks** when full — an editor that is closed, stalled or repainting slowly costs the audio
    thread nothing. Deliberately the same idiom as `FailureEngine`'s event FIFO, so the codebase has
    one shape for "audio thread hands the GUI something", not two.

    **Decimation takes the extreme of each window, not a point sample.** Flutter is a fast, spiky
    modulation; point-sampling every 64th value walks straight past a transient and draws a trace
    that looks calmer than the audio actually is. Taking the largest magnitude and keeping its sign
    preserves the spike at the cost of nothing.
*/
class PitchDeviationMeter
{
public:
    /** ~750 Hz at 48 k. The scope shows four seconds across 1250 px, so this is comfortably more
        resolution than the well can draw, with room for the GUI to run slow. */
    static constexpr int decimationFactor = 64;
    static constexpr int fifoCapacity = 4096;

    PitchDeviationMeter() : fifo(fifoCapacity), storage((size_t) fifoCapacity, 0.0f) {}

    void prepare(int maximumBlockSize)
    {
        scratch.assign((size_t) juce::jmax(1, maximumBlockSize), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        windowExtreme = 0.0f;
        windowCount = 0;
        fifo.reset();
    }

    /** Audio thread. The buffer the GEN stages accumulated into, for this block only. */
    float* getScratch(int numSamples) noexcept
    {
        if ((int) scratch.size() < numSamples)
            return nullptr;                       // never allocates here; prepare() sizes it

        std::fill(scratch.begin(), scratch.begin() + numSamples, 0.0f);
        return scratch.data();
    }

    /** Audio thread. Decimates the block and pushes; silently drops if the GUI is not draining. */
    void pushBlock(const float* deviationCents, int numSamples) noexcept
    {
        if (deviationCents == nullptr)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float v = deviationCents[i];

            if (std::abs(v) > std::abs(windowExtreme))
                windowExtreme = v;

            if (++windowCount >= decimationFactor)
            {
                writeOne(windowExtreme);
                windowExtreme = 0.0f;
                windowCount = 0;
            }
        }
    }

    /** GUI thread. Drains up to maxValues, returns how many were read. */
    int pop(float* dest, int maxValues) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(maxValues, start1, size1, start2, size2);

        int n = 0;
        for (int i = 0; i < size1; ++i) dest[n++] = storage[(size_t)(start1 + i)];
        for (int i = 0; i < size2; ++i) dest[n++] = storage[(size_t)(start2 + i)];

        fifo.finishedRead(size1 + size2);
        return n;
    }

    /** Samples per second the GUI can expect, for laying the trace out against the time axis. */
    static double outputRate(double sampleRate) noexcept
    {
        return sampleRate / (double) decimationFactor;
    }

private:
    void writeOne(float v) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);

        if (size1 + size2 < 1)
        {
            fifo.finishedWrite(0);                // full: drop, never block
            return;
        }

        storage[(size_t)(size1 > 0 ? start1 : start2)] = v;
        fifo.finishedWrite(1);
    }

    juce::AbstractFifo fifo;
    std::vector<float> storage;
    std::vector<float> scratch;
    float windowExtreme = 0.0f;
    int windowCount = 0;
};
