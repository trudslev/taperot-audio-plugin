#include "TestUtils.h"
#include "../Source/DSP/TapeModelEQ.h"
#include "../Source/DSP/TapeModelData.h"

namespace
{
    // Measures the steady-state gain (dB) a model applies at a single test tone, after letting
    // filter state and any FADE crossfade settle.
    float measureGainDb(int modelIndex, float freqHz, double sampleRate)
    {
        constexpr int blockSize = 2048;
        constexpr int numChannels = 1;
        juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels};

        TapeModelEQ eq;
        eq.prepare(spec);

        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        double inputSumSq = 0.0, outputSumSq = 0.0;

        for (int block = 0; block < 12; ++block)
        {
            auto* data = buffer.getWritePointer(0);
            for (int i = 0; i < blockSize; ++i)
                data[i] = std::sin(juce::MathConstants<float>::twoPi * freqHz
                                    * (float) (block * blockSize + i) / (float) sampleRate);

            if (block >= 6) // measure input level from the same unprocessed tone
                for (int i = 0; i < blockSize; ++i)
                    inputSumSq += (double) data[i] * data[i];

            eq.process(buffer, modelIndex, false);

            if (block >= 6)
            {
                const auto* out = buffer.getReadPointer(0);
                for (int i = 0; i < blockSize; ++i)
                    outputSumSq += (double) out[i] * out[i];
            }
        }

        const float inputRms = (float) std::sqrt(inputSumSq / (6.0 * blockSize));
        const float outputRms = (float) std::sqrt(outputSumSq / (6.0 * blockSize));
        return juce::Decibels::gainToDecibels(outputRms) - juce::Decibels::gainToDecibels(inputRms);
    }
}

// Renders a test tone at each model's own band frequencies and confirms the measured gain is in
// the right direction and rough magnitude for that band, per the brief's step-9 verification
// ("render pink noise/tones through each of the eight models and report measured frequency
// responses"). Tolerance is wider than the brief's "~1dB" for bands sitting close to another
// band's transition (e.g. a peak near a shelf's corner) since their responses combine; this is a
// sanity check that the coefficients do roughly what the table says, not a precise analytical
// match.
class TapeModelFrequencyResponseTests final : public juce::UnitTest
{
public:
    TapeModelFrequencyResponseTests() : juce::UnitTest("TapeModelFrequencyResponse", "DSP") {}

    void runTest() override
    {
        const double sampleRate = 48000.0;

        beginTest("Each model's measured gain at its own band frequencies matches the table's declared shape");

        for (size_t m = 0; m < kNumTapeModels; ++m)
        {
            const auto& model = kTapeModels[m];
            logMessage(juce::String("--- ") + model.displayName + " ---");

            for (int b = 0; b < model.numBands; ++b)
            {
                const auto& band = model.bands[(size_t) b];
                const float measuredDb = measureGainDb((int) m, band.freqHz, sampleRate) - model.makeupGainDb;

                // Only shelf/peak bands have a meaningful declared target (gainDb) to check
                // against directly. LowPass/HighPass only declare (freq, Q) - their gain at the
                // corner frequency isn't -3dB in general once Q departs from Butterworth (0.707),
                // and can be further skewed by a nearby band's own passband (e.g. a peak filter
                // whose skirt still has gain near a lowpass's corner). So those are logged for
                // inspection but not asserted against an assumed target.
                switch (band.type)
                {
                    case EQBandType::LowShelf:
                    case EQBandType::HighShelf:
                    case EQBandType::Peak:
                        logMessage("  shelf/peak @ " + juce::String(band.freqHz, 0) + "Hz: measured "
                                   + juce::String(measuredDb, 2) + " dB (band declares "
                                   + juce::String(band.gainDb, 2) + " dB)");
                        expect(std::abs(measuredDb - band.gainDb) < 2.0f,
                               juce::String(model.displayName) + " band at " + juce::String(band.freqHz, 0)
                                   + "Hz should be roughly " + juce::String(band.gainDb, 1) + "dB, measured "
                                   + juce::String(measuredDb, 2) + "dB");
                        break;
                    case EQBandType::LowPass:
                    case EQBandType::HighPass:
                    {
                        logMessage("  pass @ " + juce::String(band.freqHz, 0) + "Hz (Q=" + juce::String(band.q, 2)
                                   + "): measured " + juce::String(measuredDb, 2) + " dB at the corner (informational"
                                   " only - no declared target for pass-type bands)");

                        // What *is* well-defined regardless of Q: well past the corner, a lowpass
                        // rolls off and a highpass doesn't recover - check attenuation an octave
                        // beyond the corner instead of at it.
                        const float testFreq = band.type == EQBandType::LowPass ? band.freqHz * 2.0f : band.freqHz * 0.5f;
                        const float rolloffDb = measureGainDb((int) m, testFreq, sampleRate) - model.makeupGainDb;
                        logMessage("    an octave past corner (" + juce::String(testFreq, 0) + "Hz): "
                                   + juce::String(rolloffDb, 2) + " dB");
                        expect(rolloffDb < -3.0f,
                               juce::String(model.displayName) + "'s "
                                   + (band.type == EQBandType::LowPass ? juce::String("lowpass") : juce::String("highpass"))
                                   + " should show clear rolloff an octave past its corner frequency");
                        break;
                    }
                }
            }
        }

        beginTest("REVOX B77 is nearly flat in the midrange (deliberately subtle placeholder curve)");
        {
            const float midGainDb = measureGainDb(0, 1000.0f, sampleRate) - kTapeModels[0].makeupGainDb;
            logMessage("REVOX B77 @ 1kHz: " + juce::String(midGainDb, 2) + " dB");
            expect(std::abs(midGainDb) < 0.5f, "REVOX B77 should be within 0.5dB of flat in the midrange");
        }
    }
};

static TapeModelFrequencyResponseTests tapeModelFrequencyResponseTests;
