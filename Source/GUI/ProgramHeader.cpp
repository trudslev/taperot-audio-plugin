#include "ProgramHeader.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    // SAVE and DELETE are printed on the plate; these are their hit areas, measured off
    // panel_background_2x.png rather than taken from the spec's prose - the earlier figures were
    // 5-6px out. The plate draws both states of DELETE, so only the enable test lives here.
    const juce::Rectangle<float> saveBounds   { 899.0f, 47.0f, 71.5f, 39.5f };
    const juce::Rectangle<float> deleteBounds { 985.5f, 47.0f, 70.5f, 39.5f };
}

ProgramHeader::ProgramHeader(TapeRotAudioProcessor& p) : processorRef(p)
{
    setBounds(0, 0, (int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setInterceptsMouseClicks(true, false);
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
        processorRef.saveUserProgram(processorRef.getProgramName(processorRef.getCurrentProgram()));
        repaint();
        return;
    }

    if (deleteBounds.contains(e.position)
        && ! processorRef.isFactoryProgram(processorRef.getCurrentProgram()))
    {
        processorRef.deleteUserProgram(processorRef.getCurrentProgram());
        repaint();
    }
}

void ProgramHeader::mouseMove(const juce::MouseEvent& e)
{
    // Position-dependent, so it can't be a one-off setMouseCursor in the constructor: this
    // component spans the whole canvas and only these three cells are clickable.
    const bool clickable = Layout::programLcd.contains(e.position)
                        || saveBounds.contains(e.position)
                        || (deleteBounds.contains(e.position)
                            && ! processorRef.isFactoryProgram(processorRef.getCurrentProgram()));

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

    Text::drawTracked(g, processorRef.isFactoryProgram(index) ? "FACT" : "USER", lcdFont,
                      Layout::lcdTracking, bankArea, juce::Justification::centred, Colour::lcdText);

    Text::drawTracked(g, lcdText(), lcdFont, Layout::lcdTracking, nameArea,
                      juce::Justification::left, Colour::lcdText);

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
