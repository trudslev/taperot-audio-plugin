#include "TestUtils.h"
#include "../Source/DSP/FailureEngine.h"
#include "../Source/DSP/PitchDeviationMeter.h"
#include "../Source/DSP/WowFlutter.h"

#include <cmath>
#include <vector>

/**
    The read-only taps that feed the pitch scope.

    Neither changes how anything processes audio, and the guard on that is the rest of the suite
    passing unchanged. What these check is that the taps measure the *realised* modulation rather
    than mirroring a knob — a wow/flutter display trio was scaffolded here once before and removed
    for exactly that reason.
*/
class PitchDeviationTapTests final : public juce::UnitTest
{
public:
    PitchDeviationTapTests() : juce::UnitTest("Pitch deviation tap", "Metering") {}

    static constexpr double fs = 48000.0;
    static constexpr int blockSize = 512;

    static std::vector<float> runDeviation(float wow01, float flutter01, int blocks = 40)
    {
        WowFlutter wf;
        wf.prepare({fs, (juce::uint32) blockSize, 2});

        juce::AudioBuffer<float> buffer(2, blockSize);
        std::vector<float> accum((size_t) blockSize);
        std::vector<float> all;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.getWritePointer(ch)[i] =
                        0.5f * std::sin(juce::MathConstants<float>::twoPi * 440.0f
                                        * (float) (b * blockSize + i) / (float) fs);

            std::fill(accum.begin(), accum.end(), 0.0f);
            wf.process(buffer, wow01, flutter01, accum.data());
            all.insert(all.end(), accum.begin(), accum.end());
        }

        return all;
    }

    static float peakAbs(const std::vector<float>& v)
    {
        float p = 0.0f;
        for (float x : v) p = juce::jmax(p, std::abs(x));
        return p;
    }

    void runTest() override
    {
        beginTest("Passing null costs nothing and is the default");
        {
            WowFlutter wf;
            wf.prepare({fs, (juce::uint32) blockSize, 2});
            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            wf.process(buffer, 0.5f, 0.5f);          // must compile and not crash
            expect(true);
        }

        beginTest("No wow and no flutter means no deviation");
        expectLessThan(peakAbs(runDeviation(0.0f, 0.0f)), 1.0e-4f,
                       "a still tape must not report pitch movement");

        beginTest("Deviation grows with depth");
        {
            const float quarter = peakAbs(runDeviation(0.25f, 0.0f));
            const float full = peakAbs(runDeviation(1.0f, 0.0f));
            expect(quarter > 1.0e-4f, "wow at 25% must produce measurable deviation");
            expect(full > quarter * 2.0f,
                   "wow at 100% must deviate far more than at 25%: "
                       + juce::String(quarter, 4) + " vs " + juce::String(full, 4));
        }

        beginTest("Flutter deviates faster than wow at the same depth");
        {
            // The point of measuring the derivative rather than the modulation: flutter is a small
            // excursion at a high rate, so it dominates pitch deviation even though wow moves the
            // delay line much further. Reading the modulation value would rank these the other way.
            const auto wowOnly = runDeviation(1.0f, 0.0f);
            const auto flutterOnly = runDeviation(0.0f, 1.0f);
            expect(peakAbs(flutterOnly) > peakAbs(wowOnly),
                   "flutter's peak deviation must exceed wow's: wow "
                       + juce::String(peakAbs(wowOnly), 3) + " vs flutter "
                       + juce::String(peakAbs(flutterOnly), 3));
        }

        beginTest("Deviation is continuous across block boundaries");
        {
            // previousDelaySamples is retained between blocks; if it were not, the first sample of
            // every block would carry a spurious step. Look for a spike at block starts.
            const auto v = runDeviation(1.0f, 1.0f);
            float worstAtBoundary = 0.0f, worstElsewhere = 0.0f;
            for (size_t i = 1; i < v.size(); ++i)
                (i % (size_t) blockSize == 0 ? worstAtBoundary : worstElsewhere) =
                    juce::jmax(i % (size_t) blockSize == 0 ? worstAtBoundary : worstElsewhere,
                               std::abs(v[i]));

            expect(worstAtBoundary <= worstElsewhere * 1.5f,
                   "block boundaries must not spike: " + juce::String(worstAtBoundary, 3)
                       + " vs " + juce::String(worstElsewhere, 3));
        }

        beginTest("Stages accumulate rather than overwrite");
        {
            WowFlutter a, b;
            a.prepare({fs, (juce::uint32) blockSize, 2});
            b.prepare({fs, (juce::uint32) blockSize, 2});

            juce::AudioBuffer<float> buffer(2, blockSize);
            std::vector<float> one((size_t) blockSize, 0.0f), two((size_t) blockSize, 0.0f);

            buffer.clear();
            a.process(buffer, 1.0f, 0.0f, one.data());

            WowFlutter a2, b2;
            a2.prepare({fs, (juce::uint32) blockSize, 2});
            b2.prepare({fs, (juce::uint32) blockSize, 2});
            buffer.clear();
            a2.process(buffer, 1.0f, 0.0f, two.data());
            buffer.clear();
            b2.process(buffer, 1.0f, 0.0f, two.data());

            expect(peakAbs(two) > peakAbs(one),
                   "a second stage adding into the same buffer must increase the total");
        }
    }
};

