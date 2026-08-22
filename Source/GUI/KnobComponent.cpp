#include "KnobComponent.h"

using namespace TapeRotTheme;

namespace
{
    /** The component's box has to hold the numeral ring and the label, not just the cap. Numerals
        sit at r + 29.5 with their own 13 px line box, so the ring's outer edge is r + 36. */
    constexpr float ringPad = Layout::knobNumeralRadiusOffset + Type::scaleNumeral.lineBox * 0.5f;
}

KnobComponent::KnobComponent (const Spec& spec)
    : knobSpec (spec)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);

    /*  **Symmetric about TWELVE o'clock, and the offset is 360 rather than 180.**

        JUCE measures rotary angles clockwise from twelve and requires both bounds non-negative, so
        a 270 deg sweep centred on twelve is expressed as 360 +/- 135. Writing `pi +/- half` gives
        the same 270 deg span centred on SIX - the control points at the floor with the correct
        width, which no test of the sweep's magnitude can see.

        `RotaryArcTests` exists for exactly this and it caught this line during the rewrite: the
        construction was reintroduced from scratch and failed the arm that was written when it was
        found the first time. That is the argument for keeping a test whose subject was deleted -
        what it asserts is about the Slider, not about the artwork the old component blitted.  */
    setRotaryParameters (juce::degreesToRadians (360.0f - Layout::knobSweepDegrees * 0.5f),
                         juce::degreesToRadians (360.0f + Layout::knobSweepDegrees * 0.5f),
                         true);
    setMouseDragSensitivity (Layout::knobDragPixels);
}

juce::Rectangle<int> KnobComponent::canvasBounds() const
{
    const float r = Layout::diameterFor (knobSpec.cap) * 0.5f;
    const float top = knobSpec.pivot.y - (r + ringPad);
    // The label's line box is the box's own bottom, so a label pinned away from the ring - which is
    // exactly what §3.1 does to MODEL - still lands inside the component that draws it.
    const float bottom = juce::jmax (knobSpec.pivot.y + r + ringPad,
                                     knobSpec.labelBaselineY + Type::controlLabel.lineBox);
    const float halfW = juce::jmax (r + ringPad, 46.0f);

    return juce::Rectangle<float> (knobSpec.pivot.x - halfW, top, halfW * 2.0f, bottom - top).toNearestInt();
}

bool KnobComponent::hitTest (int x, int y)
{
    /*  Only the cap takes the drag. A component sized to its numeral ring that claimed the whole box
        would swallow clicks meant for the fascia between neighbouring knobs - which on this panel is
        124 px of pitch against a 76 px cap, so the gap is real and clickable-looking.  */
    const float r = Layout::diameterFor (knobSpec.cap) * 0.5f;
    const auto local = knobSpec.pivot - canvasBounds().toFloat().getPosition();
    return juce::Point<float> ((float) x, (float) y).getDistanceFrom (local) <= r;
}

/*  Where a printed mark's tick goes: through the PARAMETER's range, every time.

    `SliderParameterAttachment` copies the parameter's `NormalisableRange` onto the slider, so this
    reads the taper that positions the pointer rather than a second table that has to be kept in
    step with it. A stored angle and a derived one are indistinguishable while they agree; they stop
    agreeing the moment a skew moves, and only one of them moves with it.  */
float KnobComponent::angleForValue (float value) const
{
    const auto& range = getNormalisableRange();
    const float f = (float) range.convertTo0to1 ((double) value);
    return nf::sweepAngleDegrees (f, Layout::knobSweepDegrees);
}

