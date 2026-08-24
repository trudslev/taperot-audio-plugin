#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <BinaryData.h>
#include <nf/HeaderPart.h>
#include <nf/ParameterReadout.h>
#include <nf/PrintedScale.h>

#include <array>

/**
    TapeRot's design tokens, from `design/GUI-SPEC.md` (model MT-77, harmonisation round).

    **The panel is code-drawn.** Call 5 retired the filmstrips and the plate together: every knob,
    cap, pointer, tick, numeral, unit, label, shoe, lamp, divider and heading on this panel is a
    draw call. **Exactly one bitmap ships** — the wordmark, and only because Impact Label Reversed
    is donationware that cannot be embedded (§9). That is the whole asset list.

    This file used to open with *"Nothing printed is drawn in code"* and describe a 1336 x 679
    canvas with 31 sprites. Both were true of revision 1 and neither is true now. The inversion is
    worth stating rather than deleting, because the failure mode inverted with it: while the panel
    was baked, the hazard was **double-printing** a runtime draw over baked ink, which is visible.
    Now the same element fails by being **absent**, and the panel merely looks emptier than the
    render. Root `CLAUDE.md` records that shape from Chorus-60, whose hand-authored enumeration of
    the same transition came out thirteen rows short.

    So this panel's element list was **derived, not authored** — `tools/enumerate_prototype.py` over
    the delivered `design/TapeRot MT-77 Panel.dc.html`, which reports 326 objects (199 material,
    129 ink) in 17 type roles. Every figure below is measured off that enumeration or stated by the
    spec, and the two were compared rather than one trusted.

    Coordinates are 1x logical pixels from the top-left of the 1340 x 790 canvas.
*/
namespace TapeRotTheme
{

//==============================================================================
/** §6, every hex measured against its own ground by name. Functional 7:1, flavour 4.5:1, state 3:1;
    §6 records that every functional role on this panel clears its floor and no ink moved this round. */
namespace Colour
{
    /** §6's one accent. Every lamp, the scope trace and the lit GENERATION stage.
        The LCD's `lcdText` is NOT this — it is the display phosphor, and appears only on glass. */
    inline const juce::Colour accent          { 0xFFF0A94B };

    //== Fascia and its furniture =============================================
    inline const juce::Colour fasciaTop       { 0xFFEFE6D0 };
    inline const juce::Colour fasciaBottom    { 0xFFE2D8BF };
    inline const juce::Colour bezel           { 0xFFE4DAC2 };
    inline const juce::Colour bezelLine       { juce::Colour::fromRGBA (0, 0, 0, 26) };    // .10
    // contrast: 8.79-10.03:1 vs fasciaBottom,fasciaTop [functional]
    inline const juce::Colour panelInk        { 0xFF3A3328 };
    inline const juce::Colour divider         { 0xFFB2A68A };
    inline const juce::Colour screwFace       { 0xFFF2EBD8 };
    inline const juce::Colour screwSlot       { juce::Colour::fromRGBA (60, 52, 38, 140) }; // .55

    //== Header block, and the ink that sits on it ============================
    inline const juce::Colour headerTop       { 0xFF2C2923 };
    inline const juce::Colour headerBottom    { 0xFF201D18 };
    // contrast: 11.76:1 vs headerTop [functional]
    inline const juce::Colour descriptorInk   { 0xFFEFE7D3 };
    /** §10 item 7. The six-material header strip carried `#b0a695`, which measures **6.03** against
        this block — worse than the body hex it was meant to have fixed — and had drawn the block as
        `#1a1613 -> #100d0b`, a material that does not ship, so its 7.48 was against the wrong ground.
        The body's hex won and the strip was corrected to it. */
    // contrast: 8.11:1 vs headerTop [functional]
    inline const juce::Colour modelLineInk    { 0xFFCCC1A6 };

    //== The Dymo strip =======================================================
    inline const juce::Colour dymoTop         { 0xFF100E0C };
    inline const juce::Colour dymoBottom      { 0xFF1C1815 };

    //== Glass: the LCD, the scope well, the MACHINE readout ==================
    inline const juce::Colour wellTop         { 0xFF16130F };
    inline const juce::Colour wellBottom      { 0xFF100E0B };
    inline const juce::Colour wellFrame       { 0xFF4E4740 };
    // contrast: 9.96:1 vs wellTop [functional]
    inline const juce::Colour lcdText         { 0xFFF2B25C };
    // contrast: 15.03:1 vs wellTop [functional]
    inline const juce::Colour meterNumerals   { 0xFFEFE7D3 };
    // contrast: 8.70:1 vs wellTop [functional]
    inline const juce::Colour scopeLegend     { 0xFFE3A65A };
    // A graphic rather than lettering, so it takes the flavour floor - but it is measured against
    // the same well every readout is, because a trace crossing a numeral is what the clamp in
    // PitchScope exists to prevent.
    // contrast: 9.24:1 vs wellTop [flavour]
    inline const juce::Colour scopeTrace      { 0xFFF0A94B };
    inline const juce::Colour scopeHalo       { juce::Colour::fromRGBA (217, 131, 36, 140) };  // .55
    inline const juce::Colour scopeGrid       { juce::Colour::fromRGBA (240, 169, 75, 33) };   // .13
    inline const juce::Colour scopeZero       { juce::Colour::fromRGBA (240, 169, 75, 77) };   // .30
    // The strip runs #100e0c -> #1c1815, so the LIGHTER end is the worst case for pale ink.
    // contrast: 15.37:1 vs dymoBottom [functional]
    inline const juce::Colour wordmarkInk     { 0xFFF4EFE3 };