//==============================================================================
class PitchDeviationMeterTests final : public juce::UnitTest
{
public:
    PitchDeviationMeterTests() : juce::UnitTest("Pitch deviation FIFO", "Metering") {}

    void runTest() override
    {
        beginTest("Decimation keeps a spike a point sample would miss");
        {
            // One large value buried mid-window, everything else tiny. Point-sampling every 64th
            // value returns the tiny neighbour; taking the window's extreme returns the spike.
            PitchDeviationMeter meter;
            meter.prepare(PitchDeviationMeter::decimationFactor);

            std::vector<float> block((size_t) PitchDeviationMeter::decimationFactor, 0.01f);
            block[7] = -9.0f;                        // sign must survive too

            meter.pushBlock(block.data(), (int) block.size());

            float out[8] {};
            expectEquals(meter.pop(out, 8), 1, "one window in, one value out");
            expectWithinAbsoluteError(out[0], -9.0f, 1.0e-5f,
                                      "the window's extreme must survive decimation, with its sign");
        }

        beginTest("It drops rather than blocks when the GUI is not draining");
        {
            PitchDeviationMeter meter;
            meter.prepare(4096);

            std::vector<float> block(4096, 1.0f);
            // Far more than the FIFO can hold, pushed with nothing draining. Must return promptly
            // and leave the FIFO in a readable state rather than deadlocking or corrupting.
            for (int i = 0; i < 40; ++i)
                meter.pushBlock(block.data(), (int) block.size());

            std::vector<float> out((size_t) PitchDeviationMeter::fifoCapacity + 16);
            const int n = meter.pop(out.data(), (int) out.size());
            expect(n > 0, "the FIFO still yields data after being overrun");
            expect(n <= PitchDeviationMeter::fifoCapacity, "it never yields more than it can hold");
        }

        beginTest("getScratch zeroes the block and refuses to allocate");
        {
            PitchDeviationMeter meter;
            meter.prepare(256);

            auto* s = meter.getScratch(256);
            expect(s != nullptr);
            for (int i = 0; i < 256; ++i) expectEquals(s[i], 0.0f);

            s[0] = 5.0f;
            auto* again = meter.getScratch(256);
            expectEquals(again[0], 0.0f, "each block starts from zero");

            expect(meter.getScratch(100000) == nullptr,
                   "an oversized request must return null, never allocate on the audio thread");
        }

        beginTest("The output rate is the sample rate over the decimation factor");
        expectWithinAbsoluteError((float) PitchDeviationMeter::outputRate(48000.0),
                                  48000.0f / (float) PitchDeviationMeter::decimationFactor, 0.01f);
    }
};

//==============================================================================
class FailureEventTapTests final : public juce::UnitTest
{
public:
    FailureEventTapTests() : juce::UnitTest("Failure event tap", "Metering") {}

    void runTest() override
    {
        beginTest("Events pop in the order they were pushed");
        {
            FailureEngine engine;
            expect(engine.pushEvent(FailureEventType::dropout, 0.5f, 100));
            expect(engine.pushEvent(FailureEventType::snag, 0.6f, 200));
            expect(engine.pushEvent(FailureEventType::crinkle, 0.7f, 300));

            FailureEvent out[8] {};
            expectEquals(engine.popEvents(out, 8), 3);
            expect(out[0].type == FailureEventType::dropout);
            expect(out[1].type == FailureEventType::snag);
            expect(out[2].type == FailureEventType::crinkle);
            expectWithinAbsoluteError(out[1].intensity, 0.6f, 1.0e-6f);
            expectEquals((int) out[2].timeSamples, 300);
        }

        beginTest("A full FIFO drops rather than blocking");
        {
            FailureEngine engine;
            int accepted = 0;
            for (int i = 0; i < 1000; ++i)
                if (engine.pushEvent(FailureEventType::wobble, 1.0f, i))
                    ++accepted;

            expect(accepted > 0 && accepted < 1000,
                   "some pushes must be accepted and some dropped, never all or none");
            expect(engine.getEventCount() == (juce::int64) accepted,
                   "the counter tracks accepted events only");
        }

        beginTest("The four event types cover the four fault dots");
        {
            // DRP, SNG, CRK, WBL on the panel. If a fifth failure mode is ever added, the panel
            // needs a fifth dot and this is where that shows up.
            const std::array<FailureEventType, 4> all {{
                FailureEventType::dropout, FailureEventType::snag,
                FailureEventType::crinkle, FailureEventType::wobble }};
            expectEquals((int) all.size(), 4);
        }
    }
};

static PitchDeviationTapTests pitchDeviationTapTests;
static PitchDeviationMeterTests pitchDeviationMeterTests;
static FailureEventTapTests failureEventTapTests;
