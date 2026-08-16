#pragma once

#include <juce_dsp/juce_dsp.h>

class Saturator
{
public:
    /*  **Initial values are ARGUMENTS, because `reset (rate, seconds)` does not set one.**

        It is `setCurrentAndTargetValue (this->target)` internally: it sets the ramp LENGTH and snaps
        the value to whatever target the smoother last held — zero on a constructed object. So a
        stage prepared without being told where its control sits glides up from nothing across the
        first block of an instance's first playback. Measured on this casting at **-16.6 dB over the
        first 5 ms**, which is the release blocker of the eleven.

        And the value must come from the CALLER. Several of these sites carried
        `setCurrentAndTargetValue (getTargetValue())`, which is character for character what
        `reset (rate, seconds)` already did — a guard that reads the stale target back and writes it
        in. It reads as guarded, which is worse than nothing being there, and it is why the count was
        first reported as fourteen-in-four and is really twelve-in-three.

        An argument makes the omission unexpressible rather than something the next edit has to
        remember, which is the form Elmer's `OutputStage::prepare` took earlier in this stage. */
    void prepare(const juce::dsp::ProcessSpec& spec, float initialDrive01);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float driveTarget01);

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static constexpr float preShelfFreqHz = 3500.0f;
    static constexpr float shelfQ = 0.707f;
    static constexpr float maxShelfBoostDb = 18.0f;
    static constexpr float maxDriveGain = 12.0f;
    // Applied to driveNorm (physical DRIVE/100) only where driveGain is computed, not to
    // driveGainScratch itself (also used for the dry/wet crossfade further down, which should
    // stay tied to the raw physical percent). >1 gentles the gain ramp in the low-mid range while
    // leaving both ends unchanged (driveNorm=0 -> gain=1, driveNorm=1 -> gain=maxDriveGain still)
    // - keeps the "crazy" top end exactly as crazy, just reached later in the physical range.
    static constexpr float driveCurveExponent = 2.0f;
    static constexpr int numDriveSteps = 41;

    void buildCoefficientCache();

    double sampleRate = 44100.0;
    int numChannels = 2;
    int latencySamples = 0;

    juce::SmoothedValue<float> driveSmoothed{0.0f};

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numDriveSteps> preShelfCoeffCache;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numDriveSteps> postShelfCoeffCache;

    std::vector<juce::dsp::IIR::Filter<float>> preShelfFilters;
    std::vector<juce::dsp::IIR::Filter<float>> postShelfFilters;

    juce::dsp::DelayLine<float> dryCompensationDelay{192};

    juce::AudioBuffer<float> dryBuffer;
    std::vector<float> driveGainScratch;
    bool wasBypassed = true;
};
