juce_add_console_app(TapeRotTests PRODUCT_NAME "TapeRotTests")

target_sources(TapeRotTests PRIVATE
    RealtimeSafetyTests.cpp
    CpuBaselineTests.cpp
    NumericalRobustnessTests.cpp
    InstrumentValidationTests.cpp
    InvarianceTests.cpp
    LifecycleTests.cpp
    AutomationLatencyTests.cpp
    ChannelLayoutTests.cpp
    TapeRealismTests.cpp
    GeneratorInvarianceTests.cpp
    PendingProgramLockTests.cpp
    TestMain.cpp
    ReadoutConformanceTests.cpp
    EditorWiringTests.cpp
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
    PrintedScaleTests.cpp
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
    ../Source/GUI/KnobComponent.cpp
    RotaryArcTests.cpp
    ../Source/GUI/ShoeSwitch.cpp
    ../Source/GUI/LampButtonGroup.cpp
    ../Source/GUI/PitchScope.cpp
    ../Source/GUI/TapeRotPanelBackground.cpp
    ../Source/GUI/GenerationLadder.cpp
    ../Source/GUI/MachineReadout.cpp
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
# **The core commit this casting was pinned at, resolved at CONFIGURE time and baked into the CPU
# test's provenance.** The tag moves while core is pre-release, so `v1.0.0` in a baseline file would
# record nothing — the whole value of the provenance block is that a figure can be attributed to one
# state of six castings and one state of core, which is what makes "one sitting, all six, one machine
# state" checkable after the fact rather than a promise.
execute_process(
    COMMAND git -C "${neonfoundrycore_SOURCE_DIR}" rev-parse --short HEAD
    OUTPUT_VARIABLE NF_CORE_PIN_COMMIT_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT NF_CORE_PIN_COMMIT_VALUE)
    set(NF_CORE_PIN_COMMIT_VALUE "unknown")
endif()

target_compile_definitions(TapeRotTests PRIVATE
    # The test target compiles the GUI sources, and the panel footer reads this.
    NF_VERSION_SHORT="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    NF_CORE_PIN_COMMIT="${NF_CORE_PIN_COMMIT_VALUE}"
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
