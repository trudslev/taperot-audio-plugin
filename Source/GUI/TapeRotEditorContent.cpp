#include "TapeRotEditorContent.h"

using namespace TapeRotTheme;

TapeRotEditorContent::TapeRotEditorContent(TapeRotAudioProcessor& p)
    : processorRef(p), failLamp(p), scope(p), genDigitDisplay(p)
{
    setSize((int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setLookAndFeel(&lookAndFeel);

    sectionPanel.setBounds(getLocalBounds());
    addAndMakeVisible(sectionPanel);

    dymoLabel.setBounds(getLocalBounds());
    addAndMakeVisible(dymoLabel);

    for (size_t i = 0; i < Layout::knobs.size(); ++i)
    {
        const auto& spec = Layout::knobs[i];
        auto slider = TapeRotLookAndFeel::createKnobSlider(spec.label);

        const float half = Layout::knobTickOuterRadius + 3.0f;
        slider->setBounds((int) (spec.x - half), (int) (Layout::knobCentreY - half),
                           (int) (half * 2.0f), (int) (half * 2.0f));

        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, spec.paramID, *slider);

        addAndMakeVisible(*slider);
        knobSliders[i] = std::move(slider);
    }

    humSwitch.setBounds((int) Layout::humSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                         (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    humAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::hum, humSwitch);
    addAndMakeVisible(humSwitch);

    spreadSwitch.setBounds((int) Layout::spreadSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                           (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::spread, spreadSwitch);
    addAndMakeVisible(spreadSwitch);

    noiseCharacterSwitch.setBounds((int) Layout::noiseSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                                    (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    noiseCharacterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, ParamIDs::noiseCharacter, noiseCharacterSwitch);
    addAndMakeVisible(noiseCharacterSwitch);

    switchModeSwitch.setBounds((int) Layout::switchModeSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                                (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    switchModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::switchMode, switchModeSwitch);
    addAndMakeVisible(switchModeSwitch);

    modelComboBox.setBounds((int) (Layout::modelDropdownCentreX - Layout::modelDropdownW * 0.5f),
                             (int) Layout::modelDropdownY, (int) Layout::modelDropdownW, (int) Layout::modelDropdownH);
    modelComboBox.setJustificationType(juce::Justification::centred);
    for (size_t i = 0; i < kNumTapeModels; ++i)
        modelComboBox.addItem(kTapeModels[i].displayName, (int) i + 1);
    modelComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, ParamIDs::model, modelComboBox);
    addAndMakeVisible(modelComboBox);

    for (size_t i = 0; i < Layout::failureDots.size(); ++i)
    {
        const auto& spec = Layout::failureDots[i];
        const float x = Layout::failureDotFirstX + (float) i * Layout::failureDotSpacing;
        const float half = Layout::failureDotRadius + 3.0f;

        auto dot = std::make_unique<FailureDotToggle>(spec.label);
        dot->setBounds((int) (x - half), (int) (Layout::failureDotY - half), (int) (half * 2.0f), (int) (half * 2.0f));

        failureDotAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.apvts, spec.paramID, *dot);

        addAndMakeVisible(*dot);
        failureDotToggles[i] = std::move(dot);
    }

    const float genTotalW = 8.0f * Layout::genSelectorSegmentW + 7.0f * Layout::genSelectorGap;
    genSelector.setBounds((int) (Layout::genSelectorCentreX - genTotalW * 0.5f), (int) Layout::genSelectorY,
                           (int) genTotalW, (int) Layout::genSelectorSegmentH);
    genAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, ParamIDs::gen, genSelector);
    addAndMakeVisible(genSelector);

    auto setupSmallKnob = [&](SmallKnob& knob, float x, const char* paramID,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment)
    {
        const float half = Layout::smallKnobRadius + 3.0f;
        knob.setBounds((int) (x - half), (int) (Layout::smallKnobCentreY - half), (int) (half * 2.0f), (int) (half * 2.0f));
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, paramID, knob);
        addAndMakeVisible(knob);
    };

    setupSmallKnob(lpKnob, Layout::lpKnobX, ParamIDs::lp, lpAttachment);
    setupSmallKnob(rampKnob, Layout::rampKnobX, ParamIDs::ramp, rampAttachment);
    setupSmallKnob(hpKnob, Layout::hpKnobX, ParamIDs::hp, hpAttachment);

    auto setupAuxButton = [&](AuxButton& button, float x, const char* paramID,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment)
    {
        const float half = Layout::auxButtonRadius + 3.0f;
        button.setBounds((int) (x - half), (int) (Layout::auxButtonCentreY - half), (int) (half * 2.0f), (int) (half * 2.0f));
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, paramID, button);
        addAndMakeVisible(button);
    };

    setupAuxButton(stopButton, Layout::stopButtonX, ParamIDs::stop, stopAttachment);
    setupAuxButton(filterButton, Layout::filterButtonX, ParamIDs::filterAux, filterAttachment);
    setupAuxButton(failButton, Layout::failButtonX, ParamIDs::failAux, failAttachment);

    // Scope/FailLamp/GenDigitDisplay draw with absolute canvas coordinates (like SectionPanel and
    // DymoLabel), so they're sized to the full canvas rather than a sub-region - a JUCE
    // Component's paint() coordinates are always local to its own top-left, not the canvas origin.
    scope.setBounds(getLocalBounds());
    addAndMakeVisible(scope);

    failLamp.setBounds(getLocalBounds());
    addAndMakeVisible(failLamp);

    genDigitDisplay.setBounds(getLocalBounds());
    addAndMakeVisible(genDigitDisplay);
}

TapeRotEditorContent::~TapeRotEditorContent()
{
    setLookAndFeel(nullptr);
}
