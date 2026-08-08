#include "SpriteButton.h"

using namespace TapeRotTheme;

SpriteButton::SpriteButton(juce::AudioProcessorValueTreeState& s, const Layout::ButtonSpec& sp,
                           const char* onData, int onSize, const char* offData, int offSize)
    : apvts(s), spec(sp)
{
    onImage = juce::ImageCache::getFromMemory(onData, onSize);
    offImage = juce::ImageCache::getFromMemory(offData, offSize);

    setBounds(juce::Rectangle<float>(spec.spriteTopLeft.x, spec.spriteTopLeft.y,
                                     Layout::buttonW, Layout::buttonH).getSmallestIntegerContainer());
}

bool SpriteButton::isSelected() const
{
    auto* raw = apvts.getRawParameterValue(spec.paramId);

    if (raw == nullptr)
        return false;

    const float v = raw->load();
    return spec.isBoolParam ? ((v > 0.5f) == (spec.choiceIndex == 1))
                            : (juce::roundToInt(v) == spec.choiceIndex);
}

void SpriteButton::mouseDown(const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter(spec.paramId))
    {
        // Exclusive select: pressing a member always selects it, never toggles it off.
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
        const float target = ranged != nullptr ? ranged->convertTo0to1((float) spec.choiceIndex)
                                               : (float) spec.choiceIndex;
        p->beginChangeGesture();
        p->setValueNotifyingHost(target);
        p->endChangeGesture();
    }

    if (onInteraction != nullptr)
        onInteraction(spec.paramId);

    repaint();
}

void SpriteButton::paint(juce::Graphics& g)
{
    const auto& img = isSelected() ? onImage : offImage;

    if (! img.isValid())
        return;

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(img, juce::Rectangle<float>((float) getWidth(), (float) getHeight()));
}
