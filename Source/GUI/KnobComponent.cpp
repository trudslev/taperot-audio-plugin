#include "KnobComponent.h"

#include <cmath>

using namespace TapeRotTheme;

namespace
{
    /*  **A CSS blur is a Gaussian, and its alpha across the edge is that Gaussian's CDF.** The
        standard deviation is `blur / 2`, so full alpha is reached three sigma INSIDE the shape and
        the edge itself sits at exactly half — never at 0 on one side and full on the other.

        The obvious construction is a linear ramp between two radii, and it is wrong in both
        directions at once: it reaches full alpha at the rim where CSS is at 0.77 of it, and it is
        still a tenth of full alpha four pixels outside where CSS has decayed to a sixtieth.
        Measured against the delivered prototype at DRIVE's own centre, the linear form left the
        cap's bottom rim 63 sum-RGB units dark and the fascia 4.5 px to its right 45 units dark —
        which is what "the creme knobs are too dark" was.  */
    inline float gaussianEdge (float dist, float edge, float sigma)
    {
        return 0.5f * (1.0f + std::erf ((dist - edge)
                                        / (sigma * juce::MathConstants<float>::sqrt2)));
    }

    /** A radial gradient whose alpha follows an arbitrary profile, sampled finely enough that the
        linear interpolation `ColourGradient` does between stops cannot be seen. */
    template <typename AlphaFn>
    juce::ColourGradient profiledRadial (juce::Colour ink, juce::Point<float> centre, float reach,
                                         AlphaFn&& alphaAt)
    {
        juce::ColourGradient grad (ink.withAlpha (alphaAt (0.0f)), centre,
                                   ink.withAlpha (alphaAt (reach)),
                                   centre.translated (reach, 0.0f), true);

        for (int i = 1; i < 24; ++i)
        {
            const double p = (double) i / 24.0;
            grad.addColour (p, ink.withAlpha (alphaAt ((float) p * reach)));
        }

        return grad;
    }

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

/*  **Shift-fine, restored.** BRAND.md makes drag feel suite-wide rather than a per-casting
    choice — *"190px vertical coarse, 760px on Shift for fine, everywhere… a player who learns
    Shift-fine on one expects it on the next"* — and the panel rewrite dropped it: the old
    `KnobFilmstrip::mouseDown` set it and `KnobComponent` carried the constant across without the
    call. `Layout::knobFineDragPixels` sat declared and unread, which is the trace the audit found.

    **Sensitivity has to be settled BEFORE `Slider::mouseDown` records its drag anchor.** JUCE
    measures from that anchor and scales by the current sensitivity, so changing it mid-drag
    rescales the distance already travelled and the value jumps. That comment came with the line
    the rewrite deleted and comes back with it.  */
void KnobComponent::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? Layout::knobFineDragPixels
                                                  : Layout::knobDragPixels);
    juce::Slider::mouseDown (e);
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

/*  §3's cap, and **the prototype's shadow stack is four layers with stated blurs — not three
    approximations of it.** Rebuilt 2026-08-23 after the panel came back reading too dark and the
    shine reading wrong; both were one layer.

        ivory      inset 0 0 0 1.5px #443e36
                   inset 0 1px 0 rgba(255,255,255,.75)
                   inset 0 -3px 8px rgba(0,0,0,.20)
                   0 3px 6px rgba(0,0,0,.24)
        signature  the same four, at 1.5px #0e0c09 / .14 / -3px 9px .6 / 0 4px 8px .38

    **What was wrong: the inner bottom shadow was drawn as a linear gradient over the WHOLE face**,
    transparent at the top to 20 % black at the bottom. `inset 0 -3px 8px` is a shadow hugging the
    bottom rim with an 8 px blur — it reaches about a tenth of the disc. Spread across all of it,
    the cap measured **#E9E4D6 at the light point against §3's #F8F2E3, #D8D1BF at the centre and
    #BBB29D at the bottom inner edge** — 15 levels down at the top and 35 at the bottom, on every
    ivory knob and every lamp cap.

    That is worth keeping as the tell: **a wrong SHAPE for a shadow reads as a wrong COLOUR for the
    thing under it.** The report was "the shade is too dark" and the fix is in the geometry of a
    layer above it, not in the palette — `ivoryCapMid` was always exactly §3's `#efe7d2`.  */
