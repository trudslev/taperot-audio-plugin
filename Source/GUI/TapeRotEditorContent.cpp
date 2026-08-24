#include "TapeRotEditorContent.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    /*  §2 and §3, in signal-path order. Every pivot and label line is measured off the delivered
        prototype; §3.1's shared pivot y 444 and label line y 516 hold for the whole Ø104/Ø76 row,
        and the three Ø56 controls carry their own.

        **RAMP is in OUTPUT, not DECAY, and that disagrees with §2's contents column.** The
        delivered prototype puts its pivot at (1163, 580) - dead centre of the OUTPUT section and
        directly under its heading - while §2's table lists it under DECAY. The position is
        unambiguous in the artefact and ambiguous only in the prose, so the artefact is what is
        built; `design-asks/taperot-ramp-section.md` carries the disagreement. Nothing else about
        the panel changes either way, because a section is a heading and two dividers, not a
        container.  */
    struct KnobEntry
    {
        const char* paramId;
        const char* label;
        const char* unit;
        Layout::Cap cap;
        float pivotX, pivotY, labelY;
        const Marks::Mark* marks;
        int markCount;
    };

    const std::array<KnobEntry, 11> knobTable { {
        // INPUT
        { "drive",   "DRIVE",   "%",   Layout::Cap::primary,     92.0f, 444.0f, 516.0f,
          Marks::driveAndFlutter.data(), (int) Marks::driveAndFlutter.size() },
        // MACHINE - §3.3: nine detents, no numerals, the readout is the label
        { "model",   "MODEL",   "",    Layout::Cap::signature,  256.0f, 444.0f, 516.0f, nullptr, 0 },
        // TRANSPORT
        { "wow",     "WOW",     "%",   Layout::Cap::primary,    438.0f, 444.0f, 516.0f,
          Marks::wowPercent.data(), (int) Marks::wowPercent.size() },
        { "flutter", "FLUTTER", "%",   Layout::Cap::primary,    562.0f, 444.0f, 516.0f,
          Marks::driveAndFlutter.data(), (int) Marks::driveAndFlutter.size() },
        // NOISE
        { "noise",   "NOISE",   "%",   Layout::Cap::primary,    741.0f, 444.0f, 516.0f,
          Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size() },
        // DECAY
        { "failure", "FAILURE", "%",   Layout::Cap::primary,    914.0f, 444.0f, 516.0f,
          Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size() },
        // OUTPUT
        { "mix",     "MIX",     "%",   Layout::Cap::primary,   1089.0f, 444.0f, 516.0f,
          Marks::evenFifthsPercent.data(), (int) Marks::evenFifthsPercent.size() },
        { "output",  "OUTPUT",  "dB",  Layout::Cap::primary,   1237.0f, 444.0f, 516.0f,
          Marks::outputDb.data(), (int) Marks::outputDb.size() },
        { "ramp",    "RAMP",    "s",   Layout::Cap::standard,  1163.0f, 580.0f, 642.0f,
          Marks::rampSeconds.data(), (int) Marks::rampSeconds.size() },
        { "lp",      "LP",      "kHz", Layout::Cap::standard,  1069.0f, 640.0f, 702.0f,
          Marks::lpKilohertz.data(), (int) Marks::lpKilohertz.size() },
        { "hp",      "HP",      "Hz",  Layout::Cap::standard,  1257.0f, 640.0f, 702.0f,
          Marks::hpHertz.data(), (int) Marks::hpHertz.size() } } };

    /** Index-matched to `Switches::shoes`. `true` selects the RIGHT half in every case. */
    const std::array<const char*, 3> shoeParams { { "switchMode", "hum", "spread" } };

    /** Index-matched to `Switches::lampGroups`. The FAIL trio are momentary triggers; the four
        fault categories are independent toggles; NOISE BED is a three-state selector and is driven
        from its choice parameter rather than from a list of three. */
    const std::array<const char*, 3> failTriggers { { "stop", "filterAux", "failAux" } };
    const std::array<const char*, 4> faultToggles { { "failureDropouts", "failureSnags",
                                                      "failureCrinkles", "failureImbalance" } };
}

