#include "PluginEditor.h"

namespace
{
    juce::StringArray getAllParamIDsInOrder()
    {
        return {
            ParamIDs::drive, ParamIDs::wow, ParamIDs::flutter, ParamIDs::model,
            ParamIDs::noise, ParamIDs::hum, ParamIDs::failure, ParamIDs::mix, ParamIDs::output,
            ParamIDs::spread, ParamIDs::failureDropouts, ParamIDs::failureSnags,
            ParamIDs::failureCrinkles, ParamIDs::failureImbalance
        };
    }
}

TapeRotAudioProcessorEditor::TapeRotAudioProcessorEditor(TapeRotAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), panel(p.apvts, getAllParamIDsInOrder())
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(panel);
    setSize(720, 420);
}

TapeRotAudioProcessorEditor::~TapeRotAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void TapeRotAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void TapeRotAudioProcessorEditor::resized()
{
    panel.setBounds(getLocalBounds());
}
