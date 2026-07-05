#include "TapeModelEQ.h"

namespace
{
    juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients(double sampleRate, const EQBand& band)
    {
        const float gain = juce::Decibels::decibelsToGain(band.gainDb);
        const float freqHz = juce::jmin(band.freqHz, (float) (sampleRate * 0.49));
        switch (band.type)
        {
            case EQBandType::LowShelf:  return juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, freqHz, band.q, gain);
            case EQBandType::HighShelf: return juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, freqHz, band.q, gain);
            case EQBandType::Peak:      return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, freqHz, band.q, gain);
            case EQBandType::LowPass:   return juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freqHz, band.q);
            case EQBandType::HighPass:  return juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freqHz, band.q);
        }
        return juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, freqHz, band.q);
    }
}

void TapeModelEQ::buildCoefficientCache()
{
    for (size_t m = 0; m < kNumTapeModels; ++m)
    {
        const auto& model = kTapeModels[m];
        for (int b = 0; b < model.numBands; ++b)
            coeffCache[m][(size_t) b] = makeCoefficients(sampleRate, model.bands[(size_t) b]);
    }
}

void TapeModelEQ::configureChain(ChainSet& chain, int modelIdx)
{
    const auto& model = kTapeModels[(size_t) modelIdx];
    for (auto& perChannel : chain.filters)
        for (int b = 0; b < model.numBands; ++b)
            perChannel[(size_t) b].coefficients = coeffCache[(size_t) modelIdx][(size_t) b];
    chain.numBandsActive = model.numBands;
    chain.makeupGain = juce::Decibels::decibelsToGain(model.makeupGainDb);
}

void TapeModelEQ::processChainInPlace(ChainSet& chain, juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin(buffer.getNumChannels(), (int) chain.filters.size());
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& bands = chain.filters[(size_t) ch];
        for (int b = 0; b < chain.numBandsActive; ++b)
        {
            auto& filter = bands[(size_t) b];
            for (int i = 0; i < numSamples; ++i)
                data[i] = filter.processSample(data[i]);
        }
        if (chain.makeupGain != 1.0f)
            for (int i = 0; i < numSamples; ++i)
                data[i] *= chain.makeupGain;
    }
}

float TapeModelEQ::processActiveChainSample(int channel, float x) noexcept
{
    auto& chain = getActiveChain();
    auto& bands = chain.filters[(size_t) channel];
    float y = x;
    for (int b = 0; b < chain.numBandsActive; ++b)
        y = bands[(size_t) b].processSample(y);
    return y * chain.makeupGain;
}

void TapeModelEQ::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const int numChannels = (int) spec.numChannels;

    buildCoefficientCache();

    chainA.filters.resize((size_t) numChannels);
    chainB.filters.resize((size_t) numChannels);

    activeModelIndex = 0;
    pendingModelIndex = -1;
    aIsActive = true;

    configureChain(chainA, activeModelIndex);
    configureChain(chainB, activeModelIndex);

    crossfade.reset(sampleRate, 0.08);
    crossfade.setCurrentAndTargetValue(0.0f);
    crossfadeScratch.assign((size_t) spec.maximumBlockSize, 0.0f);

    pendingBuffer.setSize(numChannels, (int) spec.maximumBlockSize, false, false, true);

    clunkDipTotalSamples = juce::jmax(1, (int) std::round(clunkDipMs * 0.001f * (float) sampleRate));
    thumpDecayCoeff = std::exp(-1.0f / (thumpDecaySeconds * (float) sampleRate));
    thumpPhaseInc = juce::MathConstants<float>::twoPi * thumpFreqHz / (float) sampleRate;
    signalFollowerCoeff = 1.0f - std::exp(-1.0f / (signalFollowerMs * 0.001f * (float) sampleRate));

    reset();
}

void TapeModelEQ::reset()
{
    for (auto& perChannel : chainA.filters)
        for (auto& f : perChannel)
            f.reset();
    for (auto& perChannel : chainB.filters)
        for (auto& f : perChannel)
            f.reset();

    clunkDipActive = false;
    clunkDipSamplesRemaining = 0;
    pendingClunkModelIndex = -1;
    thumpPhase = 0.0f;
    thumpEnvelope = 0.0f;
    signalFollower = 0.0f;
}

void TapeModelEQ::process(juce::AudioBuffer<float>& buffer, int modelIndex, bool clunkMode)
{
    modelIndex = juce::jlimit(0, (int) kNumTapeModels - 1, modelIndex);

    if (clunkMode)
        processClunk(buffer, modelIndex);
    else
        processFade(buffer, modelIndex);
}

