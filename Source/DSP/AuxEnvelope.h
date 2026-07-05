#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Shared engage/release envelope for the AUX performance effects (STOP/FILTER/FAIL), driven by
// one global RAMP time. Deliberately not juce::SmoothedValue: its reset(sampleRate, time) snaps
// currentValue to target, so changing the RAMP knob mid-fade would click. This instead recomputes
// the per-sample step fresh from the current ramp time on every advance, so retargeting the ramp
// speed while a fade is in flight stays continuous - only the rate of change changes.
class AuxEnvelope
{
public:
    void setSampleRate(double sr) noexcept { sampleRate = sr; }
    void setEngaged(bool engaged) noexcept { target = engaged ? 1.0f : 0.0f; }
    void setRampSeconds(float seconds) noexcept { rampSeconds = juce::jmax(0.001f, seconds); }

    // Advances by one sample and returns the eased 0..1 value.
    float getNextValue() noexcept
    {
        advanceRaw(1);
        return shaped();
    }

    // Advances by numSamples in one step, for callers that only need one value per block.
    float advanceBlock(int numSamples) noexcept
    {
        advanceRaw(numSamples);
        return shaped();
    }

    bool isIdle() const noexcept { return current <= 0.0f && target <= 0.0f; }

private:
    void advanceRaw(int numSamples) noexcept
    {
        const float totalSamples = juce::jmax(1.0f, rampSeconds * (float) sampleRate);
        const float maxStep = (float) numSamples / totalSamples;
        if (current < target)
            current = juce::jmin(target, current + maxStep);
        else if (current > target)
            current = juce::jmax(target, current - maxStep);
    }

    float shaped() const noexcept
    {
        return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * current);
    }

    double sampleRate = 44100.0;
    float rampSeconds = 0.3f;
    float current = 0.0f;
    float target = 0.0f;
};
