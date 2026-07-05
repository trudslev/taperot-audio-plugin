#pragma once

#include "Parameters.h"
#include "DSP/Saturator.h"
#include "DSP/DegradationCore.h"
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
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    TapeRotAudioProcessor();
    ~TapeRotAudioProcessor() override;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    FailureEngine& getFailureEngine() noexcept { return failureEngine; }
    float getFailAuxDisplay() const noexcept { return failAuxDisplay.load(std::memory_order_relaxed); }
    float getStopSpeedDisplay() const noexcept { return tapeStop.getSpeedDisplay(); }
    float getGenDisplay() const noexcept { return genSmoothed.getCurrentValue(); }

    static constexpr int scopeHistorySize = 96;
    // Copies a snapshot of recent block-peak levels for the GUI scope; safe enough for a purely
    // cosmetic meter (worst case a torn read of one element on a rare repaint, no audio impact).
    void copyScopeLevels(std::array<float, scopeHistorySize>& dest, int& outWriteIndex) const noexcept
    {
        for (int i = 0; i < scopeHistorySize; ++i)
            dest[(size_t) i] = scopeLevels[(size_t) i].load(std::memory_order_relaxed);
        outWriteIndex = scopeWriteIndex.load(std::memory_order_relaxed);
    }

private:
    // Selecting a MODEL snaps DRIVE/WOW/FLUTTER/NOISE to that machine's preset (see
    // TapeModelData.h) - knobs stay adjustable afterward, every model switch just re-applies its
    // preset unconditionally. Conversely, nudging any of those four knobs away from the current
    // model's preset means the sound no longer matches a named machine, so MODEL snaps to NONE.
    // Both directions are deferred via AsyncUpdater since AudioProcessorValueTreeState's
    // parameterChanged callback isn't guaranteed to run on the message thread (it fires from
    // whichever thread changed the parameter, including host automation on the audio thread), and
    // setValueNotifyingHost is only safe to call from the message thread.
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    std::atomic<int> pendingModelPresetIndex{-1};
    std::atomic<bool> knobsMayHaveDrifted{false};
    // Suppresses the drift check while handleAsyncUpdate is itself applying a model's preset -
    // otherwise the four setValueNotifyingHost calls that apply the preset would immediately be
    // seen as "drift" and snap MODEL straight back to NONE.
    std::atomic<bool> isApplyingModelPreset{false};
    // Guards against either listener reacting to setStateInformation's apvts.replaceState(...)
    // while it's still firing per-parameter callbacks for a freshly-restored session.
    std::atomic<bool> isRestoringState{false};

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

    std::atomic<float> failAuxDisplay{0.0f};
    double displaySampleRate = 44100.0;

    std::array<std::atomic<float>, scopeHistorySize> scopeLevels{};
    std::atomic<int> scopeWriteIndex{0};

    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessor)
};