void TapeModelEQ::processFade(juce::AudioBuffer<float>& buffer, int modelIndex)
{
    if (modelIndex != activeModelIndex && modelIndex != pendingModelIndex)
    {
        pendingModelIndex = modelIndex;
        configureChain(getPendingChain(), pendingModelIndex);
        crossfade.setCurrentAndTargetValue(0.0f);
        crossfade.setTargetValue(1.0f);
    }

    if (pendingModelIndex == -1)
    {
        processChainInPlace(getActiveChain(), buffer);
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    pendingBuffer.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        pendingBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    processChainInPlace(getActiveChain(), buffer);
    processChainInPlace(getPendingChain(), pendingBuffer);

    for (int i = 0; i < numSamples; ++i)
        crossfadeScratch[(size_t) i] = crossfade.getNextValue();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* activeData = buffer.getWritePointer(ch);
        auto* pendingData = pendingBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float blend = crossfadeScratch[(size_t) i];
            activeData[i] = activeData[i] * (1.0f - blend) + pendingData[i] * blend;
        }
    }

    if (crossfade.getCurrentValue() >= 1.0f - 1.0e-6f && crossfade.getTargetValue() >= 1.0f - 1.0e-6f)
    {
        activeModelIndex = pendingModelIndex;
        pendingModelIndex = -1;
        aIsActive = !aIsActive;
    }
}

void TapeModelEQ::processClunk(juce::AudioBuffer<float>& buffer, int modelIndex)
{
    // If a FADE crossfade was left in progress when switching into CLUNK mode, snap it so
    // getActiveChain() reflects one settled model before hard-swapping further.
    if (pendingModelIndex != -1)
    {
        activeModelIndex = pendingModelIndex;
        pendingModelIndex = -1;
    }

    if (!clunkDipActive && modelIndex != activeModelIndex)
    {
        clunkDipActive = true;
        clunkDipSamplesRemaining = clunkDipTotalSamples;
        clunkSwitchSampleIndex = clunkDipTotalSamples / 2;
        pendingClunkModelIndex = modelIndex;

        thumpPhase = 0.0f;
        thumpEnvelope = thumpBaseLevel * juce::jlimit(0.0f, 1.0f, signalFollower / 0.3f);
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    constexpr int kMaxChannels = 2;
    std::array<float*, kMaxChannels> data{};
    const int activeChannels = juce::jmin(numCh, kMaxChannels);
    for (int ch = 0; ch < activeChannels; ++ch)
        data[(size_t) ch] = buffer.getWritePointer(ch);

    for (int i = 0; i < numSamples; ++i)
    {
        // Sample-by-sample (cascaded across bands per sample, not per-band-across-the-buffer like
        // processChainInPlace) so the coefficient swap below can land exactly mid-buffer.
        for (int ch = 0; ch < activeChannels; ++ch)
            data[(size_t) ch][i] = processActiveChainSample(ch, data[(size_t) ch][i]);

        float absSum = 0.0f;
        for (int ch = 0; ch < activeChannels; ++ch)
            absSum += std::abs(data[(size_t) ch][i]);
        const float absAvg = activeChannels > 0 ? absSum / (float) activeChannels : 0.0f;
        signalFollower += signalFollowerCoeff * (absAvg - signalFollower);

        float dipGain = 1.0f;
        if (clunkDipActive)
        {
            const int elapsed = clunkDipTotalSamples - clunkDipSamplesRemaining;
            const float t = (float) elapsed / (float) clunkDipTotalSamples;
            dipGain = 1.0f - std::sin(juce::MathConstants<float>::pi * t);

            if (elapsed == clunkSwitchSampleIndex)
            {
                configureChain(getActiveChain(), pendingClunkModelIndex);
                activeModelIndex = pendingClunkModelIndex;
            }

            if (--clunkDipSamplesRemaining <= 0)
                clunkDipActive = false;
        }

        float thumpSample = 0.0f;
        if (thumpEnvelope > 1.0e-4f)
        {
            thumpSample = thumpEnvelope * std::sin(thumpPhase);
            thumpPhase += thumpPhaseInc;
            if (thumpPhase > juce::MathConstants<float>::twoPi)
                thumpPhase -= juce::MathConstants<float>::twoPi;
            thumpEnvelope *= thumpDecayCoeff;
        }

        for (int ch = 0; ch < activeChannels; ++ch)
            data[(size_t) ch][i] = juce::jlimit(-1.0f, 1.0f, data[(size_t) ch][i] * dipGain + thumpSample);
    }
}
