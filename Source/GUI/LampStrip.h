#pragma once

#include "TapeRotTheme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>

class TapeRotAudioProcessor;

/**
    The three lamp clusters, all drawn from the one shared diameter-22 sprite pair.

    - GENERATION: eight segments, 1..GEN lit, following the smoothed GEN value so raising the
      control lights them in step with what the cascade is actually doing.
    - FAULT ACTIVITY: four dots that flash for 260 ms when FailureEngine reports an event of that
      type. Driven by the engine's existing event FIFO, not by the enable switches - a dot lights
      because something happened, not because something is allowed to happen.
    - FAIL buttons: STP / FLT / FAI, momentary. Held while the pointer is down.

    The scope strip's FAIL LED is lit whenever any of the three is held, so it lives here too.
*/
class LampStrip final : public juce::Component,
                        private juce::Timer
{
public:
    explicit LampStrip(TapeRotAudioProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    bool isAnyFailHeld() const noexcept;

private:
    void timerCallback() override;
    int failButtonAt(juce::Point<float>) const;

    TapeRotAudioProcessor& processorRef;

    std::array<juce::uint32, 4> faultFlashUntil {};   // ms timestamps
    int heldFailButton = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LampStrip)
};
