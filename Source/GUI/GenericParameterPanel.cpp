#include "GenericParameterPanel.h"

GenericParameterPanel::GenericParameterPanel(juce::AudioProcessorValueTreeState& state,
                                              const juce::StringArray& paramIDs)
{
    for (const auto& paramID : paramIDs)
    {
        auto* param = state.getParameter(paramID);
        if (param == nullptr)
            continue;

        auto* row = rows.add(new Row());

        row->label = std::make_unique<juce::Label>(paramID, param->getName(64));
        row->label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*row->label);

        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        {
            auto comboBox = std::make_unique<juce::ComboBox>(paramID);
            comboBox->addItemList(choiceParam->choices, 1);
            addAndMakeVisible(*comboBox);
            row->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                state, paramID, *comboBox);
            row->control = std::move(comboBox);
        }
        else if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton>(paramID);
            addAndMakeVisible(*button);
            row->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                state, paramID, *button);
            row->control = std::move(button);
        }
        else
        {
            auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                           juce::Slider::TextBoxBelow);
            addAndMakeVisible(*slider);
            row->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                state, paramID, *slider);
            row->control = std::move(slider);
        }
    }
}

void GenericParameterPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    if (rows.isEmpty())
        return;

    const int columns = 4;
    const int rowsNeeded = (rows.size() + columns - 1) / columns;
    const int cellWidth = bounds.getWidth() / columns;
    const int cellHeight = bounds.getHeight() / juce::jmax(1, rowsNeeded);

    for (int i = 0; i < rows.size(); ++i)
    {
        const int col = i % columns;
        const int row = i / columns;
        auto cell = juce::Rectangle<int>(bounds.getX() + col * cellWidth, bounds.getY() + row * cellHeight,
                                          cellWidth, cellHeight).reduced(6);

        auto* r = rows[i];
        r->label->setBounds(cell.removeFromTop(18));
        r->control->setBounds(cell);
    }
}
