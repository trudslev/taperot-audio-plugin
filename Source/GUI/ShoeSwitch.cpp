#include "ShoeSwitch.h"

using namespace TapeRotTheme;

ShoeSwitch::ShoeSwitch (const Switches::Shoe& spec)
    : shoeSpec (spec)
{
}

juce::Rectangle<int> ShoeSwitch::canvasBounds() const
{
    const float top = shoeSpec.captionY;
    const float bottom = shoeSpec.shoeY + Switches::shoeH + Switches::shoeLegendY
                       + Type::legend.lineBox;
    return juce::Rectangle<float> (shoeSpec.x, top, Switches::shoeW, bottom - top).toNearestInt();
}

void ShoeSwitch::setRightSelected (bool shouldBeRight)
{
    if (rightSelected == shouldBeRight)
        return;

    rightSelected = shouldBeRight;
    repaint();
}

void ShoeSwitch::mouseDown (const juce::MouseEvent& e)
{
    const float shoeTop = shoeSpec.shoeY - (float) canvasBounds().getY();
    const auto shoe = juce::Rectangle<float> (0.0f, shoeTop, Switches::shoeW, Switches::shoeH);

    if (! shoe.contains (e.position))
        return;

    const bool wantRight = e.position.x >= Switches::shoeHalfW;

    if (wantRight == rightSelected)
        return;

    rightSelected = wantRight;
    repaint();

    if (onSelectionChanged != nullptr)
        onSelectionChanged (rightSelected);
}

void ShoeSwitch::paintHalf (juce::Graphics& g, juce::Rectangle<float> half, bool engaged) const
{
    g.setGradientFill ({ engaged ? Colour::shoeOnTop    : Colour::shoeOffTop,    0.0f, half.getY(),
                         engaged ? Colour::shoeOnBottom : Colour::shoeOffBottom, 0.0f, half.getBottom(),
                         false });
    g.fillRect (half);

    /*  §5.1's `inset 0 1px 0 rgba(255,255,255,.9)` on the engaged half only. It is what makes the
        pale half read as a raised key rather than as a lit panel - the same distinction BRAND.md
        draws between a backlit legend and merely brighter ink.  */
    if (engaged)
    {
        g.setColour (Colour::shoeOnSheen);
        g.fillRect (half.getX(), half.getY(), half.getWidth(), 1.0f);
    }
}

void ShoeSwitch::paint (juce::Graphics& g)
{
    const float originY = (float) canvasBounds().getY();
    const float shoeTop = shoeSpec.shoeY - originY;

    //== The caption =========================================================
    {
        const auto font = Font::label (Type::switchCaption.cssPx);
        const float tracking = Font::trackingPx (Type::switchCaption.trackingEm, Type::switchCaption.cssPx);
        Text::drawTracked (g, shoeSpec.caption, font, tracking,
                           { 0.0f, shoeSpec.captionY - originY, Switches::shoeW, Type::switchCaption.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }

    //== The shoe: two 64 halves inside one ring =============================
    {
        juce::Graphics::ScopedSaveState clip (g);
        juce::Path rounded;
        rounded.addRoundedRectangle (0.0f, shoeTop, Switches::shoeW, Switches::shoeH, Switches::shoeRadius);
        g.reduceClipRegion (rounded);

        paintHalf (g, { 0.0f, shoeTop, Switches::shoeHalfW, Switches::shoeH }, ! rightSelected);
        paintHalf (g, { Switches::shoeHalfW, shoeTop, Switches::shoeHalfW, Switches::shoeH }, rightSelected);
    }

    g.setColour (Colour::shoeRing);
    g.drawRoundedRectangle (0.5f, shoeTop + 0.5f, Switches::shoeW - 1.0f, Switches::shoeH - 1.0f,
                            Switches::shoeRadius, 1.0f);

    //== The two legends, each centred under its OWN segment =================
    /*  §6 measures this ink twice: 8.79 on bare fascia, and **7.89** where a legend sits under an
        engaged segment's `#d8cdb0` shadow. The second is the one that has to clear the functional
        floor, and it does.  */
    {
        const auto font = Font::label (Type::legend.cssPx);
        const float tracking = Font::trackingPx (Type::legend.trackingEm, Type::legend.cssPx);
        const float y = shoeTop + Switches::shoeH + Switches::shoeLegendY;

        Text::drawTracked (g, shoeSpec.left, font, tracking,
                           { 0.0f, y, Switches::shoeHalfW, Type::legend.lineBox },
                           juce::Justification::centred, Colour::panelInk);
        Text::drawTracked (g, shoeSpec.right, font, tracking,
                           { Switches::shoeHalfW, y, Switches::shoeHalfW, Type::legend.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }
}
