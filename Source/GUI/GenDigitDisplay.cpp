#include "GenDigitDisplay.h"
#include "TapeRotTheme.h"

GenDigitDisplay::GenDigitDisplay(TapeRotAudioProcessor& processor) : processorRef(processor)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(15);
}

GenDigitDisplay::~GenDigitDisplay()
{
    stopTimer();
}

void GenDigitDisplay::timerCallback()
{
    const int gen = (int) std::round(processorRef.getGenDisplay());
    if (gen != displayedGen)
    {
        displayedGen = gen;
        repaint();
    }
}

void GenDigitDisplay::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    if (displayedGen < 1)
        return;

    const float cellX = Layout::digitCellFirstX + 2.0f * (Layout::digitCellW + Layout::digitCellGap);
    juce::Rectangle<float> cell(cellX, Layout::digitCellY, Layout::digitCellW, Layout::digitCellH);

    g.setFont(counterDigitFont());
    g.setColour(Colour::digitText);
    g.drawText(juce::String(displayedGen), cell, juce::Justification::centred, false);
}
