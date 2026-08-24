#pragma once

#include "GenerationLadder.h"
#include "KnobComponent.h"
#include "LampButtonGroup.h"
#include "MachineReadout.h"
#include "PitchScope.h"
#include "ProgramHeader.h"
#include "ShoeSwitch.h"
#include "TapeRotPanelBackground.h"
#include "TapeRotTheme.h"

#include <nf/AboutPart.h>

#include <memory>
#include <vector>

class TapeRotAudioProcessor;

/**
    The fixed **1340 x 790** reference canvas. `PluginEditor` applies one uniform scale transform
    above this; nothing below ever sees it.

    **Everything on it is drawn.** Revision 1 blitted a plate and placed 30 sprites over it; call 5
    retired both, so the draw order is a background component and then the controls that sit on it,
    with no element over a baked copy of itself.

    §2's section order **is the signal path** - INPUT, MACHINE, TRANSPORT, NOISE, DECAY, OUTPUT -
    and the control tables below are written in that order for the same reason: a reader looking for
    where FLUTTER lives should find it where the signal does.
*/
class TapeRotEditorContent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit TapeRotEditorContent (TapeRotAudioProcessor&);
    ~TapeRotEditorContent() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void buildKnobs();
    void buildShoes();
    void buildLampGroups();
    void refreshLampStates();

    TapeRotAudioProcessor& processorRef;

    TapeRotPanelBackground background;
    PitchScope scope;
    MachineReadout machineReadout;
    GenerationLadder generation;
    ProgramHeader header;

    /** Paints nothing and claims no clicks of its own; it exists so the Program list has a parent
        area to be laid out in. Its bounds are what stop the list moving or overflowing the panel -
        see the constructor, and ../../CLAUDE.md's "The Program dropdown". */
    juce::Component menuHost;

    /*  `ABOUT-PART.md`. The tab, the wordmark hit region and the box all live in `nf::AboutPart` —
        this casting supplies §9's materials and §1's five strings and nothing else. */
    std::unique_ptr<nf::AboutTab> aboutTab;
    std::unique_ptr<nf::AboutWordmarkHit> aboutWordmark;
    std::unique_ptr<nf::AboutBox> aboutBox;

    std::vector<std::unique_ptr<KnobComponent>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ShoeSwitch>> shoes;
    std::vector<std::unique_ptr<LampButtonGroup>> lampGroups;

    /** §7.3's "currently sounding" held briefly past each event, because the failure FIFO reports
        instants and a lamp lit for one frame per event reads as noise rather than as a state.
        260 ms is revision 1's own fault-flash figure, kept so the two rounds agree about how long
        an event stays visible. */
    juce::uint32 failSoundingUntil = 0;
    static constexpr int failLampHoldMs = 260;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeRotEditorContent)
};
