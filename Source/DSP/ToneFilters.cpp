#include "ToneFilters.h"

void ToneFilters::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    nyquistSafeHz = (float) (sampleRate * 0.49);

    channels.resize((size_t) spec.numChannels);
    for (auto& c : channels)
    {
        c.lowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, nyquistSafeHz);
        c.highPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, minFilterHz);
    }

    lpSmoothed.reset(sampleRate, smoothingSeconds);
    hpSmoothed.reset(sampleRate, smoothingSeconds);
    lpSmoothed.setCurrentAndTargetValue(nyquistSafeHz);
    hpSmoothed.setCurrentAndTargetValue(minFilterHz);

    reset();
}

void ToneFilters::reset()
{
    for (auto& c : channels)
    {
        c.lowPass.reset();
        c.highPass.reset();
    }
}

void ToneFilters::process(juce::AudioBuffer<float>& buffer, float lpHz, float hpHz)
{
    lpSmoothed.setTargetValue(juce::jmin(lpHz, nyquistSafeHz));
    hpSmoothed.setTargetValue(juce::jmax(hpHz, minFilterHz));

    const int numSamples = buffer.getNumSamples();
    lpSmoothed.skip(numSamples);
    hpSmoothed.skip(numSamples);

    const float lpCutoff = lpSmoothed.getCurrentValue();
    const float hpCutoff = hpSmoothed.getCurrentValue();

    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpCutoff);
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpCutoff);

    const int numCh = juce::jmin(buffer.getNumChannels(), (int) channels.size());
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& cf = channels[(size_t) ch];
        cf.lowPass.coefficients = lpCoeffs;
        cf.highPass.coefficients = hpCoeffs;

        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = cf.lowPass.processSample(cf.highPass.processSample(data[i]));
    }
}