TapeRotEditorContent::TapeRotEditorContent (TapeRotAudioProcessor& p)
    : processorRef (p), scope (p), header (p)
{
    setSize ((int) Layout::canvasWidth, (int) Layout::canvasHeight);

    background.setBounds (getLocalBounds());
    addAndMakeVisible (background);

    scope.setBounds (PitchScope::canvasBounds());
    addAndMakeVisible (scope);

    machineReadout.setBounds (MachineReadout::canvasBounds());
    addAndMakeVisible (machineReadout);

    generation.setBounds (generation.canvasBounds());
    generation.onGenerationChanged = [this] (int stage)
    {
        processorRef.userEdits.noteUserEdit();

        if (auto* param = processorRef.apvts.getParameter ("gen"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) stage));
            param->endChangeGesture();
        }

        header.showParameter ("gen");
        header.releaseParameter();
    };
    addAndMakeVisible (generation);

    buildKnobs();
    buildShoes();
    buildLampGroups();

    addAndMakeVisible (header);

    /*  The header covers the whole canvas and paints sparsely across it, so it must sit BEHIND the
        controls that carry the mouse. Its own `hitTest` narrows it to the LCD and the two Program
        buttons; ordering is what stops it swallowing anything else.  */
    background.toBack();
    header.toBack();
    background.toBack();

    /*  The Program list opens inside this, so it can neither move its top edge nor grow past the
        panel. A **SIBLING** of the header, never a child: the header narrows its `hitTest` to the
        glass and the two buttons, and JUCE stops searching a component's children once its own
        `hitTest` rejects the point - so a list parented there would be dead everywhere except the
        cell it drops from.  */
    const int hostTop = ProgramHeader::menuHostTop();
    menuHost.setBounds (0, hostTop, getWidth(), getHeight() - hostTop);
    menuHost.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (menuHost);
    menuHost.toFront (false);
    header.setMenuParent (&menuHost);

    startTimerHz (Runtime::animationHz);
}

TapeRotEditorContent::~TapeRotEditorContent() = default;

void TapeRotEditorContent::buildKnobs()
{
    for (const auto& entry : knobTable)
    {
        KnobComponent::Spec spec { entry.label, entry.unit, entry.cap,
                                   { entry.pivotX, entry.pivotY }, entry.labelY,
                                   entry.marks, entry.markCount };

        auto knob = std::make_unique<KnobComponent> (spec);
        knob->setBounds (knob->canvasBounds());

        const juce::String paramId { entry.paramId };

        /*  One call, not two adjacent ones. `nf::connectUserEdit` couples the LCD hand-off to the
            stale-replay disarm **because writing the hand-off and forgetting the disarm was the
            actual shape of the defect** - Reflect-84 shipped the guard with zero call sites for it.
            The guard fires only while a control is genuinely dragged, so a Program apply and a host
            automation step, which also raise a SliderAttachment's callback, do not disarm it. That
            matters on session load, where a host may write automation BEFORE replaying its
            remembered program index.  */
        nf::connectUserEdit (*knob, processorRef.userEdits,
                             [this, paramId] { header.showParameter (paramId); });

        knob->onDragEnd = [this] { header.releaseParameter(); };

        addAndMakeVisible (*knob);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.apvts, paramId, *knob));
        knobs.push_back (std::move (knob));
    }
}

