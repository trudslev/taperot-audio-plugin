#pragma once

#include "KnobFilmstrip.h"
#include "LampStrip.h"
#include "PitchScope.h"
#include "ProgramHeader.h"
#include "SpriteButton.h"
#include "TapeRotTheme.h"

#include <memory>
#include <vector>

class TapeRotAudioProcessor;

/**
    The fixed 1336 x 679 reference canvas. PluginEditor applies one uniform scale transform above
    this; nothing below ever sees it.

    Draw order is the plate, then everything that sits on it. The plate is a BARE panel - it carries
    the printed labels, scale legends, tick marks and the nameplate, but none of the live controls,
    so no element here sits over a baked copy of itself.
*/
class TapeRotEditorContent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit TapeRotEditorContent(TapeRotAudioProcessor&);
    ~TapeRotEditorContent() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    TapeRotAudioProcessor& processorRef;

    LampStrip lamps;
    PitchScope scope;
    ProgramHeader header;

    /** Paints nothing and claims no clicks of its own; it exists so the Program list has a parent
        area to be laid out in. Its bounds are what stop the list moving or overflowing the panel -
        see the constructor, and ../../CLAUDE.md's "The Program dropdown". */
    juce::Component menuHost;

    std::vector<std::unique_ptr<KnobFilmstrip>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<SpriteButton>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotEditorContent)
};
