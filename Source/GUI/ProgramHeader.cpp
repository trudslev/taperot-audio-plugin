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
        else if (! processorRef.isFactoryProgram(processorRef.getCurrentProgram()))
            processorRef.deleteUserProgram(processorRef.getCurrentProgram());

        repaint();
    }
}

void ProgramHeader::mouseMove(const juce::MouseEvent& e)
{
    // Position-dependent, so it can't be a one-off setMouseCursor in the constructor: this
    // component spans the whole canvas and only these three cells are clickable.
    const bool saveLive   = namingMode || processorRef.isProgramModified();
    const bool deleteLive = namingMode
                         || ! processorRef.isFactoryProgram(processorRef.getCurrentProgram());

    const bool clickable = Layout::programLcd.contains(e.position)
                        || (saveBounds.contains(e.position) && saveLive)
                        || (deleteBounds.contains(e.position) && deleteLive);

    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
}

void ProgramHeader::showProgramMenu()
{
    const int numPrograms = processorRef.getNumPrograms();
    const int currentIndex = processorRef.getCurrentProgram();

    // Item IDs are index + 1 because PopupMenu reserves 0 for "dismissed without choosing".
    juce::PopupMenu menu;
    menu.setLookAndFeel(&menuLookAndFeel);
    bool hasUserPrograms = false;

    menu.addSectionHeader("Factory");
    for (int i = 0; i < numPrograms; ++i)
    {
        if (processorRef.isFactoryProgram(i))
            menu.addItem(i + 1, processorRef.getProgramName(i), true, i == currentIndex);
        else
            hasUserPrograms = true;
    }

    // Second pass rather than one loop building two menus: user Programs always sort after the
    // factory bank by index, so this keeps the menu in index order with no intermediate submenu.
    if (hasUserPrograms)
    {
        menu.addSeparator();
        menu.addSectionHeader("User");
        for (int i = 0; i < numPrograms; ++i)
            if (! processorRef.isFactoryProgram(i))
                menu.addItem(i + 1, processorRef.getProgramName(i), true, i == currentIndex);
    }

    // The menu hangs off the LCD and reads as an extension of it, so it takes the glass's width
    // rather than sizing itself to the longest Program name. localAreaToGlobal already carries the
    // editor's scale transform, so this stays right at every window size.
    const auto glassOnScreen = localAreaToGlobal(Layout::programLcd.getSmallestIntegerContainer());

    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(this)
                           .withTargetScreenArea(glassOnScreen)
                           .withMinimumWidth(glassOnScreen.getWidth())
                           .withMaximumNumColumns(1),
                       [safeThis = juce::Component::SafePointer<ProgramHeader>(this)](int result)
                       {
                           if (safeThis == nullptr || result == 0)
                               return;

                           // setCurrentProgram defers through the processor's AsyncUpdater (a host
                           // can call it off the message thread), so the repaint has to wait for
                           // the apply rather than happen here.
                           safeThis->processorRef.setCurrentProgram(result - 1);
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

    return processorRef.getProgramName(processorRef.getCurrentProgram()).toUpperCase();
}

void ProgramHeader::paint(juce::Graphics& g)
{
    const int index = processorRef.getCurrentProgram();
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
    const bool userBank = namingMode || ! processorRef.isFactoryProgram(index);

    Text::drawTracked(g, userBank ? "USER" : "FACT", lcdFont,
                      Layout::lcdTracking, bankArea, juce::Justification::centred, Colour::lcdText);

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
                                   : (processorRef.isFactoryProgram(index)
                                          ? Asset::DeleteFace::disabled
                                          : Asset::DeleteFace::enabled)),
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
