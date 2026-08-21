#pragma once

#include "TapeRotTheme.h"
#include <vector>

class TapeRotAudioProcessor;

/**
    §4's wow/flutter scope: 1308 x 164 at (16, 136), and **the casting's signature display**.

    It plots combined wow and flutter as pitch deviation in cents over the visible window. §4 calls
    the trace "the sum of three sines - wow, flutter and drift"; here it is the real measured
    deviation rather than a reconstruction of it, which is the same thing when the transport is
    behaving and the honest thing when it is not. Failure events show up on their own - a dropout or
    a snag moves the delay line, and moving the delay line IS pitch deviation - so nothing special
    is drawn for them.

    **Four readouts, one per corner, all live and all drawn at runtime**, plus the FAIL lamp in the
    bottom right. §10's Outstanding records that the delivered render shows `±34 cents` and
    `WOW 0.50 Hz · FLUT 11.2 Hz` as **samples**; they are wired to real signal here.

    Everything it reads comes from lock-free taps that drop rather than block, so a scope that
    cannot keep up slows nothing down.
*/
class PitchScope final : public juce::Component,
                         private juce::Timer
{
public:
    explicit PitchScope (TapeRotAudioProcessor&);

    void paint (juce::Graphics&) override;

    /** The scope well's own box on the canvas. The component IS the well - unlike revision 1,
        whose readouts sat outside it on bare plate. */
    static juce::Rectangle<int> canvasBounds();

    /** §7.3: lit when any failure is currently sounding. Driven from the editor rather than read
        here, so the one authority for every lamp on the panel is §7.3's table. */
    void setFailLampLit (bool shouldBeLit);

private:
    void timerCallback() override;
    void paintWell (juce::Graphics&, juce::Rectangle<float> well);
    void paintReadouts (juce::Graphics&, juce::Rectangle<float> well);

    TapeRotAudioProcessor& processorRef;

    std::vector<float> trace;      // ring of decimated deviation, oldest to newest after rotation
    size_t writeIndex = 0;
    float displayedRangeCents = 0.0f;
    bool failLit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchScope)
};
