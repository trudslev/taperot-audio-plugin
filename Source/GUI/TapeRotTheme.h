#pragma once

#include <juce_graphics/juce_graphics.h>
#include <BinaryData.h>
#include <array>

// All layout numbers are in the SVG's reference space (960x400). Components draw
// in this space unconditionally; PluginEditor applies a single uniform scale
// transform so everything scales together on resize.
namespace TapeRotTheme
{
    namespace Colour
    {
        inline const juce::Colour charcoal{0xFF26231F};
        inline const juce::Colour headerTop{0xFF2B2822};
        inline const juce::Colour headerBottom{0xFF221F1A};
        inline const juce::Colour panelTop{0xFFEDE4CE};
        inline const juce::Colour panelBottom{0xFFE2D8BF};
        inline const juce::Colour knobFaceTop{0xFFF7F1E1};
        inline const juce::Colour knobFaceBottom{0xFFE4DAC2};
        inline const juce::Colour amber{0xFFD98324};
        inline const juce::Colour amberBright{0xFFF0A94B};
        inline const juce::Colour lamp{0xFFF5A83C};
        inline const juce::Colour lampGlowInner{0xFFFFB347};
        inline const juce::Colour dark{0xFF17140F};
        inline const juce::Colour rim{0xFF4A443C};
        inline const juce::Colour ink{0xFF3A342C};
        inline const juce::Colour mutedLabel{0xFF8A7E63};
        inline const juce::Colour sectionLabel{0xFF6E6450};
        inline const juce::Colour tick{0xFF9A8E71};
        inline const juce::Colour divider{0xFFB7AB8E};
        inline const juce::Colour bezelFill{0xFFE8DFC9};
        inline const juce::Colour bezelStroke{0xFFB9AE93};
        inline const juce::Colour innerRing{0xFFC8BCA0};
        inline const juce::Colour switchThumb{0xFFEFE7D3};
        inline const juce::Colour switchThumbStroke{0xFF6E675A};
        inline const juce::Colour dotOff{0xFFB3A88C};
        inline const juce::Colour screwFill{0xFFCFC8B8};
        inline const juce::Colour screwStroke{0xFF7A7263};
        inline const juce::Colour versionText{0xFFA2967A};
        inline const juce::Colour lampRing{0xFF7A4A0F};
        inline const juce::Colour specular{0xFFFFE0AC};
        inline const juce::Colour digitText{0xFFEDE6D2};
        inline const juce::Colour digitCellFill{0xFF17140F};
        inline const juce::Colour counterHousingFill{0xFF1C1915};
        inline const juce::Colour resetStubFill{0xFFC9C2B2};
        inline const juce::Colour resetStubStroke{0xFF6E675A};
        inline const juce::Colour cream{0xFFF5F2EA};
        // Brighter, slightly warm white for the Dymo emboss face - stressed/stretched label
        // plastic reads brighter than the general cream used elsewhere in the panel.
        inline const juce::Colour dymoEmbossFace{0xFFFFFDF6};
    }

    inline juce::String sansFontName() { return "Helvetica Neue"; }
    inline juce::String monoFontName() { return juce::Font::getDefaultMonospacedFontName(); }

    inline juce::Font sectionLabelFont() { return juce::FontOptions(sansFontName(), 10.0f, juce::Font::bold); }
    inline juce::Font knobLabelFont() { return juce::FontOptions(sansFontName(), 11.0f, juce::Font::bold); }
    inline juce::Font microLabelFont() { return juce::FontOptions(sansFontName(), 8.5f, juce::Font::plain); }
    inline juce::Font switchLabelFont() { return juce::FontOptions(sansFontName(), 9.0f, juce::Font::bold); }
    inline juce::Font switchCaptionFont() { return juce::FontOptions(sansFontName(), 7.0f, juce::Font::plain); }
    inline juce::Font modelReadoutFont(float sizePx) { return juce::FontOptions(sansFontName(), sizePx, juce::Font::bold); }
    inline juce::Font dotLabelFont() { return juce::FontOptions(sansFontName(), 6.5f, juce::Font::plain); }
    // Impact Label (design/impact-label/, by Michael Tension - commercial use permitted,
    // donationware), embedded as binary data rather than depending on it being installed as a
    // system font. Deliberately the "_reversed" variant: the regular one's glyphs are a pre-baked
    // solid tile with the letter cut out as a negative-space hole (a complete look on its own),
    // while "_reversed" is plain open letterforms, which is what drawEmbossedGlyph needs to work
    // with - the font's own rough strokes already read as embossed without any further bevel.
    inline juce::Font dymoFont()
    {
        static const juce::Typeface::Ptr typeface =
            juce::Typeface::createSystemTypefaceFor(BinaryData::Impact_label_reversed_ttf,
                                                     (size_t) BinaryData::Impact_label_reversed_ttfSize);
        return juce::Font(typeface).withHeight(29.4f);
    }
    inline juce::Font counterDigitFont() { return juce::FontOptions(monoFontName(), 22.0f, juce::Font::bold); }
    inline juce::Font versionFont() { return juce::FontOptions(sansFontName(), 8.0f, juce::Font::plain); }

