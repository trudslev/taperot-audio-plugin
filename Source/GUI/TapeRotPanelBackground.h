#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    The fascia and its fixed furniture: material, bezel, thumbscrews, section dividers, section
    headings and the footer row.

    **What is NOT here is as deliberate as what is.** Group captions and shoe/lamp legends belong to
    the controls they caption, not to the background, because §5.1 makes the legend part of the
    shoe's contract - "printed once per position, centred under their own segment, never re-inked
    and never moved" - and a legend drawn from a second table here is a table that has to be kept in
    step with a geometry it cannot see. Chorus-60's `GroupPrintedLayer` partitions its knobs by
    containment for the same reason: a grouping nobody has to maintain cannot drift.

    Painted into a layer keyed on device scale. Nothing on it changes.
*/
class TapeRotPanelBackground : public juce::Component
{
public:
    TapeRotPanelBackground();

    void paint (juce::Graphics&) override;
    void resized() override;

    int staticLayerBuildCount() const noexcept { return buildCount; }

private:
    void renderStaticLayer (float deviceScale);

    juce::Image staticLayer;
    float builtAtScale = 0.0f;
    int buildCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeRotPanelBackground)
};