    //== Caps, §3 =============================================================
    inline const juce::Colour ivoryCapHi      { 0xFFF8F2E3 };
    inline const juce::Colour ivoryCapMid     { 0xFFEFE7D2 };
    inline const juce::Colour ivoryCapLo      { 0xFFDED3B8 };
    inline const juce::Colour ivoryCapRim     { 0xFF443E36 };
    // contrast: 12.30:1 vs ivoryCapMid [state]
    inline const juce::Colour ivoryPointer    { 0xFF2B251C };

    inline const juce::Colour darkCapHi       { 0xFF4A4237 };
    inline const juce::Colour darkCapMid      { 0xFF2B2620 };
    inline const juce::Colour darkCapLo       { 0xFF14110D };
    inline const juce::Colour darkCapRim      { 0xFF0E0C09 };
    // §6: both pointers clear the suite's thinnest pointer separation (4.24) by a wide margin,
    // which is what a two-tone cap scheme buys and the reason §3 marks the signature control by
    // MATERIAL as well as by diameter.
    // contrast: 12.42:1 vs darkCapMid [state]
    inline const juce::Colour darkPointer     { 0xFFF2E9D6 };

    inline const juce::Colour sweepArc        { juce::Colour::fromRGBA (58, 51, 40, 71) };  // .28

    //== Shoes, §5.1 ==========================================================
    inline const juce::Colour shoeOnTop       { 0xFFF2EBD8 };
    inline const juce::Colour shoeOnBottom    { 0xFFD8CDB0 };
    inline const juce::Colour shoeOffTop      { 0xFF241F18 };
    inline const juce::Colour shoeOffBottom   { 0xFF15120D };
    inline const juce::Colour shoeRing        { 0xFFA79B80 };
    inline const juce::Colour shoeOnSheen     { juce::Colour::fromRGBA (255, 255, 255, 230) }; // .90

    //== Round lamp-buttons and lamps, §5.3 ===================================
    inline const juce::Colour lampCapHi       { 0xFF4A423A };
    inline const juce::Colour lampCapMid      { 0xFF2A251E };
    inline const juce::Colour lampCapLo       { 0xFF15120E };

    inline const juce::Colour lampLitHi       { 0xFFFFD48A };
    inline const juce::Colour lampLitMid      { 0xFFF0A94B };
    inline const juce::Colour lampLitLo       { 0xFFB4741D };
    inline const juce::Colour lampLitEdge     { 0xFF6B4310 };
    inline const juce::Colour lampLitInner    { juce::Colour::fromRGBA (240, 169, 75, 128) }; // .5
    inline const juce::Colour lampGlow        { juce::Colour::fromRGBA (240, 169, 75, 90) };

    inline const juce::Colour lampDarkHi      { 0xFF6A6152 };
    inline const juce::Colour lampDarkMid     { 0xFF4B443A };
    inline const juce::Colour lampDarkLo      { 0xFF2A251C };

    //== §7.5 bypass ==========================================================
    /** Full-bleed 0.50 `#808080` multiply over the whole panel. Pointers do not move, the scope
        freezes, every lamp goes out, no caption, no desaturation. The legibility floors do not
        apply in this state, which §6 says in as many words. */
    inline const juce::Colour bypassVeil      { 0xFF808080 };
    inline constexpr float    bypassAlpha     = 0.50f;

    /*  `ABOUT-PART.md` §9.1 and §9.2, and every ratio here is the spec's own measured figure.

        §9: **the box is this casting's display GLASS, not its fascia** — a screen, not a plate.

        §3: the About veil is `aboutGlass` at 0.72, a DARKENING scrim — deliberately not the grey
        multiply four lines above. Different colour, opposite direction, so a reader can tell which
        is which with both on screen, and they stack rather than suppressing each other.

        §9.2: **the tab's ink is measured against the WELL, not the fascia.** The recess is a
        surface this casting chooses; the fascia is not, and a 7:1 ceiling is set by the ground.

        **`aboutDim` at 7.12 is the narrowest dim margin in the suite**, which is why §9.1 says no
        casting may darken its dim ink to taste — this is the row that sets that bar. */
    inline const juce::Colour aboutGlass      { 0xFF100E0B };   // §9.1
    inline const juce::Colour aboutBody       { 0xFFF2EBD8 };   // 16.20 on glass
    inline const juce::Colour aboutDim        { 0xFFA89C85 };   //  7.12 — the suite's narrowest
    inline const juce::Colour aboutAccent     { 0xFFF2B25C };   // 10.37
    inline const juce::Colour aboutRing       { 0xFF2E281F };   // §9.1, glass lightened ~18 %

    inline const juce::Colour aboutWellTop    { 0xFF241F18 };   // §9.2
    inline const juce::Colour aboutWellBottom { 0xFF2D2720 };
    inline const juce::Colour aboutWellInk    { 0xFFE6DCC4 };   // 10.82 on the well
}

//==============================================================================
namespace Cursor
{
    /*  §2b: `help`, not `pointer`. `pointer` says *this acts*; `help` says *this explains
        something*, and an About box explains. JUCE has no help cursor in `StandardCursorType`, so
        the delivered 64 x 64 @2x asset is embedded and a cursor built from it.

        **Hotspot (7, 4) in image pixels**, which is the arrow's tip — read off the artwork rather
        than assumed at the origin, because a cursor whose hotspot is wrong is off by the distance
        from the corner to the tip on every click. */
    inline juce::MouseCursor help()
    {
        static const juce::MouseCursor c = []
        {
            const auto img = juce::ImageFileFormat::loadFrom (BinaryData::aboutcursor2x_png,
                                                              (size_t) BinaryData::aboutcursor2x_pngSize);
            return img.isValid() ? juce::MouseCursor (img, 7, 4, 2.0f)
                                 : juce::MouseCursor (juce::MouseCursor::PointingHandCursor);
        }();
        return c;
    }
}

//==============================================================================
namespace Font
{
    /** §8: panel lettering is Barlow Condensed, numerals/units/model line/readouts are Share Tech
        Mono — this casting's own mono, per call 7's split. */
    inline juce::Typeface::Ptr barlowSemiBold()
    {
        static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowCondensedSemiBold_ttf, (size_t) BinaryData::BarlowCondensedSemiBold_ttfSize);
        return t;
    }

