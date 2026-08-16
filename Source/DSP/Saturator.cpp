#include "Saturator.h"

void Saturator::buildCoefficientCache()
{
    for (int i = 0; i < numDriveSteps; ++i)
    {
        const float norm = (float) i / (float) (numDriveSteps - 1);
        const float boostDb = norm * maxShelfBoostDb;

        preShelfCoeffCache[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, preShelfFreqHz, shelfQ, juce::Decibels::decibelsToGain(boostDb));

        postShelfCoeffCache[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, preShelfFreqHz, shelfQ, juce::Decibels::decibelsToGain(-boostDb));
    }
}

void Saturator::prepare(const juce::dsp::ProcessSpec& spec, float initialDrive01)
{
    sampleRate = spec.sampleRate;
    numChannels = (int) spec.numChannels;

    driveSmoothed.reset(sampleRate, 0.02);
    driveSmoothed.setCurrentAndTargetValue(initialDrive01);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, true);
    oversampling->initProcessing((size_t) spec.maximumBlockSize);
    oversampling->reset();
    latencySamples = (int) oversampling->getLatencyInSamples();

    buildCoefficientCache();

    preShelfFilters.resize((size_t) numChannels);
    postShelfFilters.resize((size_t) numChannels);
    for (auto& f : preShelfFilters)
        f.coefficients = preShelfCoeffCache[0];
    for (auto& f : postShelfFilters)
        f.coefficients = postShelfCoeffCache[0];

    juce::dsp::ProcessSpec dryDelaySpec{spec.sampleRate, spec.maximumBlockSize, (juce::uint32) numChannels};
    dryCompensationDelay.setMaximumDelayInSamples(juce::jmax(1, latencySamples));
    dryCompensationDelay.prepare(dryDelaySpec);
    dryCompensationDelay.setDelay((float) latencySamples);

    dryBuffer.setSize(numChannels, (int) spec.maximumBlockSize, false, false, true);
    driveGainScratch.assign((size_t) spec.maximumBlockSize, 0.0f);

    reset();
}

void Saturator::reset()
{
    driveSmoothed.setCurrentAndTargetValue(driveSmoothed.getTargetValue());
    for (auto& f : preShelfFilters)
        f.reset();
    for (auto& f : postShelfFilters)
        f.reset();
    dryCompensationDelay.reset();
    if (oversampling != nullptr)
        oversampling->reset();
}

void Saturator::process(juce::AudioBuffer<float>& buffer, float driveTarget01)
{
    driveSmoothed.setTargetValue(driveTarget01);

    const bool bypassed = driveTarget01 <= 0.0f && driveSmoothed.getCurrentValue() <= 0.0f;
    if (bypassed)
    {
        wasBypassed = true;
        return;
    }

    if (wasBypassed)
    {
        for (auto& f : preShelfFilters) f.reset();
        for (auto& f : postShelfFilters) f.reset();
        dryCompensationDelay.reset();
        oversampling->reset();
        wasBypassed = false;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    const int stepIndex = juce::jlimit(0, numDriveSteps - 1,
        (int) std::round(driveSmoothed.getTargetValue() * (float) (numDriveSteps - 1)));

    for (auto& f : preShelfFilters)
        f.coefficients = preShelfCoeffCache[(size_t) stepIndex];
    for (auto& f : postShelfFilters)
        f.coefficients = postShelfCoeffCache[(size_t) stepIndex];

    for (int i = 0; i < numSamples; ++i)
        driveGainScratch[(size_t) i] = driveSmoothed.getNextValue();

    dryBuffer.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
    {
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        auto* dry = dryBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            dryCompensationDelay.pushSample(ch, dry[i]);
            dry[i] = dryCompensationDelay.popSample(ch);
        }
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = preShelfFilters[(size_t) ch].processSample(data[i]);
    }

    juce::dsp::AudioBlock<float> baseBlock(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(baseBlock);
    const int overSamples = (int) oversampledBlock.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer((size_t) ch);
        for (int i = 0; i < overSamples; ++i)
        {
            const int baseIndex = juce::jmin(numSamples - 1, i * numSamples / overSamples);
            const float driveNorm = driveGainScratch[(size_t) baseIndex];
            const float shapedDriveNorm = std::pow(driveNorm, driveCurveExponent);
            const float driveGain = juce::jmax(1.0e-3f, 1.0f + shapedDriveNorm * (maxDriveGain - 1.0f));
            data[i] = std::tanh(data[i] * driveGain) / std::tanh(driveGain);
        }
    }

    oversampling->processSamplesDown(baseBlock);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = postShelfFilters[(size_t) ch].processSample(data[i]);
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float theta = driveGainScratch[(size_t) i] * juce::MathConstants<float>::halfPi;
            const float wetGain = std::sin(theta);
            const float dryGain = std::cos(theta);
            wet[i] = dry[i] * dryGain + wet[i] * wetGain;
        }
    }
}
