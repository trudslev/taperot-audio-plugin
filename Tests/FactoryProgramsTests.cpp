#include "TestUtils.h"
#include "../Source/DSP/FactoryPrograms.h"
#include "../Source/DSP/TapeModelData.h"

// Structural sanity only (mirrors TapeModelCalibrationTests.cpp's approach to kTapeModels) - every
// entry must reference a real model and stay within each parameter's valid range, and momentary
// triggers must never be baked into a program. Tonal/character tuning is a by-ear pass, not
// something this suite checks.
class FactoryProgramsTests final : public juce::UnitTest
{
public:
    FactoryProgramsTests() : juce::UnitTest("FactoryPrograms", "DSP") {}

    void runTest() override
    {
        beginTest("Every program's modelIndex is a valid index into kTapeModels");
        {
            for (const auto& program : kFactoryPrograms)
                expect(program.modelIndex >= 0 && program.modelIndex < (int) kNumTapeModels, program.name);
        }

        beginTest("Every program's noiseCharacter is TAPE/VCR/DUST (0-2)");
        {
            for (const auto& program : kFactoryPrograms)
                expect(program.noiseCharacter >= 0 && program.noiseCharacter <= 2, program.name);
        }

        beginTest("Every program's gen is within [1, 8]");
        {
            for (const auto& program : kFactoryPrograms)
                expect(program.gen >= 1 && program.gen <= 8, program.name);
        }

        beginTest("Every program's percent fields are within [0, 100]");
        {
            for (const auto& program : kFactoryPrograms)
            {
                expect(program.drivePercent >= 0.0f && program.drivePercent <= 100.0f, program.name);
                expect(program.wowPercent >= 0.0f && program.wowPercent <= 100.0f, program.name);
                expect(program.flutterPercent >= 0.0f && program.flutterPercent <= 100.0f, program.name);
                expect(program.noisePercent >= 0.0f && program.noisePercent <= 100.0f, program.name);
                expect(program.failurePercent >= 0.0f && program.failurePercent <= 100.0f, program.name);
                expect(program.mixPercent >= 0.0f && program.mixPercent <= 100.0f, program.name);
            }
        }

        beginTest("warmCassetteProgramIndex actually names \"WARM CASSETTE\"");
        {
            expectEquals(juce::String(kFactoryPrograms[warmCassetteProgramIndex].name), juce::String("WARM CASSETTE"));
        }

        beginTest("Program names are unique");
        {
            juce::StringArray names;
            for (const auto& program : kFactoryPrograms)
                names.add(program.name);
            names.sort(false);
            for (int i = 1; i < names.size(); ++i)
                expect(names[i] != names[i - 1], names[i]);
        }
    }
};

static FactoryProgramsTests factoryProgramsTests;
