#include "ProgramHeader.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    // Hit areas are the plate rects from spec section 4, NOT the sprite rects - the 3px shadow
    // bleed around each sprite must not be clickable.
    const auto& saveBounds   = TapeRotTheme::Layout::saveHitArea;
    const auto& deleteBounds = TapeRotTheme::Layout::deleteHitArea;
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
    return Layout::programLcd.contains(p) || saveBounds.contains(p) || deleteBounds.contains(p);
}

void ProgramHeader::mouseDown(const juce::MouseEvent& e)
{
    // The glass is the selector. The plate prints a dropdown chevron at its right-hand end
    // (delta v1.0.3), so the whole cell has to be clickable or that chevron is a lie.
    if (Layout::programLcd.contains(e.position))
    {
        showProgramMenu();
        return;
    }

    if (saveBounds.contains(e.position))
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

    if (deleteBounds.contains(e.position))
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

void ProgramHeader::mouseMove(const juce::MouseEvent& e)
{
    // Position-dependent, so it can't be a one-off setMouseCursor in the constructor: this
    // component spans the whole canvas and only these three cells are clickable.
    const bool saveLive   = namingMode || processorRef.isProgramModified();
    const bool deleteLive = namingMode || deleteEnabled();

    const bool clickable = Layout::programLcd.contains(e.position)
                        || (saveBounds.contains(e.position) && saveLive)
                        || (deleteBounds.contains(e.position) && deleteLive);

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
            menu.addSectionHeader("Factory");
        }

        // The User group is absent entirely when empty, header included, rather than shown blank.
        if (id.bank == ProgramBank::user && ! std::exchange(userHeaderDone, true))
        {
            menu.addSeparator();
            menu.addSectionHeader("User");
        }

        menu.addItem(rowId(i), processorRef.displayLabelFor(id), true, id == current);
    }

    // The menu hangs off the LCD and reads as an extension of it, so it takes the glass's width
    // rather than sizing itself to the longest Program name. localAreaToGlobal already carries the
    // editor's scale transform, so this stays right at every window size.
    const auto glassOnScreen = localAreaToGlobal(Layout::programLcd.getSmallestIntegerContainer());
    const auto glass = Layout::programLcd.getSmallestIntegerContainer();

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

    stopTimer();
    editingParam = paramId;
    repaint();
}

void ProgramHeader::releaseParameter()
{
    startTimer(Layout::lcdRevertMs);
}

void ProgramHeader::timerCallback()
{
    stopTimer();
    editingParam = {};
    repaint();
}

juce::String ProgramHeader::describe(const juce::String& paramId) const
{
    auto* p = processorRef.apvts.getParameter(paramId);

    if (p == nullptr)
        return {};

    // Text goes through the parameter's own getText(), so units and precision match what the host
    // shows for the same parameter - there is no second formatting convention to keep in sync.
    const auto name = p->getName(24).toUpperCase();
    return name + ": " + p->getText(p->getValue(), 0).toUpperCase();
}

juce::String ProgramHeader::lcdText() const
{
    if (editingParam.isNotEmpty())
    {
        const auto d = describe(editingParam);

        if (d.isNotEmpty())
            return d;
    }

    const auto id = processorRef.getCurrentProgramId();

    // An identifier the session named but the bank no longer has - a Factory Program dropped in a
    // later version, or a user file deleted. The VALUES are correct and untouched; only the name is
    // unknown, so the panel says so with a "?" rather than pretending to be some other Program.
    if (id.bank == ProgramBank::unresolved)
        return id.displayName.toUpperCase() + "?";

    return processorRef.displayLabelFor(id).toUpperCase();
}

