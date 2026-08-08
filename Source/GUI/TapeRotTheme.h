#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <BinaryData.h>

#include <array>

/**
    TapeRot's design tokens, transcribed from `design/TapeRot-GUI-Spec.md`.

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
    inline const juce::Rectangle<float> programLcd  { 417.0f,  47.0f, 470.0f, 40.0f };
    inline const juce::Rectangle<float> scopeWell   {  43.0f, 158.5f, 1250.0f, 70.0f };
    inline const juce::Rectangle<float> modelReadout{ 508.5f, 476.5f, 134.0f, 27.0f };
    inline const juce::Rectangle<float> inMeter     { 1134.0f, 47.0f,  80.0f, 42.0f };
    inline const juce::Rectangle<float> outMeter    { 1226.0f, 47.0f,  80.0f, 42.0f };

    //==========================================================================
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

    inline constexpr std::array<KnobSpec, 11> knobs { {
        { "drive",   {   50.5f, 348.3f }, Cap::large },
        { "wow",     {  205.8f, 348.3f }, Cap::large },
        { "flutter", {  353.8f, 348.3f }, Cap::large },
        { "model",   {  530.5f, 348.3f }, Cap::model },
        { "noise",   {  678.5f, 348.3f }, Cap::large },
        { "failure", {  864.3f, 348.3f }, Cap::large },
        { "mix",     { 1038.0f, 348.3f }, Cap::large },
        { "output",  { 1186.0f, 348.3f }, Cap::large },
        { "lp",      { 1031.0f, 512.8f }, Cap::small },
        { "ramp",    { 1131.0f, 512.8f }, Cap::small },
        { "hp",      { 1231.0f, 512.8f }, Cap::small } } };

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
        int choiceIndex;
        juce::Point<float> spriteTopLeft;
        const char* onResource;
        const char* offResource;
    };

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

    /** The LCD shows `PARAMETER: value unit` while a control is moved and reverts to the program
        name this long after release. Only direct manipulation triggers it - a SliderAttachment
        also fires on Program apply and on every host automation step, so the takeover has to be
        guarded on the control's own drag state or the display latches and flickers. */
    inline constexpr int lcdRevertMs = 1100;

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
        static const juce::Image large = load (BinaryData::knob_drive_2x_png,
                                               BinaryData::knob_drive_2x_pngSize);
        static const juce::Image small = load (BinaryData::knob_hp_2x_png,
                                               BinaryData::knob_hp_2x_pngSize);
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
        static const juce::Image on  = load (BinaryData::dot_crk_on_2x_png,
                                             BinaryData::dot_crk_on_2x_pngSize);
        static const juce::Image off = load (BinaryData::dot_crk_off_2x_png,
                                             BinaryData::dot_crk_off_2x_pngSize);
        return lit ? on : off;
    }

    inline const juce::Image& lampPressed()
    {
        static const juce::Image i = load (BinaryData::fail_fai_press_2x_png,
                                           BinaryData::fail_fai_press_2x_pngSize);
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
