#include "DymoLabel.h"
#include "TapeRotTheme.h"

namespace
{
    // Deterministic per-character pseudo-random in [0, 1) - stable across repaints (no flicker)
    // without needing to store per-character state anywhere.
    float charPseudoRandom(int index, int salt)
    {
        const float s = std::sin((float) index * 12.9898f + (float) salt * 78.233f) * 43758.5453f;
        return s - std::floor(s);
    }

    // Impact Label already has its own rough, hand-stamped character - drawn plain, at the right
    // size, it reads as embossed on its own with no highlight/shadow bevel trick needed. Placed
    // via GlyphArrangement at an explicit baseline (not drawText's rectangle+Justification, whose
    // vertical centring goes by the font's declared ascent/descent - for a hand-drawn display font
    // those often don't match where the ink actually sits).
    void drawEmbossedGlyph(juce::Graphics& g, const juce::String& glyph, const juce::Font& font,
                            float baselineX, float baselineY, float charWidth, float rotationDegrees)
    {
        using namespace TapeRotTheme;

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(rotationDegrees),
                                                        baselineX + charWidth * 0.5f, baselineY));

        juce::GlyphArrangement ga;
        ga.addLineOfText(font, glyph, baselineX, baselineY);
        g.setColour(Colour::dymoEmbossFace);
        ga.draw(g);

        g.restoreState();
    }

    void drawEmbossedTrackedText(juce::Graphics& g, const juce::String& text, const juce::Font& font,
                                  float trackingPx, juce::Rectangle<float> area)
    {
        using namespace TapeRotTheme;

        const float totalWidth = trackedTextWidth(text, font, trackingPx);
        float x = area.getCentreX() - totalWidth * 0.5f;

        // Centre the string's actual ink - not the font's ascent/descent metrics - vertically in
        // area: measure the whole line's ink bounds relative to a y=0 baseline, then place the
        // baseline wherever makes that ink's centre land on area's centre.
        juce::GlyphArrangement measuring;
        measuring.addLineOfText(font, text, 0.0f, 0.0f);
        const auto inkBounds = measuring.getBoundingBox(0, -1, true);
        const float baselineY = area.getCentreY() - (inkBounds.getY() + inkBounds.getHeight() * 0.5f)
                                 + Layout::dymoTextVerticalNudgePx;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString(text[i]);
            const float charWidth = juce::GlyphArrangement::getStringWidth(font, ch);

            // Hand-punched variance: each letter lands slightly off-grid and tilted.
            const float jitterX = (charPseudoRandom(i, 0) * 2.0f - 1.0f) * Layout::dymoJitterPositionPx;
            const float jitterY = (charPseudoRandom(i, 1) * 2.0f - 1.0f) * Layout::dymoJitterPositionPx * 0.6f;
            const float rotationJitter = (charPseudoRandom(i, 2) * 2.0f - 1.0f) * Layout::dymoJitterRotationDegrees;

            drawEmbossedGlyph(g, ch, font, x + jitterX, baselineY + jitterY, charWidth, rotationJitter);

            x += charWidth + trackingPx;
        }
    }

    // A hand-cut tape strip isn't a clean rectangle: its left/right ends are each cut at their
    // own slight angle (not parallel to one another), and its top/bottom edges have a whisper of
    // bow rather than being dead straight. Built as a simple hexagon (corners plus one extra
    // midpoint per long edge) and rounded afterward, so the "bow" reads as a gentle continuous
    // curve rather than a sharp kink.
    juce::Path dymoPlatePath()
    {
        using namespace TapeRotTheme;

        const float w = Layout::dymoW, h = Layout::dymoH;
        const float leftCut = Layout::dymoLeftCutAnglePx;
        const float rightCut = Layout::dymoRightCutAnglePx;
        const float bow = Layout::dymoTapeBowPx;
        const float uneven = Layout::dymoEdgeImperfectionPx;

        const juce::Point<float> tl(leftCut * 0.5f, 0.0f);
        const juce::Point<float> bl(leftCut * -0.5f, h);
        const juce::Point<float> tr(w - rightCut * 0.5f, uneven);
        const juce::Point<float> br(w + rightCut * 0.5f, h - uneven * 0.6f);

        const juce::Point<float> topMid((tl.x + tr.x) * 0.5f, (tl.y + tr.y) * 0.5f - bow);
        const juce::Point<float> bottomMid((bl.x + br.x) * 0.5f, (bl.y + br.y) * 0.5f + bow);

        juce::Path path;
        path.startNewSubPath(tl);
        path.lineTo(topMid);
        path.lineTo(tr);
        path.lineTo(br);
        path.lineTo(bottomMid);
        path.lineTo(bl);
        path.closeSubPath();

        return path.createPathWithRoundedCorners(4.0f);
    }
}

void DymoLabel::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(Layout::dymoRotationDegrees))
                        .translated(Layout::dymoX, Layout::dymoY));

    const auto platePath = dymoPlatePath();
    const juce::Rectangle<float> plate(0.0f, 0.0f, Layout::dymoW, Layout::dymoH);

    // A contact shadow biased toward the lower-right (as if lit from one overhead source), so the
    // tape reads as sitting physically on top of the panel rather than being part of the panel
    // artwork itself.
    juce::DropShadow shadow(juce::Colours::black.withAlpha(Layout::dymoContactShadowAlpha),
                             (int) Layout::dymoContactShadowBlurPx,
                             {(int) Layout::dymoContactShadowOffsetX, (int) Layout::dymoContactShadowOffsetY});
    shadow.drawForPath(g, platePath);

    g.setColour(Colour::dark);
    g.fillPath(platePath);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.strokePath(platePath, juce::PathStrokeType(1.0f));

    const auto textArea = plate.reduced(4.0f, 3.0f);
    drawEmbossedTrackedText(g, "TAPEROT", dymoFont(), dymoTracking, textArea);

    g.restoreState();
}
