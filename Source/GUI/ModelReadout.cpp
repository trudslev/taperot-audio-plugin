#include "ModelReadout.h"
#include "TapeRotTheme.h"
#include "../DSP/TapeModelData.h"

ModelReadout::ModelReadout(TapeRotAudioProcessor& processor) : processorRef(processor)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

ModelReadout::~ModelReadout()
{
    stopTimer();
}

void ModelReadout::timerCallback()
{
    const int modelIndex = juce::jlimit(0, (int) kNumTapeModels - 1,
                                         (int) processorRef.apvts.getRawParameterValue(ParamIDs::model)->load());

    bool needsRepaint = false;
    if (modelIndex != displayedModelIndex)
    {
        displayedModelIndex = modelIndex;
        flashLevel = 1.0f;
        needsRepaint = true;
    }
    else if (flashLevel > 0.001f)
    {
        flashLevel *= 0.8f; // ~150ms decay at 30Hz
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void ModelReadout::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    juce::Rectangle<float> box(0.0f, 0.0f, Layout::modelReadoutW, Layout::modelReadoutH);
    g.setColour(Colour::dark);
    g.fillRoundedRectangle(box, Layout::modelReadoutRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(box, Layout::modelReadoutRadius, 1.2f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRoundedRectangle(2.0f, 2.0f, Layout::modelReadoutW - 4.0f, 3.0f, 1.5f);

    if (displayedModelIndex >= 0)
    {
        const juce::String name = kTapeModels[(size_t) displayedModelIndex].displayName;

        float fontSize = Layout::modelReadoutNominalFontPx;
        const float maxTextWidth = Layout::modelReadoutW - 8.0f;
        while (fontSize > Layout::modelReadoutMinFontPx
               && trackedTextWidth(name, modelReadoutFont(fontSize), modelReadoutTracking) > maxTextWidth)
            fontSize -= 0.5f;

        const auto textColour = Colour::amberBright.interpolatedWith(juce::Colours::white, flashLevel * 0.6f);
        drawTrackedText(g, name, modelReadoutFont(fontSize), modelReadoutTracking,
                         juce::Rectangle<float>(0.0f, 0.0f, Layout::modelReadoutW, Layout::modelReadoutH),
                         juce::Justification::centred, textColour);
    }

    drawTrackedText(g, "MODEL", microLabelFont(), microLabelTracking,
                     juce::Rectangle<float>(0.0f, Layout::modelReadoutLabelY - Layout::modelReadoutY - 6.0f,
                                             Layout::modelReadoutW, 12.0f),
                     juce::Justification::centred, Colour::mutedLabel);
}
