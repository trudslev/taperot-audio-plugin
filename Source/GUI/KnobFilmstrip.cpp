#include "KnobFilmstrip.h"

using namespace TapeRotTheme;

KnobFilmstrip::KnobFilmstrip(Layout::Cap cap)
    : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox), capKind(cap)
{
    setVelocityBasedMode(false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
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
