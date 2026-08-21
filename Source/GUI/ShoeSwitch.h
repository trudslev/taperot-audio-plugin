#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    §4B's two-state shoe: 128 x 32 in two 64 halves, with its caption above and a printed legend
    under each half.

    **The legends never change and never move.** §5.1 states it and §7.2's matrix confirms it - six
    cells, one rule, no legend change in any of them: the shoe carries the state by which half is
    pale. That is what makes this a shoe rather than a two-position lamp, and it is why the legends
    are drawn here beside the geometry that positions them rather than from a table in the panel
    background that would have to be kept in step with a shoe it cannot see.

    Three groups use it - SWITCHING (FADE / CLUNK), HUM (OFF / ON) and SPREAD (LINKED / STEREO).
    **NOISE BED does not**, under §4B's own scope clause; see `LampButtonGroup`.
*/
class ShoeSwitch : public juce::Component
{
public:
    explicit ShoeSwitch (const TapeRotTheme::Switches::Shoe& spec);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** True selects the RIGHT half. The parameter is a bool in all three groups, and which position
        `true` names is the parameter's business, not the shoe's. */
    void setRightSelected (bool shouldBeRight);
    bool isRightSelected() const noexcept { return rightSelected; }

    std::function<void (bool)> onSelectionChanged;

    /** The component's box: the shoe, its caption above and its legend row below. */
    juce::Rectangle<int> canvasBounds() const;

private:
    void paintHalf (juce::Graphics&, juce::Rectangle<float>, bool engaged) const;

    TapeRotTheme::Switches::Shoe shoeSpec;
    bool rightSelected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShoeSwitch)
};
