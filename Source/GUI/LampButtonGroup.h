#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    §5.3's round lamp-buttons: Ø26 dark caps each with an Ø11 lamp in its face, a legend below, and
    a group caption above.

    Three groups use it, and they are not the same KIND of control:

    | Group | Behaviour |
    |---|---|
    | FAIL (STP / FLT / FAI) | momentary triggers - press to fire, lamp reports the engine |
    | NOISE BED (TAPE / VCR / DUST) | **exactly one lit**, a three-state selector |
    | FAULT ACTIVITY (DRP / SNG / CRK / WBL) | independent toggles, one per fault category |

    That is why lit state is set from outside rather than owned here: a component that decided its
    own exclusivity would need to know which of the three it was, and §7.3's table is the authority
    for all three.

    **NOISE BED is a lamp group rather than a shoe under §4B's own scope clause**, not as a
    fallback: it is a three-state control in a 162 px section, and the part's 168 x 45 three-state
    footprint does not fit without moving a divider and the 176 px group beyond it. §5.2 names
    TapeRot as the clause's instance, so this is the shape the shared part anticipates.

    §5.3: **light stops at the lens edge** - no halo on the fascia, and an unlit lamp is a dark
    lens, not a hole.
*/
class LampButtonGroup : public juce::Component
{
public:
    explicit LampButtonGroup (const TapeRotTheme::Switches::LampGroup& spec);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setLit (int index, bool shouldBeLit);
    bool isLit (int index) const;

    std::function<void (int)> onPressed;
    std::function<void (int)> onReleased;

    juce::Rectangle<int> canvasBounds() const;

private:
    int indexAt (juce::Point<float> local) const;

    TapeRotTheme::Switches::LampGroup groupSpec;
    std::array<bool, 4> lit { { false, false, false, false } };
    int heldIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LampButtonGroup)
};
