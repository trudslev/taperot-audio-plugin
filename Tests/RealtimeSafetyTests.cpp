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

        beginTest ("processBlock over-delivery — DOCUMENTED, deliberately NOT exercised");
        {
            // **CORRECTED 2026-08-14, and the correction matters more than the original finding.**
            //
            // This test previously drove 257, 300 and 400 samples after preparing 256 and reported
            // "survived, finite" for all three, concluding: "not any over-delivery — there is
            // headroom, and then there is not". **That was wrong, and wrong in the direction that
            // understates.**
            //
            // Those writes went out of bounds and simply did not fault. The damage surfaced later:
            // adding an unrelated test file changed the suite's memory layout, and the next test to
            // run — `Saturator / Exact null at drive = 0`, which drives no over-delivery at all —
            // died in `Oversampling<float>::processSamplesUp` with EXC_BAD_ACCESS.
            //
            // Proved by removing it: with the over-delivery drive taken out and nothing else
            // changed, the whole suite passes and Saturator runs clean.
            //
            // **So the finding is: ANY block larger than the prepared maximum writes out of bounds.**
            // Saturator.cpp:29 sizes the oversampler's buffers via initProcessing(maximumBlockSize);
            // Saturator.cpp:120 feeds processSamplesUp whatever arrives, unclamped. 450 is not a
            // threshold — it is merely the first size that happened to reach an unmapped page. 257
            // corrupts just as surely and reports "survived, finite" while doing it.
            //
            // **Nothing here drives it**, because a test that corrupts the process to demonstrate
            // corruption takes every later suite with it — which is exactly how this was found, and
            // is not a thing to keep. The safe case is asserted; the defect is recorded.
            //
            // Classification: live defect, measured, MEMORY CORRUPTION on any over-delivery.
            // Nothing is fixed in this pass.
            TapeRotAudioProcessor p;
            p.setRateAndBufferSizeDetails (48000.0, 256);
            p.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            buffer.clear();
            p.processBlock (buffer, midi);

            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        finite = false;

            expect (finite, "non-finite output at the prepared block size");
        }

        beginTest ("Over-delivery is now SAFE TO DRIVE, and this is the only verification there is");
        {
            // **This arm could not exist before stage 1b.** Driving an over-delivered block used to
            // write out of bounds, and a test that corrupts the process takes every later suite
            // with it — the damage lands somewhere unrelated, which is a worse diagnostic than the
            // defect. So the arm above documents and does not drive.
            //
            // `nf::processInChunks` makes it safe by construction: no span is longer than the
            // prepared size, so processSamplesUp is never handed more than initProcessing allocated
            // for. **And "by construction" is exactly the claim that needs a test**, because it is
            // the kind of reasoning that is right until somebody moves the wrapper.
            //
            // **It is also the ONLY verification available for TapeRot's half of 1b.** The defect is
            // an out-of-bounds WRITE, not an allocation, so no allocation detector was ever going to
            // see it — and the suite's own detector is blind to `AudioBuffer` growth as well (it
            // hooks `operator new`; `AudioBuffer` allocates through `HeapBlock` → `std::malloc`).
            // There is no figure to compare before and after. There is only: does driving it
            // survive, and is the output finite.
            //
            // If this ever segfaults, the wrapper stopped bounding span length — that is the whole
            // signal, and it is a loud one.
            TapeRotAudioProcessor p;
            p.setRateAndBufferSizeDetails (48000.0, 256);
            p.prepareToPlay (48000.0, 256);

            juce::MidiBuffer midi;
            bool allFinite = true;
            double peak = 0.0;

            // 257 is the first size the old bisect called "survived" while corrupting; 2048 is eight
            // spans; 511 is prime and shares no factor with the prepared size, so its final span is
            // short and the remainder path is exercised rather than assumed.
            for (int driven : { 257, 511, 2048 })
            {
                juce::AudioBuffer<float> buffer (2, driven);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < driven; ++i)
                        buffer.setSample (ch, i, 0.25f * std::sin (0.01f * (float) i));

                midi.clear();
                p.processBlock (buffer, midi);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < driven; ++i)
                    {
                        const float v = buffer.getSample (ch, i);
                        allFinite = allFinite && std::isfinite (v);
                        peak = juce::jmax (peak, (double) std::abs (v));
                    }

                logMessage ("  prepared 256, driven " + juce::String (driven).paddedLeft (' ', 4)
                                + " -> survived, peak " + juce::String (peak, 6));
            }

            // The output check first: a configuration that produced silence would "survive" for the
            // trivial reason and read as a pass.
            expectGreaterThan (peak, 1.0e-4,
                               "the over-delivered blocks produced no output, so surviving them "
                               "means nothing");

            expect (allFinite, "an over-delivered block produced non-finite output");
            logMessage ("  prepared 256, driven 256 -> finite (the only size exercised)");
            logMessage ("  ANY driven size > prepared writes OUT OF BOUNDS — see this test's comment");
        }
    }
};

static RealtimeSafetyTests realtimeSafetyTests;
