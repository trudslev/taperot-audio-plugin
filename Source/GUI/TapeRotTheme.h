#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <BinaryData.h>

#include <array>

/**
    TapeRot's design tokens, transcribed from `design/GUI-SPEC.md`.

    Every coordinate is in 1x logical pixels measured from the top-left of `panel_background.png`,
    exactly as the spec states them. The bitmaps are embedded at 2x and drawn at these 1x
    coordinates, so the panel stays crisp on a Retina display; a 1:1 blit resolves the fascia
    texture to a flat wash.

    **Nothing printed is drawn in code.** Every label, scale legend, tick and unit mark - and the
    Dymo nameplate - lives in the plate, positioned by eye against its ticks. Recomputing any of it
    at runtime would move it. The runtime layer is exactly four things: the scope trace, the LCD
    text, the MODEL readout, and the IN/OUT numerals.

    Sprite placements below are the SPRITE's top-left with its transparent bleed included, so each
    blits at its stated position with no further offset. All assets carry straight (non-
    premultiplied) alpha.
*/
namespace TapeRotTheme
{

//==============================================================================
namespace Colour
{
    /** The one accent. Icon and panel must stay in step - the icon's edge stroke is #C07E23 and its
        lit-segment gradient runs #F5B85F to #D98324 against this same value. */
    inline const juce::Colour accent        { 0xFFF0A94B };

    inline const juce::Colour lcdText       { 0xFFF2B25C };
    inline const juce::Colour lcdGlow       { juce::Colour::fromRGBA (240, 169, 75, 89) };   // .35
    inline const juce::Colour meterNumerals { 0xFFEFE7D3 };
    inline const juce::Colour scopeLegend   { 0xFFE3A65A };
    inline const juce::Colour scopeTrace    { 0xFFF0A94B };
    inline const juce::Colour scopeHalo     { juce::Colour::fromRGBA (217, 131, 36, 71) };   // .28
    inline const juce::Colour scopeGrid     { juce::Colour::fromRGBA (240, 169, 75, 33) };   // .13
    inline const juce::Colour scopeCentre   { juce::Colour::fromRGBA (240, 169, 75, 77) };   // .30
    inline const juce::Colour scopeWell     { 0xFF100E0A };

    // Kept for reference against the plate; nothing below is drawn in code any more.
    inline const juce::Colour fasciaTop     { 0xFFEFE6D0 };
    inline const juce::Colour fasciaBottom  { 0xFFE2D8BF };
    inline const juce::Colour headerTop     { 0xFF2C2923 };
    inline const juce::Colour headerBottom  { 0xFF201D18 };
    inline const juce::Colour wellInterior  { 0xFF16130F };
}

//==============================================================================
namespace Font
{
    /** Share Tech Mono is the only typeface needed at runtime. Every printed word on this panel -
        Helvetica for the labels, ImpactLabel for the nameplate - is baked into the plate, so
        neither is embedded any more. */
    inline juce::Typeface::Ptr mono()
    {
        static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf, (size_t) BinaryData::ShareTechMonoRegular_ttfSize);
        return t;
    }

    /** CSS `font-size` is an em size, which is NOT juce::Font::withHeight() - that sets
        ascent+descent, a typeface-specific multiple of the em, so a spec px passed straight to
        withHeight() renders visibly small. JUCE 8's withPointHeight() expresses it directly. */
    inline juce::Font of (float cssPx)
    {
        return juce::Font (juce::FontOptions (mono()).withPointHeight (cssPx));
    }
}

//==============================================================================
namespace Text
{
    /** U+2014, from its codepoint: juce::String's const char* constructor decodes Latin-1, so a
        UTF-8 literal would render as stray glyphs. */
    inline juce::String emDash()
    {
        return juce::String::charToString ((juce::juce_wchar) 0x2014);
    }

