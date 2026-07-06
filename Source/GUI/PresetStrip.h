#pragma once

#include "PresetArrowButton.h"
#include "PresetSaveButton.h"
#include "PresetDeleteButton.h"
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Preset strip: prev/name-plate/next/save row in the header, driving TapeRotAudioProcessor's
// native program list (factory presets + user presets). "Current program" isn't an APVTS
// parameter, so this polls getCurrentProgram()/getProgramName() on a timer (FailLamp-style)
// rather than using an attachment - keeps it in sync with host automation/program changes too.
class PresetStrip final : public juce::Component, private juce::Timer
{
public:
    explicit PresetStrip(TapeRotAudioProcessor& processor);
    ~PresetStrip() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

    // PresetStrip is sized to the full canvas (like Scope/FailLamp) but, unlike those pure-display
    // overlays, sits on top of real controls and needs actual mouse input of its own (the name
    // plate's right-click menu) - without narrowing hitTest to just the plate, its full-canvas
    // bounds would swallow every click meant for the knobs/switches underneath it everywhere else
    // on the panel. Child buttons (prev/next/save/delete) are NOT hit-tested independently of this:
    // JUCE's Component::getComponentAt checks a component's own hitTest before it ever recurses
    // into children, so any child positioned outside the region this returns true for is
    // unreachable by the mouse - hence also including their bounds below.
    bool hitTest(int x, int y) override;

private:
    void timerCallback() override;
    void showSaveAsPrompt();
    void showDeleteConfirmation();
    void showPresetListMenu();
    void updateDeleteButtonEnablement();

    TapeRotAudioProcessor& processorRef;

    PresetArrowButton prevButton{false};
    PresetArrowButton nextButton{true};
    PresetSaveButton saveButton;
    PresetDeleteButton deleteButton;

    std::unique_ptr<juce::AlertWindow> saveDialog;

    int displayedProgramIndex = -1;
    juce::String displayedProgramName;
};
