#include "../Source/Parameters.h"
#include "../Source/GUI/TapeRotTheme.h"

#include <nf/ParameterReadout.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    /** The smallest processor that can hold this casting's real parameter layout.

        The layout is the thing under test, so it has to be the shipping one - a hand-listed copy
        would assert against itself and pass while the panel printed something else.
    */
    class LayoutHost final : public juce::AudioProcessor
    {
    public:
        LayoutHost() : apvts (*this, nullptr, ParamIDs::stateTreeType, createTapeRotParameterLayout()) {}

        juce::AudioProcessorValueTreeState apvts;

        const juce::String getName() const override { return "LayoutHost"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

/**
    Every parameter must print acceptably in the LCD takeover.

    **This guards the defect CLASS, not the instances.** Extracting nf::describeParameter fixed the
    six castings as they stood; it does nothing about the next parameter somebody adds without an
    interval on its NormalisableRange and without a stringFromValueFunction. That is exactly how
    TapeRot came to print "DRIVE: 20.0000000" - its formatters were not deleted, they were never
    written, one parameter at a time, while the panel looked fine for every value that happened to
    be integral.

    Swept at several points across each parameter's range rather than at its default, because the
    default is the value most likely to be round - and a round value is precisely the one that hides
    this.
*/
class ReadoutConformanceTests final : public juce::UnitTest
{
public:
    ReadoutConformanceTests() : juce::UnitTest ("Readout conformance", "GUI") {}

    void runTest() override
    {
        beginTest ("Every parameter prints cleanly across its range");
        {
            LayoutHost host;

            // Deliberately not 0, 0.5, 1: those land on round physical values for a linear range,
            // which is where a missing formatter looks correct. The awkward fractions are the point.
            for (const float position : { 0.0f, 0.13f, 0.37f, 0.5f, 0.61f, 0.89f, 1.0f })
            {
                for (auto* raw : host.getParameters())
                {
                    auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw);

                    if (p == nullptr)
                        continue;

                    p->setValueNotifyingHost (position);

                    const auto defects = nf::readoutDefects (*p, TapeRotTheme::Runtime::readoutFormat());

                    for (const auto& defect : defects)
                        expect (false, p->paramID + " at " + juce::String (position, 2)
                                     + ": " + defect);
                }
            }
        }

        beginTest ("The readout fits the LCD's character budget at every position");
        {
            // The takeover shares the cell with the Program name, and the name cap was computed
            // from that cell. A readout that overruns it is silently clipped, which reads as a
            // truncated unit rather than as a layout problem.
            //
            // The guard is the 16px step-down budget rather than the 19px one, because the readout
            // is what the step-down exists to absorb.
            LayoutHost host;
            // The name cell holds 27 characters at 18px Share Tech Mono; the readout may use all of them
            // plus the value and unit it is there to show, so the guard is the cell rather than the
            // 25-character NAME cap.
            const int budget = 27 + 8;

            for (const float position : { 0.0f, 0.37f, 1.0f })
                for (auto* raw : host.getParameters())
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw))
                    {
                        p->setValueNotifyingHost (position);

                        const auto text = nf::describeParameter (*p, TapeRotTheme::Runtime::readoutFormat());
                        expect (text.length() <= budget,
                                p->paramID + " prints " + juce::String (text.length())
                                    + " characters: \"" + text + "\"");
                    }
        }

        beginTest ("Case is authored at the source, so no display site re-cases");
        {
            // **The guard on BRAND.md's "case belongs at the source".** Core stopped upper-casing
            // the parameter name on 2026-08-13, so a parameter added with a Title-Case name makes
            // this panel print one row of its LCD in a different case from every other row - and
            // the host's automation lane shows the same mixed set. Nothing else fails.
            //
            // Asserted on getName() rather than on the literal, because that is what the readout
            // and the host both actually read.
            LayoutHost host;

            for (auto* raw : host.getParameters())
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw))
                {
                    const auto name = p->getName (128);
                    expect (name == name.toUpperCase(),
                            p->paramID + " is named \"" + name + "\", which is not authored in caps");
                }
        }

        beginTest ("A unitless, digit-free value is authored in caps");
        {
            // This casting used to set ValueCase::wordsOnly, which upper-cased exactly the values
            // matched here - an empty label and no digit, i.e. a choice name rather than a reading.
            // The flag is gone and the caps moved into Parameters.h, so this pins the half of that
            // move a reader cannot see from the call site.
            //
            // Deliberately NOT applied to values carrying a unit or a number: "6.3 kHz" and
            // "-18.5 dB" are authored as they read, and upper-casing them is the bug that named
            // ValueCase::all.
            LayoutHost host;

            for (const float position : { 0.0f, 0.5f, 1.0f })
                for (auto* raw : host.getParameters())
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw))
                    {
                        p->setValueNotifyingHost (position);

                        const auto value = p->getText (p->getValue(), 0);

                        if (p->getLabel().isNotEmpty() || value.containsAnyOf ("0123456789"))
                            continue;

                        expect (value == value.toUpperCase(),
                                p->paramID + " prints \"" + value + "\", which is not authored in caps");
                    }
        }

        beginTest ("SWITCH reads FADE / CLUNK — the one sanctioned change of this pass");
        {
            // **The guard on the only string in the whole case-at-source pass that deliberately
            // did NOT stay byte-identical.** Every other re-authored parameter prints exactly what
            // it printed before; SWITCH went from `SWITCH: OFF`/`ON` to `SWITCH: FADE`/`CLUNK`,
            // because it is a bool for storage reasons and has never meant on/off - false is FADE
            // (the parallel-chain crossfade) and true is CLUNK (the hard coefficient swap under a
            // mute dip), and the panel's own printed legends read FADE and CLUNK.
            //
            // **ModelSwitchAutomationTests is NOT this test.** Its "CLUNK"/"FADE" strings are the
            // DSP's own mode names, used to label a test case; they would still read correctly with
            // the readout reverted to On/Off. Nothing there touches getText, so without this the
            // sanctioned change can silently revert and every suite in the suite stays green.
            //
            // Read through the SHIPPING readout format rather than a copy, for the reason stated on
            // TapeRotTheme::Runtime::readoutFormat: a test that declares its own format asserts
            // against itself.
            LayoutHost host;

            // Held as the BASE type on purpose. AudioParameterBool re-declares getText as private,
            // so a juce::AudioParameterBool* cannot call it; the readout reaches it through
            // AudioProcessorParameter, and this test has to reach it the same way or it is testing
            // an access path the panel does not use.
            auto* switchMode = host.apvts.getParameter (ParamIDs::switchMode);

            expect (switchMode != nullptr, "switchMode is not in the layout");

            if (switchMode != nullptr)
            {
                const auto format = TapeRotTheme::Runtime::readoutFormat();

                switchMode->setValueNotifyingHost (0.0f);
                expectEquals (nf::describeParameter (*switchMode, format), juce::String ("SWITCH: FADE"));

                switchMode->setValueNotifyingHost (1.0f);
                expectEquals (nf::describeParameter (*switchMode, format), juce::String ("SWITCH: CLUNK"));

                // And the halves separately, so a failure says which end moved: the name is
                // authored in caps, the value is authored by the attributes, and neither is
                // re-cased on the way out.
                expectEquals (switchMode->getName (128), juce::String ("SWITCH"));
                expectEquals (switchMode->getText (0.0f, 0), juce::String ("FADE"));
                expectEquals (switchMode->getText (1.0f, 0), juce::String ("CLUNK"));
            }
        }
    }
};

static ReadoutConformanceTests readoutConformanceTests;
