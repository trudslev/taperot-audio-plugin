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

    // Approximates offsetting the glyph's outline perpendicular to itself: two hard-edged copies
    // of the same glyph, offset toward and away from the light, sandwiching the face colour so
    // only their un-overlapped rims show through as a highlight/shadow bevel.
    void drawEmbossedGlyph(juce::Graphics& g, const juce::String& glyph, const juce::Font& font,
                            juce::Rectangle<float> cell, float rotationDegrees, float pressure)
    {
        using namespace TapeRotTheme;

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(rotationDegrees),
                                                        cell.getCentreX(), cell.getCentreY()));

        // Weaker punch pressure shrinks the bevel offset (a thinner rim), not its opacity - the
        // highlight/shadow stay fully opaque so the edge always reads as hard-cut, never as a
        // translucent (and against the dark plate, gray-looking) soft glow.
        const float offsetScale = Layout::dymoBevelContrastStrength * juce::jlimit(0.0f, 1.0f, pressure);
        const float angleRad = juce::degreesToRadians(Layout::dymoLightDirectionDegrees);
        const juce::Point<float> lightDir(std::cos(angleRad), std::sin(angleRad));
        const auto highlightOffset = lightDir * (Layout::dymoBevelOffsetPx * offsetScale);
        const auto shadowOffset = lightDir * (-Layout::dymoBevelOffsetPx * offsetScale);

        g.setFont(font);

        g.setColour(Colour::dymoEmbossShadow);
        g.drawText(glyph, cell.translated(shadowOffset.x, shadowOffset.y), juce::Justification::centred, false);

        g.setColour(Colour::dymoEmbossHighlight);
        g.drawText(glyph, cell.translated(highlightOffset.x, highlightOffset.y), juce::Justification::centred, false);

        g.setColour(Colour::dymoEmbossFace);
        g.drawText(glyph, cell, juce::Justification::centred, false);

        g.restoreState();
    }

    void drawEmbossedTrackedText(juce::Graphics& g, const juce::String& text, const juce::Font& font,
                                  float trackingPx, juce::Rectangle<float> area)
    {
        using namespace TapeRotTheme;

        const float totalWidth = trackedTextWidth(text, font, trackingPx);
        float x = area.getCentreX() - totalWidth * 0.5f;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString(text[i]);
            const float charWidth = juce::GlyphArrangement::getStringWidth(font, ch);

            // Hand-punched variance: each letter lands slightly off-grid and tilted, and punches
            // with slightly different pressure (weaker pressure -> weaker bevel contrast).
            const float jitterX = (charPseudoRandom(i, 0) * 2.0f - 1.0f) * Layout::dymoJitterPositionPx;
            const float jitterY = (charPseudoRandom(i, 1) * 2.0f - 1.0f) * Layout::dymoJitterPositionPx * 0.6f;
            const float rotationJitter = (charPseudoRandom(i, 2) * 2.0f - 1.0f) * Layout::dymoJitterRotationDegrees;
            const float pressure = Layout::dymoPressureMin + (1.0f - Layout::dymoPressureMin) * charPseudoRandom(i, 3);

            const juce::Rectangle<float> cell(x + jitterX, area.getY() + jitterY, charWidth + 1.0f, area.getHeight());
            drawEmbossedGlyph(g, ch, font, cell, rotationJitter, pressure);

            x += charWidth + trackingPx;
        }
    }
}

void DymoLabel::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(Layout::dymoRotationDegrees))
                        .translated(Layout::dymoX, Layout::dymoY));

    const juce::Rectangle<float> plate(0.0f, 0.0f, Layout::dymoW, Layout::dymoH);

    juce::Path platePath;
    platePath.addRoundedRectangle(plate, 4.0f);
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 4, {0, 2});
    shadow.drawForPath(g, platePath);

    g.setColour(Colour::dark);
    g.fillPath(platePath);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.strokePath(platePath, juce::PathStrokeType(1.0f));

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(2.0f, 2.0f, 164.0f, 4.0f, 2.0f);

    const auto textArea = plate.reduced(4.0f, 3.0f);
    drawEmbossedTrackedText(g, "TAPEROT", dymoFont(), dymoTracking, textArea);

    g.restoreState();
}
