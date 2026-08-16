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
    /*  **`initialModelIndex` is an argument, and that is the fix rather than a convenience.**

        This used to set `activeModelIndex = 0` — NONE — on every prepare, while the default Program
        selects 5 (CASSETTE I). So the first block after ANY prepare found the requested model
        different from the stored one and started a transition nobody asked for: measured at 26.75 %
        of peak in FADE and **97.55 % in CLUNK**, which is the mute dip.

        It is worse than a first-run defect because a host re-fires `prepareToPlay` on every
        sample-rate and buffer-size change, not once per instance — and category 3's
        cold-against-warmed comparison is structurally blind to it, since both its arms prepare.

        **Passing the value in is what makes the defect unexpressible.** Setting it afterwards
        through a setter would work equally well today and would be a line somebody has to remember,
        which is the shape of the Reflect-84 disarm that shipped with zero call sites. Elmer's
        `OutputStage::prepare` took the same form in this stage for the same reason. */
    void prepare(const juce::dsp::ProcessSpec& spec, int initialModelIndex);
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
    // If MODEL changes again while a crossfade is already in flight, the new target is queued
    // here rather than immediately reconfiguring the in-progress pending chain: swapping its
    // coefficients mid-fade (with stale filter history from whatever it was fading from) and
    // snapping the blend position back to 0 both produce an audible jump. The queued target is
    // picked up once the current crossfade settles.
    int queuedModelIndex = -1;
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
