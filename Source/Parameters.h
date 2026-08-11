#pragma once

#include "DSP/FactoryPrograms.h"
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
    // so far, two model table redefinitions. Written into getStateInformation's XML root;
    // setStateInformation checks it and remaps legacy values before restoring. Note this is
    // separate from a parameter's ParameterID versionHint: that only affects the VST3/AU host's
    // own numeric automation-lane ID (so old automation doesn't silently reattach to a
    // parameter whose choices changed), but does NOT affect APVTS's own getStateInformation/
    // setStateInformation XML, which is keyed by the plain ID string regardless of versionHint -
    // that path needs this explicit marker instead.
    constexpr auto stateSchemaVersionAttribute = "taperotStateSchemaVersion";
    constexpr int currentStateSchemaVersion = 5;

    /** **The identity attributes, and they are a contract.** Rename one and the session still
        parses while the Program silently reverts to the default, with no error anywhere - which is
        exactly the failure mode the schema version exists to catch. Spelled as literals in
        Tests/SessionCompatibilityTests.cpp, not through these constants, or the test would rename
        itself alongside the code and assert nothing.

        `...ProgramName` is DISPLAY ONLY. It exists so an unresolved identifier can still be named
        on the panel - a factory slug is not presentable - and it never takes part in resolving. */
    constexpr auto programBankAttribute = "taperotProgramBank";
    constexpr auto programIdAttribute   = "taperotProgramId";
    constexpr auto programNameAttribute = "taperotProgramName";

    /** v4 -> v5: the session stored a positional index; it now stores bank + identifier. A v4
        session's index is mapped through the CURRENT bank, which is correct because nothing has
        shipped and the bank has not moved since v4. */
    inline juce::String bankAttributeValue (ProgramBank bank)
    {
        switch (bank)
        {
            case ProgramBank::init:       return "init";
            case ProgramBank::factory:    return "factory";
            case ProgramBank::user:       return "user";
            case ProgramBank::unresolved: return "unresolved";
        }

        return "factory";
    }

    inline ProgramBank bankFromAttribute (const juce::String& value)
    {
        if (value == "init")       return ProgramBank::init;
        if (value == "user")       return ProgramBank::user;
        if (value == "unresolved") return ProgramBank::unresolved;

        return ProgramBank::factory;
    }

    /** v3 -> v4: **Init left the numbered Factory bank**, so every Factory index shifted down by
        one and Warm Cassette became 01 instead of 02.

        This matters because the session stores the current Program **by index**, not by name. Left
        alone, every session saved before this change would reopen showing the name of the Program
        AFTER the one it was saved with - silently, and while the restored knob values stayed
        correct, so the panel would name a sound it was not making.

        The mapping is exact rather than approximate, which is why this is a migration and not a
        reset-to-default: old 0 was Init and becomes INIT's -1; every other old index k, factory or
        user alike, becomes k - 1, because the whole list above it shifted by exactly one.

        Applied to the SESSION attribute only. User Program FILES are unaffected - they store
        parameter values, never an index. */
    inline int remapProgramIndexV3ToV4(int savedIndex) noexcept
    {
        return savedIndex == 0 ? -1 : savedIndex - 1;
    }

    // v1 -> v2: old table order was VCR HiFi, Camcorder, Dictaphone, Toy, Cassette Type I,
    // Cassette Type II, Reel-to-Reel, Answering Machine (indices 0-7). Maps each to its closest
    // match in the v2 table so a pre-migration session's MODEL knob lands somewhere sensible
    // rather than on an arbitrary, unrelated machine. Output values are v2-table indices - see
    // legacyModelIndexRemapV2ToV3 below for the second hop applied on top for schema < 3 sessions.
    inline constexpr std::array<int, 8> legacyModelIndexRemapV1ToV2{
        1, // VCR HiFi          -> VCR HIFI (1 in v2)
        3, // Camcorder         -> CAMCORDER (3 in v2)
        6, // Dictaphone        -> DICTAPHONE (6 in v2)
        7, // Toy               -> TOY (7 in v2)
        4, // Cassette Type I   -> CASSETTE I (4 in v2)
        5, // Cassette Type II  -> CASSETTE II (5 in v2)
        0, // Reel-to-Reel      -> REVOX B77 (0 in v2) - both reel-to-reel machines
        6, // Answering Machine -> DICTAPHONE (6 in v2) - similar narrow telephone-band character
    };

    // v2 -> v3: NONE moved from last (8) to first (0) so it reads as "least processing" rather
    // than a trailing afterthought; every real model simply shifts up by one index to make room.
    inline constexpr std::array<int, 9> legacyModelIndexRemapV2ToV3{
        1, 2, 3, 4, 5, 6, 7, 8, 0
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
                int index = (int) child->getDoubleAttribute("value");

                if (schemaVersion < 2)
                    index = legacyModelIndexRemapV1ToV2[(size_t) juce::jlimit(
                        0, (int) legacyModelIndexRemapV1ToV2.size() - 1, index)];
                if (schemaVersion < 3)
                    index = legacyModelIndexRemapV2ToV3[(size_t) juce::jlimit(
                        0, (int) legacyModelIndexRemapV2ToV3.size() - 1, index)];

                child->setAttribute("value", (double) index);
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

    // Skewed (matching LP/HP/RAMP's existing 0.3-0.4 convention below): Saturator's drive-gain
    // curve is 1 + shape(driveNorm)*11 feeding tanh(x*driveGain)/tanh(driveGain) (see
    // Saturator::driveCurveExponent), which still reaches its audibly-maxed-out ceiling well
    // before DRIVE=100. A skew front-loads knob resolution into that useful low range instead.
    // FLUTTER shares the same skew for the same reason - it has no DSP-side curve shaping of its
    // own, but real usage clustered its musically-useful range into roughly the first 10% of a
    // linear knob (see the flutter values in FactoryPrograms.h), same complaint as DRIVE.
    //
    // 0.3 (DRIVE's original value) wasn't aggressive enough once musically-useful DRIVE values
    // turned out to sit even lower than the ~30-35% originally assumed above - dropped to 0.2 for
    // both controls so a given physical value needs noticeably more of the knob's rotation before
    // it's reached (e.g. physical DRIVE=9% moved from ~49% rotation under 0.3 to ~66% under 0.2).
    //
    // Skew safety: session-state and preset-file values are the physical (denormalised) number
    // (see AudioProcessorValueTreeState's own XML persistence, and processBlock reading
    // driveParam->load()/flutterParam->load() directly) - so "drive=55" means the same physical
    // 55% and the same sound before and after this change, in any saved session or preset file.
    // The one thing this does change is any *already-recorded host automation lane* on DRIVE/
    // FLUTTER: hosts store/play back automation as normalized [0,1] breakpoints, which
    // convertFrom0to1 maps through whichever skew is active - so an old automation curve will
    // produce a different physical value (and therefore a different sound) at the same breakpoint
    // after this change. That's an unavoidable consequence of changing any parameter's skew, not a
    // bug; it doesn't affect the parameter's default value (stored/interpreted as a physical number
    // either way) or any session/preset file.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::drive, 1}, "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.2f), 20.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::wow, 1}, "Wow",
        juce::NormalisableRange<float>(0.0f, 100.0f), 30.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::flutter, 1}, "Flutter",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.2f), 25.0f, percentAttrs));

    // versionHint bumped 1 -> 2 -> 3 (protects VST3/AU host automation-lane reattachment only -
    // see LegacyMigration above for the separate fix needed for APVTS's own state XML).
    juce::StringArray modelNames;
    for (const auto& model : kTapeModels)
        modelNames.add(model.displayName);
    constexpr int defaultModelIndex = 5; // CASSETTE I (shifted from 4 now that NONE is index 0)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::model, 3}, "Model", modelNames, defaultModelIndex));

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