void KnobComponent::paintCap (juce::Graphics& g, juce::Point<float> centre, float radius) const
{
    const bool signature = knobSpec.cap == Layout::Cap::signature;
    const auto face = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // §3's per-class figures, in the order the CSS lists them.
    const float rimW        = 1.5f;
    const float lipAlpha    = signature ? 0.14f : 0.75f;
    const float innerAlpha  = signature ? 0.60f : 0.20f;
    const float innerOffsetY = 3.0f;   // `-3px` in both classes
    const float innerBlur   = signature ? 9.0f  : 8.0f;
    const float castAlpha   = signature ? 0.38f : 0.24f;
    const float castOffsetY = signature ? 4.0f  : 3.0f;
    const float castBlur    = signature ? 8.0f  : 6.0f;

    //== `0 Npx Mpx rgba(0,0,0,a)` — the cast shadow, under the cap ===========
    /*  Drawn first so the fill covers its middle and only the fringe shows, which is what a drop
        shadow is. It used to be a 2 px black STROKE ring, which is a hard outline rather than a
        blurred drop and read as a dark line around every knob.  */
    {
        const juce::Point<float> at (centre.x, centre.y + castOffsetY);
        const float sigma = castBlur * 0.5f;
        const float reach = radius + castBlur * 2.0f;   // four sigma out is nothing at all

        g.setGradientFill (profiledRadial (juce::Colours::black, at, reach,
                                           [=] (float d)
                                           {
                                               return castAlpha * (1.0f - gaussianEdge (d, radius, sigma));
                                           }));
        g.fillEllipse (juce::Rectangle<float> (reach * 2.0f, reach * 2.0f).withCentre (at));
    }

    //== The radial fill =====================================================
    auto grad = Paint::sculptedFace (centre, radius, 0.34f, 0.24f,
                                     signature ? Colour::darkCapHi : Colour::ivoryCapHi,
                                     signature ? Colour::darkCapLo : Colour::ivoryCapLo);
    grad.addColour (signature ? 0.46 : 0.45, signature ? Colour::darkCapMid : Colour::ivoryCapMid);
    g.setGradientFill (grad);
    g.fillEllipse (face);

    //== `inset 0 -3px Npx rgba(0,0,0,a)` — confined to the rim ==============
    /*  An inset shadow is cast by everything OUTSIDE the shape, offset by 3 px UP — which is what
        biases the darkening to the bottom. So the profile is a function of the distance from a
        centre 3 px above the cap's, half alpha at the cap's own radius, and the clip drops the
        part that would fall outside.  */
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path capPath;
        capPath.addEllipse (face);
        g.reduceClipRegion (capPath);

        const juce::Point<float> at (centre.x, centre.y - innerOffsetY);
        const float sigma = innerBlur * 0.5f;
        const float reach = radius + innerOffsetY;

        g.setGradientFill (profiledRadial (juce::Colours::black, at, reach,
                                           [=] (float d)
                                           {
                                               return innerAlpha * gaussianEdge (d, radius, sigma);
                                           }));
        g.fillEllipse (face);
    }

    //== `inset 0 1px 0 rgba(255,255,255,a)` — the lip =======================
    /*  A 1 px inset shadow with no blur and no spread is the shape minus itself offset down 1 px:
        a crescent along the top, **widest at twelve o'clock and tapering to nothing at three and
        nine**. Two ellipses with even-odd winding give exactly that symmetric difference, and the
        clip drops the matching crescent at the bottom.

        It used to be a uniform 1 px arc across the whole top semicircle at the same alpha, which
        reads as a bright ring rather than a lit edge.  */
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path capPath;
        capPath.addEllipse (face);
        g.reduceClipRegion (capPath);

        juce::Path crescent;
        crescent.setUsingNonZeroWinding (false);
        crescent.addEllipse (face);
        crescent.addEllipse (face.translated (0.0f, 1.0f));

        g.setColour (juce::Colours::white.withAlpha (lipAlpha));
        g.fillPath (crescent);
    }

    //== `inset 0 0 0 1.5px <rim>` — over the insets, as the CSS order puts it
    g.setColour (signature ? Colour::darkCapRim : Colour::ivoryCapRim);
    g.drawEllipse (face.reduced (rimW * 0.5f), rimW);

    //== The specular — §3 draws it as its OWN element, over the cap =========
    /*  `inset:7px` + `radial-gradient(20% 15% at 32% 18%, white .85, white .3 55%, transparent)`.

        **This is the shine, and it was missing entirely.** The cap's own fill brightens toward
        34/24 and reads as a lit sphere, which is exactly why its absence looked like a cap that
        was merely too flat rather than one with a whole element unbuilt — the panel had a
        plausible amount of light on it. Measured at MODEL's centre the miss is 473 sum-RGB units,
        the largest single difference from the prototype anywhere on this panel; on the ivory caps
        it is 38, small enough to read as a shade problem and be reported as one.

        It is an ELLIPSE — 20 % of the inset box wide against 15 % tall — so a circular gradient is
        drawn and squashed about its own centre. Note it is NOT conditional on the cap class: the
        same white at the same alpha sits on the dark MODEL cap, which is what stops the signature
        control reading as a hole in the fascia.  */
    {
        const float sr = radius - 7.0f;   // `inset:7px`

        if (sr > 0.5f)
        {
            const auto box = juce::Rectangle<float> (sr * 2.0f, sr * 2.0f).withCentre (centre);
            const juce::Point<float> at (box.getX() + 0.32f * box.getWidth(),
                                         box.getY() + 0.18f * box.getHeight());
            const float rx = 0.20f * box.getWidth();
            const float ry = 0.15f * box.getHeight();

            juce::Graphics::ScopedSaveState state (g);
            juce::Path inner;
            inner.addEllipse (box);
            g.reduceClipRegion (inner);
            g.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, at.x, at.y));

            juce::ColourGradient spec (juce::Colours::white.withAlpha (0.85f), at,
                                       juce::Colours::transparentWhite,
                                       at.translated (rx, 0.0f), true);
            spec.addColour (0.55, juce::Colours::white.withAlpha (0.30f));
            g.setGradientFill (spec);
            g.fillEllipse (juce::Rectangle<float> (rx * 2.0f, rx * 2.0f).withCentre (at));
        }
    }
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
