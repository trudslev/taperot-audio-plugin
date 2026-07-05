#include "TestUtils.h"
#include "../Source/DSP/FactoryPresets.h"
#include "../Source/DSP/TapeModelData.h"

// Structural sanity only (mirrors TapeModelCalibrationTests.cpp's approach to kTapeModels) - every
// entry must reference a real model and stay within each parameter's valid range, and momentary
// triggers must never be baked into a preset. Tonal/character tuning is a by-ear pass, not
// something this suite checks.
class FactoryPresetsTests final : public juce::UnitTest
{
public:
    FactoryPresetsTests() : juce::UnitTest("FactoryPresets", "DSP") {}

    void runTest() override
    {
        beginTest("Every preset's modelIndex is a valid index into kTapeModels");
        {
            for (const auto& preset : kFactoryPresets)
                expect(preset.modelIndex >= 0 && preset.modelIndex < (int) kNumTapeModels, preset.name);
        }

        beginTest("Every preset's noiseCharacter is TAPE/VCR/DUST (0-2)");
        {
            for (const auto& preset : kFactoryPresets)
                expect(preset.noiseCharacter >= 0 && preset.noiseCharacter <= 2, preset.name);
        }

        beginTest("Every preset's gen is within [1, 8]");
        {
            for (const auto& preset : kFactoryPresets)
                expect(preset.gen >= 1 && preset.gen <= 8, preset.name);
        }

        beginTest("Every preset's percent fields are within [0, 100]");
        {
            for (const auto& preset : kFactoryPresets)
            {
                expect(preset.drivePercent >= 0.0f && preset.drivePercent <= 100.0f, preset.name);
                expect(preset.wowPercent >= 0.0f && preset.wowPercent <= 100.0f, preset.name);
                expect(preset.flutterPercent >= 0.0f && preset.flutterPercent <= 100.0f, preset.name);
                expect(preset.noisePercent >= 0.0f && preset.noisePercent <= 100.0f, preset.name);
                expect(preset.failurePercent >= 0.0f && preset.failurePercent <= 100.0f, preset.name);
                expect(preset.mixPercent >= 0.0f && preset.mixPercent <= 100.0f, preset.name);
            }
        }

        beginTest("warmCassetteProgramIndex actually names \"Warm Cassette\"");
        {
            expectEquals(juce::String(kFactoryPresets[warmCassetteProgramIndex].name), juce::String("Warm Cassette"));
        }

        beginTest("Preset names are unique");
        {
            juce::StringArray names;
            for (const auto& preset : kFactoryPresets)
                names.add(preset.name);
            names.sort(false);
            for (int i = 1; i < names.size(); ++i)
                expect(names[i] != names[i - 1], names[i]);
        }
    }
};

static FactoryPresetsTests factoryPresetsTests;