void KnobComponent::paintCap (juce::Graphics& g, juce::Point<float> centre, float radius) const
{
    const bool signature = knobSpec.cap == Layout::Cap::signature;

    /*  §3's cap is `radial-gradient(circle at 34% 24%, ...)`. JUCE's ColourGradient is radial about
        a centre, so the off-centre highlight is expressed by placing that centre at the stated
        34 % / 24 % of the cap's box rather than by faking a second light source.  */
    auto grad = Paint::sculptedFace (centre, radius, 0.34f, 0.24f,
                                     signature ? Colour::darkCapHi : Colour::ivoryCapHi,
                                     signature ? Colour::darkCapLo : Colour::ivoryCapLo);
    grad.addColour (0.46, signature ? Colour::darkCapMid : Colour::ivoryCapMid);

    g.setGradientFill (grad);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    /*  §3's cap is four shadows as well as a fill, and the first pass drew only the fill and the
        rim - which is why the capture came back reading as flat ivory discs.

        The prototype's stack is `inset 0 0 0 1.5px <rim>`, `inset 0 1px 0 rgba(255,255,255,.75)`,
        `inset 0 -3px 8px rgba(0,0,0,.20)` and a cast `0 3px 6px rgba(0,0,0,.24)`. What makes a knob
        read as a physical object is the last two: a shadow pooling at its lower inside edge and one
        under it on the fascia. Neither is decoration - remove them and the cap is a circle.  */
    const auto face = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // The cast shadow, under the cap and offset down. Drawn BEFORE nothing - the fill is already
    // down, so this goes around it as a ring rather than beneath, which is what an inset would be.
    {
        juce::Graphics::ScopedSaveState cast (g);
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.drawEllipse (face.translated (0.0f, 3.0f).reduced (0.5f), 2.0f);
    }

    // The inner shadow pooling at the bottom, and the lip catching light at the top.
    {
        juce::ColourGradient pool (juce::Colours::transparentBlack, centre.x, centre.y - radius,
                                   juce::Colours::black.withAlpha (signature ? 0.60f : 0.20f),
                                   centre.x, centre.y + radius, false);
        g.setGradientFill (pool);
        g.fillEllipse (face);
    }

    g.setColour (juce::Colours::white.withAlpha (signature ? 0.14f : 0.75f));
    g.drawEllipse (face.reduced (1.0f).withTrimmedBottom (radius), 1.0f);

    g.setColour (signature ? Colour::darkCapRim : Colour::ivoryCapRim);
    g.drawEllipse (face.reduced (0.75f), 1.5f);
}

/*  Everything except the pointer, rendered once per (device scale, taper).

    Drawn at the device's own pixel density rather than at logical size, so the 1.5 px minor ticks
    and the 1.4 px sweep arc stay hairlines on a retina display instead of being blitted up from a
    1x render - the same reason the suite used to embed artwork at 2x.

    **The taper is in the key.** A cache keyed on scale alone would hold a ring drawn before the
    attachment supplied the range, which is a stale ring that never rebuilds - and it would look
    entirely plausible, because a default-constructed range is 0..1 linear and every mark would land
    somewhere.  */
