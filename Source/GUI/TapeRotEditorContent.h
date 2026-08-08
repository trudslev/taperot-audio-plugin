#pragma once

#include "TapeRotTheme.h"

class TapeRotAudioProcessor;

/**
    The fixed 1336 x 679 reference canvas. PluginEditor applies one uniform scale transform above
    this; nothing below ever sees it.
*/
class TapeRotEditorContent final : public juce::Component
{
public:
    explicit TapeRotEditorContent(TapeRotAudioProcessor&);
    ~TapeRotEditorContent() override;

    void paint(juce::Graphics&) override;

private:
    TapeRotAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotEditorContent)
};