    /** §8's scale-numeral row, and **the only role on this panel in 500**.

        A numeral sits one weight below the label it qualifies, and that step is the hierarchy — it
        is why §8 asks for 500 here and 600 everywhere else, and why the ruling moved the file
        rather than changing the row. */
    inline juce::Typeface::Ptr barlowMedium()
    {
        static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowCondensedMedium_ttf, (size_t) BinaryData::BarlowCondensedMedium_ttfSize);
        return t;
    }

    inline juce::Typeface::Ptr mono()
    {
        static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf, (size_t) BinaryData::ShareTechMonoRegular_ttfSize);
        return t;
    }

    /** CSS `font-size` is an **em** size; `juce::Font::withHeight` sets **ascent + descent**, a
        typeface-specific multiple of the em. A spec px passed to `withHeight` renders visibly
        small — root `CLAUDE.md` records two castings whose menu type is 11-23 % under its nominal
        for exactly this reason, one of them below the functional floor. `withPointHeight` means
        what a spec means. */
    inline juce::Font label (float cssPx)
    {
        return juce::Font (juce::FontOptions (barlowSemiBold()).withPointHeight (cssPx));
    }

    /** §8's scale numeral: Barlow Condensed **500**, not the 600 every other Barlow role uses. */
    inline juce::Font numeral (float cssPx)
    {
        return juce::Font (juce::FontOptions (barlowMedium()).withPointHeight (cssPx));
    }

    inline juce::Font monoAt (float cssPx)
    {
        return juce::Font (juce::FontOptions (mono()).withPointHeight (cssPx));
    }

    /** §8 quotes tracking in **em**; `Text::drawTracked` takes **px**. One function so the two
        cannot be confused at a call site, which is how a `.10 em` becomes a 0.10 px. */
    inline constexpr float trackingPx (float em, float cssPx) noexcept { return em * cssPx; }
}

//==============================================================================
namespace Paint
{
    /** A CSS `radial-gradient(circle at X% Y%, ...)` over a circular face.

        **A `circle` with no size given is `farthest-corner`, and the corner is the BOX's.** Every
        one of this panel's five call sites is a round element in a square div, so the ramp reaches
        the far corner of that square - `2.01r` from a 34/24 light point - and the visible face
        shows only the inner 80 % of it. Sizing it to the far edge of the CIRCLE instead gives
        1.61r, which reaches the outer colour a quarter of the way early and leaves the whole
        lower-right quadrant flat at it.

        That is the mistake this comment used to argue FOR, in a sentence about the face spanning
        1.61r - true of the circle, and not what CSS measures. It cost 20 to 36 sum-RGB units
        across the bottom and right of every cap, measured against the delivered prototype, and it
        survived because it was written while fixing a real and much larger error beside it: the
        ramp had been running from the light point to `centre + (r, r)`, a distance measured from
        the wrong origin, which really did read as flat ivory on all eleven knobs at once.

        `juce::ColourGradient`'s radial form takes point1 as the centre and point2 as *a* point on
        the ramp's outer circle, so any point at the right distance will do - which is exactly why
        it is easy to get wrong by picking a convenient one.

        @param lightX  the CSS `at X%`, as a fraction
        @param lightY  the CSS `at Y%`, as a fraction */
    inline juce::ColourGradient sculptedFace (juce::Point<float> centre, float radius,
                                              float lightX, float lightY,
                                              juce::Colour inner, juce::Colour outer)
    {
        const juce::Point<float> lightAt (centre.x + radius * (lightX - 0.5f) * 2.0f,
                                          centre.y + radius * (lightY - 0.5f) * 2.0f);

        // farthest-corner: the largest distance from the light point to any corner of the box.
        float reach = 0.0f;

        for (const float sx : { -1.0f, 1.0f })
            for (const float sy : { -1.0f, 1.0f })
                reach = juce::jmax (reach, lightAt.getDistanceFrom ({ centre.x + sx * radius,
                                                                     centre.y + sy * radius }));

        return { inner, lightAt, outer, lightAt.translated (reach, 0.0f), true };
    }
}

//==============================================================================
namespace Text
{
    /** Every non-ASCII glyph is built from its **codepoint**. `juce::String`'s `const char*`
        constructor decodes **Latin-1, not UTF-8**, so a UTF-8 literal renders as stray glyphs — a
        `U+2212` written narrow drew `<A-circumflex>^'60` on Gatecrasher during its rewrite.

        The prototype enumeration reports exactly three above-ASCII codepoints on this panel
        (U+00B1 x1, U+00B7 x5, U+2212 x4) and **no whitespace other than U+0020**, which is checked
        separately because a U+3000 transcribes to `' '` and looks right. */
    inline juce::String middleDot()  { return juce::String::charToString ((juce::juce_wchar) 0x00B7); }
    inline juce::String plusMinus()  { return juce::String::charToString ((juce::juce_wchar) 0x00B1); }
    inline juce::String minusSign()  { return juce::String::charToString ((juce::juce_wchar) 0x2212); }
    inline juce::String emDash()     { return juce::String::charToString ((juce::juce_wchar) 0x2014); }

