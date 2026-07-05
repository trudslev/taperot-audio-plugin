#include "TapeRotEditorContent.h"

using namespace TapeRotTheme;

TapeRotEditorContent::TapeRotEditorContent(TapeRotAudioProcessor& p)
    : processorRef(p)
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
}

TapeRotEditorContent::~TapeRotEditorContent()
{
    setLookAndFeel(nullptr);
}
