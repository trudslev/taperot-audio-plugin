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

project(TapeRot VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

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
        design/assets/2x/panel_background_2x.png
        design/assets/2x/knob_large_2x.png      # DRIVE/WOW/FLUTTER/NOISE/FAILURE/MIX/OUTPUT
        design/assets/2x/knob_small_2x.png      # LP/RAMP/HP
        design/assets/2x/knob_model_2x.png      # 9 frames, one per tape model
        design/assets/2x/btn_fade_on_2x.png     design/assets/2x/btn_fade_off_2x.png
        design/assets/2x/btn_clunk_on_2x.png    design/assets/2x/btn_clunk_off_2x.png
        design/assets/2x/btn_tape_on_2x.png     design/assets/2x/btn_tape_off_2x.png
        design/assets/2x/btn_vcr_on_2x.png      design/assets/2x/btn_vcr_off_2x.png
        design/assets/2x/btn_dust_on_2x.png     design/assets/2x/btn_dust_off_2x.png
        design/assets/2x/btn_off_on_2x.png      design/assets/2x/btn_off_off_2x.png
        design/assets/2x/btn_on_on_2x.png       design/assets/2x/btn_on_off_2x.png
        design/assets/2x/btn_linked_on_2x.png   design/assets/2x/btn_linked_off_2x.png
        design/assets/2x/btn_stereo_on_2x.png   design/assets/2x/btn_stereo_off_2x.png
        # Program header. The plate leaves both frames empty (delta v1.0.7); these carry every
        # state, including CANCEL, which replaces DELETE while a Program name is being typed.
        # One three-frame strip per Program button: both-dark / top-lit / bottom-lit. The five
        # single-face sprites these replace (btn_save_on|off, btn_delete_on|off, btn_cancel) are
        # still shipped in design/assets but are no longer build inputs - btn_cancel in particular
        # was a RELABEL, the face that said CANCEL where its neighbour said DELETE, which the
        # two-legend rule forbids.
        design/assets/2x/btn_save_strip_2x.png  design/assets/2x/btn_delete_strip_2x.png
        design/assets/2x/gen_seg_on_2x.png      design/assets/2x/gen_seg_off_2x.png
        design/assets/2x/lamp_on_2x.png         # shared: 4 fault dots + 3 FAIL buttons
        design/assets/2x/lamp_off_2x.png
        design/assets/2x/lamp_press_2x.png
        design/assets/2x/led_fail_on_2x.png     design/assets/2x/led_fail_off_2x.png
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

juce_add_plugin(TapeRot
    PRODUCT_NAME "TapeRot"
    COMPANY_NAME "Neon Foundry"
    BUNDLE_ID "com.neonfoundry.taperot"
    # Direction D2. Both are exactly square at a size the .icns accepts, so the generated icon gets
    # clean 1024 and 256 entries. JUCE builds the .icns and .ico at CONFIGURE time and these PNGs are
    # not configure dependencies - after changing the artwork, re-run cmake -B build or the previous
    # icon ships silently.
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/design/icon/taperot_icon_1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/design/icon/taperot_icon_256.png"
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
    Source/GUI/KnobFilmstrip.cpp
    Source/GUI/SpriteButton.cpp
    Source/GUI/LampStrip.cpp
    Source/GUI/PitchScope.cpp
    Source/GUI/ProgramHeader.cpp
    Source/GUI/TapeRotEditorContent.cpp)

target_compile_definitions(TapeRot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeRot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        TapeRotBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

add_subdirectory(Tests)