    /** A signed decibel string with §3.2's leading plus kept, and a real U+2212 for the minus. */
    inline juce::String signedDb (float db, int places = 0)
    {
        const auto mag = juce::String (std::abs (db), places);
        return db < 0.0f ? minusSign() + mag : "+" + mag;
    }

    inline float trackedWidth (const juce::String& text, const juce::Font& font, float tracking)
    {
        float w = 0.0f;
        for (int i = 0; i < text.length(); ++i)
        {
            w += juce::GlyphArrangement::getStringWidth (font, juce::String::charToString (text[i]));
            if (i < text.length() - 1)
                w += tracking;
        }
        return w;
    }

    /** A box running one ascent above the baseline to one descent below it, so that `drawTracked`
        centring the glyphs in it puts the baseline where the spec quoted it — derived from the
        font rather than eyeballed. */
    inline juce::Rectangle<float> rowAtBaseline (const juce::Font& font, float left, float right,
                                                 float baselineY)
    {
        const float ascent = font.getAscent(), descent = font.getDescent();
        return { left, baselineY - ascent, right - left, ascent + descent };
    }

    /** `juce::Font` carries no absolute-pixel letter-spacing, so tracked text is drawn glyph by
        glyph. **A `drawText` on panel lettering silently drops the tracking**, which is one of the
        four shapes Chorus-60's type sweep found on strings that were otherwise struck as drawn. */
    inline void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                             float tracking, juce::Rectangle<float> area,
                             juce::Justification justification, juce::Colour colour)
    {
        const float total = trackedWidth (text, font, tracking);
        float x = area.getX();

        if (justification.testFlags (juce::Justification::horizontallyCentred))
            x = area.getCentreX() - total * 0.5f;
        else if (justification.testFlags (juce::Justification::right))
            x = area.getRight() - total;

        g.setFont (font);
        g.setColour (colour);

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString (text[i]);
            const float w = juce::GlyphArrangement::getStringWidth (font, ch);
            g.drawText (ch, juce::Rectangle<float> (x, area.getY(), w + 1.0f, area.getHeight()),
                        juce::Justification::centredLeft, false);
            x += w + tracking;
        }
    }
}

//==============================================================================
/** §8. Every size is a CSS px em size with a **pinned line box** (call 4), so the pair travels
    together and a size cannot be moved without someone meeting the box it has to sit in. */
namespace Type
{
    struct Role { float cssPx; float lineBox; float trackingEm; };

    inline constexpr Role wordmark          { 40.0f,  40.0f, 0.10f };   // Impact Label Reversed - artwork, §9
    inline constexpr Role functionDescriptor{ 14.0f,  17.0f, 0.26f };
    inline constexpr Role modelLine         { 11.0f,  14.0f, 0.20f };
    inline constexpr Role sectionHeading    { 12.0f,  15.0f, 0.28f };
    inline constexpr Role controlLabel      { 12.0f,  15.0f, 0.18f };
    inline constexpr Role unit              { 10.0f,  13.0f, 0.10f };
    inline constexpr Role scaleNumeral      { 11.0f,  13.0f, 0.04f };
    inline constexpr Role switchCaption     { 10.0f,  13.0f, 0.22f };
    inline constexpr Role legend            { 10.0f,  13.0f, 0.14f };   // shoe and lamp legends
    inline constexpr Role programLegend     { 11.0f,  13.0f, 0.12f };
    inline constexpr Role lcdValue          { 17.0f,  22.0f, 0.10f };

    /** Two roles §8 quotes in **absolute px** rather than em, and they are left that way rather
        than converted: 1.3 px at 12 is .1083 em and 1.2 at 12.5 is .096 em, neither of which is a
        figure anybody chose. Converting them would invent precision. */
    inline constexpr float scopeReadoutCssPx    = 12.0f,  scopeReadoutLineBox    = 15.0f,  scopeReadoutTrackingPx   = 1.3f;
    inline constexpr float machineReadoutCssPx  = 12.5f,  machineReadoutLineBox  = 16.0f,  machineReadoutTrackingPx = 1.2f;

    /** §8's scale numeral is **Barlow Condensed 500, and it is drawn in 500** as of 2026-08-22.

        It was SemiBold for as long as the file was owed, stated here rather than left to be
        discovered — which is what the ruling asked for and what Gatecrasher's rewrite had not done.
        The constant stays, reading false, because a reader who finds `Font::numeral` using a
        different face from `Font::label` should find the reason next to it rather than in a commit.

        `design-asks/barlow-condensed-500.md` closes for this casting; **Gatecrasher is still
        owed the same file** for five roles, and `tools/check_font_sets.py` is what says so. */
    inline constexpr bool scaleNumeralIsSubstituted = false;
}

//==============================================================================
namespace Layout
{
    /** §1. **Call 1 cost TapeRot 4 px** — 1336 to 1340, the smallest move in the suite, because its
        band was already close to the part's. The width is the shared part's; only the height is
        this casting's. */
    inline constexpr float canvasWidth  = (float) nf::HeaderGeometry::canvasWidth;   // 1340
    inline constexpr float canvasHeight = 790.0f;

    inline constexpr float bezelInset   = 8.0f;

