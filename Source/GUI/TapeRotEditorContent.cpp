#include "TapeRotEditorContent.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

TapeRotEditorContent::TapeRotEditorContent(TapeRotAudioProcessor& p) : processorRef(p)
{
    setSize((int) Layout::canvasWidth, (int) Layout::canvasHeight);
}

TapeRotEditorContent::~TapeRotEditorContent() = default;

void TapeRotEditorContent::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(Asset::panel(),
                juce::Rectangle<float>(Layout::canvasWidth, Layout::canvasHeight));
}
