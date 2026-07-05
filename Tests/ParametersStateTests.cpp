#include "TestUtils.h"
#include "../Source/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    // Minimal throwaway AudioProcessor just to host an APVTS in tests, avoiding any dependency on
    // the real plugin target's JucePlugin_* macros (which PluginProcessor.cpp requires and which
    // aren't available in the plain console-app Tests target).
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

class ParametersStateTests final : public juce::UnitTest
{
public:
    ParametersStateTests() : juce::UnitTest("ParametersState", "DSP") {}

    void runTest() override
    {
        beginTest("New parameter defaults match the spec (GEN=1, character=TAPE, LP/HP off, RAMP=0.3s, aux off)");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMETERS", createTapeRotParameterLayout());

            expectEquals((int) *apvts.getRawParameterValue(ParamIDs::gen), 1);
            expectEquals((int) *apvts.getRawParameterValue(ParamIDs::noiseCharacter), 0);
            expectWithinAbsoluteError((float) *apvts.getRawParameterValue(ParamIDs::lp), 20000.0f, 0.01f);
            expectWithinAbsoluteError((float) *apvts.getRawParameterValue(ParamIDs::hp), 20.0f, 0.01f);
            expect(*apvts.getRawParameterValue(ParamIDs::stop) < 0.5f);
            expect(*apvts.getRawParameterValue(ParamIDs::filterAux) < 0.5f);
            expect(*apvts.getRawParameterValue(ParamIDs::failAux) < 0.5f);
            expectWithinAbsoluteError((float) *apvts.getRawParameterValue(ParamIDs::ramp), 0.3f, 0.01f);
        }

        beginTest("Full round trip through get/setStateInformation preserves new parameter values");
        {
            DummyProcessor procA;
            juce::AudioProcessorValueTreeState apvtsA(procA, nullptr, "PARAMETERS", createTapeRotParameterLayout());

            *dynamic_cast<juce::AudioParameterInt*>(apvtsA.getParameter(ParamIDs::gen)) = 5;
            *dynamic_cast<juce::AudioParameterChoice*>(apvtsA.getParameter(ParamIDs::noiseCharacter)) = 1; // VCR
            *dynamic_cast<juce::AudioParameterFloat*>(apvtsA.getParameter(ParamIDs::lp)) = 6000.0f;
            *dynamic_cast<juce::AudioParameterFloat*>(apvtsA.getParameter(ParamIDs::hp)) = 250.0f;
            *dynamic_cast<juce::AudioParameterBool*>(apvtsA.getParameter(ParamIDs::stop)) = true;
            *dynamic_cast<juce::AudioParameterFloat*>(apvtsA.getParameter(ParamIDs::ramp)) = 1.5f;

            auto state = apvtsA.copyState();
            std::unique_ptr<juce::XmlElement> xml(state.createXml());
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary(*xml, block);

            DummyProcessor procB;
            juce::AudioProcessorValueTreeState apvtsB(procB, nullptr, "PARAMETERS", createTapeRotParameterLayout());
            std::unique_ptr<juce::XmlElement> loadedXml(
                juce::AudioProcessor::getXmlFromBinary(block.getData(), (int) block.getSize()));
            expect(loadedXml != nullptr);
            apvtsB.replaceState(juce::ValueTree::fromXml(*loadedXml));

            expectEquals((int) *apvtsB.getRawParameterValue(ParamIDs::gen), 5);
            expectEquals((int) *apvtsB.getRawParameterValue(ParamIDs::noiseCharacter), 1);
            expectWithinAbsoluteError((float) *apvtsB.getRawParameterValue(ParamIDs::lp), 6000.0f, 0.01f);
            expectWithinAbsoluteError((float) *apvtsB.getRawParameterValue(ParamIDs::hp), 250.0f, 0.01f);
            expect(*apvtsB.getRawParameterValue(ParamIDs::stop) > 0.5f);
            expectWithinAbsoluteError((float) *apvtsB.getRawParameterValue(ParamIDs::ramp), 1.5f, 0.01f);
        }

        beginTest("Missing new parameters in an old saved session fall back to defaults without crashing");
        {
            DummyProcessor procOld;
            juce::AudioProcessorValueTreeState apvtsOld(procOld, nullptr, "PARAMETERS", createTapeRotParameterLayout());

            // Set a couple of pre-existing params to non-default values, matching what a real
            // legacy session (saved before this feature existed) might contain.
            *dynamic_cast<juce::AudioParameterFloat*>(apvtsOld.getParameter(ParamIDs::drive)) = 55.0f;
            *dynamic_cast<juce::AudioParameterFloat*>(apvtsOld.getParameter(ParamIDs::noise)) = 50.0f;

            auto state = apvtsOld.copyState();
            std::unique_ptr<juce::XmlElement> xml(state.createXml());

            // Strip every new parameter's node out, simulating a session saved before this feature
            // existed (which could never have written these IDs).
            const juce::StringArray newIds{ ParamIDs::noiseCharacter, ParamIDs::gen, ParamIDs::lp, ParamIDs::hp,
                                             ParamIDs::stop, ParamIDs::filterAux, ParamIDs::failAux, ParamIDs::ramp };
            for (auto& id : newIds)
                for (int i = xml->getNumChildElements(); --i >= 0;)
                {
                    auto* child = xml->getChildElement(i);
                    if (child->getStringAttribute("id") == id)
                        xml->removeChildElement(child, true);
                }

            DummyProcessor procNew;
            juce::AudioProcessorValueTreeState apvtsNew(procNew, nullptr, "PARAMETERS", createTapeRotParameterLayout());
            apvtsNew.replaceState(juce::ValueTree::fromXml(*xml));

            expectWithinAbsoluteError((float) *apvtsNew.getRawParameterValue(ParamIDs::drive), 55.0f, 0.01f);
            expectWithinAbsoluteError((float) *apvtsNew.getRawParameterValue(ParamIDs::noise), 50.0f, 0.01f);

            expectEquals((int) *apvtsNew.getRawParameterValue(ParamIDs::gen), 1);
            expectEquals((int) *apvtsNew.getRawParameterValue(ParamIDs::noiseCharacter), 0);
            expectWithinAbsoluteError((float) *apvtsNew.getRawParameterValue(ParamIDs::lp), 20000.0f, 0.01f);
            expectWithinAbsoluteError((float) *apvtsNew.getRawParameterValue(ParamIDs::hp), 20.0f, 0.01f);
            expect(*apvtsNew.getRawParameterValue(ParamIDs::stop) < 0.5f);
            expect(*apvtsNew.getRawParameterValue(ParamIDs::filterAux) < 0.5f);
            expect(*apvtsNew.getRawParameterValue(ParamIDs::failAux) < 0.5f);
            expectWithinAbsoluteError((float) *apvtsNew.getRawParameterValue(ParamIDs::ramp), 0.3f, 0.01f);
        }
    }
};

static ParametersStateTests parametersStateTests;
