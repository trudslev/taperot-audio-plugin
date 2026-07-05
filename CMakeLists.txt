cmake_minimum_required(VERSION 3.24)
project(TapeRot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TapeRot
    PRODUCT_NAME "TapeRot"
    COMPANY_NAME "TapeRot"
    BUNDLE_ID "com.taperot.taperot"
    PLUGIN_MANUFACTURER_CODE Trot
    PLUGIN_CODE Rota
    FORMATS AU VST3 Standalone
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR   "$ENV{HOME}/Library/Audio/Plug-Ins/Components")

target_sources(TapeRot PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/Saturator.cpp
    Source/DSP/WowFlutter.cpp
    Source/DSP/TapeModelEQ.cpp
    Source/GUI/TapeRotLookAndFeel.cpp
    Source/GUI/GenericParameterPanel.cpp)

target_compile_definitions(TapeRot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeRot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

add_subdirectory(Tests)