void ProgramHeader::paint(juce::Graphics& g)
{
    const auto currentId = processorRef.getCurrentProgramId();
    const auto lcdFont = Font::of(Layout::lcdTextSize);

    // --- PROGRAM: one glass, with the bank chip as a single field that switches its text --------
    const auto glass = Layout::programLcd;
    // Both fields are measured off the plate's own divider rather than guessed insets.
    const auto bankArea = glass.withRight(Layout::lcdDivider);
    // Stops short of the baked dropdown chevron - a long User Program name would otherwise run
    // straight under it.
    const auto nameArea = glass.withLeft(Layout::lcdDivider + Layout::lcdNameInset)
                               .withRight(Layout::lcdChevron.getX() - 8.0f);

    // Naming always produces a User Program, so the chip says so from the first keystroke.
    // **An em-dash where the Program is in neither bank** - INIT, or an unresolved identifier.
    // Printing FACT or USER there would name a bank the Program is not in.
    const bool onInit   = ! namingMode && (currentId.bank == ProgramBank::init
                                            || currentId.bank == ProgramBank::unresolved);
    const bool userBank = namingMode || currentId.bank == ProgramBank::user;

    Text::drawTracked(g, onInit ? Text::emDash() : (userBank ? "USER" : "FACT"), lcdFont,
                      Layout::lcdTracking, bankArea, juce::Justification::centred,
                      onInit ? Colour::lcdText.withAlpha(0.42f) : Colour::lcdText);

    if (namingMode)
    {
        // Block caret, 1 s period at 50% duty. The editor repaints this component at 60 Hz for the
        // meters, so the blink needs no timer of its own.
        const bool caretOn = (juce::Time::getMillisecondCounter() % 1000) < 500;
        const auto caret = juce::String::charToString((juce::juce_wchar) 0x2588);   // U+2588

        Text::drawTracked(g, typedName + (caretOn ? caret : juce::String()), lcdFont,
                          Layout::lcdTracking, nameArea, juce::Justification::left, Colour::lcdText);
    }
    else
    {
        Text::drawTracked(g, lcdText(), lcdFont, Layout::lcdTracking, nameArea,
                          juce::Justification::left, Colour::lcdText);
    }

    // --- SAVE / DELETE / CANCEL -----------------------------------------------------------------
    // The plate leaves both frames empty as of delta v1.0.7, so every state is a sprite. SAVE stays
    // dark until a parameter differs from the Program on display, so it never invites a save that
    // would do nothing; DELETE is live only on a User Program, and becomes CANCEL while a name is
    // being typed.
    const auto blitButton = [&g](const juce::Image& img, juce::Point<float> topLeft)
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(img, juce::Rectangle<float>(topLeft.x, topLeft.y,
                                                Layout::headerButtonW, Layout::headerButtonH));
    };

    blitButton(Asset::saveButton(namingMode || processorRef.isProgramModified()),
               Layout::saveSpriteTopLeft);

    blitButton(Asset::deleteButton(namingMode ? Asset::DeleteFace::cancel
                                   : (deleteEnabled() ? Asset::DeleteFace::enabled
                                                      : Asset::DeleteFace::disabled)),
               Layout::deleteSpriteTopLeft);

    // --- MODEL readout --------------------------------------------------------------------------
    if (auto* modelParam = processorRef.apvts.getParameter("model"))
        Text::drawTracked(g, modelParam->getText(modelParam->getValue(), 0).toUpperCase(),
                          Font::of(Layout::modelTextSize), Layout::modelTracking,
                          Layout::modelReadout, juce::Justification::centred, Colour::lcdText);

    // --- IN / OUT -------------------------------------------------------------------------------
    const auto meterFont = Font::of(Layout::meterTextSize);
    const auto level = [&g, &meterFont](juce::Rectangle<float> r, float db)
    {
        Text::drawTracked(g, juce::String(db, 1), meterFont, 0.0f, r,
                          juce::Justification::centred, Colour::meterNumerals);
    };

    level(Layout::inMeter, processorRef.getInputLevelDb());
    level(Layout::outMeter, processorRef.getOutputLevelDb());
}