    /** §1's four thumbscrews, at the stated centres. */
    inline constexpr float screwDiameter = 11.0f;
    inline constexpr float screwSlotW = 8.0f, screwSlotH = 1.6f;
    inline constexpr std::array<juce::Point<float>, 4> screwCentres { {
        { 8.0f, 26.0f }, { 1332.0f, 26.0f }, { 8.0f, 764.0f }, { 1332.0f, 764.0f } } };

    //== §4, the wow/flutter scope ============================================
    inline constexpr float scopeX = 16.0f, scopeY = 136.0f, scopeW = 1308.0f, scopeH = 164.0f;
    inline constexpr float scopeGridX = 163.5f, scopeGridY = 41.0f;
    inline constexpr float scopeZeroY = 82.0f;               // within the well
    inline constexpr float scopeZeroDashOn = 3.0f, scopeZeroDashOff = 4.0f;
    inline constexpr float scopeTraceThickness = 1.7f;
    inline constexpr float scopeHaloRadius = 5.0f;
    /** Measured off the delivered prototype rather than stated by §4: the four readouts sit 14 px
        in from the well's left and right edges and 8 px from its top and bottom. §4 gives the
        readouts' face, size and ink and does not place them, so these are the only source. */
    inline constexpr float scopeReadoutInsetX = 14.0f, scopeReadoutInsetY = 8.0f;
    inline constexpr float scopeFailLampDiameter = 9.0f;

    //== §2, the sections. THE ORDER IS THE SIGNAL PATH =======================
    /** §10 item 2: this replaced a grouping by control **type**, which is the reorganisation the
        round made. Left to right is input to output. */
    struct Section { const char* heading; float x; float width; };
    inline constexpr std::array<Section, 6> sections { {
        { "INPUT",      20.0f,  144.0f },
        { "MACHINE",   172.0f,  168.0f },
        { "TRANSPORT", 348.0f,  304.0f },
        { "NOISE",     660.0f,  162.0f },
        { "DECAY",     830.0f,  168.0f },
        { "OUTPUT",   1006.0f,  314.0f } } };

    inline constexpr float sectionHeadingY = 326.0f;
    inline constexpr std::array<float, 5> dividerX { { 168.0f, 344.0f, 656.0f, 826.0f, 1002.0f } };
    inline constexpr float dividerY = 316.0f, dividerH = 420.0f, dividerW = 2.0f;

    //== §3, the control row ==================================================
    /** §3.1. The Ø104 and Ø76 controls share pivot y **444** and one label line at y **516**.
        MODEL's Ø104 body would put its label 14 px lower, so **its label is pinned to 516** — the
        same outcome the suite's registration rule reaches by `dy = (larger - smaller) / 2` on a
        shared box. TapeRot arrived at it independently and §9 records it as already conformant. */
    inline constexpr float mainRowPivotY = 444.0f;
    inline constexpr float mainRowLabelY = 516.0f;

    inline constexpr float knobSweepDegrees   = 270.0f;
    inline constexpr float knobNumeralRadiusOffset = 29.5f;      // §3: numeral ring at r + 29.5
    inline constexpr float knobMajorTickLength = 9.0f, knobMajorTickWidth = 2.0f;
    inline constexpr float knobMinorTickLength = 5.0f, knobMinorTickWidth = 1.5f;
    inline constexpr float knobSweepArcInset   = 6.0f;           // arc sits at r + 6
    inline constexpr float knobSweepArcWidth   = 1.4f;
    inline constexpr float knobPointerWidth    = 3.0f, knobPointerInset = 8.0f;
    inline constexpr float knobLabelGap        = 0.0f;

    inline constexpr int knobDragPixels     = 190;
    inline constexpr int knobFineDragPixels = 760;

    enum class Cap { signature, primary, standard };

    inline constexpr float diameterFor (Cap c) noexcept
    {
        return c == Cap::signature ? 104.0f : (c == Cap::primary ? 76.0f : 56.0f);
    }
}

//==============================================================================
/** §3.2's mark lists.

    **A mark stores its VALUE, never its rotation fraction.** BRAND.md makes this a rule and the
    reason is mechanical: a value drawn through the parameter's own `NormalisableRange` moves *with*
    the pointer when a taper changes, where a stored fraction keeps pointing at the old angle while
    the pointer leaves. The delivered prototype stores fractions because a prototype has no
    `NormalisableRange` to read; converting all 60 of them back through the ranges returns
    **1000 / 1200 / 1500 / 2000 / 3000 / 4000 / 5000 / 7000 / 10k / 15k / 20k** for LP and a clean
    1-1.5-2-3-5-7 decade series for HP and RAMP, which is the evidence that they were authored as
    values and published as derived output.

    A numeral is a **major** tick (2 x 9); an empty string is a **minor** (1.5 x 5). §3.2: "the
    demoted values keep their ticks - what is dropped is the numeral, not the mark."
*/
namespace Marks
{
    struct Mark
    {
        float value;
        const char* numeral;      // empty => a minor tick at a real value
        constexpr bool major() const noexcept { return numeral[0] != '\0'; }
    };

    /** §3.2. **DRIVE and FLUTTER share one legend** - 0-100 % at skew 0.2, five numerals and six
        minors. The marks bunch heavily toward the clockwise end and **that is intended**: it is what
        a skew of 0.2 looks like drawn honestly. Five is the primary-class ceiling and this ring is
        at it. */
    inline constexpr std::array<Mark, 11> driveAndFlutter { {
        { 0.0f, "0" }, { 0.1f, "" }, { 0.5f, "" }, { 1.0f, "1" }, { 2.0f, "" }, { 5.0f, "5" },
        { 10.0f, "" }, { 25.0f, "25" }, { 50.0f, "" }, { 75.0f, "" }, { 100.0f, "100" } } };

