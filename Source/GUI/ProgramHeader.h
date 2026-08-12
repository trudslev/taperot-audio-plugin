#pragma once

#include "TapeRotTheme.h"

#include <nf/ParameterReadout.h>
#include "ProgramMenuLookAndFeel.h"
#include "../DSP/FactoryPrograms.h"      // ProgramId / ProgramBank
#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

class TapeRotAudioProcessor;

/**
    The PROGRAM display, SAVE / DELETE, the MODEL readout and the numeric IN / OUT meters.

    The LCD does double duty. Normally it shows the bank chip and the program name. While a control
    is being moved it is TAKEN OVER by `PARAMETER: value unit` and reverts 1.1 s after release.

    Only direct manipulation triggers the takeover. A SliderAttachment also raises its callback when
    a Program is applied and on every host automation step, so the caller guards on the control's own
    drag state - otherwise the display latches onto whichever parameter was written last and, with
    automation running, flickers for the length of a song.
*/
class ProgramHeader final : public juce::Component,
                            private juce::Timer
{
public:
    explicit ProgramHeader(TapeRotAudioProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost (FocusChangeType) override;

    /** Spans the canvas to draw the LCD, MODEL readout and IN/OUT numerals, but owns only the
        three clickable header cells. See LampStrip::hitTest for why this matters. */
    bool hitTest(int x, int y) override;

    void showParameter(const juce::String& paramId);
    void releaseParameter();



    /** The component the Program list is laid out inside. Its bounds become the list's parent area,
        which is what fixes the list's top edge and caps its height - layout, not plumbing. Passing
        nullptr returns the list to being a free desktop window sized to its own content, which for
        a long bank overhangs the panel. See ../../CLAUDE.md, "The Program dropdown". */
    void setMenuParent(juce::Component* parent) noexcept { menuParent = parent; }

    /** The row the list's top edge lands on: the LCD's own bottom edge, so the two read as one
        object rather than a bar with a list floating under it. */
    static int menuAnchorY() noexcept
    {
        return (int) std::floor(TapeRotTheme::Layout::programLcd.getBottom());
    }

    /** Where menuHost has to start, and it is NOT the anchor: JUCE clamps a menu to
        `jmax(parentArea.getY() + 1, ...)`, so a host beginning exactly at the anchor can only open
        one pixel below it, leaving a hairline of panel between the bar and its list.

        The lead has a floor and a ceiling. Too small and the clamp bites again; too large and the
        list can grow past the panel, because JUCE sizes it to `parentArea.getHeight() - 24` while
        the room actually below the anchor is the LCD's own height less than that. */
    static int menuHostTop() noexcept { return menuAnchorY() - 8; }
    void refresh() { repaint(); }

private:
    void timerCallback() override;
    void showProgramMenu();

    /** DELETE is live for a User Program only. Disabled for every Factory Program **and for INIT**
        - INIT is not a stored thing, so there is nothing to delete. Defined in the .cpp because the
        processor is only forward-declared here. */
    bool deleteEnabled() const;

    juce::String lcdText() const;

    // --- naming a User Program, typed straight into the LCD ------------------------------------
    // SAVE opens an entry field in the glass rather than saving immediately, so a User Program can
    // be named. DELETE acts as cancel while it is open, as does Escape; Enter commits. The same
    // interaction the rest of the suite uses - see Gatecrasher's ProgramHeader.
    void enterNamingMode();
    void commitName();
    void cancelNaming();

    bool namingMode = false;
    juce::String typedName;
    /** The name field runs x 510-849 at 18px Share Tech Mono with 2px tracking, which fits about
        27 characters; this leaves room for the caret without the text reaching the chevron. */
    // **An alias, not a copy.** The cap lives on the processor because the store enforces it on
    // every save path, not only on the keystrokes typed here - see its derivation there. Stated as
    // an alias beside the constant rather than as a comment in this consumer, because a comment
    // here is where the explanation gets silently invalidated.
    static constexpr int maxProgramNameLength = kMaxProgramNameLength;

    TapeRotAudioProcessor& processorRef;

    // No menuOpen flag here, unlike CHORUS-60 / REFLECT-84 / FIFTH MEMBER: this panel's dropdown
    // chevron is PRINTED IN THE PLATE (delta v1.0.3), so there is nothing at runtime to invert
    // while the list is open. Drawing one over the top would double-print it; un-baking is a plate
    // change, raised with the designers rather than worked around in code.
    juce::Component* menuParent = nullptr;
    /** The parameter takeover: what to show, and until when. The deadline is core's; the one-shot
        Timer that notices it, the font, the cell and every pixel of the painting stay here. */
    nf::ReadoutTimer readout { TapeRotTheme::Layout::readoutFormat() };
    /** Outlives showMenuAsync's callback, so it must be a member rather than a local. */
    ProgramMenuLookAndFeel menuLookAndFeel;

    /** The Programs the open menu was built from, in row order. The callback indexes this rather
        than reconstructing a Program from a number, so a bank that changed while the menu was open
        cannot select the wrong sound. */
    std::vector<ProgramId> menuRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgramHeader)
};
