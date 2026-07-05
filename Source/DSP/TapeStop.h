#pragma once

#include "AuxEnvelope.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Motor spin-down "tape stop" effect: continuously records input into a large circular buffer and
// reads it back at a variable rate (1.0 = normal, falling toward 0.0 = fully stopped), so pitch
// dives as speed falls. WowFlutter's own delay line is small and bounded (~55ms max) for its
// wow/flutter wobble; a multi-second spin-down needs effectively unbounded delay growth instead,
// hence this separate buffer. The read/write gap is clamped to the buffer size as a safety net
// against unbounded growth if STOP is held far longer than a normal performance gesture.
class TapeStop
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, bool engaged, float rampSeconds);

private:
    static constexpr float bufferSeconds = 8.0f;
    static constexpr float safetyMarginSeconds = 0.25f;
    static constexpr float wobbleRateHz = 1.3f;
    static constexpr float wobbleDepth = 0.08f;

    std::vector<std::vector<float>> history;
    int bufferLengthSamples = 0;
    juce::int64 writePos = 0;
    std::vector<double> readPos;
    double sampleRate = 44100.0;
    double wobblePhase = 0.0;

    AuxEnvelope envelope;
};