    /** Even fifths, for the three genuinely **linear** percentage controls.

        **WOW is not one of them and must not read this table.** The delivered prototype gives all
        four the same `pct` list, which is correct for NOISE, FAILURE and MIX and wrong for WOW,
        whose range carries skew 0.3 for a documented DSP reason (`Parameters.h`). Nothing at a
        shared call site distinguishes them, which is why WOW gets its own name below rather than a
        comment here. */
    inline constexpr std::array<Mark, 5> evenFifthsPercent { {
        { 0.0f, "0" }, { 25.0f, "25" }, { 50.0f, "50" }, { 75.0f, "75" }, { 100.0f, "100" } } };

    /** §3.2's WOW ring, **re-cut by the designers 2026-08-21 and no longer even fifths**.

        It cannot take the shared `evenFifthsPercent` table: WOW's range carries skew 0.3, so even
        ANGLES would print 25 / 50 / 75 at positions the pointer reaches at **0.98 / 9.92 / 38.33 %**
        — a five-fold error at mid-travel, with both endpoints agreeing, which is why nothing showed
        it. That was raised as `design-asks/taperot-wow-ring.md`.

        The ruling is better than the two answers the ask offered. Rather than keep the five numerals
        and accept them bunching clockwise, §3.2 prints a **decade series — 0 / 1 / 10 / 40 / 100 —
        chosen so the honestly-placed angles land within 3° of even fifths**: −135 / −67.18 / +0.32 /
        +70.11 / +135. An even-LOOKING ring that is also true, where the old one only looked like one.

        **WOW's skew of 0.3 is not to be changed** — §3.2 says so, and `Parameters.h` gives the
        reason: the exponent is matched to FLUTTER's realised deviation, and five castings' transport
        feel rests on it.

        Stored as values, so the angles are the range's answer rather than this table's. That is what
        made this a five-numeral edit instead of a re-derivation. */
    inline constexpr std::array<Mark, 5> wowPercent { {
        { 0.0f, "0" }, { 1.0f, "1" }, { 10.0f, "10" }, { 40.0f, "40" }, { 100.0f, "100" } } };

    /** §3.2, "leading plus kept" - which is a decision about the numeral, and `Text::signedDb`
        is where it is spelled. The minus is U+2212, not a hyphen. */
    inline constexpr std::array<Mark, 5> outputDb { {
        { -24.0f, "-24" }, { -12.0f, "-12" }, { 0.0f, "0" }, { 12.0f, "+12" }, { 24.0f, "+24" } } };

    /** §3.2: standard class carries three numerals, and LP / HP / RAMP are logarithmic - "their
        fractions are the contract and must not be evened out". Storing the values is what honours
        that: the contract is the taper, and the taper is in the range. */
    inline constexpr std::array<Mark, 11> lpKilohertz { {
        { 1000.0f, "1" }, { 1200.0f, "" }, { 1500.0f, "" }, { 2000.0f, "" }, { 3000.0f, "3" },
        { 4000.0f, "" }, { 5000.0f, "" }, { 7000.0f, "" }, { 10000.0f, "" }, { 15000.0f, "" },
        { 20000.0f, "20" } } };

    inline constexpr std::array<Mark, 14> hpHertz { {
        { 20.0f, "20" }, { 30.0f, "" }, { 40.0f, "" }, { 50.0f, "" }, { 70.0f, "" },
        { 100.0f, "" }, { 150.0f, "" }, { 200.0f, "200" }, { 300.0f, "" }, { 500.0f, "" },
        { 700.0f, "" }, { 1000.0f, "" }, { 1500.0f, "" }, { 2000.0f, "2k" } } };

    inline constexpr std::array<Mark, 13> rampSeconds { {
        { 0.05f, "0.05" }, { 0.07f, "" }, { 0.1f, "" }, { 0.15f, "" }, { 0.2f, "" }, { 0.3f, "" },
        { 0.5f, "0.5" }, { 0.7f, "" }, { 1.0f, "" }, { 1.5f, "" }, { 2.0f, "" }, { 3.0f, "" },
        { 4.0f, "4" } } };

    /** §3.3. Nine detents, **every one a major tick, none numeralled** - the machine names print in
        the MACHINE readout, not on the fascia, because nine names around a Ø104 dial would not fit
        at the type floor and the readout has to exist anyway. The marks carry no text and full
        length, which is why `major()` reading the numeral would get this wrong; the ring is drawn
        from `modelDetentCount` rather than from a table. */
    inline constexpr int modelDetentCount = 9;
}

//==============================================================================
/** §5.1's shoes, §5.2's scope-clause exception, §5.3's lamp buttons and §5.4's ladder. */
namespace Switches
{
    /** §4B's two-state shoe at the part's **128 x 32 in two 64 halves**. §10 item 4 records that
        all three were drawn 6 px short at 128 x 26 in the first pass and are now 32, "which moves
        nothing: NOISE BED at y 548 and HUM at y 622 still clear". */
    inline constexpr float shoeW = 128.0f, shoeH = 32.0f, shoeHalfW = 64.0f, shoeRadius = 3.0f;

    /** §5.1: legends are printed once per position, centred under their own segment, and **never
        re-inked and never moved - the shoe carries the state**. That is why a legend is not part of
        the switch's own state matrix: §7.2 is six cells with one rule and no legend changes in any
        of them. */
    struct Shoe { const char* caption; const char* left; const char* right; float x, captionY, shoeY; };

