#include "SectionPanel.h"
#include "TapeRotTheme.h"

using namespace TapeRotTheme;

SectionPanel::SectionPanel()
{
    setInterceptsMouseClicks(false, false);
    generateSpeckleImage();
}

void SectionPanel::generateSpeckleImage()
{
    speckleImage = juce::Image(juce::Image::ARGB, (int) Layout::canvasWidth, (int) Layout::canvasHeight, true);
    juce::Graphics g(speckleImage);

    juce::Random random(Layout::speckleSeed);
    for (int i = 0; i < Layout::speckleCount; ++i)
    {
        const float x = 20.0f + random.nextFloat() * (Layout::canvasWidth - 40.0f);
        const float y = 122.0f + random.nextFloat() * (Layout::canvasHeight - 122.0f - 20.0f);
        const float r = 0.4f + random.nextFloat() * 0.5f;

        g.setColour(Colour::mutedLabel.withAlpha(0.08f));
        g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
    }
}

void SectionPanel::paint(juce::Graphics& g)
{
    paintBezelAndPanel(g);
    paintHeader(g);
    g.drawImageAt(speckleImage, 0, 0);
    paintSectionLabelsAndDividers(g);
    paintKnobLabels(g);
    paintSwitchLabels(g);
    paintFailureDotLabels(g);
    paintNewControlLabels(g);
    paintCounterHousing(g);
    paintFailLabel(g);
    paintScrews(g);
    paintVersionText(g);
}

void SectionPanel::paintBezelAndPanel(juce::Graphics& g)
{
    juce::Rectangle<float> outer(Layout::bezelOuterX, Layout::bezelOuterY, Layout::bezelOuterW, Layout::bezelOuterH);
    g.setColour(Colour::bezelFill);
    g.fillRoundedRectangle(outer, Layout::bezelOuterRadius);
    g.setColour(Colour::bezelStroke);
    g.drawRoundedRectangle(outer, Layout::bezelOuterRadius, 2.0f);

    juce::Rectangle<float> panel(Layout::panelX, Layout::panelY, Layout::panelW, Layout::panelH);
    juce::ColourGradient panelGradient(Colour::panelTop, panel.getX(), panel.getY(),
                                        Colour::panelBottom, panel.getX(), panel.getBottom(), false);
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(panel, Layout::panelRadius);
}

