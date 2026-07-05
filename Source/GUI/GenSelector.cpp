#include "GenSelector.h"
#include "TapeRotTheme.h"

GenSelector::GenSelector()
{
    setSliderStyle(juce::Slider::LinearHorizontal);
    setRange(1.0, 8.0, 1.0);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
}

void GenSelector::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    const int currentGen = (int) std::round(getValue());
    const float totalW = 8.0f * Layout::genSelectorSegmentW + 7.0f * Layout::genSelectorGap;
    float x = (getWidth() - totalW) * 0.5f;

    for (int i = 1; i <= 8; ++i)
    {
        juce::Rectangle<float> segment(x, 0.0f, Layout::genSelectorSegmentW, Layout::genSelectorSegmentH);

        g.setColour(i <= currentGen ? Colour::amber : Colour::dark);
        g.fillRoundedRectangle(segment, 2.0f);
        g.setColour(Colour::rim);
        g.drawRoundedRectangle(segment, 2.0f, 1.0f);

        x += Layout::genSelectorSegmentW + Layout::genSelectorGap;
    }
}
