#include "TapeRotEditorContent.h"

using namespace TapeRotTheme;

namespace
{
    // One tooltip per generic knob, for consistency across the whole knob row rather than only
    // covering the ones whose meaning isn't already obvious from the label.
    const char* genericKnobTooltip(const juce::String& paramID)
    {
        if (paramID == ParamIDs::drive)    return "Input drive into the saturation stage - more drive means more harmonic distortion.";
        if (paramID == ParamIDs::wow)      return "Wow depth: slow, wandering pitch drift, like an uneven tape reel.";
        if (paramID == ParamIDs::flutter)  return "Flutter depth: fast, fluttery pitch modulation, like a worn tape transport.";
        if (paramID == ParamIDs::model)    return "Tape machine model being emulated - each has its own EQ character.";
        if (paramID == ParamIDs::noise)    return "Noise floor amount (see the NOISE switch below for its character).";
        if (paramID == ParamIDs::failure)  return "Overall intensity of the failure effects (dropouts/snags/crinkles/wobble) below.";
        if (paramID == ParamIDs::mix)      return "Dry/wet blend between the unprocessed and processed signal.";
        if (paramID == ParamIDs::output)   return "Output level trim, applied after the dry/wet mix.";
        return nullptr;
    }
}

TapeRotEditorContent::TapeRotEditorContent(TapeRotAudioProcessor& p)
    : processorRef(p), modelReadout(p), failLamp(p), scope(p), genDigitDisplay(p), presetStrip(p)
{
    setSize((int) Layout::canvasWidth, (int) Layout::canvasHeight);
    setLookAndFeel(&lookAndFeel);

    sectionPanel.setBounds(getLocalBounds());
    addAndMakeVisible(sectionPanel);

    dymoLabel.setBounds(getLocalBounds());
    addAndMakeVisible(dymoLabel);

    auto addLabelTooltip = [this](juce::Rectangle<float> area, const char* tooltip)
    {
        auto hover = std::make_unique<TooltipHoverArea>();
        hover->setBounds(area.toNearestInt());
        hover->setTooltip(tooltip);
        addAndMakeVisible(*hover);
        labelTooltipAreas.push_back(std::move(hover));
    };

    for (size_t i = 0; i < Layout::knobs.size(); ++i)
    {
        const auto& spec = Layout::knobs[i];
        auto slider = TapeRotLookAndFeel::createKnobSlider(spec.label);
        if (const auto* tooltip = genericKnobTooltip(spec.paramID))
        {
            slider->setTooltip(tooltip);

            // MODEL's label is the ModelReadout component instead of static text (see
            // SectionPanel::paintKnobLabels) - it's a hover proxy like the rest below rather than
            // a tooltip set directly on ModelReadout, since ModelReadout deliberately opts out of
            // mouse interception entirely (setInterceptsMouseClicks(false, false), a pure-display
            // component like Scope/FailLamp) - added once its bounds are set, further down.
            if (juce::String(spec.paramID) != ParamIDs::model)
                // Narrower than SectionPanel's own 120px-wide justification box for this label
                // (fine for centering text, but would overlap a neighbour's hover area at these
                // knobs' ~108px spacing) - 90 clears the tightest gap with margin either side.
                addLabelTooltip({spec.x - 45.0f, Layout::knobCentreY + Layout::knobLabelOffsetY - 8.0f, 90.0f, 16.0f},
                                 tooltip);
        }

        const float half = Layout::knobTickOuterRadius + 3.0f;
        slider->setBounds((int) (spec.x - half), (int) (Layout::knobCentreY - half),
                           (int) (half * 2.0f), (int) (half * 2.0f));

        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, spec.paramID, *slider);

        addAndMakeVisible(*slider);
        knobSliders[i] = std::move(slider);
    }

    // Switch label + caption (e.g. "HUM" / "OFF-ON") are painted as two lines by
    // SectionPanel::paintSwitchLabels - one hover area spanning both lines covers them together.
    // Narrower than SectionPanel's own 140px-wide justification box (fine for centring text, but
    // would overlap a neighbouring switch's hover area at these switches' ~74.5px closest
    // spacing) - 60 clears that gap with margin either side.
    auto switchLabelArea = [](float trackX)
    {
        const float centreX = trackX + Layout::switchW * 0.5f;
        return juce::Rectangle<float>(centreX - 30.0f, Layout::switchLabelY - 10.0f, 60.0f,
                                       (Layout::switchCaptionY + 3.0f) - (Layout::switchLabelY - 10.0f));
    };

    humSwitch.setBounds((int) Layout::humSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                         (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    humSwitch.setTooltip("Mains hum overlay, on or off.");
    addLabelTooltip(switchLabelArea(Layout::humSwitchX), "Mains hum overlay, on or off.");
    humAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::hum, humSwitch);
    addAndMakeVisible(humSwitch);

    spreadSwitch.setBounds((int) Layout::spreadSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                           (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    spreadSwitch.setTooltip("Stereo image: LINKED keeps generations mono-compatible, STEREO widens them.");
    addLabelTooltip(switchLabelArea(Layout::spreadSwitchX),
                     "Stereo image: LINKED keeps generations mono-compatible, STEREO widens them.");
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::spread, spreadSwitch);
    addAndMakeVisible(spreadSwitch);

    noiseCharacterSwitch.setBounds((int) Layout::noiseSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                                    (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    noiseCharacterSwitch.setTooltip("Noise character: TAPE hiss, VCR whine, or DUST crackle.");
    addLabelTooltip(switchLabelArea(Layout::noiseSwitchX), "Noise character: TAPE hiss, VCR whine, or DUST crackle.");
    noiseCharacterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, ParamIDs::noiseCharacter, noiseCharacterSwitch);
    addAndMakeVisible(noiseCharacterSwitch);

    switchModeSwitch.setBounds((int) Layout::switchModeSwitchX, (int) (Layout::switchCentreY - Layout::switchThumbRadius),
                                (int) Layout::switchW, (int) (Layout::switchThumbRadius * 2.0f));
    switchModeSwitch.setTooltip("Model-switch style: FADE crossfades between models, CLUNK hard-swaps with a thump.");
    addLabelTooltip(switchLabelArea(Layout::switchModeSwitchX),
                     "Model-switch style: FADE crossfades between models, CLUNK hard-swaps with a thump.");
    switchModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, ParamIDs::switchMode, switchModeSwitch);
    addAndMakeVisible(switchModeSwitch);

    modelReadout.setBounds((int) Layout::modelReadoutX, (int) Layout::modelReadoutY,
                            (int) Layout::modelReadoutW, (int) (Layout::modelReadoutLabelY + 6.0f - Layout::modelReadoutY));
    addAndMakeVisible(modelReadout);
    if (const auto* modelTooltip = genericKnobTooltip(ParamIDs::model))
        addLabelTooltip(modelReadout.getBounds().toFloat(), modelTooltip);

    for (size_t i = 0; i < Layout::failureDots.size(); ++i)
    {
        const auto& spec = Layout::failureDots[i];
        const float x = Layout::failureDotFirstX + (float) i * Layout::failureDotSpacing;
        const float half = Layout::failureDotRadius + 3.0f;

        auto dot = std::make_unique<FailureDotToggle>(spec.label);
        dot->setBounds((int) (x - half), (int) (Layout::failureDotY - half), (int) (half * 2.0f), (int) (half * 2.0f));

        const char* dotTooltip = nullptr;
        if (juce::String(spec.paramID) == ParamIDs::failureDropouts)
            dotTooltip = "Dropouts: brief random drops in signal level. Enables this glitch type - "
                         "raise FAILURE or hold FAIL to actually trigger it.";
        else if (juce::String(spec.paramID) == ParamIDs::failureSnags)
            dotTooltip = "Snags: brief pitch-flutter stutters. Enables this glitch type - raise "
                         "FAILURE or hold FAIL to actually trigger it.";
        else if (juce::String(spec.paramID) == ParamIDs::failureCrinkles)
            dotTooltip = "Crinkles: bursts of crackling, high-passed noise. Enables this glitch "
                         "type - raise FAILURE or hold FAIL to actually trigger it.";
        else if (juce::String(spec.paramID) == ParamIDs::failureImbalance)
            dotTooltip = "Wobble: brief per-channel level imbalance blips. Enables this glitch "
                         "type - raise FAILURE or hold FAIL to actually trigger it.";

        if (dotTooltip != nullptr)
        {
            dot->setTooltip(dotTooltip);
            // Narrower than SectionPanel's own 40px-wide justification box (fine for centring
            // text, but would overlap a neighbouring dot's hover area at these dots' 28px
            // spacing) - 24 clears that gap with margin either side.
            addLabelTooltip({x - 12.0f, Layout::failureDotLabelY - 8.0f, 24.0f, 12.0f}, dotTooltip);
        }

        failureDotAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.apvts, spec.paramID, *dot);

        addAndMakeVisible(*dot);
        failureDotToggles[i] = std::move(dot);
    }

    const float genTotalW = 8.0f * Layout::genSelectorSegmentW + 7.0f * Layout::genSelectorGap;
    genSelector.setBounds((int) (Layout::genSelectorCentreX - genTotalW * 0.5f), (int) Layout::genSelectorY,
                           (int) genTotalW, (int) Layout::genSelectorSegmentH);
    genSelector.setTooltip("Generation count (1-8): how many tape-generation passes are cascaded, "
                            "each compounding wow/flutter, EQ coloration, and noise.");
    addLabelTooltip({Layout::genSelectorCentreX - 16.0f, Layout::stripLabelY - 6.0f, 32.0f, 12.0f},
                     "Generation count (1-8): how many tape-generation passes are cascaded, "
                     "each compounding wow/flutter, EQ coloration, and noise.");
    genAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, ParamIDs::gen, genSelector);
    addAndMakeVisible(genSelector);

    auto setupSmallKnob = [&](SmallKnob& knob, float x, const char* paramID, const char* tooltip,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment)
    {
        const float half = Layout::smallKnobRadius + 3.0f;
        knob.setBounds((int) (x - half), (int) (Layout::smallKnobCentreY - half), (int) (half * 2.0f), (int) (half * 2.0f));
        knob.setTooltip(tooltip);
        addLabelTooltip({x - 16.0f, Layout::stripLabelY - 6.0f, 32.0f, 12.0f}, tooltip);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, paramID, knob);
        addAndMakeVisible(knob);
    };

    setupSmallKnob(lpKnob, Layout::lpKnobX, ParamIDs::lp, "Low-pass tone filter cutoff.", lpAttachment);
    setupSmallKnob(rampKnob, Layout::rampKnobX, ParamIDs::ramp,
                   "Ramp time for the STOP/FILTER/FAIL momentary effects' fade in and out.", rampAttachment);
    setupSmallKnob(hpKnob, Layout::hpKnobX, ParamIDs::hp, "High-pass tone filter cutoff.", hpAttachment);

    auto setupAuxButton = [&](AuxButton& button, float x, const char* paramID, const char* tooltip,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment)
    {
        const float half = Layout::auxButtonRadius + 3.0f;
        button.setBounds((int) (x - half), (int) (Layout::auxButtonCentreY - half), (int) (half * 2.0f), (int) (half * 2.0f));
        button.setTooltip(tooltip);
        addLabelTooltip({x - 16.0f, Layout::stripLabelY - 6.0f, 32.0f, 12.0f}, tooltip);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, paramID, button);
        addAndMakeVisible(button);
    };

    setupAuxButton(stopButton, Layout::stopButtonX, ParamIDs::stop,
                   "Momentary tape-stop: the motor spins down and pitch dives to a halt while held.", stopAttachment);
    setupAuxButton(filterButton, Layout::filterButtonX, ParamIDs::filterAux,
                   "Momentary filter sweep: a resonant low-pass closes in while held, then reopens on release.",
                   filterAttachment);
    setupAuxButton(failButton, Layout::failButtonX, ParamIDs::failAux,
                   "Momentary failure burst: pushes FAILURE to maximum while held - only produces "
                   "glitches for the types enabled under DECAY's DRP/SNG/CRK/WBL dots, and each is "
                   "a random chance per second, so a longer hold is more likely to audibly trigger one.",
                   failAttachment);

    // Scope/FailLamp/GenDigitDisplay draw with absolute canvas coordinates (like SectionPanel and
    // DymoLabel), so they're sized to the full canvas rather than a sub-region - a JUCE
    // Component's paint() coordinates are always local to its own top-left, not the canvas origin.
    scope.setBounds(getLocalBounds());
    addAndMakeVisible(scope);

    failLamp.setBounds(getLocalBounds());
    addAndMakeVisible(failLamp);

    genDigitDisplay.setBounds(getLocalBounds());
    addAndMakeVisible(genDigitDisplay);

    presetStrip.setBounds(getLocalBounds());
    addAndMakeVisible(presetStrip);
}

TapeRotEditorContent::~TapeRotEditorContent()
{
    setLookAndFeel(nullptr);
}
