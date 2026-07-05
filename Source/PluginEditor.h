#pragma once

#include "PluginProcessor.h"
#include "GUI/TapeRotLookAndFeel.h"
#include "GUI/GenericParameterPanel.h"
#include <juce_audio_processors/juce_audio_processors.h>

class TapeRotAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TapeRotAudioProcessorEditor(TapeRotAudioProcessor&);
    ~TapeRotAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    TapeRotAudioProcessor& processorRef;
    TapeRotLookAndFeel lookAndFeel;
    GenericParameterPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessorEditor)
};
