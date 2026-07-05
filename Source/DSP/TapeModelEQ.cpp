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
    }
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

    crossfade.reset(sampleRate, 0.025);
    crossfade.setCurrentAndTargetValue(0.0f);
    crossfadeScratch.assign((size_t) spec.maximumBlockSize, 0.0f);

    pendingBuffer.setSize(numChannels, (int) spec.maximumBlockSize, false, false, true);

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
}

void TapeModelEQ::process(juce::AudioBuffer<float>& buffer, int modelIndex)
{
    modelIndex = juce::jlimit(0, (int) kNumTapeModels - 1, modelIndex);

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
