juce_add_console_app(TapeRotTests PRODUCT_NAME "TapeRotTests")

target_sources(TapeRotTests PRIVATE
    TestMain.cpp
    SaturatorTests.cpp
    WowFlutterTests.cpp
    TapeModelEQTests.cpp
    CPUCheckTests.cpp
    FailureEngineFifoTests.cpp
    NoiseSourceTests.cpp
    ../Source/DSP/Saturator.cpp
    ../Source/DSP/WowFlutter.cpp
    ../Source/DSP/TapeModelEQ.cpp
    ../Source/DSP/NoiseSource.cpp)

target_compile_definitions(TapeRotTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(TapeRotTests PRIVATE
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
