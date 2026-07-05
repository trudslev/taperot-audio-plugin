#include "PluginEditor.h"

namespace
{
    constexpr int referenceWidth = (int) TapeRotTheme::Layout::canvasWidth;
    constexpr int referenceHeight = (int) TapeRotTheme::Layout::canvasHeight;
}

TapeRotAudioProcessorEditor::TapeRotAudioProcessorEditor(TapeRotAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), content(p)
{
    addAndMakeVisible(content);

    setResizable(true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio((double) referenceWidth / (double) referenceHeight);
        constrainer->setSizeLimits(referenceWidth / 2, referenceHeight / 2,
                                    referenceWidth * 2, referenceHeight * 2);
    }

    setSize(referenceWidth, referenceHeight);
}

TapeRotAudioProcessorEditor::~TapeRotAudioProcessorEditor() = default;

void TapeRotAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) referenceWidth;
    content.setTransform(juce::AffineTransform::scale(scale));
    content.setBounds(0, 0, referenceWidth, referenceHeight);
}
