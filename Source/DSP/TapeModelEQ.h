#pragma once

#include "TapeModelData.h"
#include <juce_dsp/juce_dsp.h>

class TapeModelEQ
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, int modelIndex);

private:
    struct ChainSet
    {
        std::vector<std::array<juce::dsp::IIR::Filter<float>, kMaxBandsPerModel>> filters;
        int numBandsActive = 0;
    };

    void buildCoefficientCache();
    void configureChain(ChainSet& chain, int modelIdx);
    void processChainInPlace(ChainSet& chain, juce::AudioBuffer<float>& buffer);
    ChainSet& getActiveChain() noexcept { return aIsActive ? chainA : chainB; }
    ChainSet& getPendingChain() noexcept { return aIsActive ? chainB : chainA; }

    double sampleRate = 44100.0;

    int activeModelIndex = 0;
    int pendingModelIndex = -1;
    bool aIsActive = true;

    ChainSet chainA, chainB;
    juce::SmoothedValue<float> crossfade{0.0f};
    std::vector<float> crossfadeScratch;

    std::array<std::array<juce::dsp::IIR::Coefficients<float>::Ptr, kMaxBandsPerModel>, kNumTapeModels> coeffCache;

    juce::AudioBuffer<float> pendingBuffer;
};
