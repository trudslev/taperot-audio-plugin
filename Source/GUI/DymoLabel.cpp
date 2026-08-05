#include "DymoLabel.h"
#include "TapeRotTheme.h"

void DymoLabel::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    // A pre-baked, transparent PNG of the whole nameplate - plate, contact shadow, hand-cut edges
    // and embossed lettering together. See the header for why this stopped being drawn live.
    //
    // The art rect is deliberately larger than dymoX/Y/W/H: the plate is rotated about its own
    // top-left, so its lower-right corner swings past dymoX+dymoW, and the contact shadow extends
    // further down-right again.
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(dymoLabelImage(),
                juce::Rectangle<float>(Layout::dymoArtX, Layout::dymoArtY,
                                        Layout::dymoArtW, Layout::dymoArtH),
                juce::RectanglePlacement::stretchToFit);
}
