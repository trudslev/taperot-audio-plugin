#pragma once

#include "TapeRotTheme.h"
#include "TapeRotLookAndFeel.h"
#include "SectionPanel.h"
#include "DymoLabel.h"
#include "ToggleSwitch.h"
#include "FailureDotToggle.h"
#include "NoiseCharacterSwitch.h"
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class TapeRotEditorContent final : public juce::Component
{
public:
    explicit TapeRotEditorContent(TapeRotAudioProcessor&);
    ~TapeRotEditorContent() override;

private:
    TapeRotAudioProcessor& processorRef;
    TapeRotLookAndFeel lookAndFeel;
    SectionPanel sectionPanel;
    DymoLabel dymoLabel;

    std::array<std::unique_ptr<juce::Slider>, TapeRotTheme::Layout::knobs.size()> knobSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               TapeRotTheme::Layout::knobs.size()> knobAttachments;

    ToggleSwitch humSwitch{"Hum"};
    ToggleSwitch spreadSwitch{"Spread"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> humAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spreadAttachment;

    NoiseCharacterSwitch noiseCharacterSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseCharacterAttachment;

    std::array<std::unique_ptr<FailureDotToggle>, TapeRotTheme::Layout::failureDots.size()> failureDotToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,
               TapeRotTheme::Layout::failureDots.size()> failureDotAttachments;
};
