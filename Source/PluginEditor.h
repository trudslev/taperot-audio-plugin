#pragma once

#include "PluginProcessor.h"
#include "GUI/TapeRotEditorContent.h"
#include <juce_audio_processors/juce_audio_processors.h>

class TapeRotAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TapeRotAudioProcessorEditor(TapeRotAudioProcessor&);
    ~TapeRotAudioProcessorEditor() override;

    void resized() override;

private:
    TapeRotAudioProcessor& processorRef;
    TapeRotEditorContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessorEditor)
};
