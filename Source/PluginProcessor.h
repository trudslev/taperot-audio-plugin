#pragma once

#include "Parameters.h"
#include "DSP/FactoryPresets.h"
#include "DSP/Saturator.h"
#include "DSP/DegradationCore.h"
#include "DSP/PitchDeviationMeter.h"
#include "DSP/WowFlutter.h"
#include "DSP/Hum.h"
#include "DSP/FailureEngine.h"
#include "DSP/StereoSpread.h"
#include "DSP/ToneFilters.h"
#include "DSP/TapeStop.h"
#include "DSP/FilterSweep.h"
#include "DSP/AuxEnvelope.h"
#include "DSP/OutputStage.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

class TapeRotAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AsyncUpdater
{
public:
    TapeRotAudioProcessor();
    ~TapeRotAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgramIndex.load(std::memory_order_relaxed); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Factory presets (indices [0, kNumFactoryPresets)) are the read-only, always-present entries
    // in kFactoryPresets; user presets (indices [kNumFactoryPresets, getNumPrograms())) are files
    // in getUserPresetDirectory(), sorted alphabetically by filename. "Save" is never in-place for
    // a factory preset - the GUI's Save always calls saveUserPreset, which creates a new file.
    bool isFactoryPreset(int index) const noexcept { return index >= 0 && index < (int) kNumFactoryPresets; }
    void saveUserPreset(const juce::String& name);
    void deleteUserPreset(int index);

    juce::AudioProcessorValueTreeState apvts;

    FailureEngine& getFailureEngine() noexcept { return failureEngine; }
    float getFailAuxDisplay() const noexcept { return failAuxDisplay.load(std::memory_order_relaxed); }
    float getStopSpeedDisplay() const noexcept { return tapeStop.getSpeedDisplay(); }
    float getGenDisplay() const noexcept { return genSmoothed.getCurrentValue(); }

    // --- scope and header metering -------------------------------------------------------------
    // All read-only taps. None of them changes how anything processes audio.
    PitchDeviationMeter& getPitchDeviationMeter() noexcept { return pitchMeter; }
    double getPitchDeviationRate() const noexcept
    {
        return PitchDeviationMeter::outputRate(displaySampleRate);
    }
    float getWowRateHz() const noexcept { return wowRateDisplay.load(std::memory_order_relaxed); }
    float getFlutterRateHz() const noexcept { return flutterRateDisplay.load(std::memory_order_relaxed); }
    float getInputLevelDb() const noexcept { return inputLevelDb.load(std::memory_order_relaxed); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load(std::memory_order_relaxed); }


private:
    // Applying a factory/user preset (see setCurrentProgram) sets every parameter via
    // setValueNotifyingHost, which is message-thread-only - but a host can call setCurrentProgram
    // itself from a non-message thread (VST3 delivers program-change as an ordinary automatable
    // parameter, which can arrive on the audio/process thread during playback automation). So the
    // actual application is deferred through AsyncUpdater: setCurrentProgram just records which
    // program is pending and triggers an async update; handleAsyncUpdate (guaranteed message
    // thread) does the real work.
    void handleAsyncUpdate() override;
    std::atomic<int> pendingProgramIndex{-1};

    void applyProgramByIndex(int index);
    void applyFactoryPreset(const FactoryPreset& preset);
    void refreshUserPresetList();
    static juce::File getUserPresetDirectory();

    std::atomic<int> currentProgramIndex{(int) warmCassetteProgramIndex};
    // Sorted alphabetically by filename (stable across relaunches, unlike mtime-sort). Index i in
    // this array is program index kNumFactoryPresets + i.
    juce::Array<juce::File> userPresetFiles;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* modelParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* noiseCharacterParam = nullptr;
    std::atomic<float>* humParam = nullptr;
    std::atomic<float>* failureParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* failureDropoutsParam = nullptr;
    std::atomic<float>* failureSnagsParam = nullptr;
    std::atomic<float>* failureCrinklesParam = nullptr;
    std::atomic<float>* failureImbalanceParam = nullptr;
    std::atomic<float>* genParam = nullptr;
    std::atomic<float>* lpParam = nullptr;
    std::atomic<float>* hpParam = nullptr;
    std::atomic<float>* stopParam = nullptr;
    std::atomic<float>* filterAuxParam = nullptr;
    std::atomic<float>* failAuxParam = nullptr;
    std::atomic<float>* rampParam = nullptr;
    std::atomic<float>* switchModeParam = nullptr;

    static constexpr int maxGenerations = 8;

    Saturator saturator;
    std::array<std::unique_ptr<DegradationCore>, maxGenerations> generationStages;

    PitchDeviationMeter pitchMeter;
    std::atomic<float> wowRateDisplay{0.0f};
    std::atomic<float> flutterRateDisplay{0.0f};
    std::atomic<float> inputLevelDb{-99.9f};
    std::atomic<float> outputLevelDb{-99.9f};
    float inLevelSmoothed = 0.0f;
    float outLevelSmoothed = 0.0f;
    float levelSmoothingCoeff = 0.0f;
    juce::SmoothedValue<float> genSmoothed{1.0f};
    juce::AudioBuffer<float> genFloorSnapshot;

    Hum hum;
    FailureEngine failureEngine;
    StereoSpread stereoSpread;
    ToneFilters toneFilters;
    TapeStop tapeStop;
    FilterSweep filterSweep;
    AuxEnvelope failEnvelope;
    OutputStage outputStage;

    // NoiseSource/Hum are unconditional generators (real tape self-noise, not signal-dependent),
    // so they hiss even with the host transport stopped feeding silent buffers. Smoothly fades the
    // whole output to silence while the host reports not-playing, and back in on play, rather than
    // a hard mute - avoids a click at the transport boundary. Only engages when a host actually
    // reports transport state (getPlayHead()/getPosition() both present) - Standalone has no
    // transport concept, so it's never gated there.
    juce::SmoothedValue<float> transportGateSmoothed{1.0f};

    std::atomic<float> failAuxDisplay{0.0f};
    double displaySampleRate = 44100.0;


    juce::AudioBuffer<float> dryBuffer;

    // Each active GEN cascade stage's WowFlutter centers its pitch-modulation delay line at
    // WowFlutter::nominalDelayMs even at wow=flutter=0 (it's not an optional effect - the delay
    // line IS the modulation mechanism), so the wet path always lags the live input by that much
    // per stage, up to ~200ms at GEN=8. dryBuffer was previously mixed in unshifted, so MIX blended
    // a live dry sample against a wet sample that was actually up to ~200ms old - this delay line
    // re-times the dry copy to match, tracking genSmoothed's current (possibly mid-transition)
    // value each block. It only needs to track the *nominal* per-stage delay, not the wow/flutter
    // modulation itself - the modulation's own smearing between dry and wet is inherent to the
    // effect and can't be undone without defeating it.
    juce::dsp::DelayLine<float> dryCompensationDelay{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessor)
};