void KnobComponent::renderStaticLayer (float deviceScale)
{
    const auto bounds = getLocalBounds().toFloat();
    staticLayer = juce::Image (juce::Image::ARGB,
                               juce::roundToInt (bounds.getWidth()  * deviceScale),
                               juce::roundToInt (bounds.getHeight() * deviceScale), true);
    ++buildCount;

    juce::Graphics g (staticLayer);
    g.addTransform (juce::AffineTransform::scale (deviceScale));

    const auto centre = knobSpec.pivot - canvasBounds().toFloat().getPosition();
    const float r = Layout::diameterFor (knobSpec.cap) * 0.5f;

    //== §3's sweep arc: a 270 deg wedge masked to 1.4 px at r + 6 ============
    /*  Drawn as a stroked arc rather than a conic gradient masked to a ring. The CSS says conic
        because that is how a browser makes a wedge; the wedge is a uniform colour, so what it
        resolves to IS an arc. Sweeping it by hand would reproduce the mechanism, not the mark.  */
    {
        const float half = juce::degreesToRadians (Layout::knobSweepDegrees * 0.5f);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, r + Layout::knobSweepArcInset,
                           r + Layout::knobSweepArcInset, 0.0f, -half, half, true);
        g.setColour (Colour::sweepArc);
        g.strokePath (arc, juce::PathStrokeType (Layout::knobSweepArcWidth));
    }

    //== Ticks ================================================================
    /*  Both weights share an inner end at r + 8; what differs is the outer, which is what makes
        "major 2 x 9, minor 1.5 x 5" a pair of ink lengths rather than two radii.  */
    const float tickInner = r + 8.0f;

    const auto drawTick = [&] (float angleDeg, bool major)
    {
        const float ink   = major ? Layout::knobMajorTickLength : Layout::knobMinorTickLength;
        const float width = major ? Layout::knobMajorTickWidth  : Layout::knobMinorTickWidth;

        juce::Path tick;
        tick.addRectangle (-width * 0.5f, -(tickInner + ink), width, ink);
        g.setColour (Colour::panelInk);
        g.fillPath (tick, juce::AffineTransform::rotation (juce::degreesToRadians (angleDeg))
                              .translated (centre.x, centre.y));
    };

    if (knobSpec.marks == nullptr)
    {
        /*  §3.3's MODEL: nine detents, every one a major tick, none numeralled. Drawn from the
            count rather than from a table of nine empty strings - a table whose `major()` reads the
            numeral would score every one of them minor, which is the opposite of what §3.3 says.  */
        for (int i = 0; i < Marks::modelDetentCount; ++i)
            drawTick (nf::sweepAngleDegrees ((float) i / (float) (Marks::modelDetentCount - 1),
                                             Layout::knobSweepDegrees), true);
    }
    else
    {
        for (int i = 0; i < knobSpec.markCount; ++i)
            drawTick (angleForValue (knobSpec.marks[i].value), knobSpec.marks[i].major());
    }

    //== Numerals, at r + 29.5, counter-rotated to upright ====================
    if (knobSpec.marks != nullptr)
    {
        // §8's scale numeral is the panel's one 500-weight role — Font::numeral, not Font::label.
        const auto font = Font::numeral (Type::scaleNumeral.cssPx);
        const float tracking = Font::trackingPx (Type::scaleNumeral.trackingEm, Type::scaleNumeral.cssPx);
        const float radius = r + Layout::knobNumeralRadiusOffset;

        for (int i = 0; i < knobSpec.markCount; ++i)
        {
            const auto& mark = knobSpec.marks[i];
            if (! mark.major())
                continue;

            /*  §3.2's OUTPUT ring keeps a leading plus and takes a real U+2212 for its minus. The
                table authors the ASCII form so the constant stays greppable; the substitution
                happens once, here, rather than in five string literals.  */
            juce::String numeral (mark.numeral);
            if (numeral.startsWithChar ('-'))
                numeral = Text::minusSign() + numeral.substring (1);

            const float rad = juce::degreesToRadians (angleForValue (mark.value));
            const juce::Point<float> at (centre.x + std::sin (rad) * radius,
                                         centre.y - std::cos (rad) * radius);

            const float w = Text::trackedWidth (numeral, font, tracking) + 4.0f;
            Text::drawTracked (g, numeral, font, tracking,
                               { at.x - w * 0.5f, at.y - Type::scaleNumeral.lineBox * 0.5f,
                                 w, Type::scaleNumeral.lineBox },
                               juce::Justification::centred, Colour::panelInk);
        }
    }

    //== The unit, in the arc's bottom gap ====================================
    if (juce::String (knobSpec.unit).isNotEmpty())
    {
        const auto font = Font::monoAt (Type::unit.cssPx);
        const float tracking = Font::trackingPx (Type::unit.trackingEm, Type::unit.cssPx);
        Text::drawTracked (g, knobSpec.unit, font, tracking,
                           { centre.x - 40.0f, centre.y + r + 20.0f, 80.0f, Type::unit.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }

    //== The cap =============================================================
    paintCap (g, centre, r);

    //== The label ===========================================================
    {
        const auto font = Font::label (Type::controlLabel.cssPx);
        const float tracking = Font::trackingPx (Type::controlLabel.trackingEm, Type::controlLabel.cssPx);
        const float y = knobSpec.labelBaselineY - canvasBounds().toFloat().getY();
        Text::drawTracked (g, knobSpec.label, font, tracking,
                           { 0.0f, y, bounds.getWidth(), Type::controlLabel.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }
}

void KnobComponent::resized()
{
    staticLayer = {};
    builtAtScale = 0.0f;
}

void KnobComponent::paint (juce::Graphics& g)
{
    const float deviceScale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto& range = getNormalisableRange();

    if (! staticLayer.isValid() || ! juce::approximatelyEqual (builtAtScale, deviceScale)
        || ! juce::approximatelyEqual (builtStart, range.start)
        || ! juce::approximatelyEqual (builtEnd, range.end)
        || ! juce::approximatelyEqual (builtSkew, range.skew))
    {
        renderStaticLayer (deviceScale);
        builtAtScale = deviceScale;
        builtStart = range.start;
        builtEnd = range.end;
        builtSkew = range.skew;
    }

    g.drawImageTransformed (staticLayer, juce::AffineTransform::scale (1.0f / deviceScale));

    //== The pointer, the only live element ==================================
    const auto centre = knobSpec.pivot - canvasBounds().toFloat().getPosition();
    const float r = Layout::diameterFor (knobSpec.cap) * 0.5f;
    const float rad = juce::degreesToRadians (angleForValue ((float) getValue()));

    juce::Path pointer;
    pointer.addRoundedRectangle (-Layout::knobPointerWidth * 0.5f, -(r - Layout::knobPointerInset),
                                 Layout::knobPointerWidth, r - Layout::knobPointerInset,
                                 Layout::knobPointerWidth * 0.5f);

    g.setColour (knobSpec.cap == Layout::Cap::signature ? Colour::darkPointer : Colour::ivoryPointer);
    g.fillPath (pointer, juce::AffineTransform::rotation (rad).translated (centre.x, centre.y));
}
