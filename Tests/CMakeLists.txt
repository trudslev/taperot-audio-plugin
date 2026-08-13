juce_add_console_app(TapeRotTests PRODUCT_NAME "TapeRotTests")

target_sources(TapeRotTests PRIVATE
    RealtimeSafetyTests.cpp
    NumericalRobustnessTests.cpp
    TestMain.cpp
    ReadoutConformanceTests.cpp
    MenuMetricsTests.cpp
    SaturatorTests.cpp
    WowFlutterTests.cpp
    TapeModelEQTests.cpp
    CPUCheckTests.cpp
    FailureEngineFifoTests.cpp
    NoiseSourceTests.cpp
    ToneFiltersTests.cpp
    AuxEffectsTests.cpp
    ParametersStateTests.cpp
    ParameterTextTests.cpp
    GenerationCascadeTests.cpp
    FilmstripConformanceTests.cpp
    MeteringTests.cpp
    TapeModelCalibrationTests.cpp
    TapeModelFrequencyResponseTests.cpp
    ModelSwitchAutomationTests.cpp
    FactoryProgramsTests.cpp
    SessionCompatibilityTests.cpp
    ProgramIdentityTests.cpp
    # The REAL processor, not a stand-in. ProgramIdentityTests drives setStateInformation and
    # setCurrentProgram in the order a host uses them, and a harness that reimplemented that
    # ordering would be asserting against a copy of the code rather than the code.
    ../Source/PluginProcessor.cpp
    ../Source/PluginEditor.cpp
    ../Source/GUI/KnobFilmstrip.cpp
    ../Source/GUI/SpriteButton.cpp
    ../Source/GUI/LampStrip.cpp
    ../Source/GUI/PitchScope.cpp
    ../Source/GUI/ProgramHeader.cpp
    ../Source/GUI/TapeRotEditorContent.cpp
    ../Source/DSP/Hum.cpp
    ../Source/DSP/FailureEngine.cpp
    ../Source/DSP/StereoSpread.cpp
    ../Source/DSP/OutputStage.cpp
    ../Source/DSP/Saturator.cpp
    ../Source/DSP/WowFlutter.cpp
    ../Source/DSP/TapeModelEQ.cpp
    ../Source/DSP/NoiseSource.cpp
    ../Source/DSP/DegradationCore.cpp
    ../Source/DSP/ToneFilters.cpp
    ../Source/DSP/TapeStop.cpp
    ../Source/DSP/FilterSweep.cpp
)

# The plugin target gets these from its generated JuceHeader; a console app does not, and
# PluginProcessor.cpp needs them for the user-Program directory.
# NF_COMPANY_NAME/NF_PRODUCT_NAME come from the parent CMakeLists and must match what the plugin
# target gets - PluginProcessor.cpp is compiled into BOTH, and builds the user-Programs path from
# them. Inherited from the parent scope rather than restated here, so there is nothing to re-sync:
# a hand-synced copy in CHORUS-60 drifted to a dead company name and quietly pointed saved Programs
# at a directory nothing reads.
#
# The JucePlugin_* pair stays because juce_add_console_app does not define them and PluginProcessor
# still uses JucePlugin_Name for getName().
target_compile_definitions(TapeRotTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="${TAPEROT_PRODUCT_NAME}"
    JucePlugin_Manufacturer="${TAPEROT_COMPANY_NAME}"
    NF_COMPANY_NAME="${TAPEROT_COMPANY_NAME}"
    NF_PRODUCT_NAME="${TAPEROT_PRODUCT_NAME}")

target_link_libraries(TapeRotTests PRIVATE
    # The bug sweep's measurement drivers. TEST target only — never the plugin, which must not
    # carry an operator new override. See nf/testing/ProcessorHarness.h.
    nf::testing
    # PluginProcessor.cpp is compiled into both targets and calls into core, so both link it.
    nf::core
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_core
    TapeRotBinaryData
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
