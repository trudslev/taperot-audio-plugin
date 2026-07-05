#include "TapeRotLookAndFeel.h"

TapeRotLookAndFeel::TapeRotLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff2b2320));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffd98e4a));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff8a5a30));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd98e4a));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff5a4a3a));
    setColour(juce::Label::textColourId, juce::Colour(0xffe8dcc8));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff3a2f28));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe8dcc8));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe8dcc8));
}
