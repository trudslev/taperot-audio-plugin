#include "../Source/PluginProcessor.h"
#include "../Source/DSP/ToneFilters.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    **Validating the magnitude-response instrument itself, on the one casting where it is possible.**

    Category 3 verifies filter cutoffs by measuring what the filter actually does — sine tones in,
    output RMS read — because a coefficient computed from a normalised frequency still *reports* the
    cutoff it was asked for and only its response moves.

    That instrument takes a **per-sample callable** rather than a processor, and it has to:
    **four of the suite's five cutoffs never reach their plugin's output.** Elmer's sidechain HP feeds
    the detector, Gatecrasher's trigger HP and LP feed the gate, Reflect-84's damping sits inside the
    tanks. For those the callable is the only instrument that works — and an instrument that cannot be
    cross-checked is one that simply has to be trusted.

    **TapeRot is the single opportunity to check it.** `ToneFilters` sits on the audio path, so its
    cutoffs are reachable BOTH by the callable and by driving the whole processor. Measuring both and
    comparing either validates the callable against a path where an independent check exists — and the
    four callable-only cases inherit that validation — or produces a finding about the instrument
    before any of its results are believed.

    Every other instrument in this sweep had to earn its results by being made to fail. This is the
    one that can be earned by agreement with a second method instead, and there is exactly one
    measurement in which to do it.

    ## What the two paths do NOT share

    They are independent in the way that matters — one drives a bare `ToneFilters`, the other drives
    the shipping `processBlock` — but the processor path carries TapeRot's whole chain, not the
    filters alone. So the two are compared for **agreement in shape**, and the comparison is set up so
    the rest of the chain contributes as little as possible: every other stage at its most neutral.
    A residual difference is expected and is reported rather than asserted to zero.
*/
class InstrumentValidationTests final : public juce::UnitTest
{
public:
    InstrumentValidationTests() : juce::UnitTest ("Instrument validation", "DSP") {}

    void runTest() override
    {
        beginTest ("The per-sample callable agrees with a processor-level measurement");
        {
            const std::vector<double> probes { 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0 };

            constexpr double fs = 48000.0;
            constexpr float lpHz = 2000.0f;      // the cutoff under test
            constexpr float hpHz = 20.0f;        // effectively open, so the LP is what shapes it

            // --- path A: the per-sample callable, driving ToneFilters alone -----------------------
            ToneFilters filters;
            filters.prepare ({ fs, 512, 2 });

            // ToneFilters is block-oriented, so the callable wraps a one-sample buffer. That is the
            // adapter the four callable-only castings would need too, so validating it here validates
            // the shape they use.
            juce::AudioBuffer<float> one (2, 1);

            const auto callableResponse = nf::testing::measureMagnitudeResponse (
                [&] (float x)
                {
                    one.setSample (0, 0, x);
                    one.setSample (1, 0, x);
                    filters.process (one, lpHz, hpHz);
                    return one.getSample (0, 0);
                },
                [&] { filters.reset(); },
                fs, probes);

            // --- path B: the whole processor ------------------------------------------------------
            TapeRotAudioProcessor processor;

            // Everything else as neutral as it goes, so the tone filters are what shapes the result.
            set (processor, ParamIDs::drive, 0.0f);
            set (processor, ParamIDs::wow, 0.0f);
            set (processor, ParamIDs::flutter, 0.0f);
            set (processor, ParamIDs::noise, 0.0f);
            set (processor, ParamIDs::failure, 0.0f);
            set (processor, ParamIDs::gen, 0.0f);

            // **The two paths must measure the SAME filter, and the first run did not.** LP defaults
            // to 20 kHz — wide open — while the callable was driving ToneFilters at 2 kHz, so the
            // processor measured an unfiltered chain and read flat. The disagreement looked exactly
            // like an instrument fault and was a configuration one.
            //
            // Set by PHYSICAL value through the parameter's own range, not by a normalised guess:
            // this range is skewed 0.3 over 1000-20000, so 2 kHz is nowhere near 0.05 normalised.
            setHz (processor, ParamIDs::lp, lpHz);
            setHz (processor, ParamIDs::hp, hpHz);

            logMessage ("  processor LP -> " + readBack (processor, ParamIDs::lp)
                            + ", HP -> " + readBack (processor, ParamIDs::hp));

            const auto processorResponse =
                nf::testing::measureProcessorMagnitudeResponse (processor, fs, 512, probes);

            // --- compare --------------------------------------------------------------------------
            for (size_t i = 0; i < probes.size(); ++i)
                logMessage ("  " + juce::String (probes[i], 0) + " Hz -> callable "
                                + juce::String (callableResponse[i].gainDb, 3) + " dB, processor "
                                + juce::String (processorResponse[i].gainDb, 3) + " dB, delta "
                                + juce::String (std::abs (callableResponse[i].gainDb
                                                          - processorResponse[i].gainDb), 3) + " dB");

            // **The SHAPE is what must agree.** Both paths should roll off together above the LP
            // cutoff, so the difference between adjacent probes should track between them even if
            // an absolute offset exists from the rest of TapeRot's chain.
            double worstSlopeDelta = 0.0;

            for (size_t i = 1; i < probes.size(); ++i)
            {
                const auto callableSlope = callableResponse[i].gainDb - callableResponse[i - 1].gainDb;
                const auto processorSlope = processorResponse[i].gainDb - processorResponse[i - 1].gainDb;
                worstSlopeDelta = juce::jmax (worstSlopeDelta,
                                              std::abs (callableSlope - processorSlope));
            }

            logMessage ("  worst per-octave slope disagreement -> "
                            + juce::String (worstSlopeDelta, 3) + " dB");

            // Reported first, asserted second, and the bar is deliberately loose: the processor path
            // carries the whole chain and the callable carries one stage, so exact agreement is not
            // the claim. What is claimed is that they describe the same filter.
            expectLessThan (worstSlopeDelta, 3.0,
                            "the callable and the processor disagree about the SHAPE of the same "
                            "filter. That is a finding about the instrument, and every "
                            "callable-only result in category 3 depends on it: "
                                + juce::String (worstSlopeDelta, 3) + " dB");
        }
    }

private:
    static void set (TapeRotAudioProcessor& p, const char* id, float normalised)
    {
        if (auto* param = p.apvts.getParameter (id))
            param->setValueNotifyingHost (normalised);
    }

    /** Sets a physical value through the parameter's own range, so a skew cannot silently move it. */
    static void setHz (TapeRotAudioProcessor& p, const char* id, float hz)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (hz));
    }

    static juce::String readBack (TapeRotAudioProcessor& p, const char* id)
    {
        if (auto* param = p.apvts.getParameter (id))
            return param->getCurrentValueAsText();

        return "<missing>";
    }
};

static InstrumentValidationTests instrumentValidationTests;
