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

# **MSVC needs /utf-8, and without it a non-ASCII literal is silently replaced by '?'.**
#
# MSVC's default execution charset is the system ANSI code page, so a `\u2212` in a narrow string
# literal has no representation and the compiler substitutes a question mark, warning C4566 and
# carrying on. Gatecrasher's first Windows test run emitted **168** of those warnings and failed two
# printed-scale marks: `\u2212` in a mark's numeral became `?`, `parseNumeral` read `?60` as 0, and
# the check correctly reported that a mark labelled 0 was drawn at the wrong angle.
#
# It is not a bad way of writing the character — a universal-character-name is the portable form,
# and the runtime `charToString ((juce_wchar) 0x2212)` used elsewhere in this suite is unaffected.
# It is the execution charset, and /utf-8 sets both source and execution to UTF-8.
#
# Nothing changes on macOS or Linux, where UTF-8 is already both.
if(MSVC)
    add_compile_options(/utf-8)
endif()

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
        design/fonts/BarlowCondensed-SemiBold.ttf
        # **Medium (500) landed 2026-08-22 and the substitution is over.** §8's scale-numeral row
        # asks for 500 and this build drew SemiBold, stated at the constant while it was owed.
        #
        # Ruled by `shared/FONTS.md` that the weight stands and the file moves rather than §8's rows
        # changing to 600. Verified against Fifth Member's copy before wiring rather than trusted:
        # a DIFFERENT cut of the same release — 680 glyphs against 694 — but identical where it
        # matters, same fontRevision, unitsPerEm 1000, weightClass 500, widthClass 3, ascender,
        # descender and capHeight all equal, and **the same 525 codepoints**. Nothing this panel
        # draws is in the 14 unmapped glyphs that differ.
        #
        # That check is the point rather than the paperwork: two cuts of one face agreeing on a
        # NAME says nothing, and a type scale calibrated through `withPointHeight` moves with the
        # metrics. They do not move here.
        design/fonts/BarlowCondensed-Medium.ttf
        design/fonts/share-tech-mono/ShareTechMono-Regular.ttf
        # ABOUT-PART.md §2b's cursor. JUCE has no help cursor in StandardCursorType, so the
        # delivered asset is embedded and a MouseCursor is built from it.
        design/assets/about-cursor-2x.png)

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

# **Local builds install into the user's plugin folders; CI must not.**
#
# A runner has no DAW to load them into, so the copy is pure waste there — but the reason it is an
# option rather than a tidy-up is the other direction. On a machine that DOES have those folders, a
# build overwrites what is installed, and this repository has already paid for that: a
# defect-causing run copied a deliberately broken AU over the user's working one, minutes after the
# fix was committed, with the tree clean and the remote up to date. Every signal said done.
#
# Defaults ON because the local loop depends on it — build, then load in the DAW. CI passes
# -DNF_COPY_PLUGIN_AFTER_BUILD=OFF.
option(NF_COPY_PLUGIN_AFTER_BUILD
       "Install the built plugin into the user's plugin folders after building" ON)

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
    COPY_PLUGIN_AFTER_BUILD ${NF_COPY_PLUGIN_AFTER_BUILD}
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
    NF_VERSION="${PROJECT_VERSION}"
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

# ---------------------------------------------------------------------------------------------
# THIRD-PARTY-LICENCES.txt — into every bundle's Contents/Resources
#
# **Embedding a font in the binary is redistribution**, and both licences these faces carry — OFL
# 1.1 and Apache 2.0 — require the copyright notice and licence text to travel with what is
# redistributed. The unit of redistribution is the PLUG-IN BUNDLE: someone receives a .vst3 on its
# own, so a licence file at the repo root discharges nothing for them. It has to be inside.
#
# The file is GENERATED from this CMakeLists' own juce_add_binary_data block by
# tools/generate_third_party_licences.py, so it cannot drift from what actually ships. Regenerate
# after adding or removing an embedded face; `--check` fails if any casting is stale.
#
# **macOS only, stated rather than silently true.** MACOSX_PACKAGE_LOCATION is what puts a file in
# a bundle, and on Windows and Linux the licence has to reach the user through the installer
# instead — see installer/ . Adding the .txt to target_sources off Apple would have CMake try to
# build it, which is why this is guarded rather than merely inert.
# **The path and its guard sit OUTSIDE the platform branch, and that is load-bearing.** Both used
# to live inside `if(APPLE)`, where the `set()` only executes on Apple — so every Windows and Linux
# build dereferenced an undefined variable and handed `copy_if_different` an empty source argument,
# and the missing-file FATAL_ERROR could not fire there either.
#
# It survived because nothing had ever built off Apple: all six castings' CI is macOS-first, three
# had never run at all, and a defect on a path nobody walks leaves no trace. Found while planning
# the release pipeline, proved with a minimal CMake reproduction rather than by reading.
set(NF_LICENCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-LICENCES.txt")
if(NOT EXISTS "${NF_LICENCE_FILE}")
    message(FATAL_ERROR
        "THIRD-PARTY-LICENCES.txt is missing. Run: python3 tools/generate_third_party_licences.py")
endif()

if(APPLE)
    set_source_files_properties("${NF_LICENCE_FILE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    foreach(_nf_fmt IN ITEMS AU AUv3 VST3 Standalone)
        if(TARGET TapeRot_${_nf_fmt})
            target_sources(TapeRot_${_nf_fmt} PRIVATE "${NF_LICENCE_FILE}")
        endif()
    endforeach()
else()
    # **Windows and Linux: the VST3 is a bundle DIRECTORY too**, and it is the format that ships
    # there, so the licence belongs inside it for the same reason it does on macOS - the .vst3 is
    # what someone receives on its own. JUCE puts the binary at <bundle>/Contents/<arch>, so
    # Contents/Resources is one level up from TARGET_FILE_DIR.
    #
    # The Standalone off Apple is a bare executable with no bundle to put anything in; it reaches
    # the user through the installer or the tarball instead.
    if(TARGET TapeRot_VST3)
        add_custom_command(TARGET TapeRot_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_FILE_DIR:TapeRot_VST3>/../Resources"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${NF_LICENCE_FILE}"
                    "$<TARGET_FILE_DIR:TapeRot_VST3>/../Resources/THIRD-PARTY-LICENCES.txt"
            VERBATIM)
    endif()
endif()
