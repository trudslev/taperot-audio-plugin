#include "GenerationLadder.h"

using namespace TapeRotTheme;

GenerationLadder::GenerationLadder() = default;

juce::Rectangle<int> GenerationLadder::canvasBounds() const
{
    const float left = Switches::genStageX0;
    const float right = Switches::genStageX0
                      + Switches::genStagePitch * (float) (Switches::genStageCount - 1)
                      + Switches::genStageW;
    // The numeral row is the box's bottom; 14 is Share Tech Mono's line box at 11 px, which is the
    // one figure §8 has no row for because the ladder's numerals are not one of its thirteen roles.
    constexpr float numeralLineBox = 14.0f;
    return juce::Rectangle<float> (left, Switches::genCaptionY, right - left,
                                   Switches::genNumeralY + numeralLineBox - Switches::genCaptionY)
               .toNearestInt();
}

void GenerationLadder::setGeneration (int oneBased)
{
    const int clamped = juce::jlimit (1, Switches::genStageCount, oneBased);

    if (generation == clamped)
        return;

    generation = clamped;
    repaint();
}

void GenerationLadder::mouseDown (const juce::MouseEvent& e)
{
    const float originX = (float) canvasBounds().getX();
    const float stageTop = Switches::genStageY - (float) canvasBounds().getY();

    for (int i = 0; i < Switches::genStageCount; ++i)
    {
        const juce::Rectangle<float> stage (Switches::genStageX0 + Switches::genStagePitch * (float) i - originX,
                                            stageTop, Switches::genStageW, Switches::genStageH);

        // The numeral under a stage is part of that stage's target: a 20 px square is a small thing
        // to hit, and the numeral is directly below it with nothing else competing for the pixels.
        if (stage.withBottom (stage.getBottom() + 26.0f).contains (e.position))
        {
            const int wanted = i + 1;

            if (wanted != generation)
            {
                generation = wanted;
                repaint();

                if (onGenerationChanged != nullptr)
                    onGenerationChanged (generation);
            }
            return;
        }
    }
}

void GenerationLadder::paint (juce::Graphics& g)
{
    const auto origin = canvasBounds().toFloat().getPosition();

    //== The caption =========================================================
    {
        const auto font = Font::label (Type::switchCaption.cssPx);
        const float tracking = Font::trackingPx (Type::switchCaption.trackingEm, Type::switchCaption.cssPx);
        Text::drawTracked (g, "GENERATION", font, tracking,
                           { 0.0f, Switches::genCaptionY - origin.y, (float) getWidth(),
                             Type::switchCaption.lineBox },
                           juce::Justification::centred, Colour::panelInk);
    }

    const auto numeralFont = Font::monoAt (Switches::genNumeralCssPx);

    for (int i = 0; i < Switches::genStageCount; ++i)
    {
        const juce::Rectangle<float> stage (Switches::genStageX0 + Switches::genStagePitch * (float) i - origin.x,
                                            Switches::genStageY - origin.y,
                                            Switches::genStageW, Switches::genStageH);
        const bool on = (i + 1) <= generation;

        //== §7.4's two faces ================================================
        /*  **Stages 1..GEN are lit, not just stage GEN, and this very nearly went the other way.**

            The reasoning that almost changed it: §7.4's table has exactly two rows, selected and
            unselected, with no row for "below the selection"; §5.4 says "lit stage" in the
            singular. Both read as a single lit stage. **Neither is evidence**, and the delivered
            prototype cannot settle it because it renders at GEN 1, where a bar and a single stage
            are the same picture.

            What settles it is what the control MEANS. §5.4's own argument for a ladder over a knob
            is that "the parameter is an integer count of tape generations" - and a count of five
            reads as five lit stages, not as one lit stage in the fifth position. The shipped build
            has always drawn it as a bar, so changing it would be altering behaviour on an inference
            from a two-row table. `design-asks/taperot-generation-ladder.md` asks for the ruling.

            Root `CLAUDE.md` calls this shape a true conclusion resting on a borrowed reason: the
            table's shape is a real observation and it is about the FACE of a stage, not about how
            many stages wear it.  */
        juce::ColourGradient face (on ? Colour::lampLitHi : Colour::lampCapHi,
                                   stage.getX() + stage.getWidth() * 0.38f,
                                   stage.getY() + stage.getHeight() * 0.30f,
                                   on ? Colour::lampLitEdge : Colour::lampCapLo,
                                   stage.getRight(), stage.getBottom(), true);
        face.addColour (on ? 0.42 : 0.55, on ? Colour::lampLitMid : Colour::lampCapMid);

        if (on)
        {
            // §7.4's `0 0 7px 1px rgba(240,169,75,.35)`, kept inside the stage's own footprint for
            // the same reason §5.3 keeps a lamp's light at the lens edge.
            g.setColour (Colour::lampGlow);
            g.fillRoundedRectangle (stage.expanded (2.0f), Switches::genStageRadius + 2.0f);
        }

        g.setGradientFill (face);
        g.fillRoundedRectangle (stage, Switches::genStageRadius);

        //== The numeral =====================================================
        g.setFont (numeralFont);
        g.setColour (Colour::panelInk);
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<float> (stage.getCentreX() - 14.0f,
                                            Switches::genNumeralY - origin.y, 28.0f, 14.0f),
                    juce::Justification::centred, false);
    }
}
