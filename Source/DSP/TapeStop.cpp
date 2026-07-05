#include "TapeStop.h"
#include <algorithm>

void TapeStop::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    bufferLengthSamples = juce::jmax(1, (int) std::round(bufferSeconds * sampleRate));

    history.assign((size_t) spec.numChannels, std::vector<float>((size_t) bufferLengthSamples, 0.0f));
    readPos.assign((size_t) spec.numChannels, 0.0);

    envelope.setSampleRate(sampleRate);

    reset();
}

void TapeStop::reset()
{
    for (auto& ch : history)
        std::fill(ch.begin(), ch.end(), 0.0f);
    for (auto& p : readPos)
        p = 0.0;
    writePos = 0;
    wobblePhase = 0.0;
}

void TapeStop::process(juce::AudioBuffer<float>& buffer, bool engaged, float rampSeconds)
{
    envelope.setEngaged(engaged);
    envelope.setRampSeconds(rampSeconds);

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), (int) history.size());
    const double wobblePhaseInc = juce::MathConstants<double>::twoPi * wobbleRateHz / sampleRate;
    const double maxLag = (double) bufferLengthSamples - (double) juce::jmax(1, (int) std::round(safetyMarginSeconds * (float) sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const float auxValue = envelope.getNextValue();
        const float baseSpeed = 1.0f - auxValue;
        const float wobble = 1.0f + wobbleDepth * auxValue * (float) std::sin(wobblePhase);
        const float speed = juce::jlimit(0.0f, 1.0f, baseSpeed * wobble);

        for (int ch = 0; ch < numCh; ++ch)
            history[(size_t) ch][(size_t) (writePos % bufferLengthSamples)] = buffer.getWritePointer(ch)[i];
        ++writePos;

        for (int ch = 0; ch < numCh; ++ch)
        {
            if ((double) writePos - readPos[(size_t) ch] > maxLag)
                readPos[(size_t) ch] = (double) writePos - maxLag;

            const double pos = readPos[(size_t) ch];
            const juce::int64 idx0 = (juce::int64) std::floor(pos);
            const float frac = (float) (pos - (double) idx0);
            const int i0 = (int) (((idx0 % bufferLengthSamples) + bufferLengthSamples) % bufferLengthSamples);
            const int i1 = (i0 + 1) % bufferLengthSamples;

            const auto& hist = history[(size_t) ch];
            buffer.getWritePointer(ch)[i] = hist[(size_t) i0] * (1.0f - frac) + hist[(size_t) i1] * frac;

            readPos[(size_t) ch] += (double) speed;
        }

        wobblePhase += wobblePhaseInc;
        if (wobblePhase > juce::MathConstants<double>::twoPi)
            wobblePhase -= juce::MathConstants<double>::twoPi;
    }
}
