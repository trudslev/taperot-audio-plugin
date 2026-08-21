#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TapeRotTheme.h"

/**
    §3.3's MACHINE readout: 134 x 27 at (189, 566), Share Tech Mono 12.5 / 16, `#f2b25c` on the LCD
    material.

    **This readout is MODEL's label.** §3.3 makes that explicit and gives the reason: nine machine
    names printed around a Ø104 dial would not fit at the type floor, and the readout already has to
    exist to show which is loaded - so the fascia carries no machine names and the ring carries no
    numerals. The two halves of that decision are load-bearing together: delete the readout and the
    signature control becomes an unlabelled nine-position dial.
*/
class MachineReadout : public juce::Component
{
public:
    MachineReadout();

    void paint (juce::Graphics&) override;

    void setMachineName (const juce::String& name);

    static juce::Rectangle<int> canvasBounds();

private:
    juce::String machineName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MachineReadout)
};
