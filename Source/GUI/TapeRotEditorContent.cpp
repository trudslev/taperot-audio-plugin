#include "TapeRotEditorContent.h"
#include "../PluginProcessor.h"

using namespace TapeRotTheme;

namespace
{
    struct ButtonAssets { const char* on; int onSize; const char* off; int offSize; };

    // Index-matched to Layout::buttons.
    const std::array<ButtonAssets, 9> buttonAssets { {
        { BinaryData::btn_fade_on_2x_png,   BinaryData::btn_fade_on_2x_pngSize,
          BinaryData::btn_fade_off_2x_png,  BinaryData::btn_fade_off_2x_pngSize },
        { BinaryData::btn_clunk_on_2x_png,  BinaryData::btn_clunk_on_2x_pngSize,
          BinaryData::btn_clunk_off_2x_png, BinaryData::btn_clunk_off_2x_pngSize },
        { BinaryData::btn_tape_on_2x_png,   BinaryData::btn_tape_on_2x_pngSize,
          BinaryData::btn_tape_off_2x_png,  BinaryData::btn_tape_off_2x_pngSize },
        { BinaryData::btn_vcr_on_2x_png,    BinaryData::btn_vcr_on_2x_pngSize,
          BinaryData::btn_vcr_off_2x_png,   BinaryData::btn_vcr_off_2x_pngSize },
        { BinaryData::btn_dust_on_2x_png,   BinaryData::btn_dust_on_2x_pngSize,
          BinaryData::btn_dust_off_2x_png,  BinaryData::btn_dust_off_2x_pngSize },
        { BinaryData::btn_off_on_2x_png,    BinaryData::btn_off_on_2x_pngSize,
          BinaryData::btn_off_off_2x_png,   BinaryData::btn_off_off_2x_pngSize },
        { BinaryData::btn_on_on_2x_png,     BinaryData::btn_on_on_2x_pngSize,
          BinaryData::btn_on_off_2x_png,    BinaryData::btn_on_off_2x_pngSize },
        { BinaryData::btn_linked_on_2x_png, BinaryData::btn_linked_on_2x_pngSize,
          BinaryData::btn_linked_off_2x_png,BinaryData::btn_linked_off_2x_pngSize },
        { BinaryData::btn_stereo_on_2x_png, BinaryData::btn_stereo_on_2x_pngSize,
          BinaryData::btn_stereo_off_2x_png,BinaryData::btn_stereo_off_2x_pngSize } } };
}

TapeRotEditorContent::TapeRotEditorContent(TapeRotAudioProcessor& p)
    : processorRef(p), lamps(p), scope(p, lamps), header(p)
{
    setSize((int) Layout::canvasWidth, (int) Layout::canvasHeight);

    addAndMakeVisible(scope);
    addAndMakeVisible(lamps);
    addAndMakeVisible(header);

    for (const auto& spec : Layout::knobs)
    {
        auto knob = std::make_unique<KnobFilmstrip>(spec.cap);
        knob->setSpriteTopLeft(spec.spriteTopLeft);

        const juce::String paramId { spec.paramId };
        auto* raw = knob.get();

        knob->onDragStart = [this, paramId] { header.showParameter(paramId); };
        knob->onDragEnd   = [this] { header.releaseParameter(); };

        // Guarded on the drag state: a SliderAttachment fires this on Program apply and on every
        // host automation step too, and without the guard the LCD latches and flickers.
        knob->onValueChange = [this, paramId, raw]
        {
            if (raw->isMouseButtonDown())
                header.showParameter(paramId);
        };

        addAndMakeVisible(*knob);
        attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, paramId, *knob));
        knobs.push_back(std::move(knob));
    }

    for (size_t i = 0; i < Layout::buttons.size(); ++i)
    {
        const auto& a = buttonAssets[i];
        auto b = std::make_unique<SpriteButton>(processorRef.apvts, Layout::buttons[i],
                                                a.on, a.onSize, a.off, a.offSize);
        b->onInteraction = [this](const juce::String& id)
        {
            header.showParameter(id);
            header.releaseParameter();
        };
        addAndMakeVisible(*b);
        buttons.push_back(std::move(b));
    }

    // The lamps and header both cover the whole canvas and paint sparsely, so they must not sit
    // over the knobs and buttons that carry the mouse.
    lamps.toBack();
    header.toBack();
    scope.toBack();

    startTimerHz(Layout::animationHz);
}

TapeRotEditorContent::~TapeRotEditorContent() = default;

void TapeRotEditorContent::timerCallback()
{
    // The header's IN/OUT numerals and the lamps' GEN segments both follow values that change
    // without any user action, so they need a poll rather than a callback.
    header.refresh();
    lamps.repaint();
}

void TapeRotEditorContent::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(Asset::panel(), juce::Rectangle<float>(Layout::canvasWidth, Layout::canvasHeight));
}