    // Section-header letter spacing (px), applied via juce::AttributedString/Font tracking helpers.
    constexpr float sectionLabelTracking = 3.5f;
    constexpr float knobLabelTracking = 2.5f;
    constexpr float microLabelTracking = 2.5f;
    constexpr float switchLabelTracking = 2.0f;
    constexpr float switchCaptionTracking = 0.3f;
    constexpr float modelReadoutTracking = 1.5f;
    constexpr float dotLabelTracking = 0.5f;
    constexpr float dymoTracking = 4.0f;
    constexpr float versionTracking = 2.0f;

    namespace Layout
    {
        constexpr float canvasWidth = 960.0f;
        constexpr float canvasHeight = 400.0f;

        constexpr float bezelOuterX = 2.0f, bezelOuterY = 2.0f;
        constexpr float bezelOuterW = 956.0f, bezelOuterH = 396.0f, bezelOuterRadius = 14.0f;
        constexpr float panelX = 8.0f, panelY = 8.0f;
        constexpr float panelW = 944.0f, panelH = 384.0f, panelRadius = 10.0f;

        constexpr float headerX = 8.0f, headerY = 8.0f;
        constexpr float headerW = 944.0f, headerH = 108.0f;
        constexpr float headerSeparatorY = 116.0f;

        constexpr float dymoX = 32.0f, dymoY = 40.0f, dymoW = 168.0f, dymoH = 34.0f;
        // A hand-applied Dymo label doesn't sit dead-square on the panel - keep this visibly but
        // subtly off from 0deg (roughly -1.5 to -3deg reads as "manually stuck on" without looking
        // broken).
        constexpr float dymoRotationDegrees = -2.8f;

        // Physical-label imperfections: the tape sits ON TOP of the panel (contact shadow, biased
        // toward the lower-right like a single overhead light), its cut ends aren't a clean
        // parallel rectangle (hand-cut/simple cutter), and it has a whisper of bow/unevenness
        // rather than dead-flat edges. An earlier, more conservative pass at these values read as
        // "no different" at actual on-screen size, so these are pushed noticeably further than
        // feels necessary in an isolated close-up render - keep that in mind before nudging them
        // back down.
        constexpr float dymoContactShadowAlpha = 0.55f;
        constexpr float dymoContactShadowBlurPx = 6.0f;
        constexpr float dymoContactShadowOffsetX = 3.0f;
        constexpr float dymoContactShadowOffsetY = 5.0f;
        constexpr float dymoLeftCutAnglePx = -3.5f;
        constexpr float dymoRightCutAnglePx = 5.0f;
        constexpr float dymoTapeBowPx = 0.8f;
        constexpr float dymoEdgeImperfectionPx = 1.0f;

        // Per-character hand-punched variance: small random position/rotation jitter, deterministic
        // per character index so it doesn't flicker between repaints.
        constexpr float dymoJitterPositionPx = 0.5f;
        constexpr float dymoJitterRotationDegrees = 2.2f;
        // Ink-bounds centring (see drawEmbossedTrackedText) still read slightly high in practice -
        // this nudges the whole line down a touch to sit visually centred on the strip.
        constexpr float dymoTextVerticalNudgePx = 2.0f;

