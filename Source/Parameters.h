#pragma once

#include "DSP/TapeModelData.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    constexpr auto drive = "drive";
    constexpr auto wow = "wow";
    constexpr auto flutter = "flutter";
    constexpr auto model = "model";
    constexpr auto noise = "noise";
    constexpr auto hum = "hum";
    constexpr auto failure = "failure";
    constexpr auto mix = "mix";
    constexpr auto output = "output";
    constexpr auto spread = "spread";
    constexpr auto failureDropouts = "failureDropouts";
    constexpr auto failureSnags = "failureSnags";
    constexpr auto failureCrinkles = "failureCrinkles";
    constexpr auto failureImbalance = "failureImbalance";
    constexpr auto noiseCharacter = "noiseCharacter";
    constexpr auto gen = "gen";
    constexpr auto lp = "lp";
    constexpr auto hp = "hp";
    constexpr auto stop = "stop";
    constexpr auto filterAux = "filterAux";
    constexpr auto failAux = "failAux";
    constexpr auto ramp = "ramp";
    constexpr auto switchMode = "switchMode";
}

namespace NoiseCharacterNames
{
    constexpr auto tape = "TAPE";
    constexpr auto vcr = "VCR";
    constexpr auto dust = "DUST";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createTapeRotParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percentAttrs = juce::AudioParameterFloatAttributes().withLabel("%");
    auto dbAttrs = juce::AudioParameterFloatAttributes().withLabel("dB");

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::drive, 1}, "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f), 20.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::wow, 1}, "Wow",
        juce::NormalisableRange<float>(0.0f, 100.0f), 30.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::flutter, 1}, "Flutter",
        juce::NormalisableRange<float>(0.0f, 100.0f), 25.0f, percentAttrs));

    // versionHint bumped 1 -> 2: the model table was completely redefined (different machines,
    // different order), so an old session's stored index would silently select the wrong machine
    // if it reattached to this parameter. Bumping the hint means old automation/state for "model"
    // simply doesn't apply to the new parameter, and it starts at its own default instead - the
    // only sensible outcome once a choice list's *meaning* changes, not just gains an entry.
    juce::StringArray modelNames;
    for (const auto& model : kTapeModels)
        modelNames.add(model.displayName);
    constexpr int defaultModelIndex = 4; // CASSETTE I
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::model, 2}, "Model", modelNames, defaultModelIndex));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::noise, 1}, "Noise",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::hum, 1}, "Hum", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::failure, 1}, "Failure",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::mix, 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::output, 1}, "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, dbAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::spread, 1}, "Spread", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureDropouts, 1}, "Dropouts", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureSnags, 1}, "Snags", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureCrinkles, 1}, "Crinkles", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureImbalance, 1}, "Imbalance", true));

    // New parameters are appended below this line, never inserted above, to keep existing
    // sessions' parameter IDs stable (see BUILDING.md / CLAUDE.md backward-compatibility note).
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::noiseCharacter, 1}, "Noise Character",
        juce::StringArray{NoiseCharacterNames::tape, NoiseCharacterNames::vcr, NoiseCharacterNames::dust},
        0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::gen, 1}, "Generation", 1, 8, 1));

    auto hzAttrs = juce::AudioParameterFloatAttributes().withLabel("Hz");

    // Skewed toward the lower end, as is conventional for frequency parameters, since that's
    // where most of the perceptually-relevant tonal action happens. Defaults sit at the
    // transparent extreme of each range (LP wide open, HP fully open) so the filters are "off".
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::lp, 1}, "LP",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.0f, 0.3f), 20000.0f, hzAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::hp, 1}, "HP",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.3f), 20.0f, hzAttrs));

    // Momentary performance triggers: exposed as ordinary automatable bools, which is already
    // enough for any host's own MIDI-learn to map a note/CC to them without extra plugin-side
    // MIDI handling.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::stop, 1}, "Stop", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::filterAux, 1}, "Filter", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failAux, 1}, "Fail", false));

    auto secondsAttrs = juce::AudioParameterFloatAttributes().withLabel("s");
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::ramp, 1}, "Ramp",
        juce::NormalisableRange<float>(0.05f, 4.0f, 0.0f, 0.4f), 0.3f, secondsAttrs));

    // false = FADE (default), true = CLUNK.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::switchMode, 1}, "Switch", false));

    return {params.begin(), params.end()};
}
