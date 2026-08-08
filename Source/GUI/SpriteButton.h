#pragma once

#include "TapeRotTheme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

/**
    One member of an exclusive-select group, drawn from an on/off sprite pair.

    The plate never changes between states - only the amber LED and its glow, both baked into the
    "on" sprite. Clicking selects this member; there is no toggle-off, because every group has
    exactly one member lit at all times.
*/
class SpriteButton final : public juce::Component
{
public:
    SpriteButton(juce::AudioProcessorValueTreeState&, const TapeRotTheme::Layout::ButtonSpec&,
                 const char* onData, int onSize, const char* offData, int offSize);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

    /** Fires on a user press so the header can take the LCD over. */
    std::function<void(const juce::String&)> onInteraction;

private:
    bool isSelected() const;

    juce::AudioProcessorValueTreeState& apvts;
    TapeRotTheme::Layout::ButtonSpec spec;
    juce::Image onImage, offImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpriteButton)
};
