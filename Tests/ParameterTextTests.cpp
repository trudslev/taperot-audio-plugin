#include "TestUtils.h"
#include "../Source/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    class DummyProcessor final : public juce::AudioProcessor
    {
    public:
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };
}

/**
    What each parameter actually RENDERS - the string, not the value.

    This exists because the panel spent a long time printing `DRIVE: 20.0000000` while every test
    here passed. No float parameter carried a `stringFromValueFunction`, and
    AudioParameterFloat builds a fallback from the range's interval: when that interval is 0 - as
    it is for every float in this plugin - `numDecimalPlaces` is left at its initial **7**
    (juce_AudioParameterFloat.cpp:53-70). The value was right; only its rendering was wrong, and
    nothing asserted a rendering.

    **The expected strings are literals.** Derived from the same formatters the code uses, this
    test would rename itself alongside a regression and assert nothing - the trap that let a
    migration-guard bug survive its own test elsewhere in this suite.

    Values are deliberately non-integral. An integral value renders identically under the broken
    and the fixed formatter, so a test written against 20.0 would have passed throughout.
*/
class ParameterTextTests final : public juce::UnitTest
{
public:
    ParameterTextTests() : juce::UnitTest("Parameter text", "DSP") {}

    void runTest() override
    {
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMETERS",
                                                  createTapeRotParameterLayout());

        const auto textAt = [&apvts] (const juce::String& id, float value)
        {
            auto* p = apvts.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
            return p->getText(p->getValue(), 0);
        };
        const auto labelOf = [&apvts] (const juce::String& id)
        {
            return apvts.getParameter(id)->getLabel();
        };

        beginTest("Percentages are whole numbers at every value, not just integral ones");
        expectEquals(textAt(ParamIDs::drive, 33.333332f), juce::String("33"));
        expectEquals(textAt(ParamIDs::drive, 66.666664f), juce::String("67"));
        expectEquals(textAt(ParamIDs::mix, 99.9f), juce::String("100"));
        expectEquals(textAt(ParamIDs::wow, 20.0f), juce::String("20"));

        beginTest("Output is one decimal, and keeps its sign");
        expectEquals(textAt(ParamIDs::output, -16.3999977f), juce::String("-16.4"));
        expectEquals(textAt(ParamIDs::output, 0.0f), juce::String("0.0"));

        beginTest("RAMP keeps two decimals so the bottom of its range stays distinguishable");
        // At one decimal 0.05 and 0.14 both render "0.1" - the low end would read as one value.
        expectEquals(textAt(ParamIDs::ramp, 0.05f), juce::String("0.05"));
        expectEquals(textAt(ParamIDs::ramp, 0.14f), juce::String("0.14"));

        beginTest("Frequencies carry their own unit and switch at 1 kHz");
        expectEquals(textAt(ParamIDs::hp, 180.437f), juce::String("180 Hz"));
        expectEquals(textAt(ParamIDs::lp, 20000.0f), juce::String("20.0 kHz"));
        // The unit is value-dependent, so it lives in the text and hzAttrs carries NO label -
        // otherwise the readout would double it up as "20.0 kHz Hz".
        expect(labelOf(ParamIDs::hp).isEmpty(), "HP must not carry a fixed label");
        expect(labelOf(ParamIDs::lp).isEmpty(), "LP must not carry a fixed label");

        beginTest("Units that ARE fixed stay on the label, so the readout can join them");
        expectEquals(labelOf(ParamIDs::drive), juce::String("%"));
        expectEquals(labelOf(ParamIDs::output), juce::String("dB"));
        expectEquals(labelOf(ParamIDs::ramp), juce::String("s"));

        beginTest("No numeric parameter ever renders more than two decimal places");
        // The general form of the defect. A length cap does not catch it - the zero-decimal
        // failure mode renders "33.3333", seven characters, which fits the LCD budget fine. What
        // is always wrong is the decimal count: std::ostream's default is six significant digits
        // and JUCE's interval fallback is seven places, so anything past two has fallen through.
        for (auto* p : apvts.processor.getParameters())
        {
            if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr
                || dynamic_cast<juce::AudioParameterBool*>(p) != nullptr
                || dynamic_cast<juce::AudioParameterInt*>(p) != nullptr)
                continue;

            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                for (float t : { 0.0f, 0.137f, 0.5f, 0.618f, 0.911f, 1.0f })
                {
                    const auto rendered = ranged->getText(t, 0);
                    const auto dot = rendered.indexOfChar('.');
                    if (dot < 0)
                        continue;

                    int decimals = 0;
                    for (int i = dot + 1; i < rendered.length()
                                          && juce::CharacterFunctions::isDigit(rendered[i]); ++i)
                        ++decimals;

                    expect(decimals <= 2,
                           ranged->getParameterID() + " renders \"" + rendered + "\" ("
                               + juce::String(decimals) + " decimals) at t=" + juce::String(t));
                }
        }
    }
};

static ParameterTextTests parameterTextTests;
