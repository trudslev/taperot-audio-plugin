#include "ProgramHeader.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    // SAVE and DELETE are printed on the plate; these are their hit areas, taken from the spec's
    // header layout. The plate draws both states of DELETE, so only the enable test lives here.
    const juce::Rectangle<float> saveBounds   { 905.0f, 47.0f, 66.0f, 40.0f };
    const juce::Rectangle<float> deleteBounds { 981.0f, 47.0f, 76.0f, 40.0f };
}

ProgramHeader::ProgramHeader(TapeRotAudioProcessor& p) : processorRef(p)
{
    setBounds(0, 0, (int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setInterceptsMouseClicks(true, false);
}

void ProgramHeader::mouseDown(const juce::MouseEvent& e)
{
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
    const auto bankArea = glass.withWidth(72.0f).reduced(6.0f, 0.0f);
    // Stops short of the baked dropdown chevron - a long User Program name would otherwise run
    // straight under it.
    const auto nameArea = glass.withTrimmedLeft(84.0f)
                               .withRight(Layout::lcdChevron.getX() - 8.0f)
                               .reduced(8.0f, 0.0f);

    Text::drawTracked(g, processorRef.isFactoryProgram(index) ? "FACT" : "USER", lcdFont,
                      Layout::lcdTracking, bankArea, juce::Justification::left, Colour::lcdText);

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
