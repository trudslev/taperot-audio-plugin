#include "PitchScope.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    /** One pixel column per trace sample at the widest the well ever gets. */
    constexpr int traceCapacity = 2048;

    /** §4's grid pitch. 1308 / 163.5 is exactly 8 columns and 164 / 41 exactly 4 rows, so the
        stated pitches and the stated well size agree - which is worth deriving rather than
        transcribing a column count, because a well that resizes without its pitch moving would
        otherwise silently gain a partial column at one edge. */
    constexpr int gridColumns = (int) (Layout::scopeW / Layout::scopeGridX);   // 8
    constexpr int gridRows    = (int) (Layout::scopeH / Layout::scopeGridY);   // 4

    static_assert (gridColumns == 8 && gridRows == 4,
                   "§4's grid pitch and well size must divide exactly");

    /** The gap either side of the FAIL lamp in the bottom-right group, measured off the delivered
        prototype: `GEN n` ends 9 px left of the lamp and `FAIL` starts 9 px right of it. */
    constexpr float failLampGap = 9.0f;

    /** §4's span. Kept here rather than in the theme because it is a property of what the trace
        MEANS - the `500 ms / DIV` readout is this divided by the column count - and a reader
        changing one without the other would print a legend the trace does not obey. */
    constexpr float msPerDivision = 500.0f;
    constexpr float spanSeconds = msPerDivision * (float) gridColumns / 1000.0f;   // 4.0 s

    /*  **The trace stops short of the readout rows, and that is a legibility floor rather than a
        style choice.**

        §6 measures the scope readouts at **8.70:1 - on the well**. A trace behind them is not the
        well: at full excursion the amber path runs straight through `PITCH DEV +/- N cents` and the
        ratio collapses to nothing measurable. The first capture of this panel showed exactly that,
        and it is the auto-range working correctly - the range eases to fit whatever the transport
        is doing, so a large deviation fills the well by design.

        §4 places four readouts in the four corners and a trace between them, which only holds if
        the trace cannot reach a corner. So the excursion is clamped to leave both readout rows
        clear, and the readouts keep the ground their contrast was measured against.

        Worth stating what is NOT wrong here: the deviation itself. Root `CLAUDE.md` records this
        casting's wow and flutter depths as roughly 2 % pitch deviation against 0.05-0.3 % for real
        transports, under re-measurement - so a full-scale trace at defaults is the DSP being
        honestly displayed, and the display is the first thing that has shown it.  */
    constexpr float traceHalfHeight = Layout::scopeZeroY
                                    - Layout::scopeReadoutInsetY
                                    - TapeRotTheme::Type::scopeReadoutLineBox
                                    - 4.0f;                                     // 55
}

PitchScope::PitchScope (TapeRotAudioProcessor& p)
    : processorRef (p), trace ((size_t) traceCapacity, 0.0f)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (60);
}

juce::Rectangle<int> PitchScope::canvasBounds()
{
    return juce::Rectangle<float> (Layout::scopeX, Layout::scopeY,
                                   Layout::scopeW, Layout::scopeH).toNearestInt();
}

void PitchScope::setFailLampLit (bool shouldBeLit)
{
    if (failLit == shouldBeLit)
        return;

    failLit = shouldBeLit;
    repaint();
}

