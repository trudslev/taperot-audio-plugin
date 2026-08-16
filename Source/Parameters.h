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
    /** **Gated on the version this migration belongs to, NOT on the current one.**

        The MODEL table was reordered twice - v1->v2 and v2->v3 - so this applies to sessions below
        3 and to nothing else. It used to read `>= currentStateSchemaVersion`, which was correct
        exactly once: every later schema bump silently widened its reach to versions it knows
        nothing about. Gatecrasher had the same shape and it was not harmless there - bumping the
        schema re-ran its algorithm remap on already-migrated sessions and rotated the choice a
        second time on every load.

        Harmless here only by luck: both inner hops are themselves version-gated, so a v3 or v4
        session fell through doing nothing. The next hop added without one would not be. */
    inline void remapLegacyModelIndexIfNeeded(juce::XmlElement& xml)
    {
        constexpr int modelRemapAppliesBelow = 3;

        const int schemaVersion = xml.getIntAttribute(stateSchemaVersionAttribute, 1);
        if (schemaVersion >= modelRemapAppliesBelow)
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

    // **Every float parameter needs an explicit stringFromValueFunction.** withLabel() only feeds
    // getLabel(); it does not touch getText(). An AudioParameterFloat with no formatter of its own
    // builds one from the range's interval, and when that interval is 0 - as it is for every float
    // here - `numDecimalPlaces` is left at its initial 7
    // (juce_AudioParameterFloat.cpp:53-70). The panel printed `DRIVE: 20.0000000`, and so did the
    // host's automation lane and any generic editor.
    //
    // roundToInt rather than fixed-0 for whole numbers: juce::String(double, int) only sets a
    // formatting flag when the count is greater than zero (juce_String.cpp:486-492), so 0 decimals
    // falls through to std::ostream's default of six significant digits and renders 33.333332 as
    // "33.3333". It reads like a rounding instruction and is not one.
    auto whole  = [] (float v, int) { return juce::String(juce::roundToInt(v)); };
    auto oneDp  = [] (float v, int) { return juce::String(v, 1); };
    auto twoDp  = [] (float v, int) { return juce::String(v, 2); };

    auto percentAttrs = juce::AudioParameterFloatAttributes().withLabel("%")
                            .withStringFromValueFunction(whole);
    auto dbAttrs = juce::AudioParameterFloatAttributes().withLabel("dB")
                       .withStringFromValueFunction(oneDp);

    // **ON/OFF is authored here, not upper-cased at the readout.** JUCE's default bool text is
    // "On"/"Off" (juce_AudioParameterBool.cpp:47), and the LCD used to upper-case any unitless,
    // digit-free value on the way out - which made the display the only place that knew this panel
    // spells its switch legends in caps, and made the host's automation lane disagree with it.
    // BRAND.md: case belongs at the source. Every bool below carries this except SWITCH.
    auto onOffAttrs = juce::AudioParameterBoolAttributes()
                          .withStringFromValueFunction([] (bool v, int) { return v ? "ON" : "OFF"; });

    // **SWITCH is the one parameter whose readout deliberately CHANGES**, from `SWITCH: OFF`/`ON`
    // to `SWITCH: FADE`/`CLUNK`. It is a bool for storage reasons, but it has never meant on/off:
    // false is FADE (parallel-chain crossfade) and true is CLUNK (hard coefficient swap under a
    // mute dip). The old text named the storage rather than the control, and the panel's own
    // printed legends read FADE and CLUNK, so the LCD contradicted the plate it sits in.
    auto switchModeAttrs = juce::AudioParameterBoolAttributes()
                               .withStringFromValueFunction([] (bool v, int) { return v ? "CLUNK" : "FADE"; });

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
        juce::ParameterID{ParamIDs::drive, 1}, "DRIVE",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.2f), 20.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::wow, 1}, "WOW",
        // **Skew 0.2, matching FLUTTER, and the maximum is deliberately unchanged.**
        //
        // WOW was the only one of the pair with a LINEAR taper, so half travel was half of range:
        // 22.44 cents rms = 1.30 %, past "a dying transport". A good deck at 0.05-0.08 % needed
        // roughly 2-3 % of travel - the bottom sliver of the knob, which is the exact shape
        // FLUTTER's skew exists to avoid. Two adjacent controls, same section, same units, opposite
        // tapers, and nothing had chosen that.
        //
        // **Re-tapering rather than capping, because capping would cost a sound.** 2.59 % is a
        // warped-record wobble and a legitimate extreme for a degradation effect; lowering the
        // maximum to fix the middle would throw away the top to repair the bottom. The skew moves
        // where the travel spends its time without removing anywhere it can reach.
        //
        // FLUTTER's own skew was deliberate and confirmed and is untouched - the earlier rule
        // "fix the maximum, not the taper" was about that control, where re-tapering would have
        // undone a decision made for its own reasons. WOW has no taper to undo.
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.2f), 30.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::flutter, 1}, "FLUTTER",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.2f), 25.0f, percentAttrs));

    // versionHint bumped 1 -> 2 -> 3 (protects VST3/AU host automation-lane reattachment only -
    // see LegacyMigration above for the separate fix needed for APVTS's own state XML).
    juce::StringArray modelNames;
    for (const auto& model : kTapeModels)
        modelNames.add(model.displayName);
    constexpr int defaultModelIndex = 5; // CASSETTE I (shifted from 4 now that NONE is index 0)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::model, 3}, "MODEL", modelNames, defaultModelIndex));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::noise, 1}, "NOISE",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::hum, 1}, "HUM", false, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::failure, 1}, "FAILURE",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::mix, 1}, "MIX",
        juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::output, 1}, "OUTPUT",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, dbAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::spread, 1}, "SPREAD", false, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureDropouts, 1}, "DROPOUTS", true, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureSnags, 1}, "SNAGS", true, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureCrinkles, 1}, "CRINKLES", true, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureImbalance, 1}, "IMBALANCE", true, onOffAttrs));

    // New parameters are appended below this line, never inserted above, to keep existing
    // sessions' parameter IDs stable (see BUILDING.md / CLAUDE.md backward-compatibility note).
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::noiseCharacter, 1}, "NOISE CHARACTER",
        juce::StringArray{NoiseCharacterNames::tape, NoiseCharacterNames::vcr, NoiseCharacterNames::dust},
        0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::gen, 1}, "GENERATION", 1, 8, 1));

    // No withLabel here, unlike the others: the unit is value-dependent, so it has to live in the
    // text. LP spans 1-20 kHz and HP spans 20-2000 Hz, and the printed scales switch at 1k the same
    // way. A label would double the unit up ("6.3 kHz Hz").
    auto hzAttrs = juce::AudioParameterFloatAttributes()
                       .withStringFromValueFunction([] (float v, int)
                       {
                           return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz"
                                                : juce::String(juce::roundToInt(v)) + " Hz";
                       });

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
        juce::ParameterID{ParamIDs::stop, 1}, "STOP", false, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::filterAux, 1}, "FILTER", false, onOffAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failAux, 1}, "FAIL", false, onOffAttrs));

    // Two decimals, not one: RAMP starts at 0.05 s, which one decimal rounds to "0.1" - the same
    // string it gives at 0.14, so the bottom of the range would read as a single value.
    auto secondsAttrs = juce::AudioParameterFloatAttributes().withLabel("s")
                            .withStringFromValueFunction(twoDp);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::ramp, 1}, "RAMP",
        juce::NormalisableRange<float>(0.05f, 4.0f, 0.0f, 0.4f), 0.3f, secondsAttrs));

    // false = FADE (default), true = CLUNK.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::switchMode, 1}, "SWITCH", false, switchModeAttrs));

    return {params.begin(), params.end()};
}
