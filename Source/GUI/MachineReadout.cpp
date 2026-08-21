#include "MachineReadout.h"

using namespace TapeRotTheme;

MachineReadout::MachineReadout()
{
    setInterceptsMouseClicks (false, false);
}

juce::Rectangle<int> MachineReadout::canvasBounds()
{
    return juce::Rectangle<float> (Readouts::machineX, Readouts::machineY,
                                   Readouts::machineW, Readouts::machineH).toNearestInt();
}

void MachineReadout::setMachineName (const juce::String& name)
{
    if (machineName == name)
        return;

    machineName = name;
    repaint();
}

void MachineReadout::paint (juce::Graphics& g)
{
    const auto well = getLocalBounds().toFloat();

    g.setGradientFill ({ Colour::wellTop, 0.0f, well.getY(),
                         Colour::wellBottom, 0.0f, well.getBottom(), false });
    g.fillRoundedRectangle (well, Readouts::machineRadius);

    g.setColour (Colour::wellFrame);
    g.drawRoundedRectangle (well.reduced (0.5f), Readouts::machineRadius, 1.0f);

    /*  §3.3: "Position names are the enum's" - so the string comes from the parameter's own choice
        list rather than from a second table here. Root `CLAUDE.md`'s case-at-the-source ruling is
        the same argument: the panel and the host's automation lane read one parameter, so a name
        re-derived at a display site is a second convention that can disagree with the first.  */
    Text::drawTracked (g, machineName, Font::monoAt (Type::machineReadoutCssPx),
                       Type::machineReadoutTrackingPx,
                       well.withSizeKeepingCentre (well.getWidth(), Type::machineReadoutLineBox),
                       juce::Justification::centred, Colour::lcdText);
}
