#pragma once

#include "TapeRotTheme.h"
#include "TapeRotLookAndFeel.h"
#include "SectionPanel.h"
#include "DymoLabel.h"
#include "ToggleSwitch.h"
#include "FailureDotToggle.h"
#include "NoiseCharacterSwitch.h"
#include "SmallKnob.h"
#include "GenSelector.h"
#include "AuxButton.h"
#include "FailLamp.h"
#include "Scope.h"
#include "GenDigitDisplay.h"
#include "ModelReadout.h"
#include "PresetStrip.h"
#include "TooltipHoverArea.h"
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
    ToggleSwitch switchModeSwitch{"Switch"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> humAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> switchModeAttachment;

    NoiseCharacterSwitch noiseCharacterSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseCharacterAttachment;

    ModelReadout modelReadout;

    std::array<std::unique_ptr<FailureDotToggle>, TapeRotTheme::Layout::failureDots.size()> failureDotToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,
               TapeRotTheme::Layout::failureDots.size()> failureDotAttachments;

    GenSelector genSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> genAttachment;

    SmallKnob lpKnob, rampKnob, hpKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpAttachment, rampAttachment, hpAttachment;

    AuxButton stopButton{"Stop"}, filterButton{"Filter"}, failButton{"Fail"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stopAttachment, filterAttachment, failAttachment;

    FailLamp failLamp;
    Scope scope;
    GenDigitDisplay genDigitDisplay;
    PresetStrip presetStrip;

    // SectionPanel paints every control's label as static text and never intercepts mouse input,
    // so hovering a label alone (rather than the control itself) wouldn't show a tooltip without
    // these - one invisible hover-only area per label, positioned to match SectionPanel's painted
    // rect exactly, each carrying the same tooltip text as its associated control.
    std::vector<std::unique_ptr<TooltipHoverArea>> labelTooltipAreas;

    // Single shared popup for every control's setTooltip() text below - scoped to this component
    // (rather than nullptr/whole-desktop) so it only ever considers TapeRot's own controls.
    juce::TooltipWindow tooltipWindow{this};
};