        constexpr float scopeX = 224.0f, scopeXEnd = 640.0f, scopeCentreY = 62.0f;

        constexpr float counterHousingX = 672.0f, counterHousingY = 34.0f;
        constexpr float counterHousingW = 110.0f, counterHousingH = 52.0f, counterHousingRadius = 6.0f;
        constexpr float digitCellW = 26.0f, digitCellH = 38.0f, digitCellGap = 6.0f, digitCellRadius = 3.0f;
        constexpr float digitCellFirstX = 682.0f, digitCellY = 41.0f;
        constexpr float resetStubX = 800.0f, resetStubY = 80.0f, resetStubRadius = 5.0f;

        constexpr float lampX = 838.0f, lampY = 58.0f, lampRadius = 8.0f, lampGlowRadius = 20.0f;
        constexpr float lampSpecularX = 835.5f, lampSpecularY = 55.0f, lampSpecularRadius = 2.2f;

        constexpr float sectionLabelY = 146.0f;
        constexpr float inputLabelX = 82.0f, transportLabelX = 244.0f, machineLabelX = 460.0f,
                        decayLabelX = 634.0f, outputLabelX = 814.0f;

        constexpr float dividerTop = 132.0f, dividerBottom = 362.0f;
        constexpr std::array<float, 4> dividerX{136.0f, 352.0f, 568.0f, 700.0f};

        constexpr float knobCentreY = 212.0f, knobRadius = 33.0f;
        constexpr float knobLabelOffsetY = 63.0f;
        constexpr float knobTickInnerRadius = knobRadius + 6.0f;
        constexpr float knobTickOuterRadius = knobRadius + 11.0f;
        constexpr float knobInnerRingRadius = knobRadius - 7.0f;
        constexpr float knobPointerInnerRadius = 9.0f;
        constexpr float knobPointerOuterRadius = knobRadius - 4.0f;
        constexpr int knobNumTicks = 11;
        constexpr float knobArcStartDegrees = -135.0f;
        constexpr float knobArcEndDegrees = 135.0f;

        struct KnobSpec
        {
            const char* paramID;
            const char* label;
            float x;
        };

        inline constexpr std::array<KnobSpec, 8> knobs{{
            {"drive", "DRIVE", 82.0f},
            {"wow", "WOW", 190.0f},
            {"flutter", "FLUTTER", 298.0f},
            {"model", "MODEL", 406.0f},
            {"noise", "NOISE", 514.0f},
            {"failure", "FAILURE", 634.0f},
            {"mix", "MIX", 760.0f},
            {"output", "OUTPUT", 868.0f},
        }};

        constexpr float switchY = 309.5f, switchH = 13.0f, switchW = 48.0f, switchRadius = 6.5f;
        constexpr float switchThumbRadius = 8.0f, switchThumbDotRadius = 2.4f;
        constexpr float switchCentreY = 316.0f;
        constexpr float switchLabelY = 338.0f, switchCaptionY = 350.0f;

        // MACHINE section switch row positions, per the updated design/taperot-interface.svg.
        constexpr float switchModeSwitchX = 370.0f;
        constexpr float noiseSwitchX = 449.0f;
        constexpr float humSwitchX = 518.0f;
        constexpr float spreadSwitchX = 610.0f;

        // ModelReadout box, per the updated SVG (sits under the MODEL knob, replacing its
        // generic knob-label text).
        constexpr float modelReadoutX = 361.0f, modelReadoutY = 261.0f;
        constexpr float modelReadoutW = 90.0f, modelReadoutH = 22.0f, modelReadoutRadius = 4.0f;
        constexpr float modelReadoutLabelY = 297.0f;
        constexpr float modelReadoutNominalFontPx = 10.0f;
        constexpr float modelReadoutMinFontPx = 6.5f;

        constexpr float failureDotY = 365.0f, failureDotRadius = 4.0f, failureDotLabelY = 377.0f;
        constexpr float failureDotFirstX = 592.0f, failureDotSpacing = 28.0f;

        struct FailureDotSpec
        {
            const char* paramID;
            const char* label;
        };

