#pragma once

#include "TapeModelData.h"
#include <juce_dsp/juce_dsp.h>

// Model EQ with two switching behaviors, selected per-block by the shared SWITCH parameter:
//  - FADE: ~80ms linear crossfade between outgoing/incoming filter chains (unchanged mechanism
//    from before, just a longer crossfade time and per-model makeup gain added).
//  - CLUNK: hard coefficient swap timed at the bottom of a short (~8ms) mute dip, plus a decaying
//    low-frequency thump scaled by recent signal level (hard-limited so it can never clip).
// Both run identically on every DegradationCore/GEN stage since each instance reacts to the same
// shared model/clunkMode values every block - no cross-stage coordination is needed for them to
// switch in sync.
class TapeModelEQ
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, int modelIndex, bool clunkMode);

private:
    struct ChainSet
    {
        std::vector<std::array<juce::dsp::IIR::Filter<float>, kMaxBandsPerModel>> filters;
        int numBandsActive = 0;
        float makeupGain = 1.0f;
    };

    void buildCoefficientCache();
    void configureChain(ChainSet& chain, int modelIdx);
    void processChainInPlace(ChainSet& chain, juce::AudioBuffer<float>& buffer);
    float processActiveChainSample(int channel, float x) noexcept;
    ChainSet& getActiveChain() noexcept { return aIsActive ? chainA : chainB; }
    ChainSet& getPendingChain() noexcept { return aIsActive ? chainB : chainA; }

    void processFade(juce::AudioBuffer<float>& buffer, int modelIndex);
    void processClunk(juce::AudioBuffer<float>& buffer, int modelIndex);

    double sampleRate = 44100.0;

    int activeModelIndex = 0;
    int pendingModelIndex = -1;
    bool aIsActive = true;

    ChainSet chainA, chainB;
    juce::SmoothedValue<float> crossfade{0.0f};
    std::vector<float> crossfadeScratch;

    std::array<std::array<juce::dsp::IIR::Coefficients<float>::Ptr, kMaxBandsPerModel>, kNumTapeModels> coeffCache;

    juce::AudioBuffer<float> pendingBuffer;

    // CLUNK mode state.
    static constexpr float clunkDipMs = 8.0f;
    static constexpr float thumpFreqHz = 60.0f;
    static constexpr float thumpDecaySeconds = 0.12f;
    static constexpr float thumpBaseLevel = 0.15f; // linear, tune here
    static constexpr float signalFollowerMs = 15.0f;

    bool clunkDipActive = false;
    int clunkDipSamplesRemaining = 0;
    int clunkDipTotalSamples = 1;
    int clunkSwitchSampleIndex = 0;
    int pendingClunkModelIndex = -1;

    float thumpPhase = 0.0f;
    float thumpEnvelope = 0.0f;
    float thumpDecayCoeff = 0.0f;
    float thumpPhaseInc = 0.0f;

    float signalFollower = 0.0f;
    float signalFollowerCoeff = 0.0f;
};
