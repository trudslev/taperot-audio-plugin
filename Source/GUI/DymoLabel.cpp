#include "DymoLabel.h"
#include "TapeRotTheme.h"

void DymoLabel::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(Layout::dymoRotationDegrees))
                        .translated(Layout::dymoX, Layout::dymoY));

    const juce::Rectangle<float> plate(0.0f, 0.0f, Layout::dymoW, Layout::dymoH);

    juce::Path platePath;
    platePath.addRoundedRectangle(plate, 4.0f);
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 4, {0, 2});
    shadow.drawForPath(g, platePath);

    g.setColour(Colour::dark);
    g.fillPath(platePath);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.strokePath(platePath, juce::PathStrokeType(1.0f));

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(2.0f, 2.0f, 164.0f, 4.0f, 2.0f);

    const auto textArea = plate.withTrimmedBottom(plate.getHeight() * 0.32f);
    drawTrackedText(g, "TAPEROT", dymoFont(), dymoTracking, textArea,
                     juce::Justification::centred, Colour::cream);
    drawTrackedText(g, "TAPEROT", dymoFont(), dymoTracking,
                     textArea.translated(0.0f, -1.0f),
                     juce::Justification::centred, juce::Colours::black.withAlpha(0.35f));
    drawTrackedText(g, "TAPEROT", dymoFont(), dymoTracking, textArea,
                     juce::Justification::centred, Colour::cream);

    g.restoreState();
}