        inline constexpr std::array<FailureDotSpec, 4> failureDots{{
            {"failureDropouts", "DRP"},
            {"failureSnags", "SNG"},
            {"failureCrinkles", "CRK"},
            {"failureImbalance", "WBL"},
        }};

        constexpr float screwRadius = 6.5f;
        inline constexpr std::array<std::pair<float, float>, 4> screwPositions{{
            {24.0f, 24.0f}, {936.0f, 24.0f}, {24.0f, 376.0f}, {936.0f, 376.0f}
        }};
        constexpr int screwSeed = 1337;

        constexpr float versionTextX = 480.0f, versionTextY = 390.0f;

        constexpr int speckleSeed = 4242;
        constexpr int speckleCount = 150;

        // GEN/LP/HP/aux controls (step 6) sit in the previously-empty sub-row space under
        // INPUT/TRANSPORT/OUTPUT rather than the already-packed MACHINE/DECAY sub-rows (which
        // already hold the NOISE/HUM switches and the SPREAD switch + failure dots respectively),
        // so nothing needs to be resized or crowded to fit them.
        constexpr float smallKnobCentreY = 335.0f, smallKnobRadius = 18.0f;
        constexpr float smallKnobLabelOffsetY = 28.0f;

        constexpr float lpKnobX = 760.0f, rampKnobX = 814.0f, hpKnobX = 868.0f; // under OUTPUT

        constexpr float genSelectorCentreX = 244.0f, genSelectorY = 327.0f; // under TRANSPORT
        constexpr float genSelectorSegmentW = 14.0f, genSelectorSegmentH = 22.0f, genSelectorGap = 4.0f;
        constexpr float genSelectorLabelOffsetY = 30.0f;

        constexpr float auxButtonCentreY = 335.0f, auxButtonRadius = 16.0f; // under INPUT
        constexpr float stopButtonX = 36.0f, filterButtonX = 72.0f, failButtonX = 108.0f;
        constexpr float auxButtonLabelOffsetY = 26.0f;
    }

    // Angle (degrees, clockwise from 12 o'clock) for a normalised 0..1 value across the knob arc.
    inline float knobAngleForValue01(float value01) noexcept
    {
        return Layout::knobArcStartDegrees
             + value01 * (Layout::knobArcEndDegrees - Layout::knobArcStartDegrees);
    }

    // Unit direction vector for an angle measured clockwise from 12 o'clock.
    inline juce::Point<float> directionForAngleDegrees(float degrees) noexcept
    {
        const float radians = juce::degreesToRadians(degrees);
        return {std::sin(radians), -std::cos(radians)};
    }

    inline juce::Point<float> pointOnCircle(juce::Point<float> centre, float radius, float angleDegrees) noexcept
    {
        return centre + directionForAngleDegrees(angleDegrees) * radius;
    }

    inline float trackedTextWidth(const juce::String& text, const juce::Font& font, float trackingPx)
    {
        float width = 0.0f;
        for (int i = 0; i < text.length(); ++i)
        {
            width += juce::GlyphArrangement::getStringWidth(font, juce::String::charToString(text[i]));
            if (i < text.length() - 1)
                width += trackingPx;
        }
        return width;
    }

    // juce::Font has no absolute-pixel letter-spacing, so this draws glyph-by-glyph
    // to reproduce the SVG's `letter-spacing` attribute exactly.
    inline void drawTrackedText(juce::Graphics& g, const juce::String& text, const juce::Font& font,
                                 float trackingPx, juce::Rectangle<float> area,
                                 juce::Justification justification, juce::Colour colour)
    {
        g.setFont(font);
        g.setColour(colour);

        const float totalWidth = trackedTextWidth(text, font, trackingPx);
        float x = area.getX();
        if (justification.testFlags(juce::Justification::horizontallyCentred))
            x = area.getCentreX() - totalWidth * 0.5f;
        else if (justification.testFlags(juce::Justification::right))
            x = area.getRight() - totalWidth;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString(text[i]);
            const float charWidth = juce::GlyphArrangement::getStringWidth(font, ch);
            g.drawText(ch, juce::Rectangle<float>(x, area.getY(), charWidth + 1.0f, area.getHeight()),
                       juce::Justification::centredLeft, false);
            x += charWidth + trackingPx;
        }
    }
}
