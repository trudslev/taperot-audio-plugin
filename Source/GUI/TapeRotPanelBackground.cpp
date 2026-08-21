#include "TapeRotPanelBackground.h"

using namespace TapeRotTheme;

TapeRotPanelBackground::TapeRotPanelBackground()
{
    /*  The fascia spans the canvas and is behind everything, so it must not take a single click.
        Root `CLAUDE.md` records TapeRot's own Program dropdown, SAVE and DELETE all being signed
        off as working while completely dead, because two full-canvas components both intercepted
        clicks and the one in front swallowed everything meant for the other.  */
    setInterceptsMouseClicks (false, false);
}

void TapeRotPanelBackground::resized()
{
    staticLayer = {};
    builtAtScale = 0.0f;
}

void TapeRotPanelBackground::renderStaticLayer (float deviceScale)
{
    const auto bounds = getLocalBounds().toFloat();
    staticLayer = juce::Image (juce::Image::ARGB,
                               juce::roundToInt (bounds.getWidth()  * deviceScale),
                               juce::roundToInt (bounds.getHeight() * deviceScale), true);
    ++buildCount;

    juce::Graphics g (staticLayer);
    g.addTransform (juce::AffineTransform::scale (deviceScale));

    //== §1's fascia, bezel and inner line ===================================
    g.setGradientFill ({ Colour::fasciaTop, 0.0f, 0.0f,
                         Colour::fasciaBottom, 0.0f, bounds.getHeight(), false });
    g.fillRect (bounds);

    {
        const auto inner = bounds.reduced (Layout::bezelInset);
        g.setColour (Colour::bezel);
        g.drawRect (bounds, Layout::bezelInset);
        g.setColour (Colour::bezelLine);
        g.drawRect (inner, 1.0f);
    }

    //== §1's four thumbscrews ===============================================
    for (const auto& c : Layout::screwCentres)
    {
        const float r = Layout::screwDiameter * 0.5f;
        g.setGradientFill (Paint::sculptedFace (c, r, 0.38f, 0.30f, Colour::screwFace,
                                                Colour::screwFace.darker (0.55f)));
        g.fillEllipse (c.x - r, c.y - r, Layout::screwDiameter, Layout::screwDiameter);

        g.setColour (Colour::screwSlot);
        g.fillRect (c.x - Layout::screwSlotW * 0.5f, c.y - Layout::screwSlotH * 0.5f,
                    Layout::screwSlotW, Layout::screwSlotH);
    }

    //== §1's five section dividers ==========================================
    g.setColour (Colour::divider);
    for (const float x : Layout::dividerX)
        g.fillRect (x, Layout::dividerY, Layout::dividerW, Layout::dividerH);

    //== §2's headings, centred on their own column ==========================
    /*  §2: "The order is the signal path and it is the reorganisation this round made" - INPUT,
        MACHINE, TRANSPORT, NOISE, DECAY, OUTPUT, replacing a grouping by control type. Each heading
        centres on its SECTION rather than on the control beneath it, so a section holding two knobs
        at different pitches still reads as one column.  */
    {
        const auto font = Font::label (Type::sectionHeading.cssPx);
        const float tracking = Font::trackingPx (Type::sectionHeading.trackingEm, Type::sectionHeading.cssPx);

        for (const auto& s : Layout::sections)
            Text::drawTracked (g, s.heading, font, tracking,
                               { s.x, Layout::sectionHeadingY, s.width, Type::sectionHeading.lineBox },
                               juce::Justification::centred, Colour::panelInk);
    }

    //== The footer row ======================================================
    {
        const auto font = Font::monoAt (Readouts::footerCssPx);
        const float tracking = Font::trackingPx (Readouts::footerTrackingEm, Readouts::footerCssPx);
        const auto dot = Text::middleDot();

        Text::drawTracked (g, "MT-77 " + dot + " SN 0143", font, tracking,
                           { Readouts::footerLeftX, Readouts::footerY, 300.0f, Readouts::footerLineBox },
                           juce::Justification::left, Readouts::footerInk);

        /*  NF_VERSION_SHORT rather than a baked "v1.0". Root `CLAUDE.md` lists TapeRot as one of
            three castings that bake their version into the panel plate, so their footers read v1.0
            as artwork and need a re-cut whenever the number moves. With the plate gone that is no
            longer true here, and deriving it from PROJECT_VERSION is what stops the footer being a
            fourth thing to remember at release.  */
        Text::drawTracked (g, "TAPEROT " + dot + " v" + juce::String (NF_VERSION_SHORT), font, tracking,
                           { Readouts::footerRightCentre - 150.0f, Readouts::footerY, 300.0f,
                             Readouts::footerLineBox },
                           juce::Justification::centred, Readouts::footerInk);
    }
}

void TapeRotPanelBackground::paint (juce::Graphics& g)
{
    const float deviceScale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! staticLayer.isValid() || ! juce::approximatelyEqual (builtAtScale, deviceScale))
    {
        renderStaticLayer (deviceScale);
        builtAtScale = deviceScale;
    }

    g.drawImageTransformed (staticLayer, juce::AffineTransform::scale (1.0f / deviceScale));
}
