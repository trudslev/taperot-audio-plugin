#include "ProgramHeader.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    /*  The hit rects ARE the drawn cells now, and that is the change worth naming.

        Revision 1 kept a separate pair of "plate rects" because each sprite carried a 3 px shadow
        bleed that must not be clickable - so there were two rectangles per button, one drawn and
        one clicked, and nothing checked they agreed. With the faces drawn there is no bleed and no
        second rectangle: `nf::HeaderGeometry::saveButton()` is both.  */
    inline juce::Rectangle<float> saveBounds()   { return TapeRotTheme::Header::saveButton(); }
    inline juce::Rectangle<float> deleteBounds() { return TapeRotTheme::Header::deleteButton(); }
}

ProgramHeader::ProgramHeader(TapeRotAudioProcessor& p) : processorRef(p)
{
    setBounds(0, 0, (int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setInterceptsMouseClicks(true, false);
    setWantsKeyboardFocus(true);          // naming a User Program is typed straight into the LCD
}

void ProgramHeader::enterNamingMode()
{
    namingMode = true;
    typedName.clear();
    grabKeyboardFocus();
    repaint();
}

void ProgramHeader::commitName()
{
    // An empty name is handled by the processor's own fallback rather than duplicated here.
    processorRef.saveUserProgram(typedName);

    namingMode = false;
    typedName.clear();
    giveAwayKeyboardFocus();
    repaint();
}

void ProgramHeader::cancelNaming()
{
    // Must not touch any parameter: knob values tweaked but not yet saved have to survive a
    // cancel, so leaving the mode is the whole of it.
    namingMode = false;
    typedName.clear();
    giveAwayKeyboardFocus();
    repaint();
}

bool ProgramHeader::keyPressed(const juce::KeyPress& key)
{
    if (! namingMode)
        return false;

    if (key.isKeyCode(juce::KeyPress::returnKey))   { commitName();  return true; }
    if (key.isKeyCode(juce::KeyPress::escapeKey))   { cancelNaming(); return true; }

    if (key.isKeyCode(juce::KeyPress::backspaceKey))
    {
        if (typedName.isNotEmpty())
            typedName = typedName.dropLastCharacters(1);
        repaint();
        return true;
    }

    const juce::juce_wchar c = key.getTextCharacter();

    if (c >= 32 && c != 127 && typedName.length() < maxProgramNameLength)
    {
        // Uppercase throughout: the LCD has no lowercase idiom anywhere on this panel.
        typedName += juce::String::charToString(c).toUpperCase();
        repaint();
        return true;
    }

    return false;
}

bool ProgramHeader::hitTest(int x, int y)
{
    const juce::Point<float> p((float) x, (float) y);
    return Header::lcd().contains(p) || saveBounds().contains(p) || deleteBounds().contains(p);
}

void ProgramHeader::mouseDown(const juce::MouseEvent& e)
{
    // The glass is the selector. The plate prints a dropdown chevron at its right-hand end
    // (delta v1.0.3), so the whole cell has to be clickable or that chevron is a lie.
    //
    // **Except while naming**, when the glass belongs to the name field. Opening the list then
    // applies a Program underneath a half-typed name, leaving a stale field sitting over a Program
    // that never had it - and the field stays open, so the next keystroke edits a name for the
    // wrong Program entirely. The other five castings all guard this; Fifth Member's
    // ProgramHeader.cpp names it as TapeRot's bug and deliberately does not copy it.
    if (Header::lcd().contains(e.position) && ! namingMode)
    {
        showProgramMenu();
        return;
    }

    if (saveBounds().contains(e.position))
    {
        // SAVE always creates a new Program and never overwrites, so there is no "New" action.
        // First press opens the name field; the second commits what was typed, so the button works
        // for someone who never touches the keyboard beyond typing the name.
        if (namingMode)
            commitName();
        else if (processorRef.isProgramModified())
            enterNamingMode();
        return;
    }

    if (deleteBounds().contains(e.position))
    {
        // While naming, this button IS cancel - it wears the CANCEL sprite, and Escape does the
        // same thing from the keyboard.
        if (namingMode)
            cancelNaming();
        else if (deleteEnabled())
            processorRef.deleteUserProgram(processorRef.getCurrentProgramId());

        repaint();
    }
}

void ProgramHeader::focusLost(FocusChangeType)
{
    // **Losing focus cancels naming.** Half-typed names must not survive a click elsewhere on the
    // panel: the field would stay open over a Program the user has moved on from, and the next
    // keystroke would edit a name for the wrong one. Cancel touches no parameter, so whatever was
    // tweaked before SAVE survives - leaving the mode is the whole of it.
    //
    // Reflect-84, Fifth Member and Elmer already did this; TapeRot, Gatecrasher and Chorus-60 did
    // not, which is drift rather than a decision - no casting's notes ever argued for keeping it.
    if (namingMode)
        cancelNaming();
}

void ProgramHeader::mouseMove(const juce::MouseEvent& e)
{
    // Position-dependent, so it can't be a one-off setMouseCursor in the constructor: this
    // component spans the whole canvas and only these three cells are clickable.
    const bool saveLive   = namingMode || processorRef.isProgramModified();
    const bool deleteLive = namingMode || deleteEnabled();

    const bool clickable = Header::lcd().contains(e.position)
                        || (saveBounds().contains(e.position) && saveLive)
                        || (deleteBounds().contains(e.position) && deleteLive);

    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
}

bool ProgramHeader::deleteEnabled() const
{
    // Only a User Program can be deleted. INIT and an unresolved id are not stored things, and a
    // Factory Program is read-only.
    return processorRef.getCurrentProgramId().bank == ProgramBank::user;
}

void ProgramHeader::showProgramMenu()
{
    const auto current = processorRef.getCurrentProgramId();

    // **Row IDs are positions in THIS menu, not Program indices.** PopupMenu needs an int per row
    // and reserves 0 for "dismissed"; the callback maps the row straight back to the ProgramId it
    // was built from, so no Program is ever addressed by a bank position here.
    menuRows = processorRef.listPrograms();

    juce::PopupMenu menu;
    menu.setLookAndFeel(&menuLookAndFeel);

    const auto rowId = [](size_t i) { return (int) i + 1; };

    bool factoryHeaderDone = false;
    bool userHeaderDone = false;

    for (size_t i = 0; i < menuRows.size(); ++i)
    {
        const auto& id = menuRows[i];

        // INIT first, unnumbered, above the Factory group with a divider beneath it.
        if (id.bank == ProgramBank::factory && ! std::exchange(factoryHeaderDone, true))
        {
            menu.addSeparator();
            menu.addSectionHeader("FACTORY");
        }

        if (id.bank == ProgramBank::user && ! std::exchange(userHeaderDone, true))
        {
            menu.addSeparator();
            menu.addSectionHeader("USER");
        }

        menu.addItem(rowId(i), processorRef.displayLabelFor(id), true, id == current);
    }

    // **The USER section is always shown, with a placeholder when the bank is empty.** An absent
    // section is ambiguous between "nothing saved yet" and "this plugin does not do that", and the
    // player cannot tell which without saving something to find out. Reflect-84 had it first.
    if (! userHeaderDone)
    {
        menu.addSeparator();
        menu.addSectionHeader("USER");
        menu.addItem(-1, Text::emDash() + " none saved " + Text::emDash(), false, false);
    }

    // The menu hangs off the LCD and reads as an extension of it, so it takes the glass's width
    // rather than sizing itself to the longest Program name. localAreaToGlobal already carries the
    // editor's scale transform, so this stays right at every window size.
    const auto glassOnScreen = localAreaToGlobal(Header::lcd().getSmallestIntegerContainer());
    const auto glass = Header::lcd().getSmallestIntegerContainer();

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(this)
                       .withTargetScreenArea(glassOnScreen)
                       .withMaximumNumColumns(1);

    if (menuParent != nullptr)
    {
        // The list is laid out INSIDE menuHost rather than as its own desktop window. JUCE fits a
        // menu to its parent area, so an area running from the glass's bottom edge to the panel's
        // gives both guarantees at once: the top cannot move and the height cannot exceed the
        // panel. A bank too long to fit scrolls. See ../../CLAUDE.md, "The Program dropdown".
        //
        // Anchor to a 1px strip on the glass's bottom EDGE, not the glass. With a parent, JUCE
        // first does constrainedWithin(parentArea), which slides the whole 40px glass down into
        // the host before measuring and opens the list 40px too low. 1px and not zero: a
        // zero-height rectangle is isEmpty(), which drops the list out of align-to-rectangle into
        // the sideways placement meant for submenus.
        const juce::Rectangle<int> anchor { glass.getX(), menuAnchorY() - 1, glass.getWidth(), 1 };

        options = options.withTargetScreenArea(localAreaToGlobal(anchor))
                         .withParentComponent(menuParent)
                         .withMinimumWidth(glass.getWidth());
    }
    else
    {
        options = options.withMinimumWidth(glassOnScreen.getWidth());
    }

    menu.showMenuAsync(options,
                       [safeThis = juce::Component::SafePointer<ProgramHeader>(this)](int result)
                       {
                           if (safeThis == nullptr || result == 0)
                               return;

                           // requestProgramChange defers through the processor's AsyncUpdater, so
                           // the repaint waits for the apply rather than happening here.
                           const auto row = (size_t) (result - 1);

                           if (row < safeThis->menuRows.size())
                               safeThis->processorRef.requestProgramChange(safeThis->menuRows[row]);
                       });
}

void ProgramHeader::showParameter(const juce::String& paramId)
{
    if (namingMode)
        return;           // the glass belongs to the name field until it is committed or cancelled

    auto* param = processorRef.apvts.getParameter(paramId);

    if (param == nullptr)
        return;

    stopTimer();

    // Straight through nf::describeParameter, which is straight through the parameter's own getText
    // and getLabel - so the LCD and the host cannot disagree about what a control reads. That
    // guarantee is the whole reason the string moved to core; the case rule this panel wants is
    // stated in TapeRotTheme::Runtime::readoutFormat() rather than hand-written here.
    readout.show(nf::describeParameter(*param, TapeRotTheme::Runtime::readoutFormat()));
    repaint();
}

void ProgramHeader::releaseParameter()
{
    // A one-shot Timer rather than polling, which is this casting's own choice and stays. The
    // DEADLINE is core's - readout.revertMs() is the suite's 900 ms, where this panel carried 1100.
    readout.release(juce::Time::getMillisecondCounter());
    startTimer(readout.revertMs());
}

void ProgramHeader::timerCallback()
{
    stopTimer();
    readout.suppress();
    repaint();
}

juce::String ProgramHeader::lcdText() const
{
    if (const auto takeover = readout.textAt(juce::Time::getMillisecondCounter());
        takeover.isNotEmpty())
        return takeover;

    const auto id = processorRef.getCurrentProgramId();

    // An identifier the session named but the bank no longer has - a Factory Program dropped in a
    // later version, or a user file deleted. The VALUES are correct and untouched; only the name is
    // unknown, so the panel says so with a "?" rather than pretending to be some other Program.
    if (id.bank == ProgramBank::unresolved)
        return id.displayName.toUpperCase() + "?";

    // **A trailing " *" while the loaded Program has been edited**, matching Gatecrasher,
    // Chorus-60, Fifth Member and Elmer. TapeRot signalled dirty only through SAVE's enabled
    // sprite, which has to be looked for; the marker is seen at a glance, and the two read the
    // same predicate so they cannot disagree.
    // No toUpperCase() here: the bank stores its display names upper-cased, so case has exactly
    // one source. Doing it at the LCD instead made this the only site that applied it - the
    // dropdown and the host's own Program menu both read the label raw, so the same Program showed
    // as "01 WARM CASSETTE" on the glass and "01 Warm Cassette" in the list beneath it.
    return processorRef.displayLabelFor(id)
         + (processorRef.isProgramModified() ? " *" : "");
}
//==============================================================================
/*  What varies, and nothing else.

    Program identity and its dirty marker, the naming state and whatever has been typed, and
    whether the list is open. **Not the meter values** - they change on every tick and are 10 % of
    the cost, so a key carrying them would rebuild the layer 60 times a second and cache nothing.

    The live parameter readout IS in the key, and that is correct rather than an oversight: while a
    control is dragged the glass genuinely differs every frame, so the layer rebuilding per frame is
    the layer doing its job.  */
juce::String ProgramHeader::staticCacheKey() const
{
    const auto id = processorRef.getCurrentProgramId();

    return juce::String ((int) id.bank) + id.id
         + "|" + id.displayName
         + "|" + juce::String ((int) processorRef.isProgramModified())
         + "|" + juce::String ((int) namingMode) + typedName
         + "|" + juce::String ((int) menuOpen)
         + "|" + juce::String ((int) deleteEnabled())
         + "|" + readout.textAt (juce::Time::getMillisecondCounter());
}

/*  §9's Dymo strip and the wordmark that sits on it.

    **The wordmark is the one bitmap this panel ships**, and §9 states why: Impact Label Reversed is
    donationware, so the letterforms ship as artwork and the font does not. `design/fonts/ABSENT.md`
    records that as *absent by licensing, not missing* - the distinction matters because an absent
    font that is not declared looks like a delivery defect and gets "fixed" by substituting a face,
    which moves every measurement taken from the nameplate.

    The cut is 694 x 150 at 3x and it is a **44 px plate rotated -1.5 deg**: 230.2 x 44 at that
    angle bounds to 693.8 x 150.0. So the bitmap arrives pre-rotated and is blitted upright - the
    rotation is IN the artwork, and rotating it again here would double it.  */
void ProgramHeader::paintNameplate (juce::Graphics& g) const
{
    const auto zone = Header::nameplate();

    /*  The plate's own unrotated box, from which the cut's bounding box is centred. Deriving the
        blit from the plate rather than from the cut is what keeps the descriptor on the shared
        anchor: 30 + 44 + 4 = 78, which `Header`'s static_assert pins.  */
    const juce::Rectangle<float> plate (zone.getX(), zone.getY(),
                                        Header::dymoPlateW, Header::dymoPlateH);

    const float cutW = Header::dymoCutW / 3.0f;
    const float cutH = Header::dymoCutH / 3.0f;
    const auto blit = juce::Rectangle<float> (cutW, cutH).withCentre (plate.getCentre());

    const auto wordmark = juce::ImageCache::getFromMemory (BinaryData::taperotwordmark_png,
                                                           BinaryData::taperotwordmark_pngSize);

    if (wordmark.isValid())
    {
        g.drawImage (wordmark, blit, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        /*  **A visible failure rather than a silent one.** A missing wordmark that falls back to
            bare fascia reads as a design decision; this reads as a fault. Root `CLAUDE.md` records
            a prototype whose font silently failed to resolve being written up as a missing glyph on
            a shipped plate - a fallback that looks like a finding costs more than a fault does.  */
        g.setGradientFill ({ Colour::dymoTop, plate.getX(), plate.getY(),
                             Colour::dymoBottom, plate.getX(), plate.getBottom(), false });
        g.fillRoundedRectangle (plate, Header::dymoRadius);
        g.setColour (Colour::wordmarkInk);
        g.drawText ("TAPEROT", plate, juce::Justification::centred, false);
    }

    //== The function descriptor, on the SHARED anchor =======================
    Text::drawTracked (g, "TAPE DEGRADATION PROCESSOR",
                       Font::label (Type::functionDescriptor.cssPx),
                       Font::trackingPx (Type::functionDescriptor.trackingEm, Type::functionDescriptor.cssPx),
                       { zone.getX(), (float) nf::HeaderGeometry::descriptorY,
                         zone.getWidth(), (float) nf::HeaderGeometry::descriptorH },
                       juce::Justification::centredLeft, Colour::descriptorInk);

    //== The model line ======================================================
    Text::drawTracked (g, "MODEL MT-77 " + Text::middleDot() + " STEREO",
                       Font::monoAt (Type::modelLine.cssPx),
                       Font::trackingPx (Type::modelLine.trackingEm, Type::modelLine.cssPx),
                       { zone.getX(), (float) nf::HeaderGeometry::modelLineY,
                         zone.getWidth(), (float) nf::HeaderGeometry::modelLineH },
                       juce::Justification::centredLeft, Colour::modelLineInk);
}

/*  §7.1's two-legend Program button.

    Each button carries BOTH legends at all times and lights the one that applies - so SAVE/STORE
    and DELETE/CANCEL are positions, not states of one word. BRAND.md forbids the older form where
    the button relabelled itself, and the reason is legibility under the hand: a control whose
    legend changes is one the player has to read before pressing.  */
void ProgramHeader::paintProgramButton (juce::Graphics& g, juce::Rectangle<float> cell,
                                        const char* upper, const char* lower,
                                        bool upperLit, bool lowerLit) const
{
    g.setGradientFill ({ Colour::wellTop, cell.getX(), cell.getY(),
                         Colour::wellBottom, cell.getX(), cell.getBottom(), false });
    g.fillRoundedRectangle (cell, 3.0f);
    g.setColour (Colour::wellFrame);
    g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);

    const auto font = Font::label (Type::programLegend.cssPx);
    const float tracking = Font::trackingPx (Type::programLegend.trackingEm, Type::programLegend.cssPx);

    const auto row = [&] (const char* text, float y, bool isLit)
    {
        Text::drawTracked (g, text, font, tracking,
                           { cell.getX(), y, cell.getWidth(), Type::programLegend.lineBox },
                           juce::Justification::centred,
                           isLit ? Colour::lcdText : Colour::modelLineInk.withAlpha (0.55f));
    };

    row (upper, Header::legendUpperY, upperLit);
    row (lower, Header::legendLowerY, lowerLit);
}

/*  **The cached region is the header BLOCK, not the component's bounds, and the difference is two
    orders of magnitude.**

    This component spans the canvas - it has to, so the Program list can be anchored and so its
    `hitTest` can claim three cells scattered across the band - but it INKS a 1308 x 104 strip.
    Caching its bounds would allocate 1340 x 790 at device scale: 16.9 MB at 2x, blitted 60 times a
    second to deliver two meter numerals.

    Root `CLAUDE.md` records that exact trade from Chorus-60's ModScope, in as many words:
    *"caching 1340 x 812 would have been a 17 MB image to save 3 ms, which is the wrong trade and
    exactly the kind a cache makes silently."* It was made here anyway on the first pass, and what
    caught it was the CPU bar - four editor cells at 1.33-1.39x baseline. A cache that is too big
    does not fail, it just costs, which is why the bar is the only thing that sees it.  */
juce::Rectangle<int> ProgramHeader::cachedRegion()
{
    return { (int) TapeRotTheme::Header::blockX, (int) TapeRotTheme::Header::blockY,
             (int) TapeRotTheme::Header::blockW, (int) TapeRotTheme::Header::blockH };
}

void ProgramHeader::renderStaticLayer (float deviceScale, const juce::String& key)
{
    const auto region = cachedRegion().toFloat();
    staticLayer = juce::Image (juce::Image::ARGB,
                               juce::roundToInt (region.getWidth()  * deviceScale),
                               juce::roundToInt (region.getHeight() * deviceScale), true);
    ++staticBuilds;
    builtKey = key;

    juce::Graphics g (staticLayer);
    g.addTransform (juce::AffineTransform::scale (deviceScale));
    // Everything below is written in CANVAS coordinates, so the layer is translated rather than
    // every rect being rebased - which would mean two coordinate systems in one function.
    g.addTransform (juce::AffineTransform::translation (-region.getX(), -region.getY()));

    //== The header block, drawn for the first time ==========================
    /*  The block, the band, the LCD cell, the Program buttons and both meter wells are the shared
        part - every rectangle below comes from `nf::HeaderGeometry`, none is transcribed. Chorus-60
        aliased its LCD and left SAVE, DELETE and both wells as literals from a previous canvas,
        **29 px right and 29 px down**, and it was invisible for as long as the plate baked those
        faces. A literal that happens to agree with core reads exactly like an alias.  */
    {
        const juce::Rectangle<float> block (Header::blockX, Header::blockY, Header::blockW, Header::blockH);

        g.setGradientFill ({ Colour::headerTop, block.getX(), block.getY(),
                             Colour::headerBottom, block.getX(), block.getBottom(), false });
        g.fillRoundedRectangle (block, Header::blockRadius);
    }

    paintNameplate (g);

    //== The three captions on the band's caption row ========================
    {
        const auto font = Font::label (Type::switchCaption.cssPx);
        const float tracking = Font::trackingPx (0.24f, Type::switchCaption.cssPx);
        const auto caption = [&] (const char* text, juce::Rectangle<float> over, float trackEm)
        {
            Text::drawTracked (g, text, font, Font::trackingPx (trackEm, Type::switchCaption.cssPx),
                               { over.getX(), Header::captionY, over.getWidth(), Header::captionH },
                               juce::Justification::centredLeft, Colour::modelLineInk);
        };

        caption (namingMode ? "NAME PROGRAM" : "PROGRAM", Header::lcd(), 0.24f);
        caption ("IN",  Header::inWell(),  0.28f);
        caption ("OUT", Header::outWell(), 0.28f);
        juce::ignoreUnused (tracking);
    }

    //== The LCD well and both meter wells ===================================
    const auto well = [&] (juce::Rectangle<float> cell)
    {
        g.setGradientFill ({ Colour::wellTop, cell.getX(), cell.getY(),
                             Colour::wellBottom, cell.getX(), cell.getBottom(), false });
        g.fillRoundedRectangle (cell, 3.0f);
    };

    well (Header::lcd());

    //== §7.1's matrix, applied ==============================================
    {
        const bool edited = processorRef.isProgramModified();
        const bool canDelete = deleteEnabled();

        paintProgramButton (g, Header::saveButton(), "SAVE", "STORE",
                            ! namingMode && edited, namingMode);
        paintProgramButton (g, Header::deleteButton(), "DELETE", "CANCEL",
                            ! namingMode && canDelete, namingMode);
    }

    //== The LCD's own text: bank tag, name, or the parameter takeover =======
    {
        const auto lcdFont = Font::monoAt (Type::lcdValue.cssPx);
        const float tracking = Font::trackingPx (Type::lcdValue.trackingEm, Type::lcdValue.cssPx);
        const auto liveReadout = readout.textAt (juce::Time::getMillisecondCounter());

        if (liveReadout.isNotEmpty())
        {
            Text::drawTracked (g, liveReadout, lcdFont, tracking, Header::lcd().reduced (16.0f, 0.0f),
                               juce::Justification::centred, Colour::lcdText);
        }
        else
        {
            const auto id = processorRef.getCurrentProgramId();

            /*  **`nf::programBankTag` is core's and it already carries the INIT rule**, so this
                does not re-derive it: an em-dash for INIT and for an unresolved identifier, because
                both are in neither bank and either word would be a lie. Writing the branch here
                would be a second copy of a rule five siblings read from one place - which is the
                divergence this suite has measured seven times.  */
            const bool inNeitherBank = id.bank == ProgramBank::init
                                    || id.bank == ProgramBank::unresolved;

            Text::drawTracked (g, nf::programBankTag (id, namingMode), lcdFont, tracking,
                               Header::bankCell(), juce::Justification::centred,
                               inNeitherBank && ! namingMode ? Colour::lcdText.withAlpha (0.42f)
                                                             : Colour::lcdText);

            Text::drawTracked (g, lcdText(), lcdFont, tracking, Header::nameCell(),
                               juce::Justification::left, Colour::lcdText);
        }
    }

    //== §10 item 6's chevron: the shared 14 x 8 stroked path ================
    if (! namingMode && readout.textAt (juce::Time::getMillisecondCounter()).isEmpty())
    {
        const auto box = Header::chevron();
        juce::Path chevron;

        /*  Mirrored about its own centre line while the list is open rather than rotated, so the
            apex stays on one vertical axis and it reads as flipping in place instead of sliding
            sideways. Cleared in `showMenuAsync`'s callback, which JUCE also runs on a dismissal -
            without that, clicking away leaves the mark stuck.  */
        if (menuOpen)
        {
            chevron.startNewSubPath (box.getX(), box.getBottom());
            chevron.lineTo (box.getCentreX(), box.getY());
            chevron.lineTo (box.getRight(), box.getBottom());
        }
        else
        {
            chevron.startNewSubPath (box.getX(), box.getY());
            chevron.lineTo (box.getCentreX(), box.getBottom());
            chevron.lineTo (box.getRight(), box.getY());
        }

        g.setColour (Colour::lcdText);
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    well (Header::inWell());
    well (Header::outWell());
}

/*  The two meter wells always, and the block only when something in the key moved.

    The key is what the static layer is built from, so comparing against it here asks exactly the
    right question: *would a repaint draw anything different?* If not, the only pixels that can have
    changed are the two numerals, and those are 64 px cells.  */
void ProgramHeader::refresh()
{
    if (staticCacheKey() != builtKey)
    {
        repaint (cachedRegion());
        return;
    }

    repaint (TapeRotTheme::Header::inWell().getSmallestIntegerContainer());
    repaint (TapeRotTheme::Header::outWell().getSmallestIntegerContainer());
}

void ProgramHeader::paint (juce::Graphics& g)
{
    const float deviceScale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto key = staticCacheKey();

    if (! staticLayer.isValid() || builtKey != key
        || ! juce::approximatelyEqual (builtAtScale, deviceScale))
    {
        renderStaticLayer (deviceScale, key);
        builtAtScale = deviceScale;
    }

    const auto region = cachedRegion().toFloat();
    g.drawImageTransformed (staticLayer,
                            juce::AffineTransform::scale (1.0f / deviceScale)
                                .translated (region.getX(), region.getY()));

    //== The only live pixels: two meter numerals ============================
    /*  §6 measures this ink at **15.03** on the well. The format is the suite-wide ruling: floor
        sentinel, +99.9 ceiling, one decimal always, and an explicit sign decision - the widest
        string is 5 and 64 px of well holds it on all six.

        **This casting was LIVE DEFECT 2 of that ruling.** `PluginProcessor.cpp`'s
        `linear > 1.0e-5f ? 20*log10(linear) : -99.9f` has no GUI clamp at all, and 20·log10(1e-5)
        is exactly -100.0 - so a linear value just above the threshold rounds to `"-100.0"`, six
        characters, from the one casting whose GUI never clamped. The band is 0.58 % wide and a
        smoothed level crosses it **whenever audio stops**. The clamp is applied here, at the
        display, which is where the other five castings' floors already are.  */
    {
        const auto font = Font::monoAt (Type::lcdValue.cssPx);
        const float tracking = Font::trackingPx (Type::lcdValue.trackingEm, Type::lcdValue.cssPx);

        const auto level = [&] (juce::Rectangle<float> cell, float db)
        {
            const float clamped = juce::jlimit (-99.9f, 99.9f, db);
            Text::drawTracked (g, juce::String (clamped, 1), font, tracking, cell,
                               juce::Justification::centred, Colour::meterNumerals);
        };

        level (Header::inWell(),  processorRef.getInputLevelDb());
        level (Header::outWell(), processorRef.getOutputLevelDb());
    }
}
