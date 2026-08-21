#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    §5.4's GENERATION selector: eight stages, the lit one in the accent, unlit in the dark lamp
    material, with its caption above and a numeral under each stage.

    **A ladder rather than a knob, and §5.4 gives the reason**: the parameter is an integer count of
    tape generations, and a pointer implies interpolation between them. That is a statement about
    what the control means, not about how it looks, so it survives a restyling.

    Root `CLAUDE.md` holds an open ruling above this control - **GEN becomes non-automatable** - made
    on latency figures (~25 ms per stage, ~200 ms at GEN 8) that are themselves under re-measurement.
    Nothing here depends on that ruling either way: the ladder reads and writes a parameter, and
    whether a host may automate it is the parameter's business.
*/
class GenerationLadder : public juce::Component
{
public:
    GenerationLadder();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** 1-based, matching the printed numerals and the parameter. */
    void setGeneration (int oneBased);
    int getGeneration() const noexcept { return generation; }

    std::function<void (int)> onGenerationChanged;

    juce::Rectangle<int> canvasBounds() const;

private:
    int generation = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenerationLadder)
};