void TapeRotEditorContent::buildShoes()
{
    for (size_t i = 0; i < Switches::shoes.size(); ++i)
    {
        auto shoe = std::make_unique<ShoeSwitch> (Switches::shoes[i]);
        shoe->setBounds (shoe->canvasBounds());

        const juce::String paramId { shoeParams[i] };

        if (auto* param = processorRef.apvts.getParameter (paramId))
            shoe->setRightSelected (param->getValue() > 0.5f);

        shoe->onSelectionChanged = [this, paramId] (bool right)
        {
            /*  A shoe is a plain Component rather than a Slider, so there is no drag state to guard
                on and `nf::connectUserEdit` has no matching callback to install. It does not need
                one: `onSelectionChanged` fires only on a real click, so it is user-originated by
                construction - which is exactly the property the drag guard exists to establish for
                controls an attachment also writes to.  */
            processorRef.userEdits.noteUserEdit();

            if (auto* param = processorRef.apvts.getParameter (paramId))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (right ? 1.0f : 0.0f);
                param->endChangeGesture();
            }

            header.showParameter (paramId);
            header.releaseParameter();
        };

        addAndMakeVisible (*shoe);
        shoes.push_back (std::move (shoe));
    }
}

void TapeRotEditorContent::buildLampGroups()
{
    for (size_t i = 0; i < Switches::lampGroups.size(); ++i)
    {
        auto group = std::make_unique<LampButtonGroup> (Switches::lampGroups[i]);
        group->setBounds (group->canvasBounds());

        const int which = (int) i;

        /*  **BRAND.md: "Every control that changes a parameter reports there — switches
            included."** These ten did not, between the panel rewrite and 2026-08-22. The old
            `SpriteButton::onInteraction` did `noteUserEdit()`, `showParameter()` and
            `releaseParameter()`; the rewrite replaced that class, kept the first and dropped the
            other two here — while keeping all three on the shoes and the GENERATION ladder, which
            is why it read as done.

            The rule's own justification is these controls exactly: *"some switch changes are the
            least obvious things on a panel… Turning a knob explains itself; flipping a switch does
            not."* A lamp going on tells you something changed and not what it was called.

            A switch settles the moment it is thrown, so it announces and releases in the one
            callback — there is no drag to end. Gatecrasher's `reportSwitch` says the same thing.  */
        const auto report = [this] (const char* paramId)
        {
            header.showParameter (paramId);
            header.releaseParameter();
        };

        group->onPressed = [this, which, report] (int index)
        {
            processorRef.userEdits.noteUserEdit();

            if (which == 0)
            {
                /*  §7.3's FAIL trio are **momentary triggers**. Root `CLAUDE.md`'s rule is that a
                    momentary wants neither storage nor a dirty-check entry but a forced-off on
                    every Program apply, so no Program can load one stuck engaged - which
                    `applyProgramSnapshot` does for exactly these three.  */
                if (auto* param = processorRef.apvts.getParameter (failTriggers[(size_t) index]))
                {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (1.0f);
                    report (failTriggers[(size_t) index]);
                }
            }
            else if (which == 1)
            {
                // NOISE BED: a three-state selector, so exactly one is lit and the parameter is a
                // choice rather than three bools.
                if (auto* param = processorRef.apvts.getParameter ("noiseCharacter"))
                {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
                    param->endChangeGesture();
                    report ("noiseCharacter");
                }
            }
            else
            {
                if (auto* param = processorRef.apvts.getParameter (faultToggles[(size_t) index]))
                {
                    const bool now = param->getValue() > 0.5f;
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (now ? 0.0f : 1.0f);
                    param->endChangeGesture();
                    report (faultToggles[(size_t) index]);
                }
            }
        };

        group->onReleased = [this, which] (int index)
        {
            if (which != 0)
                return;

            if (auto* param = processorRef.apvts.getParameter (failTriggers[(size_t) index]))
            {
                param->setValueNotifyingHost (0.0f);
                param->endChangeGesture();
            }
        };

        addAndMakeVisible (*group);
        lampGroups.push_back (std::move (group));
    }

    /*  `ABOUT-PART.md`. §9's materials and §1's five strings are all this casting supplies. */
    {
        constexpr int frameOriginX = 0;   // no rack ears: the frame IS the window

        const nf::AboutMaterials aboutMaterials {
            Colour::aboutGlass, Colour::aboutBody, Colour::aboutDim, Colour::aboutAccent,
            Colour::aboutRing,
            Colour::aboutWellTop, Colour::aboutWellBottom, Colour::aboutWellInk,
            Font::barlowSemiBold(), Font::barlowMedium(), Font::mono(),
            Cursor::help()
        };

        /*  §8: the credits name the faces this casting EMBEDS, not the ones it draws with — so
            **Impact Label Reversed is not here**, and that is a licence fact rather than an
            oversight. It is donationware and not embeddable, the wordmark's letterforms ship as
            ARTWORK instead, and `design/fonts/ABSENT.md` records it. §9.3 says the same in words:
            naming it would assert an embedding its licence forbids. */
        const nf::AboutContent aboutContent {
            "TAPEROT", "MT-77",
            NF_VERSION,                 // semver, from PROJECT_VERSION - never a literal
            nf::suiteRelease,           // §1: a separate string, and neither derives from the other
            "github.com/trudslev/taperot-audio-plugin",
            "Barlow Condensed and Share Tech Mono, both under the SIL Open Font License."
        };

        aboutBox = std::make_unique<nf::AboutBox> (aboutMaterials, aboutContent, frameOriginX);

        /*  §2, revision 3: the tab takes **the face and size this casting's stamp already uses** —
            §11.1's Share Tech Mono 10 / 13 / .18 em, which is what the footer's right-hand string
            drew.

            **The string is full semver, and the panel's was not.** Every delivered prototype that
            spells its tab's version literally prints `v1.0.0` — this one, chorus-60, elmer and
            gatecrasher — and §1 states the plugin version as semver. `NF_VERSION_SHORT` is right
            for a panel stamp and wrong here, which is what Fifth Member's §12 says in as many
            words about its own. */
        aboutTab = std::make_unique<nf::AboutTab> (aboutMaterials, Font::mono(),
                                                   "TAPEROT " + Text::middleDot()
                                                       + " v" + juce::String (NF_VERSION),
                                                   Readouts::footerCssPx,
                                                   Readouts::footerTrackingEm);
        aboutTab->onClick = [this] { aboutBox->open(); };

        /*  §2a: the wordmark is the PRIMARY affordance, and this is one of the two castings whose
            wordmark is a BITMAP — here because Impact Label Reversed cannot be embedded at all. A
            hit region needs only a rectangle, and `HeaderGeometry::nameplate()` is the same
            rectangle over artwork as over live text. It draws nothing. */
        aboutWordmark = std::make_unique<nf::AboutWordmarkHit> (Cursor::help());
        aboutWordmark->onClick = [this] { aboutBox->open(); };

        /*  **Registered LAST, and that is not tidiness.** JUCE paints children in the order they
            were added, so registering these beside their construction puts the tab under the panel
            background — drawn, correct, and invisible in a capture. */
        aboutTab->layoutFor (getHeight(), frameOriginX);
        aboutWordmark->setBounds (nf::AboutWordmarkHit::zone (frameOriginX));
        aboutBox->setBounds (getLocalBounds());
        addAndMakeVisible (*aboutWordmark);
        addAndMakeVisible (*aboutTab);
        addChildComponent (*aboutBox);
    }
}

