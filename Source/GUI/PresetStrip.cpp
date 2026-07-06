#include "PresetStrip.h"
#include "TapeRotTheme.h"

PresetStrip::PresetStrip(TapeRotAudioProcessor& processor) : processorRef(processor)
{
    using namespace TapeRotTheme;

    const float half = Layout::presetArrowRadius + 3.0f;
    prevButton.setBounds((int) (Layout::presetPrevCentreX - half), (int) (Layout::presetArrowCentreY - half),
                          (int) (half * 2.0f), (int) (half * 2.0f));
    prevButton.setTooltip("Previous preset.");
    prevButton.onClick = [this]
    {
        const int total = processorRef.getNumPrograms();
        const int current = processorRef.getCurrentProgram();
        processorRef.setCurrentProgram((current - 1 + total) % total);
    };
    addAndMakeVisible(prevButton);

    nextButton.setBounds((int) (Layout::presetNextCentreX - half), (int) (Layout::presetArrowCentreY - half),
                          (int) (half * 2.0f), (int) (half * 2.0f));
    nextButton.setTooltip("Next preset.");
    nextButton.onClick = [this]
    {
        const int total = processorRef.getNumPrograms();
        const int current = processorRef.getCurrentProgram();
        processorRef.setCurrentProgram((current + 1) % total);
    };
    addAndMakeVisible(nextButton);

    saveButton.setBounds((int) Layout::presetSaveX, (int) Layout::presetSaveY,
                          (int) Layout::presetSaveW, (int) Layout::presetSaveH);
    saveButton.setTooltip("Save the current settings as a new user preset.");
    saveButton.onClick = [this] { showSaveAsPrompt(); };
    addAndMakeVisible(saveButton);

    deleteButton.setBounds((int) Layout::presetDeleteX, (int) Layout::presetDeleteY,
                            (int) Layout::presetDeleteW, (int) Layout::presetDeleteH);
    deleteButton.setTooltip("Delete the current user preset (disabled for factory presets).");
    deleteButton.onClick = [this] { showDeleteConfirmation(); };
    addAndMakeVisible(deleteButton);

    displayedProgramIndex = processorRef.getCurrentProgram();
    displayedProgramName = processorRef.getProgramName(displayedProgramIndex);
    updateDeleteButtonEnablement();

    startTimerHz(10);
}

PresetStrip::~PresetStrip()
{
    stopTimer();
}

bool PresetStrip::hitTest(int x, int y)
{
    using namespace TapeRotTheme;

    const juce::Point<int> point(x, y);

    return juce::Rectangle<float>(Layout::presetNamePlateX, Layout::presetNamePlateY,
                                   Layout::presetNamePlateW, Layout::presetNamePlateH)
               .contains(point.toFloat())
        || prevButton.getBounds().contains(point)
        || nextButton.getBounds().contains(point)
        || saveButton.getBounds().contains(point)
        || deleteButton.getBounds().contains(point);
}

void PresetStrip::timerCallback()
{
    const int index = processorRef.getCurrentProgram();
    if (index != displayedProgramIndex)
    {
        displayedProgramIndex = index;
        displayedProgramName = processorRef.getProgramName(index);
        updateDeleteButtonEnablement();
        repaint();
    }
}

void PresetStrip::updateDeleteButtonEnablement()
{
    deleteButton.setEnabled(!processorRef.isFactoryPreset(displayedProgramIndex));
}

void PresetStrip::mouseDown(const juce::MouseEvent& e)
{
    using namespace TapeRotTheme;

    const juce::Rectangle<float> plate(Layout::presetNamePlateX, Layout::presetNamePlateY,
                                        Layout::presetNamePlateW, Layout::presetNamePlateH);
    if (!plate.contains(e.position))
        return;

    // Right-click on the name plate is a second, equally-valid way to delete a preset alongside
    // the visible DELETE button - kept rather than replaced, since a hidden gesture and a visible
    // control aren't mutually exclusive and some users will reach for either. Only offered for
    // user presets - factory presets are never deletable.
    if (e.mods.isPopupMenu())
    {
        if (processorRef.isFactoryPreset(displayedProgramIndex))
            return;

        juce::PopupMenu menu;
        menu.addItem("Delete \"" + displayedProgramName + "\"", [this] { showDeleteConfirmation(); });
        menu.showMenuAsync(juce::PopupMenu::Options());
        return;
    }

    showPresetListMenu();
}

void PresetStrip::showPresetListMenu()
{
    const int total = processorRef.getNumPrograms();

    juce::PopupMenu menu;
    for (int i = 0; i < total; ++i)
    {
        if (i == 0 || (processorRef.isFactoryPreset(i) != processorRef.isFactoryPreset(i - 1)))
            menu.addSectionHeader(processorRef.isFactoryPreset(i) ? "Factory" : "User");

        menu.addItem(processorRef.getProgramName(i), true, i == displayedProgramIndex,
                     [this, i] { processorRef.setCurrentProgram(i); });
    }

    menu.showMenuAsync(juce::PopupMenu::Options());
}

void PresetStrip::showDeleteConfirmation()
{
    const int indexToDelete = displayedProgramIndex;
    const auto name = displayedProgramName;

    auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
        juce::MessageBoxIconType::WarningIcon, "Delete Preset",
        "Delete the user preset \"" + name + "\"? This can't be undone.", "Delete", "Cancel");

    juce::AlertWindow::showAsync(options, [this, indexToDelete](int result)
    {
        if (result == 1)
            processorRef.deleteUserPreset(indexToDelete);
    });
}

void PresetStrip::showSaveAsPrompt()
{
    saveDialog = std::make_unique<juce::AlertWindow>(
        "Save Preset", "Enter a name for this preset:", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor("name", displayedProgramName);
    saveDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    saveDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    saveDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1)
        {
            const auto name = saveDialog->getTextEditorContents("name").trim();
            if (name.isNotEmpty())
                processorRef.saveUserPreset(name);
        }
        saveDialog.reset();
    }));
}

void PresetStrip::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    juce::Rectangle<float> plate(Layout::presetNamePlateX, Layout::presetNamePlateY,
                                  Layout::presetNamePlateW, Layout::presetNamePlateH);
    g.setColour(Colour::dark);
    g.fillRoundedRectangle(plate, Layout::presetNamePlateRadius);
    g.setColour(Colour::rim);
    g.drawRoundedRectangle(plate, Layout::presetNamePlateRadius, 1.2f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRoundedRectangle(plate.getX() + 2.0f, plate.getY() + 2.0f, plate.getWidth() - 4.0f, 3.0f, 1.5f);

    drawTrackedText(g, displayedProgramName.toUpperCase(), modelReadoutFont(11.5f), presetNameTracking,
                     plate, juce::Justification::centred, Colour::amberBright);
}
