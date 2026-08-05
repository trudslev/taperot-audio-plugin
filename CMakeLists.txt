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

juce_add_binary_data(TapeRotBinaryData
    SOURCES
        design/dymo/taperot-dymo@3x.png
        design/inter/Inter-Regular.ttf
        design/inter/Inter-Bold.ttf)

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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/design/icon/taperot-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/design/icon/taperot-256.png"
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
    Source/GUI/TapeRotLookAndFeel.cpp
    Source/GUI/DymoLabel.cpp
    Source/GUI/SectionPanel.cpp
    Source/GUI/ToggleSwitch.cpp
    Source/GUI/FailureDotToggle.cpp
    Source/GUI/NoiseCharacterSwitch.cpp
    Source/GUI/SmallKnob.cpp
    Source/GUI/GenSelector.cpp
    Source/GUI/AuxButton.cpp
    Source/GUI/FailLamp.cpp
    Source/GUI/Scope.cpp
    Source/GUI/GenDigitDisplay.cpp
    Source/GUI/ModelReadout.cpp
    Source/GUI/PresetArrowButton.cpp
    Source/GUI/PresetSaveButton.cpp
    Source/GUI/PresetDeleteButton.cpp
    Source/GUI/PresetStrip.cpp
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
