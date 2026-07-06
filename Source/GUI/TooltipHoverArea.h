#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Invisible passive area carrying a tooltip, for label text painted by SectionPanel (which never
// intercepts mouse input, since it's otherwise a pure background layer) - without this, hovering a
// control's *label* did nothing, only hovering the control itself did, since JUCE only shows a
// tooltip for whichever component is actually found under the mouse.
class TooltipHoverArea final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TooltipHoverArea() { setInterceptsMouseClicks(true, false); }
};
