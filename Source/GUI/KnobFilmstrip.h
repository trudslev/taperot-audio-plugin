#pragma once

#include "TapeRotTheme.h"

/**
    A knob drawn from a vertical bitmap filmstrip.

    Subclasses juce::Slider purely for its drag-to-value mapping and SliderAttachment compatibility;
    paint() fully replaces the look, so no LookAndFeel is involved. The sprite is the CAP ONLY - the
    ticks, numerals, unit and the control's name are printed in the panel plate, positioned by eye,
    and nothing here may redraw them.

    MODEL uses the same class with a 9-frame strip indexed directly by model index. The generic
    frame formula collapses to the same thing for a stepped slider, so there is no special case:
    round(value01 * (frames - 1)) is the model index when the slider's range is 0..8.
*/
class KnobFilmstrip final : public juce::Slider
{
public:
    explicit KnobFilmstrip(TapeRotTheme::Layout::Cap cap);

    void paint(juce::Graphics&) override;

    /** Places the component at the sprite's top-left, as the spec states it (bleed included). */
    void setSpriteTopLeft(juce::Point<float> topLeft);

private:
    TapeRotTheme::Layout::Cap capKind;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobFilmstrip)
};
