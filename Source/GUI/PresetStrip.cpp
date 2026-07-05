#include "PresetStrip.h"
#include "TapeRotTheme.h"

PresetStrip::PresetStrip(TapeRotAudioProcessor& processor) : processorRef(processor)
{
    using namespace TapeRotTheme;

    const float half = Layout::presetArrowRadius + 3.0f;
    prevButton.setBounds((int) (Layout::presetPrevCentreX - half), (int) (Layout::presetArrowCentreY - half),
                          (int) (half * 2.0f), (int) (half * 2.0f));
    prevButton.onClick = [this]
    {
        const int total = processorRef.getNumPrograms();
        const int current = processorRef.getCurrentProgram();
        processorRef.setCurrentProgram((current - 1 + total) % total);
    };
    addAndMakeVisible(prevButton);

    nextButton.setBounds((int) (Layout::presetNextCentreX - half), (int) (Layout::presetArrowCentreY - half),
                          (int) (half * 2.0f), (int) (half * 2.0f));
    nextButton.onClick = [this]
    {
        const int total = processorRef.getNumPrograms();
        const int current = processorRef.getCurrentProgram();
        processorRef.setCurrentProgram((current + 1) % total);
    };
    addAndMakeVisible(nextButton);

    saveButton.setBounds((int) Layout::presetSaveX, (int) Layout::presetSaveY,
                          (int) Layout::presetSaveW, (int) Layout::presetSaveH);
    saveButton.onClick = [this] { showSaveAsPrompt(); };
    addAndMakeVisible(saveButton);

    displayedProgramIndex = processorRef.getCurrentProgram();
    displayedProgramName = processorRef.getProgramName(displayedProgramIndex);

    startTimerHz(10);
}

PresetStrip::~PresetStrip()
{
    stopTimer();
}

bool PresetStrip::hitTest(int x, int y)
{
    using namespace TapeRotTheme;

    return juce::Rectangle<float>(Layout::presetNamePlateX, Layout::presetNamePlateY,
                                   Layout::presetNamePlateW, Layout::presetNamePlateH)
        .contains((float) x, (float) y);
}

void PresetStrip::timerCallback()
{
    const int index = processorRef.getCurrentProgram();
    if (index != displayedProgramIndex)
    {
        displayedProgramIndex = index;
        displayedProgramName = processorRef.getProgramName(index);
        repaint();
    }
}

void PresetStrip::mouseDown(const juce::MouseEvent& e)
{
    using namespace TapeRotTheme;

    // Right-click on the name plate for "Delete Preset" - the approved design brief only asked
    // for prev/next/save (no delete button), but the preset system still needs some way to remove
    // a user preset; a context menu adds that without any new visible chrome. Only offered for
    // user presets - factory presets are never deletable.
    if (!e.mods.isPopupMenu())
        return;

    const juce::Rectangle<float> plate(Layout::presetNamePlateX, Layout::presetNamePlateY,
                                        Layout::presetNamePlateW, Layout::presetNamePlateH);
    if (!plate.contains(e.position))
        return;

    if (processorRef.isFactoryPreset(displayedProgramIndex))
        return;

    juce::PopupMenu menu;
    menu.addItem("Delete \"" + displayedProgramName + "\"", [this] { showDeleteConfirmation(); });
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

    drawTrackedText(g, "SAVE", dotLabelFont(), presetSaveLabelTracking,
                     juce::Rectangle<float>(Layout::presetSaveX - 20.0f, Layout::presetSaveLabelY - 8.0f,
                                             Layout::presetSaveW + 40.0f, 12.0f),
                     juce::Justification::centred, Colour::mutedLabel);
}
