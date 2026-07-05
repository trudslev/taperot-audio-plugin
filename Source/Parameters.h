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

namespace LegacyMigration
{
    // Bumped whenever a stored parameter's *meaning* (not just its ID) changes incompatibly -
    // currently just the model table redefinition. Written into getStateInformation's XML root;
    // setStateInformation checks it and remaps legacy values before restoring. Note this is
    // separate from a parameter's ParameterID versionHint: that only affects the VST3/AU host's
    // own numeric automation-lane ID (so old automation doesn't silently reattach to a
    // parameter whose choices changed), but does NOT affect APVTS's own getStateInformation/
    // setStateInformation XML, which is keyed by the plain ID string regardless of versionHint -
    // that path needs this explicit marker instead.
    constexpr auto stateSchemaVersionAttribute = "taperotStateSchemaVersion";
    constexpr int currentStateSchemaVersion = 2;

    // Old table order was: VCR HiFi, Camcorder, Dictaphone, Toy, Cassette Type I, Cassette Type
    // II, Reel-to-Reel, Answering Machine (indices 0-7). Maps each to its closest match in the
    // new table so a pre-migration session's MODEL knob lands somewhere sensible rather than on
    // an arbitrary, unrelated machine.
    inline constexpr std::array<int, 8> legacyModelIndexRemap{
        1, // VCR HiFi          -> VCR HIFI (1)
        3, // Camcorder         -> CAMCORDER (3)
        6, // Dictaphone        -> DICTAPHONE (6)
        7, // Toy               -> TOY (7)
        4, // Cassette Type I   -> CASSETTE I (4)
        5, // Cassette Type II  -> CASSETTE II (5)
        0, // Reel-to-Reel      -> REVOX B77 (0) - both reel-to-reel machines
        6, // Answering Machine -> DICTAPHONE (6) - similar narrow telephone-band character
    };

    // Free function (rather than a TapeRotAudioProcessor method) so it's unit-testable with a
    // synthetic XmlElement, without needing the real plugin target's JucePlugin_* macros.
    inline void remapLegacyModelIndexIfNeeded(juce::XmlElement& xml)
    {
        const int schemaVersion = xml.getIntAttribute(stateSchemaVersionAttribute, 1);
        if (schemaVersion >= currentStateSchemaVersion)
            return;

        for (int i = 0; i < xml.getNumChildElements(); ++i)
        {
            auto* child = xml.getChildElement(i);
            if (child != nullptr && child->getStringAttribute("id") == ParamIDs::model)
            {
                const int oldIndex = juce::jlimit(0, (int) legacyModelIndexRemap.size() - 1,
                                                   (int) child->getDoubleAttribute("value"));
                child->setAttribute("value", (double) legacyModelIndexRemap[(size_t) oldIndex]);
                break;
            }
        }
    }
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

    // versionHint bumped 1 -> 2 (protects VST3/AU host automation-lane reattachment only - see
    // LegacyMigration above for the separate fix needed for APVTS's own state XML).
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