/*  §7.3 is the ONE authority for every lamp on this panel, which is why they are all refreshed from
    one place rather than each component deciding for itself. Three different behaviours - engine
    state, an exclusive selection and independent toggles - reading one table.  */
void TapeRotEditorContent::refreshLampStates()
{
    const auto value = [this] (const char* id)
    {
        auto* param = processorRef.apvts.getParameter (id);
        return param != nullptr && param->getValue() > 0.5f;
    };

    for (int i = 0; i < 3; ++i)
        lampGroups[0]->setLit (i, value (failTriggers[(size_t) i]));

    if (auto* character = processorRef.apvts.getParameter ("noiseCharacter"))
    {
        const int selected = juce::roundToInt (character->convertFrom0to1 (character->getValue()));
        for (int i = 0; i < 3; ++i)
            lampGroups[1]->setLit (i, i == selected);
    }

    for (int i = 0; i < 4; ++i)
        lampGroups[2]->setLit (i, value (faultToggles[(size_t) i]));

    /*  §7.3: the scope's FAIL lamp is lit "when any failure is currently sounding", which is an
        EVENT, where the four FAULT ACTIVITY lamps above are lit "when that fault category is
        enabled", which is a parameter. Two different questions about the same engine, and the
        reorganisation is what separated them - revision 1 flashed the fault lamps off this FIFO and
        had no lamp for the first question at all.

        This is the FIFO's single consumer. `popEvents` drains, so a second reader would take events
        the first never sees.  */
    {
        FailureEvent events[64];
        const int n = processorRef.getFailureEngine().popEvents (events, 64);
        const auto now = juce::Time::getMillisecondCounter();

        if (n > 0)
            failSoundingUntil = now + (juce::uint32) failLampHoldMs;

        scope.setFailLampLit (failSoundingUntil != 0 && now < failSoundingUntil);
    }
}