void SectionPanel::paintHeader(juce::Graphics& g)
{
    juce::Rectangle<float> header(Layout::headerX, Layout::headerY, Layout::headerW, Layout::headerH);
    juce::ColourGradient headerGradient(Colour::headerTop, header.getX(), header.getY(),
                                         Colour::headerBottom, header.getX(), header.getBottom(), false);
    g.setGradientFill(headerGradient);
    g.fillRoundedRectangle(header, Layout::panelRadius);
    // Square off the header's bottom corners so it butts flush against the body.
    g.fillRect(juce::Rectangle<float>(Layout::headerX, Layout::headerSeparatorY - 10.0f, Layout::headerW, 10.0f));

    g.setColour(juce::Colour(0xFF0E0C09));
    g.drawLine(Layout::headerX, Layout::headerSeparatorY, Layout::headerX + Layout::headerW,
               Layout::headerSeparatorY, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawLine(Layout::headerX, Layout::headerSeparatorY + 1.5f, Layout::headerX + Layout::headerW,
               Layout::headerSeparatorY + 1.5f, 1.0f);
}

void SectionPanel::paintSectionLabelsAndDividers(juce::Graphics& g)
{
    const auto font = sectionLabelFont();
    auto drawLabel = [&](float x, const char* text)
    {
        drawTrackedText(g, text, font, sectionLabelTracking,
                         juce::Rectangle<float>(x - 100.0f, Layout::sectionLabelY - 12.0f, 200.0f, 16.0f),
                         juce::Justification::centred, Colour::sectionLabel);
    };

    drawLabel(Layout::inputLabelX, "INPUT");
    drawLabel(Layout::transportLabelX, "TRANSPORT");
    drawLabel(Layout::machineLabelX, "MACHINE");
    drawLabel(Layout::decayLabelX, "DECAY");
    drawLabel(Layout::outputLabelX, "OUTPUT");

    for (float x : Layout::dividerX)
    {
        g.setColour(Colour::divider);
        g.drawLine(x, Layout::dividerTop, x, Layout::dividerBottom, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawLine(x + 1.2f, Layout::dividerTop, x + 1.2f, Layout::dividerBottom, 1.0f);
    }
}

void SectionPanel::paintKnobLabels(juce::Graphics& g)
{
    const auto font = knobLabelFont();
    for (const auto& knob : Layout::knobs)
    {
        // MODEL gets the ModelReadout component instead of a generic label - see the SVG (its
        // knob has no trailing label text, unlike every other knob).
        if (juce::String(knob.paramID) == "model")
            continue;

        drawTrackedText(g, knob.label, font, knobLabelTracking,
                         juce::Rectangle<float>(knob.x - 60.0f, Layout::knobCentreY + Layout::knobLabelOffsetY - 8.0f,
                                                 120.0f, 16.0f),
                         juce::Justification::centred, Colour::ink);
    }
}

void SectionPanel::paintSwitchLabels(juce::Graphics& g)
{
    auto drawSwitchText = [&](float trackX, const char* label, const juce::String& caption)
    {
        const float centreX = trackX + Layout::switchW * 0.5f;
        drawTrackedText(g, label, switchLabelFont(), switchLabelTracking,
                         juce::Rectangle<float>(centreX - 60.0f, Layout::switchLabelY - 10.0f, 120.0f, 14.0f),
                         juce::Justification::centred, Colour::ink);
        drawTrackedText(g, caption, switchCaptionFont(), switchCaptionTracking,
                         juce::Rectangle<float>(centreX - 70.0f, Layout::switchCaptionY - 9.0f, 140.0f, 12.0f),
                         juce::Justification::centred, Colour::mutedLabel);
    };

    // juce::String's const char* constructor treats bytes as Latin-1, not UTF-8, so the
    // middle-dot separator must be decoded explicitly via fromUTF8 to avoid mojibake. Dots have
    // no surrounding spaces, per the updated SVG's tighter caption typography.
    drawSwitchText(Layout::switchModeSwitchX, "SWITCH", juce::String::fromUTF8("FADE\xC2\xB7" "CLUNK"));
    drawSwitchText(Layout::noiseSwitchX, "NOISE", juce::String::fromUTF8("TAPE\xC2\xB7" "VCR\xC2\xB7" "DUST"));
    drawSwitchText(Layout::humSwitchX, "HUM", juce::String::fromUTF8("OFF\xC2\xB7ON"));
    drawSwitchText(Layout::spreadSwitchX, "SPREAD", juce::String::fromUTF8("LINKED\xC2\xB7STEREO"));
}

void SectionPanel::paintFailureDotLabels(juce::Graphics& g)
{
    const auto font = dotLabelFont();
    for (size_t i = 0; i < Layout::failureDots.size(); ++i)
    {
        const float x = Layout::failureDotFirstX + (float) i * Layout::failureDotSpacing;
        drawTrackedText(g, Layout::failureDots[i].label, font, dotLabelTracking,
                         juce::Rectangle<float>(x - 20.0f, Layout::failureDotLabelY - 8.0f, 40.0f, 12.0f),
                         juce::Justification::centred, Colour::mutedLabel);
    }
}

void SectionPanel::paintNewControlLabels(juce::Graphics& g)
{
    const auto font = dotLabelFont();
    constexpr float labelY = 366.0f;

    auto drawBelow = [&](float x, const char* text)
    {
        drawTrackedText(g, text, font, dotLabelTracking,
                         juce::Rectangle<float>(x - 16.0f, labelY - 6.0f, 32.0f, 12.0f),
                         juce::Justification::centred, Colour::mutedLabel);
    };

    drawBelow(Layout::stopButtonX, "STP");
    drawBelow(Layout::filterButtonX, "FLT");
    drawBelow(Layout::failButtonX, "FAI");

    drawBelow(Layout::genSelectorCentreX, "GEN");

    drawBelow(Layout::lpKnobX, "LP");
    drawBelow(Layout::rampKnobX, "RAMP");
    drawBelow(Layout::hpKnobX, "HP");
}

void SectionPanel::paintCounterHousing(juce::Graphics& g)
{
    juce::Rectangle<float> housing(Layout::counterHousingX, Layout::counterHousingY,
                                    Layout::counterHousingW, Layout::counterHousingH);
    g.setColour(Colour::counterHousingFill);
    g.fillRoundedRectangle(housing, Layout::counterHousingRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(housing, Layout::counterHousingRadius, 1.5f);

    for (int i = 0; i < 3; ++i)
    {
        const float cellX = Layout::digitCellFirstX + (float) i * (Layout::digitCellW + Layout::digitCellGap);
        juce::Rectangle<float> cell(cellX, Layout::digitCellY, Layout::digitCellW, Layout::digitCellH);
        g.setColour(Colour::digitCellFill);
        g.fillRoundedRectangle(cell, Layout::digitCellRadius);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(cell, Layout::digitCellRadius, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(cellX + 2.0f, Layout::digitCellY + 6.0f, cellX + Layout::digitCellW - 2.0f,
                   Layout::digitCellY + 6.0f, 1.0f);
        g.drawLine(cellX + 2.0f, Layout::digitCellY + 32.0f, cellX + Layout::digitCellW - 2.0f,
                   Layout::digitCellY + 32.0f, 1.0f);
    }

    drawTrackedText(g, "GEN COUNT", microLabelFont(), microLabelTracking,
                     juce::Rectangle<float>(Layout::counterHousingX - 20.0f, 94.0f,
                                             Layout::counterHousingW + 40.0f, 14.0f),
                     juce::Justification::centred, Colour::mutedLabel);

    g.setColour(Colour::resetStubFill);
    g.fillEllipse(Layout::resetStubX - Layout::resetStubRadius, Layout::resetStubY - Layout::resetStubRadius,
                  Layout::resetStubRadius * 2.0f, Layout::resetStubRadius * 2.0f);
    g.setColour(Colour::resetStubStroke);
    g.drawEllipse(Layout::resetStubX - Layout::resetStubRadius, Layout::resetStubY - Layout::resetStubRadius,
                  Layout::resetStubRadius * 2.0f, Layout::resetStubRadius * 2.0f, 1.0f);
}

void SectionPanel::paintFailLabel(juce::Graphics& g)
{
    drawTrackedText(g, "FAIL", microLabelFont(), microLabelTracking,
                     juce::Rectangle<float>(Layout::lampX - 40.0f, 94.0f, 80.0f, 14.0f),
                     juce::Justification::centred, Colour::mutedLabel);
}

void SectionPanel::paintScrews(juce::Graphics& g)
{
    juce::Random random(Layout::screwSeed);
    for (const auto& pos : Layout::screwPositions)
    {
        const float cx = pos.first, cy = pos.second;
        g.setColour(Colour::screwFill);
        g.fillEllipse(cx - Layout::screwRadius, cy - Layout::screwRadius,
                      Layout::screwRadius * 2.0f, Layout::screwRadius * 2.0f);
        g.setColour(Colour::screwStroke);
        g.drawEllipse(cx - Layout::screwRadius, cy - Layout::screwRadius,
                      Layout::screwRadius * 2.0f, Layout::screwRadius * 2.0f, 1.5f);

        const float angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        const float slotHalfLength = Layout::screwRadius * 0.85f;
        const auto d = juce::Point<float>(std::cos(angle), std::sin(angle));
        g.drawLine(cx - d.x * slotHalfLength, cy - d.y * slotHalfLength,
                   cx + d.x * slotHalfLength, cy + d.y * slotHalfLength, 1.5f);
    }
}

void SectionPanel::paintVersionText(juce::Graphics& g)
{
    drawTrackedText(g, juce::String::fromUTF8("TAPEROT \xC2\xB7 v") + JucePlugin_VersionString
                         + juce::String::fromUTF8(" \xC2\xB7 STEREO TAPE DEGRADATION"),
                     versionFont(), versionTracking,
                     juce::Rectangle<float>(Layout::versionTextX - 250.0f, Layout::versionTextY - 10.0f, 500.0f, 14.0f),
                     juce::Justification::centred, Colour::versionText);
}
