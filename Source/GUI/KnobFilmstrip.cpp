#include "KnobFilmstrip.h"

using namespace TapeRotTheme;

KnobFilmstrip::KnobFilmstrip(Layout::Cap cap)
    : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox), capKind(cap)
{
    setVelocityBasedMode(false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setMouseDragSensitivity(Layout::knobDragPixels);

    // Stated rather than inherited - see Layout::knobSweepDegrees for why a fully-overridden
    // paint() is exactly the situation in which an unstated sweep goes unnoticed.
    setRotaryParameters(juce::degreesToRadians(180.0f - Layout::knobSweepDegrees * 0.5f),
                        juce::degreesToRadians(180.0f + Layout::knobSweepDegrees * 0.5f),
                        true);
}

void KnobFilmstrip::mouseDown(const juce::MouseEvent& e)
{
    // Sensitivity has to be settled BEFORE Slider::mouseDown records its drag anchor: JUCE measures
    // the drag from that anchor and scales by the current sensitivity, so changing it part-way
    // through rescales the distance already travelled and the value jumps.
    setMouseDragSensitivity(e.mods.isShiftDown() ? Layout::knobFineDragPixels
                                                 : Layout::knobDragPixels);

    juce::Slider::mouseDown(e);
}

void KnobFilmstrip::setSpriteTopLeft(juce::Point<float> topLeft)
{
    const float f = Layout::frameSizeFor(capKind);
    setBounds(juce::Rectangle<float>(topLeft.x, topLeft.y, f, f).getSmallestIntegerContainer());
}

void KnobFilmstrip::paint(juce::Graphics& g)
{
    const auto& strip = Asset::capStrip(capKind);

    if (! strip.isValid())
        return;

    const int frames = Layout::frameCountFor(capKind);
    const double proportion = valueToProportionOfLength(getValue());
    const int frame = juce::jlimit(0, frames - 1, juce::roundToInt(proportion * (frames - 1)));

    // The strip is embedded at 2x; the source rectangle is in its own pixels, the destination in
    // the 1x design space this component lives in.
    const int src = (int) Layout::frameSizeFor(capKind) * Layout::assetScale;

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(strip,
                0, 0, getWidth(), getHeight(),
                0, frame * src, src, src);
}
