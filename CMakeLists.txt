cmake_minimum_required(VERSION 3.24)
# LANGUAGES needs C as well as CXX: JUCE/VST3-SDK/other fetched third-party deps bundle some .c
# sources, and Xcode/Visual Studio tolerate that without an explicit enable_language(C), but
# Linux's single-config Makefiles/Ninja generator does not - it fails at CMake's generate step
# with "CMAKE_C_COMPILE_OBJECT not set" if C was never enabled at the top level.
# These MUST come before project(): project() creates these cache entries itself, after which a
# plain set(... CACHE ...) is a silent no-op. Sitting below project() they had never taken effect -
# builds inherited the host architecture and the default SDK deployment target instead, which only
# surfaced when a CI-published .pkg turned out to contain an arm64-only binary.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal: Apple Silicon + Intel")
endif()

project(TapeRot VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------------------------
# neon-foundry-core: shared BEHAVIOUR across the suite, pinned by tag exactly like JUCE above.
#
# **This is not a dependency on a sibling casting.** Depending on one of those is still forbidden -
# it couples two things that must ship on their own schedules. This is a versioned library that no
# casting owns, so a change to it forces nothing on anyone until they move their own pin, which is
# the same relationship every casting already has with JUCE. The root CLAUDE.md draws that
# distinction under "Structure".
#
# **Declared AFTER JUCE, and that ordering is load-bearing.** Core links juce::juce_core and does
# not fetch its own when consumed; it fails with an explicit message if JUCE is not already
# available. Two JUCE trees in one build link two juce_core builds into one binary and surface as
# duplicate symbols a long way from the cause.
#
FetchContent_Declare(NeonFoundryCore
    GIT_REPOSITORY https://github.com/trudslev/neon-foundry-core.git
    GIT_TAG v1.0.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(NeonFoundryCore)


# The 2x asset set - 29 sprites, role-named rather than per-control.
#
# Several controls deliberately share one sprite: knob_large serves all seven large knobs (the cap
# is identical; ticks, numerals and the control name live in the plate), knob_small serves all
# three small ones, and one diameter-22 lamp serves both the FAULT ACTIVITY dots and the FAIL
# buttons. Source/GUI/TapeRotTheme.h maps each control to its sprite - if a strip looks "missing",
# check that map before adding a file.
#
# 2x only, scaled down for smaller windows: the fascia texture is fine enough that a 1:1 blit to a
# Retina display resolves it to a flat wash.
juce_add_binary_data(TapeRotBinaryData
    SOURCES
        # **The whole runtime asset list, and it is one bitmap.**
        #
        # Call 5 retired the filmstrips and the plate together: every knob, cap, pointer, tick,
        # numeral, label, shoe, lamp, divider and heading is a draw call now. Revision 1 embedded a
        # panel plate and 29 sprites; all 30 are gone, and the code that blitted them is DELETED
        # rather than adapted, because a sprite path left in place is a second way to draw a control
        # that nothing exercises.
        #
        # The wordmark is the exception and §9 states why: **Impact Label Reversed is donationware
        # and cannot be embedded**, so the letterforms ship as artwork and the font does not. That
        # is absent BY LICENSING, not missing - `design/fonts/ABSENT.md` says so, because an absent
        # font that is not declared looks like a delivery defect and gets "fixed" by substituting a
        # face, which moves every measurement taken from the nameplate.
        #
        # The cut is 694 x 150 at 3x, which is a 230.2 x 44 plate rotated -1.5 deg. It arrives
        # pre-rotated; `ProgramHeader::paintNameplate` blits it upright.
        design/assets/taperot-wordmark.png
        # §8's two faces. Barlow Condensed 600 is panel lettering; Share Tech Mono carries numerals,
        # units, the model line and both readouts, per call 7's split.
        #
        # **§8 asks for Barlow Condensed 500 for scale numerals and no 500 weight has been
        # delivered to any casting in this suite.** SemiBold stands in, which Gatecrasher's rewrite
        # also did without recording it; `design-asks/barlow-condensed-500.md` asks for the cut.
        design/fonts/BarlowCondensed-SemiBold.ttf
        design/fonts/share-tech-mono/ShareTechMono-Regular.ttf)

# AU is Apple-only; Windows only ever builds VST3 + Standalone. VST3_COPY_DIR/AU_COPY_DIR are only
# set on Apple - leaving them unset on Windows lets JUCE apply its own correct default
# (%COMMONPROGRAMFILES%/VST3) rather than a macOS path built on a non-guaranteed $ENV{HOME}.
if(APPLE)
    set(TAPEROT_FORMATS AU VST3 Standalone)
    set(TAPEROT_EXTRA_ARGS
        VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
        AU_COPY_DIR   "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
else()
    set(TAPEROT_FORMATS VST3 Standalone)
    set(TAPEROT_EXTRA_ARGS "")
endif()

# Company and product live here rather than as literals, because ProgramManager's user-Program
# path is built from them and that path is where saved work goes. Gatecrasher shipped a hand-synced
# copy that drifted to a dead company name; CMake handing every target the same values makes drift a
# build error instead of silent data loss.
set(TAPEROT_COMPANY_NAME "Neon Foundry")
set(TAPEROT_PRODUCT_NAME "TapeRot")

juce_add_plugin(TapeRot
    PRODUCT_NAME "${TAPEROT_PRODUCT_NAME}"
    COMPANY_NAME "${TAPEROT_COMPANY_NAME}"
    BUNDLE_ID "com.neonfoundry.taperot"
    # Direction D2. Both are exactly square at a size the .icns accepts, so the generated icon gets
    # clean 1024 and 256 entries. JUCE builds the .icns and .ico at CONFIGURE time and these PNGs are
    # not configure dependencies - after changing the artwork, re-run cmake -B build or the previous
    # icon ships silently.
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/design/icons/taperot-icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/design/icons/taperot-icon-256.png"
    # Without this the Standalone cannot get audio INPUT on macOS, and fails silently doing it.
    # An app with no NSMicrophoneUsageDescription is never granted capture access - the OS does not
    # prompt and does not error, the input stream simply delivers zeros. On an effect that reads as
    # dead metering: IN sits at the -99.9 floor and OUT follows it, because there is nothing to
    # process. The meters were wired correctly the whole time; the host had no signal to give them.
    #
    # Plugin formats are unaffected - the host owns the input there - so this only ever showed up
    # when someone opened the Standalone to listen.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "TapeRot needs audio input so the Standalone can process what you play into it."
    PLUGIN_MANUFACTURER_CODE Nfdy
    PLUGIN_CODE Rota
    FORMATS ${TAPEROT_FORMATS}
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    ${TAPEROT_EXTRA_ARGS})

target_sources(TapeRot PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/Saturator.cpp
    Source/DSP/WowFlutter.cpp
    Source/DSP/TapeModelEQ.cpp
    Source/DSP/NoiseSource.cpp
    Source/DSP/DegradationCore.cpp
    Source/DSP/Hum.cpp
    Source/DSP/FailureEngine.cpp
    Source/DSP/StereoSpread.cpp
    Source/DSP/ToneFilters.cpp
    Source/DSP/TapeStop.cpp
    Source/DSP/FilterSweep.cpp
    Source/DSP/OutputStage.cpp
    Source/GUI/KnobComponent.cpp
    Source/GUI/ShoeSwitch.cpp
    Source/GUI/LampButtonGroup.cpp
    Source/GUI/PitchScope.cpp
    Source/GUI/TapeRotPanelBackground.cpp
    Source/GUI/GenerationLadder.cpp
    Source/GUI/MachineReadout.cpp
    Source/GUI/ProgramHeader.cpp
    Source/GUI/TapeRotEditorContent.cpp)

target_compile_definitions(TapeRot PUBLIC
    # **TapeRot derives its version now, where it used to bake it.** Root CLAUDE.md lists this
    # casting with gatecrasher and fifth-member as the three whose footers read "v1.0" as ARTWORK,
    # needing a re-cut whenever the number moves. The plate is gone, so the footer is drawn from
    # PROJECT_VERSION like chorus-60's, reflect-84's and elmer's - one fewer thing to remember at
    # release, and one fewer place for the suite to disagree with itself about its own version.
    NF_VERSION_SHORT="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NF_COMPANY_NAME="${TAPEROT_COMPANY_NAME}"
    NF_PRODUCT_NAME="${TAPEROT_PRODUCT_NAME}")

target_link_libraries(TapeRot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        TapeRotBinaryData
        # PluginProcessor.cpp is compiled into both this target and the tests, and calls into core,
        # so both link it.
        nf::core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

add_subdirectory(Tests)