    inline constexpr std::array<Shoe, 3> shoes { {
        { "SWITCHING", "FADE",   "CLUNK",  436.0f, 622.0f, 642.0f },
        { "HUM",       "OFF",    "ON",     677.0f, 622.0f, 642.0f },
        { "SPREAD",    "LINKED", "STEREO", 850.0f, 548.0f, 568.0f } } };

    /** The gap between a shoe's bottom edge and its legend row, **measured off the delivered
        prototype at all three shoes**: SWITCHING and HUM both sit at 674 with legends at 680, and
        SPREAD at 600 with legends at 606.

        It was written as 26 on the first pass - a figure with no run behind it, in a file whose own
        header says every figure below is measured. It collided FAULT ACTIVITY's caption with
        SPREAD's legends, which is the only reason it was caught: a wrong gap that happened to land
        in clear fascia would have looked deliberate. */
    inline constexpr float shoeLegendY = 6.0f;

    /** §5.3. Ø26 dark caps each with an Ø11 lamp in the face and its legend below.

        **NOISE BED is here rather than in `shoes` and that is §4B's scope clause, not a lapse.**
        It is a three-state control in a 162 px section (x 660 -> 822) with the group at x 677, so
        the part's 168 x 45 three-state footprint does not fit. Widening the section would move the
        divider at 826 and with it FAULT ACTIVITY's 176 px group, on the densest panel in the
        suite - re-planning a casting's layout to satisfy a footprint, which inverts the round's
        scope: the header is the part, body layout is the casting's. TapeRot is the clause's
        **named instance**, so this is the shape the part anticipates rather than an exception. */
    inline constexpr float lampButtonDiameter = 26.0f, lampDiameter = 11.0f;
    inline constexpr float lampGlowRadius = 7.0f;

    struct LampGroup
    {
        const char* caption;
        float captionY, buttonY, legendY;
        std::array<const char*, 4> legends;   // trailing nulls where a group is shorter
        std::array<float, 4> buttonX;
        int count;
    };

    inline constexpr std::array<LampGroup, 3> lampGroups { {
        { "FAIL",           548.0f, 565.0f, 597.0f, { "STP", "FLT", "FAI", nullptr },
          { 37.0f, 79.0f, 121.0f, 0.0f }, 3 },
        { "NOISE BED",      548.0f, 568.0f, 600.0f, { "TAPE", "VCR", "DUST", nullptr },
          { 686.0f, 728.0f, 770.0f, 0.0f }, 3 },
        { "FAULT ACTIVITY", 622.0f, 642.0f, 674.0f, { "DRP", "SNG", "CRK", "WBL" },
          { 844.0f, 882.0f, 920.0f, 958.0f }, 4 } } };

    /** §5.4. "The selector is a stage ladder rather than a knob because the parameter is an integer
        count of tape generations, and a pointer implies interpolation between them." */
    inline constexpr float genStageW = 20.0f, genStageH = 20.0f, genStageRadius = 3.0f;
    inline constexpr float genStageX0 = 392.0f, genStagePitch = 28.0f;
    inline constexpr float genStageY = 568.0f, genCaptionY = 548.0f, genNumeralY = 594.0f;
    inline constexpr int   genStageCount = 8;
    inline constexpr float genNumeralCssPx = 11.0f;   // Share Tech Mono, untracked - §8 has no row
}

//==============================================================================
namespace Readouts
{
    /** §3.3, stated exactly: 134 x 27 at (189, 566), Share Tech Mono 12.5 / 16, `#f2b25c` on the
        LCD material. **The readout is the label**, which is what lets §3.3 leave the fascia bare. */
    inline constexpr float machineX = 189.0f, machineY = 566.0f, machineW = 134.0f, machineH = 27.0f;
    inline constexpr float machineRadius = 2.0f;

    /** §1's footer row, both ends. The middle dot is U+00B7 built from its codepoint. */
    inline constexpr float footerY = 752.0f;
    /** Both footer strings are inset **10 px from the block's own edges** and aligned outward —
        the left one left-aligned from 26 (= blockX + 10), the right one **right-aligned to 1314**
        (= blockX + blockW − 10). Symmetric, and it puts the version label under OUTPUT.

        **This read `footerRightCentre = 914` and centred the string there, which is DECAY.** 914 is
        the prototype's *left edge* of a 400-wide right-aligned box, and it coincides with DECAY's
        centre (830 + 168/2) — so a figure read as a centre landed on a plausible-looking section
        and stayed. The ink is 100.8 wide, so right-aligned it occupies 1213–1314 and centred it
        occupied 863–964: two sections out, from one misread alignment. */
    inline constexpr float footerLeftX  = 26.0f;
    inline constexpr float footerRightX = 1314.0f;
    inline constexpr float footerCssPx = 10.0f, footerLineBox = 13.0f, footerTrackingEm = 0.18f;
    inline const juce::Colour footerInk { 0xFF5F5749 };
}

//==============================================================================
/** The shared header part. Every figure is `nf::HeaderGeometry`'s - **aliased, not transcribed.**

    Chorus-60's header pass is the reason this is spelled out: it aliased its LCD to the part and
    left SAVE, DELETE and both meter wells as literals from the previous canvas, **29 px right and
    29 px down**, invisible for as long as the plate baked those faces. A literal that happens to
    agree with core is indistinguishable from an alias by reading, which is why there are no
    numbers in this namespace.
*/
namespace Header
{
    inline constexpr float blockX = (float) nf::HeaderGeometry::blockX;
    inline constexpr float blockY = (float) nf::HeaderGeometry::blockY;
    inline constexpr float blockW = (float) nf::HeaderGeometry::blockW;
    inline constexpr float blockH = (float) nf::HeaderGeometry::blockH;
    inline constexpr float blockRadius = 5.0f;      // material, and this casting's

