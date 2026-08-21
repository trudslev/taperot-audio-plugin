#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    One rotary control: its sweep arc, tick ring, numerals, unit, cap, pointer and label.

    **Renamed from `KnobFilmstrip`, and the rename is the point.** Call 5 retired the sheets; this
    draws the cap in code. Chorus-60 shipped a `KnobFilmstripComponent` that drew no filmstrip for
    long enough that a performance investigation went looking for a resampled sheet blit that did
    not exist - the mechanism was right in kind and wrong in object, and the name is what aimed it
    wrong. A class named for artwork it no longer touches sends the next reader to the wrong lever.

    **The ring is drawn from the parameter's own `NormalisableRange`, never from stored angles.**
    `juce::SliderParameterAttachment` copies the parameter's range onto the slider, so
    `getNormalisableRange()` here IS the taper that drives the pointer. That makes the class of
    defect BRAND.md calls a correctness requirement - a ring legending a taper the control no longer
    has - unreachable by construction rather than checked for: change the skew and the numerals move
    with the pointer, because they are the same computation.
*/
class KnobComponent : public juce::Slider
{
public:
    struct Spec
    {
        const char* label;
        const char* unit;                       // empty where §3.2 prints none
        TapeRotTheme::Layout::Cap cap;
        juce::Point<float> pivot;
        float labelBaselineY;                   // canvas y; §3.1 pins MODEL's to the shared line
        const TapeRotTheme::Marks::Mark* marks; // null for MODEL, which is detents
        int markCount;
    };

    explicit KnobComponent (const Spec& spec);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;

    /** The component's box on the canvas: wide enough for the numeral ring and tall enough for the
        label, which is why it is not the cap's own diameter. */
    juce::Rectangle<int> canvasBounds() const;

    /** Rebuild count, asserted by the tests. **A cache with no rebuild counter is a cache nobody
        has checked** - root `CLAUDE.md` records ModScope's 2.5 ms going two rounds unexplained for
        exactly this reason, as the one cache of three without one. */
    int staticLayerBuildCount() const noexcept { return buildCount; }

private:
    void renderStaticLayer (float deviceScale);
    float angleForValue (float value) const;
    void paintCap (juce::Graphics&, juce::Point<float> centre, float radius) const;

    Spec knobSpec;
    juce::Image staticLayer;
    float builtAtScale = 0.0f;
    double builtStart = 0.0, builtEnd = 0.0, builtSkew = 0.0;
    int buildCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobComponent)
};
