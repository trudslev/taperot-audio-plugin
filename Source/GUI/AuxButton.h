#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Momentary push-button for the STOP/FILTER/FAIL aux triggers: press to engage, release to
// disengage (rather than click-to-latch), while still driving an ordinary bool parameter via
// ButtonAttachment underneath - clickingTogglesState is deliberately left off, and mouseDown/
// mouseUp instead set the toggle state directly so the parameter reflects "held" rather than
// "toggled".
class AuxButton final : public juce::Button
{
public:
    explicit AuxButton(const juce::String& name);

private:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
};