void PitchScope::timerCallback()
{
    float incoming[1024];
    int n;
    bool any = false;

    while ((n = processorRef.getPitchDeviationMeter().pop (incoming, 1024)) > 0)
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

void PitchScope::paintWell (juce::Graphics& g, juce::Rectangle<float> well)
{
    //== §4's material =======================================================
    g.setGradientFill ({ Colour::wellTop, 0.0f, well.getY(),
                         Colour::wellBottom, 0.0f, well.getBottom(), false });
    g.fillRect (well);

    //== The grid ============================================================
    g.setColour (Colour::scopeGrid);

    for (int c = 1; c < gridColumns; ++c)
    {
        const float x = well.getX() + Layout::scopeGridX * (float) c;
        g.drawLine (x, well.getY(), x, well.getBottom(), 1.0f);
    }

    for (int r = 1; r < gridRows; ++r)
    {
        const float y = well.getY() + Layout::scopeGridY * (float) r;

        // The zero line replaces the grid line it coincides with rather than being drawn over it -
        // two 1 px lines at the same y read as one heavier line, not as a dashed one.
        if (juce::approximatelyEqual (y - well.getY(), Layout::scopeZeroY))
            continue;

        g.drawLine (well.getX(), y, well.getRight(), y, 1.0f);
    }

    //== §4's zero line: dashed 3 on / 4 off =================================
    {
        const float y = well.getY() + Layout::scopeZeroY;
        g.setColour (Colour::scopeZero);
        const float dashes[] { Layout::scopeZeroDashOn, Layout::scopeZeroDashOff };
        g.drawDashedLine ({ well.getX(), y, well.getRight(), y }, dashes, 2, 1.0f);
    }

    //== The trace ===========================================================
    const double rate = processorRef.getPitchDeviationRate();
    const int visible = juce::jlimit (2, (int) trace.size(),
                                      (int) std::round (rate * (double) spanSeconds));

    float peak = 0.0f;
    for (int i = 0; i < visible; ++i)
    {
        const size_t idx = (writeIndex + trace.size() - (size_t) visible + (size_t) i) % trace.size();
        peak = juce::jmax (peak, std::abs (trace[idx]));
    }

    // The range readout follows the trace but only ever eases downward, so a single transient does
    // not make the scale jump back and forth while you watch it.
    const float target = juce::jmax (2.0f, std::ceil (peak));
    displayedRangeCents = target > displayedRangeCents
                        ? target
                        : displayedRangeCents + (target - displayedRangeCents) * 0.05f;

    const float halfSpan = juce::jmax (1.0f, displayedRangeCents);

    juce::Path path;
    for (int i = 0; i < visible; ++i)
    {
        const size_t idx = (writeIndex + trace.size() - (size_t) visible + (size_t) i) % trace.size();
        const float x = well.getX() + well.getWidth() * (float) i / (float) (visible - 1);
        const float norm = juce::jlimit (-1.0f, 1.0f, trace[idx] / halfSpan);
        const float y = well.getY() + Layout::scopeZeroY - norm * traceHalfHeight;

        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }

    /*  §4's `drop-shadow(0 0 5px rgba(217,131,36,.55))`, drawn as a wider soft stroke under the
        trace rather than as a blur pass.

        **That substitution is deliberate and it is the one Chorus-60 paid for.** Its ModScope drew
        the equivalent as two `juce::DropShadow` passes - software box blurs over a 1035 px path at
        60 Hz - and they measured **4844 µs of a 4934 µs component, 98 % of it**. The fix there was
        to blur at half resolution, which cost a measured mean delta of 3.33/255 over the real
        glass and needed a design approval to ship. Not repeating the construction is cheaper than
        optimising it afterwards, and at 1.7 px of trace against a 5 px radius the difference a blur
        buys over a soft stroke is smaller than the one that approval was granted for.  */
    g.setColour (Colour::scopeHalo);
    g.strokePath (path, juce::PathStrokeType (Layout::scopeTraceThickness + Layout::scopeHaloRadius,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (Colour::scopeTrace);
    g.strokePath (path, juce::PathStrokeType (Layout::scopeTraceThickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    //== §4's frame and inner shadow =========================================
    g.setColour (Colour::wellFrame);
    g.drawRect (well, 1.0f);
}

void PitchScope::paintReadouts (juce::Graphics& g, juce::Rectangle<float> well)
{
    const auto font = Font::monoAt (Type::scopeReadoutCssPx);
    const float tracking = Type::scopeReadoutTrackingPx;
    const auto dot = Text::middleDot();

    const float left  = well.getX() + Layout::scopeReadoutInsetX;
    const float right = well.getRight() - Layout::scopeReadoutInsetX;
    const float topY    = well.getY() + Layout::scopeReadoutInsetY;
    const float bottomY = well.getBottom() - Layout::scopeReadoutInsetY - Type::scopeReadoutLineBox;

    const auto row = [&] (float y, float l, float r)
    {
        return juce::Rectangle<float> (l, y, r - l, Type::scopeReadoutLineBox);
    };

    //== Top left / top right ================================================
    Text::drawTracked (g, "PITCH DEV " + dot + " " + Text::plusMinus()
                          + juce::String (juce::roundToInt (displayedRangeCents)) + " cents",
                       font, tracking, row (topY, left, right),
                       juce::Justification::left, Colour::scopeLegend);

    Text::drawTracked (g, juce::String (juce::roundToInt (msPerDivision)) + " ms / DIV",
                       font, tracking, row (topY, left, right),
                       juce::Justification::right, Colour::scopeLegend);

    //== Bottom left =========================================================
    Text::drawTracked (g, "WOW " + juce::String (processorRef.getWowRateHz(), 2) + " Hz " + dot
                          + " FLUT " + juce::String (processorRef.getFlutterRateHz(), 1) + " Hz",
                       font, tracking, row (bottomY, left, right),
                       juce::Justification::left, Colour::scopeLegend);

    //== Bottom right: GEN n · lamp · FAIL ===================================
    /*  Laid out right to left from the well's own inset, so the lamp sits between two strings whose
        widths are measured rather than assumed. Revision 1 drew this as one string either side of a
        gap the sprite left, which meant the gap was a constant and the lamp's position was not
        derived from anything.  */
    Text::drawTracked (g, "FAIL", font, tracking, row (bottomY, left, right),
                       juce::Justification::right, Colour::scopeLegend);

    const float failW = Text::trackedWidth ("FAIL", font, tracking);
    const float lampRight = right - failW - failLampGap;
    const float lampR = Layout::scopeFailLampDiameter * 0.5f;
    const juce::Point<float> lampCentre (lampRight - lampR, bottomY + Type::scopeReadoutLineBox * 0.5f);

    if (failLit)
    {
        g.setColour (Colour::lampGlow);
        g.fillEllipse (lampCentre.x - lampR - 3.0f, lampCentre.y - lampR - 3.0f,
                       (lampR + 3.0f) * 2.0f, (lampR + 3.0f) * 2.0f);
    }

    auto lens = Paint::sculptedFace (lampCentre, lampR, 0.38f, 0.30f,
                                     failLit ? Colour::lampLitHi : Colour::lampDarkHi,
                                     failLit ? Colour::lampLitEdge : Colour::lampDarkLo);
    lens.addColour (failLit ? 0.42 : 0.60, failLit ? Colour::lampLitMid : Colour::lampDarkMid);
    g.setGradientFill (lens);
    g.fillEllipse (lampCentre.x - lampR, lampCentre.y - lampR,
                   Layout::scopeFailLampDiameter, Layout::scopeFailLampDiameter);

    Text::drawTracked (g, "GEN " + juce::String (juce::roundToInt (processorRef.getGenDisplay())),
                       font, tracking,
                       row (bottomY, left, lampCentre.x - lampR - failLampGap),
                       juce::Justification::right, Colour::scopeLegend);
}

void PitchScope::paint (juce::Graphics& g)
{
    const auto well = getLocalBounds().toFloat();
    paintWell (g, well);
    paintReadouts (g, well);
}