void TapeRotEditorContent::timerCallback()
{
    // The header's IN/OUT numerals, the lamps and the MACHINE readout all follow values that change
    // without any user action, so they need a poll rather than a callback.
    header.refresh();
    refreshLampStates();

    if (auto* model = processorRef.apvts.getParameter ("model"))
    {
        const int index = juce::roundToInt (model->convertFrom0to1 (model->getValue()));
        machineReadout.setMachineName (model->getCurrentValueAsText());
        juce::ignoreUnused (index);
    }

    for (size_t i = 0; i < shoes.size(); ++i)
        if (auto* param = processorRef.apvts.getParameter (shoeParams[i]))
            shoes[i]->setRightSelected (param->getValue() > 0.5f);

    if (auto* gen = processorRef.apvts.getParameter ("gen"))
        generation.setGeneration (juce::roundToInt (gen->convertFrom0to1 (gen->getValue())));
}

void TapeRotEditorContent::paint (juce::Graphics&)
{
    // The background component draws the fascia. Nothing is painted here, and that is the whole
    // difference from revision 1, which blitted a plate at this point.
}

/*  §7.5's bypass: a full-bleed 0.50 `#808080` **multiply** over the whole panel.

    Pointers do not move, the scope freezes, every lamp goes out, no caption, no desaturation - and
    §6 states in as many words that the legibility floors do not apply in this state, which is what
    makes a flat multiply the right treatment rather than a dimmed redraw. Drawn over the children
    rather than under them, because a veil that is not on top is not a veil.  */
void TapeRotEditorContent::paintOverChildren (juce::Graphics& g)
{
    /*  **This casting has no bypass parameter, so this veil never draws today** - and that is a
        conflict worth stating rather than a branch worth deleting.

        `PluginProcessor.h` records the decision at length: *"this effect is a tape path, and a
        bypass would be the tape being out of the machine"*, with Reflect-84 named as the reference
        if it is ever reversed. §7.5 of this round's spec describes a host-driven bypass with a
        full-bleed 0.50 multiply and no on-panel control. `getBypassParameter()` returns nullptr
        here, so the state is unreachable.

        Written as the spec asks and driven from the parameter that would enable it, so that
        reversing the decision is one override rather than a panel change.
        `design-asks/taperot-bypass-state.md` carries it.  */
    auto* bypass = processorRef.getBypassParameter();

    if (bypass == nullptr || bypass->getValue() <= 0.5f)
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.setColour (Colour::bypassVeil.withAlpha (Colour::bypassAlpha));
    g.fillRect (getLocalBounds());
}
