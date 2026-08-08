#include "LampStrip.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    juce::Rectangle<float> lampRect(float x, float y)
    {
        return { x, y, Layout::lampSize, Layout::lampSize };
    }
}

LampStrip::LampStrip(TapeRotAudioProcessor& p) : processorRef(p)
{
    setBounds(0, 0, (int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setInterceptsMouseClicks(true, false);
    startTimerHz(30);
}

bool LampStrip::isAnyFailHeld() const noexcept
{
    return heldFailButton >= 0;
}

int LampStrip::failButtonAt(juce::Point<float> p) const
{
    for (int i = 0; i < (int) Layout::failButtonX.size(); ++i)
        if (lampRect(Layout::failButtonX[(size_t) i], Layout::failButtonY).contains(p))
            return i;

    return -1;
}

void LampStrip::mouseDown(const juce::MouseEvent& e)
{
    const int hit = failButtonAt(e.position);

    if (hit < 0)
        return;

    heldFailButton = hit;

    if (auto* param = processorRef.apvts.getParameter(Layout::failParamIds[(size_t) hit]))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(1.0f);
    }

    repaint();
}

void LampStrip::mouseUp(const juce::MouseEvent&)
{
    if (heldFailButton < 0)
        return;

    if (auto* param = processorRef.apvts.getParameter(Layout::failParamIds[(size_t) heldFailButton]))
    {
        param->setValueNotifyingHost(0.0f);
        param->endChangeGesture();
    }

    heldFailButton = -1;
    repaint();
}

void LampStrip::timerCallback()
{
    // Drain the failure engine's event FIFO and start a flash per event. This is the consumer the
    // FIFO was built for and never had.
    FailureEvent events[64];
    const int n = processorRef.getFailureEngine().popEvents(events, 64);
    const auto now = juce::Time::getMillisecondCounter();
    bool changed = false;

    for (int i = 0; i < n; ++i)
    {
        const auto index = (size_t) events[i].type;

        if (index < faultFlashUntil.size())
        {
            faultFlashUntil[index] = now + (juce::uint32) Layout::faultFlashMs;
            changed = true;
        }
    }

    for (auto until : faultFlashUntil)
        if (until != 0 && now >= until)
            changed = true;

    if (changed || n > 0)
        repaint();
}

void LampStrip::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    const auto blit = [&g](const juce::Image& img, juce::Rectangle<float> r)
    {
        if (img.isValid())
            g.drawImage(img, r);
    };

    // --- GENERATION: segments 1..GEN lit ------------------------------------------------------
    const int genLit = juce::jlimit(0, Layout::genSegmentCount,
                                    juce::roundToInt(processorRef.getGenDisplay()));

    for (int i = 0; i < Layout::genSegmentCount; ++i)
        blit(Asset::genSegment(i < genLit),
             { Layout::genSegmentX0 + (float) i * Layout::genSegmentPitch, Layout::genSegmentY,
               Layout::genSegmentW, Layout::genSegmentH });

    // --- FAULT ACTIVITY: lit while a flash is running -----------------------------------------
    const auto now = juce::Time::getMillisecondCounter();

    for (size_t i = 0; i < Layout::faultDotX.size(); ++i)
        blit(Asset::lamp(faultFlashUntil[i] != 0 && now < faultFlashUntil[i]),
             lampRect(Layout::faultDotX[i], Layout::faultDotY));

    // --- FAIL buttons -------------------------------------------------------------------------
    for (int i = 0; i < (int) Layout::failButtonX.size(); ++i)
    {
        const auto r = lampRect(Layout::failButtonX[(size_t) i], Layout::failButtonY);

        if (heldFailButton == i)
            blit(Asset::lampPressed(), r);
        else
            blit(Asset::lamp(false), r);
    }

    // --- the scope strip's FAIL LED -----------------------------------------------------------
    blit(Asset::failLed(isAnyFailHeld()),
         { Layout::failLedTopLeft.x, Layout::failLedTopLeft.y,
           Layout::failLedSize, Layout::failLedSize });
}
