#include "TestUtils.h"
#include "../Source/GUI/TapeRotLookAndFeel.h"
#include "../Source/DSP/TapeModelData.h"
#include <juce_gui_basics/juce_gui_basics.h>

class TapeRotLookAndFeelTests final : public juce::UnitTest
{
public:
    TapeRotLookAndFeelTests() : juce::UnitTest("TapeRotLookAndFeel", "GUI") {}

    void runTest() override
    {
        beginTest("A choice-bound (stepped) slider's tick count matches the model table size");
        {
            juce::Slider slider;
            // Mirrors what SliderParameterAttachment does for an AudioParameterChoice-bound
            // slider: NormalisableRange(0, numChoices - 1, 1).
            slider.setRange(0.0, (double) (kNumTapeModels - 1), 1.0);

            expectEquals(TapeRotLookAndFeel::getTickCountForSlider(slider), (int) kNumTapeModels);
        }

        beginTest("A continuous (unstepped) slider falls back to the generic tick count");
        {
            juce::Slider slider;
            slider.setRange(0.0, 100.0, 0.0);

            expectEquals(TapeRotLookAndFeel::getTickCountForSlider(slider), TapeRotTheme::Layout::knobNumTicks);
        }

        beginTest("Adding a model to the table changes the derived tick count with no other change");
        {
            juce::Slider slider;
            constexpr int dummyExtraModel = 1;
            slider.setRange(0.0, (double) (kNumTapeModels - 1 + dummyExtraModel), 1.0);

            expectEquals(TapeRotLookAndFeel::getTickCountForSlider(slider), (int) kNumTapeModels + dummyExtraModel);
        }
    }
};

static TapeRotLookAndFeelTests tapeRotLookAndFeelTests;
