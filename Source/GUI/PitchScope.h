#pragma once

#include "TapeRotTheme.h"
#include <vector>

class TapeRotAudioProcessor;
class LampStrip;

/**
    The pitch-deviation scope: the decorative waveform's replacement.

    It plots combined wow and flutter as pitch deviation in cents over the last four seconds, with
    margin readouts for the deviation range, the division rate, the current wow and flutter rates
    and the GEN count. Failure events show up in the trace on their own - a dropout or a snag moves
    the delay line, and moving the delay line IS pitch deviation - so nothing special is drawn for
    them.

    Everything it reads comes from lock-free taps that drop rather than block, so a scope that
    cannot keep up slows nothing down.
*/
class PitchScope final : public juce::Component,
                         private juce::Timer
{
public:
    PitchScope(TapeRotAudioProcessor&, const LampStrip&);

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;
    const LampStrip& lamps;

    std::vector<float> trace;      // ring of decimated deviation, oldest to newest after rotation
    size_t writeIndex = 0;
    float displayedRangeCents = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchScope)
};
