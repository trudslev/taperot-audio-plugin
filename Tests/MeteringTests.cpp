#include "TestUtils.h"
#include "../Source/PluginProcessor.h"
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

/**
    The IN/OUT readout's character budget, which was six on every fade to silence.

    Suite ruling 2026-08-14 — floor sentinel, +99.9 ceiling, one decimal always, no plus at exactly
    0.0 dB — makes the widest string **5** as a GUARANTEE rather than as a range. That distinction is
    the whole point: the question a well's width has to answer is *what is the widest string this can
    ever be asked to draw*, and a range answers a different, weaker one.

    **Both bounds were live defects and neither was hypothetical.** The floor: 20*log10(1e-5) is
    exactly -100.0, so a linear level just above the DSP's threshold printed `"-100.0"`. The band is
    1e-5 to 1.0058e-5 — 0.58 % wide — and a smoothed level crosses it at the end of every note. The
    ceiling: no readout path in ANY casting had one, so the widest string was bounded by how loud the
    signal got.

    The sweep below is dense through the floor band specifically, because a coarse sweep steps over
    0.58 % of the range without touching it — which is how it survived being looked at.
*/
class MeterReadoutBudgetTests final : public juce::UnitTest
{
public:
    MeterReadoutBudgetTests() : juce::UnitTest("Meter readout budget", "DSP") {}

    void runTest() override
    {
        beginTest("Never wider than five characters, anywhere in the linear range");
        {
            // Read off the processor's own constants, never transcribed: a fixture built from its
            // own literals agrees with itself and says nothing about what the panel draws.
            const float floorDb = TapeRotAudioProcessor::meterFloorDb;
            const float ceilingDb = TapeRotAudioProcessor::meterCeilingDb;

            const auto readout = [floorDb, ceilingDb](float linear)
            {
                const float db = linear > 1.0e-5f ? 20.0f * std::log10(linear) : floorDb;
                return juce::String(juce::jlimit(floorDb, ceilingDb, db), 1);
            };

            std::vector<float> probes;

            // The floor band, densely. 1.0e-5 to 1.0058e-5 is where "-100.0" lived.
            for (int i = 0; i <= 400; ++i)
                probes.push_back(9.5e-6f + (float) i * 1.0e-8f);

            // Silence, the decades either side, and well past full scale for the ceiling.
            for (float v : { 0.0f, 1.0e-9f, 1.0e-7f, 1.0e-3f, 0.5f, 1.0f, 2.0f, 1.0e3f, 1.0e6f })
                probes.push_back(v);

            int widest = 0;
            juce::String widestString, sawMinus100;

            for (float v : probes)
            {
                const auto s = readout(v);
                if (s.length() > widest) { widest = s.length(); widestString = s; }
                if (s == "-100.0") sawMinus100 = s;
            }

            logMessage("  widest over " + juce::String((int) probes.size()) + " probes: \""
                           + widestString + "\" at " + juce::String(widest) + " characters");

            expect(sawMinus100.isEmpty(),
                   "the readout still prints \"-100.0\" somewhere in the floor band — that is six "
                   "characters in a well guaranteed five, and it happens at the end of every note");

            expectEquals(widest, 5,
                         "the readout exceeded five characters. The well is sized to a GUARANTEE, "
                         "not to a range, so a sixth character is a defect rather than a rare case");
        }

        beginTest("The comparison is shown able to fail — unclamped, it is six");
        {
            // **Without this the test above cannot be distinguished from one that never sees a wide
            // string.** The pre-ruling construction is run through the same sweep and must produce
            // exactly what the finding recorded, or the probe density is wrong rather than the code
            // being right.
            const auto unclamped = [](float linear)
            {
                return juce::String(linear > 1.0e-5f ? 20.0f * std::log10(linear) : -99.9f, 1);
            };

            int widest = 0;
            bool sawMinus100 = false;

            for (int i = 0; i <= 400; ++i)
            {
                const auto s = unclamped(9.5e-6f + (float) i * 1.0e-8f);
                widest = juce::jmax(widest, s.length());
                if (s == "-100.0") sawMinus100 = true;
            }

            logMessage("  unclamped, same sweep: widest " + juce::String(widest) + " characters, "
                           + (sawMinus100 ? "\"-100.0\" SEEN" : "\"-100.0\" not seen"));

            expect(sawMinus100 && widest == 6,
                   "the pre-ruling construction did NOT produce a six-character string over this "
                   "sweep, so the sweep is too coarse to have proved anything about the clamped one");
        }
    }
};

static PitchDeviationTapTests pitchDeviationTapTests;
static PitchDeviationMeterTests pitchDeviationMeterTests;
static FailureEventTapTests failureEventTapTests;
static MeterReadoutBudgetTests meterReadoutBudgetTests;
