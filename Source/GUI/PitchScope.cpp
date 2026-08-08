#include "PitchScope.h"
#include "LampStrip.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    /** One pixel column per trace sample at the widest the well ever gets. */
    constexpr int traceCapacity = 2048;
}

PitchScope::PitchScope(TapeRotAudioProcessor& p, const LampStrip& l)
    : processorRef(p), lamps(l), trace((size_t) traceCapacity, 0.0f)
{
    setInterceptsMouseClicks(false, false);
    // Covers the well AND both legend rows, which sit outside the drawable area on the plate.
    setBounds(Layout::scopeWell.getUnion(Layout::scopeLegendTop)
                               .getUnion(Layout::scopeLegendBottom)
                               .getSmallestIntegerContainer());
    startTimerHz(Layout::animationHz);
}

void PitchScope::timerCallback()
{
    float incoming[1024];
    int n;
    bool any = false;

    while ((n = processorRef.getPitchDeviationMeter().pop(incoming, 1024)) > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            trace[writeIndex] = incoming[i];
            writeIndex = (writeIndex + 1) % trace.size();
        }
        any = true;
    }

    if (any)
        repaint();
}

void PitchScope::paint(juce::Graphics& g)
{
    const auto origin = getBounds().toFloat().getPosition();
    const auto well = Layout::scopeWell.translated(-origin.x, -origin.y);

    g.setColour(Colour::scopeWell);
    g.fillRect(well);

    // --- grid -----------------------------------------------------------------------------------
    g.setColour(Colour::scopeGrid);

    for (int c = 1; c < Layout::scopeGridColumns; ++c)
    {
        const float x = well.getX() + well.getWidth() * (float) c / (float) Layout::scopeGridColumns;
        g.drawLine(x, well.getY(), x, well.getBottom(), 1.0f);
    }

    for (int r = 1; r < Layout::scopeGridRows; ++r)
    {
        const float y = well.getY() + well.getHeight() * (float) r / (float) Layout::scopeGridRows;

        if (r * 2 == Layout::scopeGridRows)
            continue;                                  // centre line drawn dashed below

        g.drawLine(well.getX(), y, well.getRight(), y, 1.0f);
    }

    {
        const float y = well.getCentreY();
        g.setColour(Colour::scopeCentre);
        const float dashes[] { 3.0f, 4.0f };
        g.drawDashedLine({ well.getX(), y, well.getRight(), y }, dashes, 2, 1.0f);
    }

    // --- trace ----------------------------------------------------------------------------------
    // The visible window is the last scopeSpanSeconds of samples at the meter's output rate; older
    // ones simply fall off the left.
    const double rate = processorRef.getPitchDeviationRate();
    const int visible = juce::jlimit(2, (int) trace.size(),
                                     (int) std::round(rate * (double) Layout::scopeSpanSeconds));

    float peak = 0.0f;
    for (int i = 0; i < visible; ++i)
    {
        const size_t idx = (writeIndex + trace.size() - (size_t) visible + (size_t) i) % trace.size();
        peak = juce::jmax(peak, std::abs(trace[idx]));
    }

    // The range readout follows the trace but only ever eases downward, so a single transient does
    // not make the scale jump back and forth while you watch it.
    const float target = juce::jmax(2.0f, std::ceil(peak));
    displayedRangeCents = target > displayedRangeCents ? target
                                                       : displayedRangeCents + (target - displayedRangeCents) * 0.05f;

    const float halfSpan = juce::jmax(1.0f, displayedRangeCents);

    juce::Path path;
    for (int i = 0; i < visible; ++i)
    {
        const size_t idx = (writeIndex + trace.size() - (size_t) visible + (size_t) i) % trace.size();
        const float x = well.getX() + well.getWidth() * (float) i / (float) (visible - 1);
        const float norm = juce::jlimit(-1.0f, 1.0f, trace[idx] / halfSpan);
        const float y = well.getCentreY() - norm * (well.getHeight() * 0.5f - 3.0f);

        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }

    g.setColour(Colour::scopeHalo);
    g.strokePath(path, juce::PathStrokeType(Layout::scopeHaloThickness,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(Colour::scopeTrace);
    g.strokePath(path, juce::PathStrokeType(Layout::scopeTraceThickness,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // --- margin readouts, in the well's own segment face ----------------------------------------
    const auto font = Font::of(Layout::scopeLegendSize);
    const float inset = 6.0f;
    juce::ignoreUnused(inset);
    const auto topRow = Layout::scopeLegendTop.translated(-origin.x, -origin.y);
    const auto bottomRow = Layout::scopeLegendBottom.translated(-origin.x, -origin.y);
    const auto dot = Text::middleDot();

    Text::drawTracked(g, "PITCH DEV " + dot + " " + juce::String::charToString((juce::juce_wchar) 0x00B1)
                          + juce::String(juce::roundToInt(displayedRangeCents)) + " cents",
                      font, Layout::scopeLegendTracking, topRow,
                      juce::Justification::left, Colour::scopeLegend);

    Text::drawTracked(g, juce::String(juce::roundToInt(Layout::scopeMsPerDivision)) + " ms / DIV",
                      font, Layout::scopeLegendTracking, topRow,
                      juce::Justification::right, Colour::scopeLegend);

    Text::drawTracked(g, "WOW " + juce::String(processorRef.getWowRateHz(), 2) + " Hz " + dot
                          + " FLUT " + juce::String(processorRef.getFlutterRateHz(), 1) + " Hz",
                      font, Layout::scopeLegendTracking, bottomRow,
                      juce::Justification::left, Colour::scopeLegend);

    // GEN sits left of the FAIL lamp, which LampStrip draws over the plate.
    Text::drawTracked(g, "GEN " + juce::String(juce::roundToInt(processorRef.getGenDisplay()))
                          + "        FAIL",
                      font, Layout::scopeLegendTracking, bottomRow,
                      juce::Justification::right, Colour::scopeLegend);
}
