#pragma once

#include "Parameters.h"
#include "DSP/Saturator.h"
#include "DSP/WowFlutter.h"
#include "DSP/TapeModelEQ.h"
#include "DSP/NoiseSource.h"
#include "DSP/Hum.h"
#include "DSP/FailureEngine.h"
#include "DSP/StereoSpread.h"
#include "DSP/OutputStage.h"
#include <juce_audio_processors/juce_audio_processors.h>

class TapeRotAudioProcessor final : public juce::AudioProcessor
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    FailureEngine& getFailureEngine() noexcept { return failureEngine; }
    float getWowDisplay() const noexcept { return wowDisplay.load(std::memory_order_relaxed); }
    float getFlutterDisplay() const noexcept { return flutterDisplay.load(std::memory_order_relaxed); }
    float getFailureDisplay() const noexcept { return failureDisplay.load(std::memory_order_relaxed); }

private:
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

    Saturator saturator;
    WowFlutter wowFlutter;
    TapeModelEQ tapeModelEQ;
    NoiseSource noiseSource;
    Hum hum;
    FailureEngine failureEngine;
    StereoSpread stereoSpread;
    OutputStage outputStage;

    std::atomic<float> wowDisplay{0.0f};
    std::atomic<float> flutterDisplay{0.0f};
    std::atomic<float> failureDisplay{0.0f};
    double displaySampleRate = 44100.0;

    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessor)
};
