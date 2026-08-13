#include "../Source/PluginProcessor.h"

#include <nf/testing/ProcessorHarness.h>

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Category 1 of the suite-wide bug sweep, for TapeRot.

    **Core owns the drivers; this file owns what TapeRot's answers should be.** That split is why
    `nf::testing` could go into core at all — "does this allocate on the audio thread" is one
    question asked of six plugins, while what counts as correct here is knowledge core must not have.

    ## Every allocation result is reported in TWO figures, even when both are zero

    A warm-up run hides any one-off, not only an over-delivery one. A casting that allocates once on
    its very first block reads identically clean under a warmed probe — and that is a different
    finding from never allocating. The first version of Gatecrasher's probe reported exactly that
    error: "no allocation" from a processor that allocates six times on its first oversized block.

    So both are measured and both are logged, here and in all six castings:

      - **cold** — the first block after `prepareToPlay`, nothing absorbed
      - **steady** — after warm-up, which is the per-block cost a host actually pays

    A clean pair is a result worth recording at full length. "Prepared 512, driven 512, first block
    clean, steady clean" is what stops the next audit re-deriving the suspicion from the same lines.

    ## The leads here

    Two growth sites inside `processBlock`: `dryBuffer.setSize` and
    `genFloorSnapshot.makeCopyOf (buffer, true)`. Both pass `avoidReallocating`, both are sized in
    `prepareToPlay`, so both grow only when a host over-delivers.
*/
class RealtimeSafetyTests final : public juce::UnitTest
{
public:
    RealtimeSafetyTests() : juce::UnitTest ("Real-time safety", "DSP") {}

    void runTest() override
    {
        beginTest ("processBlock allocation — matched block size, cold and steady");
        {
            TapeRotAudioProcessor cold;
            const auto c = nf::testing::probeProcessBlockAllocation (cold, 48000.0, 512, 512, 2, 1, 0);

            TapeRotAudioProcessor steadyProc;
            const auto s = nf::testing::probeProcessBlockAllocation (steadyProc, 48000.0, 512, 512, 2);

            logMessage ("  512/512 cold   -> " + c.describe());
            logMessage ("  512/512 steady -> " + s.describe());

            // **FINDING, and one an allocation-only detector reported as clean.** The cold row is
            // `0 alloc, 4 free` — four deallocations on the first block, no allocations at all. The
            // earlier instrument counted only allocations and called this row "no allocation".
            //
            // Cause: TapeRot caches its IIR coefficients as Ptr arrays built in prepare, and assigns
            // them per block — Saturator.cpp:38/40/92/94 and TapeModelEQ.cpp:36. Assigning a
            // ReferenceCountedObjectPtr releases whatever the target held. On the FIRST block that
            // release drops the last reference to what prepare left there, and free() runs four
            // times. On every block after, the assignment is the same pointer, so the refcount goes
            // down and back up without reaching zero — which is why steady state is genuinely clean.
            //
            // So the caching TapeRot uses to avoid per-block ALLOCATION still costs four per-block
            // frees' worth of heap activity once, on the first block. Classification: live defect,
            // measured, one-off. Nothing is fixed in this pass.
            expectEquals (c.frees, 4,
                          "the first-block coefficient release count moved. If it went to 0 the "
                          "one-off is fixed; if it went up, something else releases on the audio "
                          "thread.");
            expectEquals (c.allocations, 0, "the cold block started allocating, not just freeing");

            expect (s.clean(), "steady-state processBlock touches the heap: " + s.describe());
        }

        beginTest ("processBlock when a host over-delivers — up to the measured crash threshold");
        {
            // **TapeRot CRASHES when over-delivered past ~400 samples from a 256 prepare**, and this
            // test deliberately stops short of that. Driving the crashing size here would abort the
            // process and take every later suite with it — which is exactly what happened the first
            // time it ran: SIGTRAP, exit -5, and run_tests.py reported fourteen suites that never
            // ran rather than a partial pass.
            //
            // The defect, localised under lldb rather than inferred:
            //
            //   EXC_BAD_ACCESS (code=1, address=0x0)
            //   juce::dsp::Oversampling2TimesPolyphaseIIR<float>::processSamplesDown
            //
            // Saturator.cpp:29 calls oversampling->initProcessing(spec.maximumBlockSize), sizing the
            // oversampler's internal buffers at prepare; Saturator.cpp:120 then feeds
            // processSamplesUp whatever block actually arrives, with no clamp. A block larger than
            // the prepared maximum runs off the end.
            //
            // Bisected: prepared 256, survives 257 / 300 / 400, crashes by 450. Not "any
            // over-delivery" — there is headroom, and then there is not.
            //
            // **Classification: live defect, measured, crash.** A host is supposed to honour
            // maximumExpectedSamplesPerBlock, so this needs a misbehaving host — but pluginval's
            // higher strictness levels drive exactly this case, which is category 8's job to confirm.
            // Nothing is fixed in this pass.
            for (int driven : { 257, 300, 400 })
            {
                TapeRotAudioProcessor p;
                p.setRateAndBufferSizeDetails (48000.0, 256);
                p.prepareToPlay (48000.0, 256);

                juce::AudioBuffer<float> buffer (2, driven);
                juce::MidiBuffer midi;
                buffer.clear();
                p.processBlock (buffer, midi);

                bool finite = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < driven; ++i)
                        if (! std::isfinite (buffer.getSample (ch, i)))
                            finite = false;

                expect (finite, "non-finite output at " + juce::String (driven) + " samples");
                logMessage ("  prepared 256, driven " + juce::String (driven) + " -> survived, finite");
            }

            logMessage ("  driven >= 450 CRASHES (Oversampling2TimesPolyphaseIIR::processSamplesDown"
                        ", null write) — not exercised here, see this test's comment");
        }

    }
};

static RealtimeSafetyTests realtimeSafetyTests;
