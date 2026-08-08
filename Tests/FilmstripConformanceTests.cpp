#include "TestUtils.h"
#include "../Source/GUI/TapeRotTheme.h"
#include "../Source/DSP/TapeModelData.h"

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Guards the seam between the design assets and the code.

    This replaces the old tick-count tests. Those checked that a stepped slider drew one tick per
    tape model, which mattered while the ticks were drawn in code; they are now baked into
    `panel_background.png`, positioned by eye, and nothing at runtime can move them.

    What still matters — more, in fact — is that the MODEL filmstrip has exactly as many frames as
    the model table has entries. Add a model and forget to re-export the strip and the last machine
    silently renders as the one before it, with nothing to notice at build time. The spec itself
    flagged this as an open question ("say the word if the build wants eight and one gets dropped");
    the answer is nine, and this is what keeps it nine.
*/
class FilmstripConformanceTests final : public juce::UnitTest
{
public:
    FilmstripConformanceTests() : juce::UnitTest("Filmstrip conformance", "GUI") {}

    void runTest() override
    {
        using namespace TapeRotTheme;

        beginTest("The MODEL strip has exactly one frame per tape model");
        {
            const auto& strip = Asset::capStrip(Layout::Cap::model);
            expect(strip.isValid(), "the MODEL filmstrip did not load");

            const int frameSize = (int) Layout::largeFrame * Layout::assetScale;
            expectEquals(strip.getWidth(), frameSize, "MODEL frame width");
            expectEquals(strip.getHeight() / frameSize, (int) kNumTapeModels,
                         "MODEL frame count must equal the model table size");
            expectEquals(strip.getHeight() % frameSize, 0,
                         "MODEL strip height must be a whole number of frames");
            expectEquals(Layout::modelFrames, (int) kNumTapeModels,
                         "the theme's declared frame count must match the model table");
        }

        beginTest("The continuous strips carry their full 128 frames");
        for (auto cap : { Layout::Cap::large, Layout::Cap::small })
        {
            const auto& strip = Asset::capStrip(cap);
            const int frameSize = (int) Layout::frameSizeFor(cap) * Layout::assetScale;

            expect(strip.isValid(), "a knob filmstrip did not load");
            expectEquals(strip.getWidth(), frameSize, "frame width");
            expectEquals(strip.getHeight() / frameSize, Layout::frameCountFor(cap), "frame count");
            expectEquals(strip.getHeight() % frameSize, 0, "whole number of frames");
        }

        beginTest("The panel plate is the size the spec states");
        {
            const auto& panel = Asset::panel();
            expect(panel.isValid(), "the panel plate did not load");
            expectEquals(panel.getWidth(), (int) Layout::canvasWidth * Layout::assetScale);
            expectEquals(panel.getHeight(), (int) Layout::canvasHeight * Layout::assetScale);
        }

        beginTest("The scope legend rows are blank on the plate");
        {
            // These rows carry live values - the deviation range, the wow and flutter rates, GEN -
            // so spec section 6 draws them at runtime. The v1.0.1 plate had them baked in as well,
            // frozen at the mock's sample values, and the two overprinted: "GEN 2 GEN 4" on screen.
            // Delta v1.0.2 cleared them. Amber ink reappearing in either row means a re-exported
            // plate has re-baked them and the doubling is back.
            const auto& panel = Asset::panel();
            const int s = Layout::assetScale;

            const auto amberPixels = [&] (juce::Rectangle<float> row)
            {
                const auto r = (row * (float) s).getSmallestIntegerContainer()
                                   .getIntersection(panel.getBounds());
                int count = 0;

                for (int y = r.getY(); y < r.getBottom(); ++y)
                    for (int x = r.getX(); x < r.getRight(); ++x)
                    {
                        const auto p = panel.getPixelAt(x, y);
                        // #E3A65A against the near-black strip: strongly red-dominant and not dim.
                        if (p.getRed() > 120 && p.getRed() > p.getBlue() + 40 && p.getGreen() > 60)
                            ++count;
                    }

                return count;
            };

            expectEquals(amberPixels(Layout::scopeLegendTopRow), 0,
                         "the top legend row is baked into the plate and will double up");
            expectEquals(amberPixels(Layout::scopeLegendBottomRow), 0,
                         "the bottom legend row is baked into the plate and will double up");
        }

        beginTest("Every runtime frame sits inside the plate");
        // A frame placed off the edge would simply not draw, with no error anywhere.
        const juce::Rectangle<float> canvas { Layout::canvasWidth, Layout::canvasHeight };
        for (auto r : { Layout::programLcd, Layout::scopeWell, Layout::modelReadout,
                        Layout::inMeter, Layout::outMeter,
                        Layout::scopeLegendTopRow, Layout::scopeLegendBottomRow })
            expect(canvas.contains(r), "a runtime frame falls outside the panel");

        beginTest("Every knob sprite sits inside the plate");
        for (const auto& k : Layout::knobs)
        {
            const float f = Layout::frameSizeFor(k.cap);
            expect(canvas.contains(juce::Rectangle<float>(k.spriteTopLeft.x, k.spriteTopLeft.y, f, f)),
                   juce::String(k.paramId) + " sprite falls outside the panel");
        }

        beginTest("The shared sprites really are shared");
        // The handoff ships seven identical large strips, three identical small ones and fourteen
        // identical lamps; only one of each is embedded. If someone adds the duplicates back, this
        // is where the redundancy shows up.
        expect(&Asset::capStrip(Layout::Cap::large) != &Asset::capStrip(Layout::Cap::small),
               "large and small caps must be different images");
        expect(&Asset::lamp(true) != &Asset::lamp(false),
               "lit and unlit lamps must be different images");
        expect(Asset::lamp(true).getWidth() == (int) Layout::lampSize * Layout::assetScale,
               "the shared lamp is not the size the spec states");
    }
};

static FilmstripConformanceTests filmstripConformanceTests;