    /** §9's Dymo strip. **The wordmark ships as artwork and the font does not** - Impact Label
        Reversed is donationware and cannot be embedded, which is *absent by licensing, not
        missing*; `design/fonts/ABSENT.md` records it so nobody "fixes" it by substituting a face,
        which would move every measurement taken from the nameplate.

        The delivered cut is **694 x 150** and it is a **44 px plate rotated -1.5 deg**: a
        230.2 x 44 strip at that angle has a bounding box of 693.8 x 150.0 at 3x. That arithmetic
        is what settles the anchor. Core records TapeRot's published §4 stack as
        `30 + 38 + 4 = 72`, six short of the shared `descriptorY` of 78, and the delivered
        prototype draws the descriptor at **84**, six past it. Neither matches; the cut does:
        `30 + 44 + 4 = 78`, exactly. So the plate is 44, and §9's own construction (a 40 px line
        box with 2 x 18 padding) says 44 too.

        Asserted below rather than trusted, because three sources disagreeing is precisely the case
        where a figure gets transcribed from whichever one was open. */
    inline constexpr float dymoPlateW = 230.2f, dymoPlateH = 44.0f;
    inline constexpr float dymoLeading = 4.0f;
    inline constexpr float dymoRotationDegrees = -1.5f;
    inline constexpr float dymoRadius = 2.0f;
    inline constexpr float dymoCutW = 694.0f, dymoCutH = 150.0f;   // the shipped bitmap, at 3x

    static_assert (nf::HeaderGeometry::landsOnDescriptorAnchor (nf::HeaderGeometry::nameplateY,
                                                                (int) dymoPlateH,
                                                                (int) dymoLeading),
                   "TapeRot's nameplate stack must land the function descriptor on the shared "
                   "anchor. See HEADER-PART.md §4 and the cut arithmetic above.");

    //== The band's cells, every one of them core's ==========================
    inline juce::Rectangle<float> lcd()          { return nf::HeaderGeometry::lcd().toFloat(); }
    inline juce::Rectangle<float> saveButton()   { return nf::HeaderGeometry::saveButton().toFloat(); }
    inline juce::Rectangle<float> deleteButton() { return nf::HeaderGeometry::deleteButton().toFloat(); }
    inline juce::Rectangle<float> inWell()       { return nf::HeaderGeometry::inWell().toFloat(); }
    inline juce::Rectangle<float> outWell()      { return nf::HeaderGeometry::outWell().toFloat(); }
    inline juce::Rectangle<float> nameplate()    { return nf::HeaderGeometry::nameplate().toFloat(); }

    inline constexpr float captionY = (float) nf::HeaderGeometry::captionY;
    inline constexpr float captionH = (float) nf::HeaderGeometry::captionH;
    inline constexpr float bandY    = (float) nf::HeaderGeometry::bandY;
    inline constexpr float bandH    = (float) nf::HeaderGeometry::bandH;

    /** §5's bank cell, and the inset between it and the name. The LCD's own box is core's; how it
        is DIVIDED is this casting's, because what a Program shows is the casting's. */
    inline constexpr float bankCellW = 73.0f;
    inline constexpr float nameInset = 19.0f;

    /** §10 item 6: **the chevron is the shared 14 x 8 stroked path now**, replacing this casting's
        own 9 x 9 rotated box.

        It also means this panel can invert it while the list is open, which it could not before:
        root `CLAUDE.md` records TapeRot and Gatecrasher as the two castings that "cannot do this -
        their chevrons are printed in the plate, so there is nothing at runtime to invert", and adds
        that neither carries a `menuOpen` flag "because it would be dead state". **Both plates are
        gone.** The state stopped being dead in the same commit that deleted the pixels. */
    inline constexpr float chevronW = 14.0f, chevronH = 8.0f;
    inline constexpr float chevronRightInset = 30.0f;

    inline juce::Rectangle<float> chevron()
    {
        const auto glass = lcd();
        return { glass.getRight() - chevronRightInset - chevronW,
                 glass.getCentreY() - chevronH * 0.5f, chevronW, chevronH };
    }

    inline juce::Rectangle<float> bankCell() { return lcd().withWidth (bankCellW); }

    inline juce::Rectangle<float> nameCell()
    {
        return lcd().withTrimmedLeft (bankCellW + nameInset)
                    .withRight (chevron().getX() - 8.0f);
    }

    /** §7.1's two-legend Program buttons: each carries **two** legends stacked, and which of the
        four is lit is that matrix. BRAND.md forbids the older form where a button relabelled itself
        STORE / CANCEL - a control whose legend changes is one the player has to read before
        pressing. */
    inline constexpr float legendUpperY = 65.0f, legendLowerY = 78.0f;
}

//==============================================================================
namespace Runtime
{
    inline constexpr int animationHz = 60;

    /** The readout's spelling lives in the THEME, never in `ProgramHeader`. That header reaches
        `PluginProcessor.h`, whose `JucePlugin_*` macros exist only in the plugin target, so a test
        reading the format from there cannot link - and a test declaring its own copy asserts
        against itself and passes while the panel prints something else. */
    inline nf::ReadoutFormat readoutFormat() { return {}; }
}

}  // namespace TapeRotTheme
