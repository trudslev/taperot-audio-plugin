#pragma once

#include "TapeRotTheme.h"
#include "ProgramMenuLookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>

class TapeRotAudioProcessor;

/**
    The PROGRAM display, SAVE / DELETE, the MODEL readout and the numeric IN / OUT meters.

    The LCD does double duty. Normally it shows the bank chip and the program name. While a control
    is being moved it is TAKEN OVER by `PARAMETER: value unit` and reverts 1.1 s after release.

    Only direct manipulation triggers the takeover. A SliderAttachment also raises its callback when
    a Program is applied and on every host automation step, so the caller guards on the control's own
    drag state - otherwise the display latches onto whichever parameter was written last and, with
    automation running, flickers for the length of a song.
*/
class ProgramHeader final : public juce::Component,
                            private juce::Timer
{
public:
    explicit ProgramHeader(TapeRotAudioProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    /** Spans the canvas to draw the LCD, MODEL readout and IN/OUT numerals, but owns only the
        three clickable header cells. See LampStrip::hitTest for why this matters. */
    bool hitTest(int x, int y) override;

    void showParameter(const juce::String& paramId);
    void releaseParameter();
    void refresh() { repaint(); }

private:
    void timerCallback() override;
    void showProgramMenu();
    juce::String lcdText() const;
    juce::String describe(const juce::String& paramId) const;

    TapeRotAudioProcessor& processorRef;
    juce::String editingParam;
    /** Outlives showMenuAsync's callback, so it must be a member rather than a local. */
    ProgramMenuLookAndFeel menuLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgramHeader)
};
