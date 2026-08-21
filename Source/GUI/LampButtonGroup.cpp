#include "LampButtonGroup.h"

using namespace TapeRotTheme;

LampButtonGroup::LampButtonGroup (const Switches::LampGroup& spec)
    : groupSpec (spec)
{
}

juce::Rectangle<int> LampButtonGroup::canvasBounds() const
{
    float left = groupSpec.buttonX[0], right = groupSpec.buttonX[0] + Switches::lampButtonDiameter;

    for (int i = 0; i < groupSpec.count; ++i)
    {
        left  = juce::jmin (left,  groupSpec.buttonX[(size_t) i]);
        right = juce::jmax (right, groupSpec.buttonX[(size_t) i] + Switches::lampButtonDiameter);
    }

    // The legends are wider than the caps they sit under, so the box is padded to hold them.
    const float pad = 12.0f;
    const float top = groupSpec.captionY;
    const float bottom = groupSpec.legendY + Type::legend.lineBox;

    return juce::Rectangle<float> (left - pad, top, (right - left) + pad * 2.0f, bottom - top).toNearestInt();
}

void LampButtonGroup::setLit (int index, bool shouldBeLit)
{
    if (! juce::isPositiveAndBelow (index, groupSpec.count) || lit[(size_t) index] == shouldBeLit)
        return;

    lit[(size_t) index] = shouldBeLit;
    repaint();
}

bool LampButtonGroup::isLit (int index) const
{
    return juce::isPositiveAndBelow (index, groupSpec.count) && lit[(size_t) index];
}

int LampButtonGroup::indexAt (juce::Point<float> local) const
{
    const float originX = (float) canvasBounds().getX();
    const float r = Switches::lampButtonDiameter * 0.5f;

    for (int i = 0; i < groupSpec.count; ++i)
    {
        const juce::Point<float> centre (groupSpec.buttonX[(size_t) i] - originX + r,
                                         groupSpec.buttonY - (float) canvasBounds().getY() + r);
        if (local.getDistanceFrom (centre) <= r)
            return i;
    }

    return -1;
}

void LampButtonGroup::mouseDown (const juce::MouseEvent& e)
{
    heldIndex = indexAt (e.position);

    if (heldIndex >= 0 && onPressed != nullptr)
        onPressed (heldIndex);
}

void LampButtonGroup::mouseUp (const juce::MouseEvent&)
{
    /*  The release fires unconditionally once a press was taken, rather than only when the pointer
        is still inside. A momentary trigger released off-target must still release - root
        `CLAUDE.md`'s rule is that a Program must never load one stuck engaged, and a drag off the
        cap is exactly how one gets stuck.  */
    if (heldIndex >= 0 && onReleased != nullptr)
        onReleased (heldIndex);

    heldIndex = -1;
}

void LampButtonGroup::paint (juce::Graphics& g)
{
    const auto origin = canvasBounds().toFloat().getPosition();

    //== The group caption ===================================================
    {
        const auto font = Font::label (Type::switchCaption.cssPx);
        const float tracking = Font::trackingPx (Type::switchCaption.trackingEm, Type::switchCaption.cssPx);
        Text::drawTracked (g, groupSpec.caption, font, tracking,
                           { 0.0f, groupSpec.captionY - origin.y, (float) getWidth(),
                             Type::switchCaption.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }

    const auto legendFont = Font::label (Type::legend.cssPx);
    const float legendTracking = Font::trackingPx (Type::legend.trackingEm, Type::legend.cssPx);

    for (int i = 0; i < groupSpec.count; ++i)
    {
        const float capR = Switches::lampButtonDiameter * 0.5f;
        const juce::Point<float> centre (groupSpec.buttonX[(size_t) i] - origin.x + capR,
                                         groupSpec.buttonY - origin.y + capR);

        //== §5.3's dark cap, radial at 36% 28% ==============================
        {
            auto grad = Paint::sculptedFace (centre, capR, 0.36f, 0.28f,
                                             Colour::lampCapHi, Colour::lampCapLo);
            grad.addColour (0.52, Colour::lampCapMid);
            g.setGradientFill (grad);
            g.fillEllipse (centre.x - capR, centre.y - capR,
                           Switches::lampButtonDiameter, Switches::lampButtonDiameter);
        }

        //== The Ø11 lamp in its face ========================================
        const float lampR = Switches::lampDiameter * 0.5f;
        const bool on = lit[(size_t) i];

        /*  §5.3: **light stops at the lens edge.** The glow is drawn INSIDE the lens rather than
            around it, so a lit lamp never bleeds onto the fascia - which is the difference between
            a lens and a hole in the panel, and the reason an unlit lamp still reads as glass.  */
        if (on)
        {
            juce::ColourGradient glow (Colour::lampGlow, centre.x, centre.y,
                                       Colour::lampGlow.withAlpha (0.0f),
                                       centre.x, centre.y - (lampR + Switches::lampGlowRadius), true);
            g.setGradientFill (glow);
            g.fillEllipse (centre.x - lampR - Switches::lampGlowRadius,
                           centre.y - lampR - Switches::lampGlowRadius,
                           (lampR + Switches::lampGlowRadius) * 2.0f,
                           (lampR + Switches::lampGlowRadius) * 2.0f);
        }

        auto lens = Paint::sculptedFace (centre, lampR, 0.38f, 0.30f,
                                         on ? Colour::lampLitHi : Colour::lampDarkHi,
                                         on ? Colour::lampLitEdge : Colour::lampDarkLo);
        lens.addColour (on ? 0.42 : 0.60, on ? Colour::lampLitMid : Colour::lampDarkMid);
        if (on)
            lens.addColour (0.78, Colour::lampLitLo);

        g.setGradientFill (lens);
        g.fillEllipse (centre.x - lampR, centre.y - lampR,
                       Switches::lampDiameter, Switches::lampDiameter);

        //== The legend ======================================================
        const juce::String legend (groupSpec.legends[(size_t) i]);
        Text::drawTracked (g, legend, legendFont, legendTracking,
                           { centre.x - 30.0f, groupSpec.legendY - origin.y, 60.0f, Type::legend.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }
}
