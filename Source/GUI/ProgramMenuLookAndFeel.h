#pragma once

#include <nf/MenuMetrics.h>

#include "TapeRotTheme.h"

/**
    Dresses the Program dropdown in the panel's own language.

    The menu is the one piece of this GUI that cannot be a bitmap: its size depends on how many
    User Programs exist, so it has to be drawn. Left to JUCE's default it renders as a system-grey
    list that belongs to a different product entirely - the glass, the amber and the mono face all
    stop at the edge of the LCD.

    So this paints the menu as an extension of the PROGRAM glass: the same near-black fill, the
    same amber ink, Share Tech Mono throughout, and the muted amber rule the plate uses between the
    bank chip and the program name. Nothing here invents a colour - every value comes from
    TapeRotTheme::Colour.
*/
class ProgramMenuLookAndFeel final : public nf::MenuMetricsLookAndFeel
{
public:
    ProgramMenuLookAndFeel()
        : nf::MenuMetricsLookAndFeel (metrics())
    {
        // Covers the few bits JUCE draws without asking us first (the drop shadow's backdrop).
        setColour (juce::PopupMenu::backgroundColourId, glass);
        setColour (juce::PopupMenu::textColourId, TapeRotTheme::Colour::lcdText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   TapeRotTheme::Colour::accent.withAlpha (0.20f));
        setColour (juce::PopupMenu::highlightedTextColourId, TapeRotTheme::Colour::meterNumerals);
    }

protected:
    /** Plain, untracked measurement - this panel draws its rows with a plain run. */
    float measureMenuItemText (const juce::String& text) override
    {
        return juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), text);
    }

public:
    juce::Font getPopupMenuFont() override
    {
        return TapeRotTheme::Font::monoAt (itemTextSize);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        const juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (glass);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (rule);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }


    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override
    {
        juce::ignoreUnused (hasSubMenu, shortcutKeyText, icon, textColourToUse);

        if (isSeparator)
        {
            auto line = area.toFloat().reduced (8.0f, 0.0f).withHeight (1.0f)
                            .withY (area.toFloat().getCentreY());
            g.setColour (rule);
            g.fillRect (line);
            return;
        }

        auto r = area.toFloat().reduced (3.0f, 1.0f);

        if (isHighlighted && isActive)
        {
            g.setColour (TapeRotTheme::Colour::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 2.0f);
        }

        // A filled amber pip rather than a tick glyph: the panel has no checkmark idiom anywhere,
        // but it is full of lit lamps and segments, so this reads as "this one is lit".
        if (isTicked)
        {
            const float d = 5.0f;
            g.setColour (TapeRotTheme::Colour::accent);
            g.fillEllipse (r.getX() + 7.0f, r.getCentreY() - d * 0.5f, d, d);
        }

        auto ink = isActive ? TapeRotTheme::Colour::lcdText
                            : TapeRotTheme::Colour::lcdText.withAlpha (0.35f);

        if (isHighlighted && isActive)
            ink = TapeRotTheme::Colour::meterNumerals;

        TapeRotTheme::Text::drawTracked (g, text, getPopupMenuFont(), tracking,
                                         r.withTrimmedLeft ((float) tickColumn),
                                         juce::Justification::left, ink);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        auto r = area.toFloat().reduced (3.0f, 0.0f);

        // **Drawn as authored, never re-cased.** The caption is authored FACTORY/USER at the
        // addSectionHeader call in ProgramHeader.cpp, per BRAND.md's rule that case belongs at
        // the source. It held a .toUpperCase() here until 2026-08-13. Nothing else reads these
        // two strings today, which is a fact about today rather than a property of the code:
        // the moment a caption comes from data, the site that re-cases it is the site that
        // gets it wrong. Re-arguing the exception each time costs more than the rule.
        TapeRotTheme::Text::drawTracked (g, sectionName,
                                         TapeRotTheme::Font::monoAt (headerTextSize), headerTracking,
                                         r.withTrimmedLeft ((float) tickColumn),
                                         juce::Justification::left,
                                         // **Opaque.** BRAND.md permits opacity for STATE and
                                         // forbids it for HIERARCHY, and a section header is
                                         // hierarchy - not disabled, inactive or not-current. The
                                         // smaller size and wider tracking against the rows already
                                         // carry it; the .55 was doing the same job twice, the
                                         // second time by the mechanism the brand book rules out.
                                         TapeRotTheme::Colour::lcdText);

        g.setColour (rule);
        g.fillRect (r.reduced (5.0f, 0.0f).withHeight (1.0f).withY (r.getBottom() - 1.0f));
    }

private:
    /** The PROGRAM glass itself, so the menu reads as the same pane of dark plastic. */
    const juce::Colour glass { 0xFF1A1712 };
    /** The muted amber the plate uses for the bank-chip divider. */
    const juce::Colour rule  { 0xFF614624 };

    static constexpr float itemTextSize   = 15.0f;
    static constexpr float tracking       = 1.0f;
    static constexpr float headerTextSize = 11.0f;
    static constexpr float headerTracking = 1.6f;
    /** **This panel's dropdown sizes, stated rather than inherited.**

        `sectionHeaderHeight` is 36 because that is what this panel has ALWAYS drawn - JUCE's
        `LookAndFeel_V2` sizes a section caption at the row height plus half again, and this casting
        took that default by omission. It is written down now so it is a number somebody can see and
        argue with; **it is deliberately not changed here**, because altering it moves every row
        below FACTORY and that is the designers' call, not a refactor's. Elmer designed 19 against
        22px rows and is the only casting that chose.

        The row height is pinned and never grows to the platform's standard item height - see
        nf::MenuMetricsLookAndFeel. */
    static nf::MenuMetrics metrics()
    {
        nf::MenuMetrics m;
        m.rowHeight = rowHeight;
        // **The construction, not a literal.** padding + the line box this panel's own caption
        // type produces - nf::captionHeight. It comes out 19 here, the same as Elmer's, and that
        // is a COINCIDENCE: Share Tech Mono's line box is 1.127 em against Plex Mono's 1.300, and
        // 11px of one plus 9px of the other happen to meet once the padding is added. Writing 19
        // would lose the rule and break it silently at the first type change - which is how this
        // caption came to inherit JUCE's rowHeight + rowHeight / 2 in the first place.
        //
        // 3px above and 4px below is the suite's ADOPTED default, not a derived figure: only
        // Elmer and Reflect-84 have designer-authored caption padding and they disagree (3/4
        // against 9/4), so there is no suite constant to derive. See BRAND.md.
        m.sectionHeaderHeight = nf::captionHeight (TapeRotTheme::Font::monoAt (headerTextSize), 3, 4);
        m.separatorHeight = 9;
        m.leadingColumn = tickColumn;
        m.horizontalPadding = 26;
        return m;
    }

    static constexpr int   rowHeight      = 24;
    static constexpr int   tickColumn     = 22;
};
