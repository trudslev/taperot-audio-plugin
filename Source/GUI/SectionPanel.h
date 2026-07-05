#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SectionPanel final : public juce::Component
{
public:
    SectionPanel();
    void paint(juce::Graphics&) override;

private:
    void generateSpeckleImage();

    void paintBezelAndPanel(juce::Graphics&);
    void paintHeader(juce::Graphics&);
    void paintSectionLabelsAndDividers(juce::Graphics&);
    void paintKnobLabels(juce::Graphics&);
    void paintSwitchLabels(juce::Graphics&);
    void paintFailureDotLabels(juce::Graphics&);
    void paintNewControlLabels(juce::Graphics&);
    void paintCounterHousing(juce::Graphics&);
    void paintFailLabel(juce::Graphics&);
    void paintScrews(juce::Graphics&);
    void paintVersionText(juce::Graphics&);

    juce::Image speckleImage;
};