    inline juce::String middleDot()
    {
        // juce::String's const char* constructor decodes Latin-1, not UTF-8, so a "\xc2\xb7"
        // literal renders as a stray A-circumflex.
        return juce::String::charToString ((juce::juce_wchar) 0x00B7);
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

    /** Delta v1.0.2 quotes the scope legends by *baseline*, not by box, and juce::Graphics has no
        baseline-anchored drawText. A box running exactly one ascent above the baseline to one
        descent below it puts the baseline where it was specified once drawTracked centres the
        glyphs in it - derived from the font rather than eyeballed against the plate. */
    inline juce::Rectangle<float> rowAtBaseline (const juce::Font& font, float left, float right,
                                                 float baselineY)
    {
        const float ascent = font.getAscent(), descent = font.getDescent();
        return { left, baselineY - ascent, right - left, ascent + descent };
    }

    /** juce::Font carries no absolute-pixel letter-spacing, so tracked text is drawn glyph by
        glyph. Every tracking figure in this spec is quoted in px. */
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
namespace Layout
{
    inline constexpr float canvasWidth  = 1336.0f;
    inline constexpr float canvasHeight = 679.0f;

    /** Assets are embedded at 2x and drawn at 1x coordinates. */
    inline constexpr int assetScale = 2;

    // --- frames the plate leaves empty, filled at runtime (spec section 1) ------------------
    /** **The header band: y 63, height 34, shared by all five parts.** The LCD, both Program
        buttons and both meter wells stand the same height on the same Y, which is BRAND.md's
        suite-wide header figure rather than this panel's - the castings are differently-sized
        units, not scales of one design.

        TapeRot is the only casting where the header got SMALLER: it was 40 (and its meters 42, two
        pixels taller than their own row-mates). Verified against the v1.0.8 plate rather than the
        spec table: the LCD glass, the IN well and the OUT well all measure 64..96 there, which is
        the 32px content box, so the border-box is 63..97 for all three. */
    inline const juce::Rectangle<float> programLcd  { 416.2f,  63.0f, 470.0f, 34.0f };
    /** The dropdown chevron, baked into the plate (delta v1.0.1 replaced a solid triangle with a
        stroked one, "16x16 box, 14px in from the frame's inner edge"). Nothing draws it, but the
        program name has to stop short of it or a long User Program name runs underneath.

        This is the RIGHT-hand inner edge: 887 - 14 - 16. The v1.0.2 plate has both the chevron and
        the bank-chip divider baked at the LEFT instead - chevron ink at x 457.5-468.0, rule at
        x 444 - where they land on top of the FACT/USER chip (which renders at x 425-466). Confirmed
        with the designers as an asset defect, so these coordinates stay as the spec describes them
        and the plate moves to meet them. Do not "correct" this to 457. */
    inline const juce::Rectangle<float> lcdChevron  { 857.0f,  72.0f,  16.0f, 16.0f };

    /** The baked rule separating the bank chip from the program name, measured off the v1.0.3
        plate at x 491.0. The chip centres in the glass's left edge to here; the name starts a
        fixed gap past it. Both were guessed before this rule existed, which left FACT sitting
        8.5px left of centre in its own field. */
    inline constexpr float lcdDivider    = 491.0f;
    inline constexpr float lcdNameInset  =  19.0f;

    /** SAVE and DELETE/CANCEL. Delta v1.0.7 cleared both from the plate and moved every state into
        sprites, so these are the only header controls drawn rather than baked. Plate is 76 x 40
        with a 3px shadow bleed, hence an 82 x 46 sprite whose top-left is 3px up and left of the
        plate's - the same convention as the two-state buttons in spec section 3. */
    inline constexpr float headerButtonW = 82.0f;
    inline constexpr float headerButtonH = 40.0f;
    inline constexpr juce::Point<float> saveSpriteTopLeft   { 893.2f, 60.0f };
    inline constexpr juce::Point<float> deleteSpriteTopLeft { 979.2f, 60.0f };
    /** The plate rects, which are the hit areas - the sprite's bleed must not be clickable. */
    inline const juce::Rectangle<float> saveHitArea   { 896.2f, 63.0f, 76.0f, 34.0f };
    inline const juce::Rectangle<float> deleteHitArea { 982.2f, 63.0f, 76.0f, 34.0f };
    inline const juce::Rectangle<float> scopeWell   {  43.0f, 158.5f, 1250.0f, 70.0f };
    /** The two legend rows above and below the well. They carry live values - deviation range, wow
        and flutter rates, GEN - so spec section 6 lists them as runtime drawn. The v1.0.1 plate had
        them baked in as well, with the mock's sample values, and the two doubled up; delta v1.0.2
        cleared both rows from the plate, static words included, and quotes the runtime geometry by
        BASELINE rather than by box (see Text::rowAtBaseline). */
    inline constexpr float scopeLegendLeft           =   43.0f;
    inline constexpr float scopeLegendRight          = 1293.0f;
    inline constexpr float scopeLegendBaselineTop    =  151.0f;
    inline constexpr float scopeLegendBaselineBottom =  244.0f;

    /** GEN sits this far left of the FAIL LED sprite; the LED and its label close the row out on
        the right. Spec: "GEN n, then an 18 px gap, then the FAIL LED and its label". */
    inline constexpr float scopeGenToLedGap = 18.0f;

    /** The regions v1.0.2 cleared, verbatim. Only used to size the scope component - the text
        inside them is placed off the baselines above, not off these boxes. */
    inline const juce::Rectangle<float> scopeLegendTopRow
        { scopeLegendLeft, 140.0f, scopeLegendRight - scopeLegendLeft, 14.0f };
    inline const juce::Rectangle<float> scopeLegendBottomRow
        { scopeLegendLeft, 233.0f, scopeLegendRight - scopeLegendLeft, 14.0f };
    inline const juce::Rectangle<float> modelReadout{ 508.5f, 476.5f, 134.0f, 27.0f };
    /** 34 tall on y 63, like every other part of the band. These were 42 at y 47 - two pixels
        taller than the LCD and the buttons they sit beside, which is the drift the suite audit
        found in four castings and nobody had noticed by eye. */
    inline const juce::Rectangle<float> inMeter     { 1132.4f, 63.0f,  80.8f, 34.0f };
    inline const juce::Rectangle<float> outMeter    { 1225.2f, 63.0f,  80.8f, 34.0f };

    //==========================================================================
    /** **270 degrees, -135 to +135, and it is a CONSTANT now rather than only prose.**

        It is baked into every filmstrip, so it is a property of the artwork - but nothing was
        telling JUCE. With paint() fully overridden and RotaryVerticalDrag in use the default arc
        never rendered, so the figure survived only in the comment below, and anything later
        reading the Slider's own rotary parameters would have got JUCE's default and been quietly
        wrong. Elmer is the one casting that is not 270; BRAND.md records that as explicit
        per-casting freedom, which only means anything if the other five state their figure. */
    inline constexpr float knobSweepDegrees = 270.0f;

    /** 190px of vertical drag spans the full range, 760 while Shift is held. Both are suite
        figures: six castings had six drag feels - this one was on JUCE's untouched 250 - so the
        same hand got a different response from each. 190 is the plurality; the Shift fine mode
        comes from Reflect-84, because a player who learns it on one casting expects it on the
        next. */
    inline constexpr int knobDragPixels = 190;
    inline constexpr int knobFineDragPixels = 760;

    /** Knob filmstrips. Frame index = round(value01 * 127); the pointer sweeps -135 to +135
        degrees, baked frame by frame. The sprite is the CAP ONLY - ticks, numerals, unit and the
        control's name are in the plate.

        MODEL is the exception: 9 frames, indexed directly by model index with no interpolation.
        Nine is correct - TapeModelData.h holds NONE plus eight machines, and the spec's open
        question about dropping one is answered by the code. */
    enum class Cap { large, small, model };

    inline constexpr int    largeFrames = 128;
    inline constexpr float  largeFrame  = 90.0f;
    inline constexpr int    smallFrames = 128;
    inline constexpr float  smallFrame  = 52.0f;
    inline constexpr int    modelFrames = 9;

    struct KnobSpec
    {
        const char* paramId;
        juce::Point<float> spriteTopLeft;
        Cap cap;
    };

    /** Sprite top-left. The y values here are 7.27px HIGHER than every handoff before delta
        v1.0.5 stated, and that correction is the fix for the whole "needle doesn't line up"
        investigation - not the sweep angle, which was a red herring.

        The spec's old "cap centre" column measured the centre of the whole knob ELEMENT (dial box
        plus the control name printed beneath it), so the name dragged it 7.27px below the actual
        tick-arc centre. Blitting the cap there put the needle's pivot below the arc it sweeps, and
        the tip then landed ~7 degrees past the printed end mark - which reads as the knob showing a
        small negative value at minimum.

        Dial centre = this point + frameSize/2, i.e. y 386.0 (large row) and y 531.6 (small row).
        Tick arc radius is 46 (large) and 27 (small) from that centre. */
    inline constexpr std::array<KnobSpec, 11> knobs { {
        { "drive",   {   50.5f, 341.0f }, Cap::large },
        { "wow",     {  205.8f, 341.0f }, Cap::large },
        { "flutter", {  353.8f, 341.0f }, Cap::large },
        { "model",   {  530.5f, 341.0f }, Cap::model },
        { "noise",   {  678.5f, 341.0f }, Cap::large },
        { "failure", {  864.3f, 341.0f }, Cap::large },
        { "mix",     { 1038.0f, 341.0f }, Cap::large },
        { "output",  { 1186.0f, 341.0f }, Cap::large },
        { "lp",      { 1031.0f, 505.6f }, Cap::small },
        { "ramp",    { 1131.0f, 505.6f }, Cap::small },
        { "hp",      { 1231.0f, 505.6f }, Cap::small } } };

    inline constexpr float frameSizeFor (Cap c) noexcept
    {
        return c == Cap::small ? smallFrame : largeFrame;
    }

    inline constexpr int frameCountFor (Cap c) noexcept
    {
        return c == Cap::model ? modelFrames : (c == Cap::small ? smallFrames : largeFrames);
    }

    //==========================================================================
    /** Two-state buttons. The plate is 98 x 25 with a 2 px shadow bleed, so the sprite is 102 x 29
        and its top-left is 2 px up and left of the plate's. Lit state is an amber LED plus its
        glow; the plate itself does not change. Each group is exclusive-select. */
    inline constexpr float buttonW = 102.0f;
    inline constexpr float buttonH = 29.0f;

    struct ButtonSpec
    {
        const char* paramId;
        int choiceIndex;             // the value this member selects
        bool isBoolParam;            // bool params take 0/1; noiseCharacter is a 3-way choice
        juce::Point<float> spriteTopLeft;
    };

    inline constexpr std::array<ButtonSpec, 9> buttons { {
        // SWITCHING - switchMode is a bool: false FADE, true CLUNK
        { "switchMode",     0, true,  { 482.5f, 534.5f } },
        { "switchMode",     1, true,  { 482.5f, 566.0f } },
        // NOISE BED - noiseCharacter is a 3-way choice, TAPE/VCR/DUST
        { "noiseCharacter", 0, false, { 598.5f, 534.5f } },
        { "noiseCharacter", 1, false, { 598.5f, 566.0f } },
        { "noiseCharacter", 2, false, { 598.5f, 597.5f } },
        // HUM - bool: false OFF, true ON
        { "hum",            0, true,  { 714.5f, 534.5f } },
        { "hum",            1, true,  { 714.5f, 566.0f } },
        // SPREAD - bool: false LINKED, true STEREO
        { "spread",         0, true,  { 858.3f, 499.5f } },
        { "spread",         1, true,  { 858.3f, 531.0f } } } };

    //==========================================================================
    /** GENERATION: 8 segments, 20 x 17 with a 3 px glow bleed, so the sprite is 26 x 23. Two files
        serve all eight positions; the 1-8 numerals underneath are printed in the plate. Segments
        1..GEN are lit. */
    inline constexpr float genSegmentW = 26.0f;
    inline constexpr float genSegmentH = 23.0f;
    inline constexpr float genSegmentY = 493.5f;
    inline constexpr float genSegmentX0 = 224.3f;
    inline constexpr float genSegmentPitch = 25.0f;
    inline constexpr int   genSegmentCount = 8;

    /** FAULT ACTIVITY dots and the FAIL buttons share the same diameter-22 lamp: a 34 x 34 sprite
        after its 6 px bleed. One on/off pair covers all seven, which is why the asset list embeds
        two lamp files rather than fourteen. */
    inline constexpr float lampSize = 34.0f;

    inline constexpr float faultDotY = 567.5f;
    inline constexpr std::array<float, 4> faultDotX { { 839.4f, 874.4f, 909.4f, 944.8f } };
    /** DRP, SNG, CRK, WBL - the order FailureEventType already uses. */
    inline constexpr std::array<const char*, 4> faultParamIds { {
        "failureDropouts", "failureSnags", "failureCrinkles", "failureImbalance" } };
    inline constexpr int faultFlashMs = 260;

    inline constexpr float failButtonY = 490.5f;
    inline constexpr std::array<float, 3> failButtonX { { 44.5f, 78.8f, 112.8f } };
    /** STP, FLT, FAI - momentary. */
    inline constexpr std::array<const char*, 3> failParamIds { { "stop", "filterAux", "failAux" } };

    /** The scope strip's FAIL LED: a diameter-8 lamp, 18 x 18 after its 5 px bleed. Lit while any
        of STP/FLT/FAI is held. */
    inline constexpr juce::Point<float> failLedTopLeft { 1241.9f, 230.3f };
    inline constexpr float failLedSize = 18.0f;

    //==========================================================================
    // --- runtime type (spec section 6) ------------------------------------------------------
    inline constexpr float lcdTextSize      = 18.0f;
    inline constexpr float lcdTracking      = 2.0f;
    inline constexpr float modelTextSize    = 12.5f;
    inline constexpr float modelTracking    = 1.2f;
    inline constexpr float meterTextSize    = 19.0f;
    inline constexpr float scopeLegendSize  = 12.0f;
    inline constexpr float scopeLegendTracking = 1.3f;

    /** **The readout revert lives in core now - `nf::ReadoutFormat::revertMs`, 900 ms.**

        It was 1100 here. The suite ran 800 / 900 / 1100 / 1200 under three different constant names
        and two mechanisms, and no spec anywhere justified any of them; 900 is what three castings
        already had. `ProgramHeader::readoutFormat()` is where this panel states its readout
        spelling, and the delay comes with it rather than being a separate number here that nothing
        binds to the others.

        Left as a comment rather than deleted silently, because a reader looking for the old
        constant should find out where it went rather than conclude the revert was removed. */

    inline constexpr int animationHz = 60;

    // --- scope ------------------------------------------------------------------------------
    inline constexpr float scopeTraceThickness = 1.7f;
    inline constexpr float scopeHaloThickness  = 5.0f;
    inline constexpr int   scopeGridColumns = 8;
    inline constexpr int   scopeGridRows = 4;
    /** 8 columns across the well, labelled 500 ms / DIV, so the visible span is four seconds. */
    inline constexpr float scopeMsPerDivision = 500.0f;
    inline constexpr float scopeSpanSeconds =
        scopeMsPerDivision * (float) scopeGridColumns / 1000.0f;

    // --- window -----------------------------------------------------------------------------
    /** 0.5x to 2x, per BRAND.md: the scaling range has to be a genuine accessibility lever, not a
        token 10-15%. */
    inline constexpr float minScale = 0.5f;
    inline constexpr float maxScale = 2.0f;
}

//==============================================================================
/** Maps a role to its embedded sprite. Several controls deliberately share one asset - all seven
    large knobs are the same cap, all three small ones likewise, and one lamp serves both the fault
    dots and the FAIL buttons. The handoff ships those as separate identical files; only one copy of
    each is embedded. */
namespace Asset
{
    inline juce::Image load (const char* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    inline const juce::Image& panel()
    {
        static const juce::Image i = load (BinaryData::panel_background_2x_png,
                                           BinaryData::panel_background_2x_pngSize);
        return i;
    }

    inline const juce::Image& capStrip (Layout::Cap c)
    {
        static const juce::Image large = load (BinaryData::knob_large_2x_png,
                                               BinaryData::knob_large_2x_pngSize);
        static const juce::Image small = load (BinaryData::knob_small_2x_png,
                                               BinaryData::knob_small_2x_pngSize);
        static const juce::Image model = load (BinaryData::knob_model_2x_png,
                                               BinaryData::knob_model_2x_pngSize);

        switch (c)
        {
            case Layout::Cap::small: return small;
            case Layout::Cap::model: return model;
            case Layout::Cap::large:
            default:                 return large;
        }
    }

    inline const juce::Image& lamp (bool lit)
    {
        static const juce::Image on  = load (BinaryData::lamp_on_2x_png,
                                             BinaryData::lamp_on_2x_pngSize);
        static const juce::Image off = load (BinaryData::lamp_off_2x_png,
                                             BinaryData::lamp_off_2x_pngSize);
        return lit ? on : off;
    }

    inline const juce::Image& lampPressed()
    {
        static const juce::Image i = load (BinaryData::lamp_press_2x_png,
                                           BinaryData::lamp_press_2x_pngSize);
        return i;
    }

    inline const juce::Image& genSegment (bool lit)
    {
        static const juce::Image on  = load (BinaryData::gen_seg_on_2x_png,
                                             BinaryData::gen_seg_on_2x_pngSize);
        static const juce::Image off = load (BinaryData::gen_seg_off_2x_png,
                                             BinaryData::gen_seg_off_2x_pngSize);
        return lit ? on : off;
    }

    /** **Each Program button is one three-frame strip, and the order is fixed.**

        Both buttons carry two permanently printed legends - SAVE above STORE, DELETE above CANCEL -
        and never change their face. Only which legend is backlit changes, so the three frames are
        lighting states rather than labels:

        | Frame | Backlight  | SAVE means                | DELETE means                    |
        |-------|------------|---------------------------|---------------------------------|
        | 0     | both dark  | nothing to do             | nothing to do (Factory or INIT) |
        | 1     | top lit    | SAVE live                 | DELETE live (a User Program)    |
        | 2     | bottom lit | STORE live (commit name)  | CANCEL live (abandon name)      |

        Both legends lit is not a state and is deliberately not exported: the two functions are
        mutually exclusive, so a fourth face could only ever indicate a bug.

        **Frame 0 is inert in code as well as in appearance.** A button showing both legends dark
        must not act when clicked, or the backlight is claiming something the control contradicts.

        This replaces `saveButton(bool)` and a `DeleteFace` enum whose third value was a *relabel* -
        the sprite that said CANCEL where the other said DELETE. A printed panel legend cannot
        rewrite itself, which is what the second legend exists to solve. The old two-face SAVE also
        had no STORE at all, so naming left it reading SAVE while acting as STORE. */
    enum class ProgramButtonFrame { bothDark = 0, topLit = 1, bottomLit = 2 };

    inline const juce::Image& saveButtonStrip()
    {
        static const juce::Image strip = load (BinaryData::btn_save_strip_2x_png,
                                               BinaryData::btn_save_strip_2x_pngSize);
        return strip;
    }

    inline const juce::Image& deleteButtonStrip()
    {
        static const juce::Image strip = load (BinaryData::btn_delete_strip_2x_png,
                                               BinaryData::btn_delete_strip_2x_pngSize);
        return strip;
    }

    inline const juce::Image& failLed (bool lit)
    {
        static const juce::Image on  = load (BinaryData::led_fail_on_2x_png,
                                             BinaryData::led_fail_on_2x_pngSize);
        static const juce::Image off = load (BinaryData::led_fail_off_2x_png,
                                             BinaryData::led_fail_off_2x_pngSize);
        return lit ? on : off;
    }
}

} // namespace TapeRotTheme
