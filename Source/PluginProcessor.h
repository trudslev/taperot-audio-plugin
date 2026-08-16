#pragma once

#include "Parameters.h"
#include "DSP/FactoryPrograms.h"
#include "DSP/Saturator.h"
#include "DSP/DegradationCore.h"
#include "DSP/PitchDeviationMeter.h"
#include "DSP/WowFlutter.h"
#include "DSP/Hum.h"
#include "DSP/FailureEngine.h"
#include "DSP/StereoSpread.h"
#include "DSP/ToneFilters.h"
#include "DSP/TapeStop.h"
#include "DSP/FilterSweep.h"
#include "DSP/AuxEnvelope.h"
#include "DSP/OutputStage.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <nf/UserEditGate.h>
#include <nf/ParameterSnapshot.h>
#include <nf/UserProgramStore.h>
#include <array>
#include <memory>

class TapeRotAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AsyncUpdater,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    /** @param userDirectoryOverride  where User Programs live. Defaults to the real per-OS
                                      location; a test passes a temporary directory so it never
                                      writes into the user's own Programs folder.

        **The seam exists because the tests were writing into the live folder.** ProgramIdentityTests
        saved and deleted `IDENTITY TEST A` in the directory holding the user's own work - by exact
        name, which is the safe form, but still one interrupted run away from leaving litter there.
        Defaulted, so JUCE's `createPluginFilter()` is unaffected. */
    explicit TapeRotAudioProcessor(juce::File userDirectoryOverride = {});
    ~TapeRotAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;

    /** Declares the cascade's real latency. Recomputed on a GEN change — see the definition. */
    void updateLatency();

private:
    /** GEN only. A latency that MOVES forces a host graph rebuild, which is the whole reason GEN is
        non-automatable — so the recompute has to happen wherever GEN can change, not only in
        prepareToPlay. Delivered on the message thread by the APVTS. */
    void parameterChanged (const juce::String& parameterID, float newValue) override;

public:
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    /** **TapeRot has no bypass, and that is a decision rather than an omission.**

        BRAND.md makes a disengaged state optional; the suite audit found four castings documenting
        which they had chosen and two - this one and Gatecrasher - saying nothing either way, which
        left a reader unable to tell a decision from an oversight.

        The decision: this effect is a tape path, and a bypass would be the tape being out of the
        machine. The Standalone already gates its generated hiss and hum on the transport, and a
        host bypasses the plugin itself; adding a second, panel-level disengage would give two
        controls for one idea and invite the caption BRAND.md forbids. Reflect-84 is the reference
        if that is ever reversed - a real AudioParameterBool behind getBypassParameter(), a 0.50
        multiply over the whole canvas, no caption, no control on the panel. */
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    /** **The host adapter - the ONLY place a Program is addressed by position.**

        Everything else in this plugin identifies a Program by ProgramId. These four exist because
        the JUCE API is positional, and they translate at the boundary.

        **The list is the Factory bank and nothing else.** Not INIT, not User Programs. That is a
        conformance requirement, not a preference - juce_AudioProcessor.h documents getNumPrograms
        as: "The value returned must be valid as soon as this object is created, and must not change
        over its lifetime." A count including User Programs changes the moment one is saved, which is
        what this plugin used to do.

        The consequences of that non-conformance are worth knowing before anyone "fixes" this by
        making the count dynamic again. JUCE's VST3 wrapper builds the automatable Program parameter
        ONCE in its constructor with range 0..getNumPrograms()-1, so a Program saved afterwards was
        simply unreachable from the host and a deleted one left the range overrunning the list. The
        frozen range was the API keeping its documented promise, not a bug to work around.

        Excluding INIT as well buys exact alignment: host index n IS Factory Program n+1, so
        automating "Program 3" selects the Program the panel prints as 03.

        **Accepted divergence.** With User Programs off the list, getCurrentProgram has to answer
        with SOME factory position while a User Program is loaded, and it answers 0. A host's own
        menu will then show a Factory name while the panel shows the user's Program. The sound and
        the panel are both correct; only the host's menu is wrong, and that is the format's
        limitation rather than something to paper over. */
    int getNumPrograms() override { return (int) kNumFactoryPrograms; }
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;

    /** Deliberately a no-op, and this comment is the point of it.

        With Factory-only exposure there is nothing on the host's list that can be renamed: Factory
        names are fixed at authoring time and User Programs are not exposed. Implementing this would
        be a back door into the Factory bank, which is exactly what the permanent slugs exist to
        prevent. Renaming a User Program is a panel operation on a file. */
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    /** The current Program's identity. Everything on the panel reads this. */
    ProgramId getCurrentProgramId() const;

    /** The identity of a Factory Program at a bank position, and INIT's. Position is an argument
        here and nowhere else - these are how the host adapter and the dropdown cross the boundary
        from "which entry" to "which Program". */
    static ProgramId factoryIdAt(int factoryPosition);
    static ProgramId initId();

    /** The Factory bank position of a slug, or -1 if no entry carries it. */
    static int factoryPositionOf(const juce::String& slug);

    /** Applies a Program by identity. Safe from any thread - defers through the AsyncUpdater. */
    void requestProgramChange(const ProgramId& id);

    /*  **The pending-program handshake, and it is public so a test can reach it.**

        These two functions ARE the critical section: everything between taking `pendingLock` and
        releasing it happens inside them, and nothing else touches `pendingProgram`. An allocation
        sentinel is not lock-aware, so a probe around `requestProgramChange` cannot distinguish heap
        work under the lock from heap work beside it — the totals are identical either way. Arming
        it around a function that is exactly the locked region is the only honest way to assert the
        property, and that is worth the two names on this class.

        See their definitions for what moved out of the lock and why 0.12 us was never the argument. */
    ProgramId exchangePendingProgram (ProgramId incoming);
    bool takePendingProgram (ProgramId& out);


    /** Resolves an identifier, or returns an `unresolved` ProgramId carrying the name to show. */
    ProgramId resolve(ProgramBank bank, const juce::String& id, const juce::String& displayName) const;

    /** The list the dropdown paints, in display order: INIT, then Factory, then User. */
    std::vector<ProgramId> listPrograms() const;

    /** **What the LCD and the dropdown print - a label, not a key.**

        The two-digit number is computed from the Program's position in the Factory bank at paint
        time. It is not stored and nothing looks anything up by it. User Programs carry no number at
        all: they sort alphabetically, so any number would change whenever one was saved. INIT is
        unnumbered because it is outside the bank. */
    juce::String displayLabelFor(const ProgramId& id) const;

    void saveUserProgram(const juce::String& name);
    void deleteUserProgram(const ProgramId& id);

    /** The User Program name cap. Derived and explained in FactoryPrograms.h, beside the identity
        types, so the store and the header read one constant rather than two copies. */
    static constexpr int maxProgramNameLength = kMaxProgramNameLength;

    static juce::String getProgramFileExtension() { return ".taperotprogram"; }

    /** Where this instance stores User Programs, and the real per-OS location regardless of it. */
    juce::File getUserProgramDirectory() const;
    static juce::File getDefaultUserProgramDirectory();

    /** Applies a deferred change right now instead of waiting for the message loop. Only the tests
        need this: the console app they run in has no message loop to deliver the async callback, so
        without it every requestProgramChange would silently never arrive. Matches the siblings'
        ProgramManager::flushPendingChange. */
    void flushPendingProgramChange() { handleUpdateNowIfNeeded(); }

    /** True once any stored parameter differs from the Program that is currently showing, so the
        GUI can keep SAVE disabled until there is actually something worth saving. The snapshot is
        retaken whenever a Program is applied or a session is restored, and deliberately ignores
        the momentary STOP/FILTER/FAIL triggers - those are never part of a Program, so holding one
        must not light SAVE up. */
    bool isProgramModified() const;

    /** **Guards a host replaying a stale program index over a just-restored session.** Armed by
        setStateInformation, consumed by the next setCurrentProgram (which ignores it only when the
        index matches what getCurrentProgram already reports — the shape of a replay), disarmed by
        the first USER-originated edit. **Automation must not disarm it**: a host may write
        automation on load before replaying, and that would reopen the hole.

        Public because the editor hands it to `nf::connectUserEdit` for every control, which is the
        point of it living in core: Reflect-84 once shipped this guard with zero call sites for its
        disarm, and coupling the disarm to the LCD hand-off is what makes that omission
        inexpressible. See nf/UserEditGate.h. */
    nf::UserEditGate userEdits;

    juce::AudioProcessorValueTreeState apvts;

    FailureEngine& getFailureEngine() noexcept { return failureEngine; }
    float getFailAuxDisplay() const noexcept { return failAuxDisplay.load(std::memory_order_relaxed); }
    float getStopSpeedDisplay() const noexcept { return tapeStop.getSpeedDisplay(); }
    float getGenDisplay() const noexcept { return genSmoothed.getCurrentValue(); }

    // --- scope and header metering -------------------------------------------------------------
    // All read-only taps. None of them changes how anything processes audio.
    PitchDeviationMeter& getPitchDeviationMeter() noexcept { return pitchMeter; }
    double getPitchDeviationRate() const noexcept
    {
        return PitchDeviationMeter::outputRate(displaySampleRate);
    }
    float getWowRateHz() const noexcept { return wowRateDisplay.load(std::memory_order_relaxed); }
    float getFlutterRateHz() const noexcept { return flutterRateDisplay.load(std::memory_order_relaxed); }
    /*  The meter readout's two bounds, public because the test asserts against these rather than
        against transcribed copies of them — a fixture built from its own literals agrees with itself
        and says nothing about what the panel draws.

        Suite ruling 2026-08-14: floor sentinel, +99.9 ceiling, one decimal always. Both were live
        defects here — see `toDb` in the .cpp for the 0.58 %-wide band that printed "-100.0" on every
        fade to silence, and for why no casting had a ceiling at all. */
    static constexpr float meterFloorDb = -99.9f;
    static constexpr float meterCeilingDb = 99.9f;

    float getInputLevelDb() const noexcept { return inputLevelDb.load(std::memory_order_relaxed); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load(std::memory_order_relaxed); }

private:
    // Applying a factory/user program (see setCurrentProgram) sets every parameter via
    // setValueNotifyingHost, which is message-thread-only - but a host can call setCurrentProgram
    // itself from a non-message thread (VST3 delivers program-change as an ordinary automatable
    // parameter, which can arrive on the audio/process thread during playback automation). So the
    // actual application is deferred through AsyncUpdater: setCurrentProgram just records which
    // program is pending and triggers an async update; handleAsyncUpdate (guaranteed message
    // thread) does the real work.
    void handleAsyncUpdate() override;
    // A ProgramId is not trivially copyable, so "nothing pending" is its own flag rather than a
    // sentinel value - which also removes the last reason to reserve magic negative numbers.

    bool hasPendingProgram = false;
    ProgramId pendingProgram;
    juce::SpinLock pendingLock;

    void applyProgram(const ProgramId& id);
    void setCurrentId(const ProgramId& id);
    void applyFactoryProgram(const FactoryProgram& program);

    /** The User bank on disk. Scanning, sorting, naming, the collision check, save and delete are
        core's; WHAT a Program contains - the whole APVTS state, less the three momentary triggers -
        stays here. */
    nf::UserProgramStore store;

    // Taken from the live APVTS right after a Program is applied or a session restored, rather
    // than reconstructed from the Program's definition - that way there is exactly one description
    // of what a Program sets, in applyFactoryProgram, and no second copy to drift out of step.
    //
    // The SpinLock that used to sit beside this is core's now: setStateInformation carries no
    // thread guarantee and the GUI polls isProgramModified on the message thread, and four of the
    // six castings had that unguarded. See nf/ParameterSnapshot.h.
    void captureProgramSnapshot();
    static bool isMomentaryTrigger(const juce::String& parameterID);
    nf::ParameterSnapshot programSnapshot;

    // Guarded rather than atomic: a ProgramId holds two juce::Strings. Contention is near-zero -
    // writes happen on a Program change only - so the spin lock costs nothing and never allocates.
    mutable juce::SpinLock currentIdLock;
    ProgramId currentId;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* modelParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* noiseCharacterParam = nullptr;
    std::atomic<float>* humParam = nullptr;
    std::atomic<float>* failureParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* failureDropoutsParam = nullptr;
    std::atomic<float>* failureSnagsParam = nullptr;
    std::atomic<float>* failureCrinklesParam = nullptr;
    std::atomic<float>* failureImbalanceParam = nullptr;
    std::atomic<float>* genParam = nullptr;
    std::atomic<float>* lpParam = nullptr;
    std::atomic<float>* hpParam = nullptr;
    std::atomic<float>* stopParam = nullptr;
    std::atomic<float>* filterAuxParam = nullptr;
    std::atomic<float>* failAuxParam = nullptr;
    std::atomic<float>* rampParam = nullptr;
    std::atomic<float>* switchModeParam = nullptr;

    static constexpr int maxGenerations = 8;

    Saturator saturator;
    std::array<std::unique_ptr<DegradationCore>, maxGenerations> generationStages;

    PitchDeviationMeter pitchMeter;
    std::atomic<float> wowRateDisplay{0.0f};
    std::atomic<float> flutterRateDisplay{0.0f};
    std::atomic<float> inputLevelDb{-99.9f};
    std::atomic<float> outputLevelDb{-99.9f};
    float inLevelSmoothed = 0.0f;
    float outLevelSmoothed = 0.0f;
    float levelSmoothingCoeff = 0.0f;
    juce::SmoothedValue<float> genSmoothed{1.0f};
    juce::AudioBuffer<float> genFloorSnapshot;

    Hum hum;
    FailureEngine failureEngine;
    StereoSpread stereoSpread;
    ToneFilters toneFilters;
    TapeStop tapeStop;
    FilterSweep filterSweep;
    AuxEnvelope failEnvelope;
    OutputStage outputStage;

    // NoiseSource/Hum are unconditional generators (real tape self-noise, not signal-dependent),
    // so they hiss even with the host transport stopped feeding silent buffers. Smoothly fades the
    // whole output to silence while the host reports not-playing, and back in on play, rather than
    // a hard mute - avoids a click at the transport boundary. Only engages when a host actually
    // reports transport state (getPlayHead()/getPosition() both present) - Standalone has no
    // transport concept, so it's never gated there.
    juce::SmoothedValue<float> transportGateSmoothed{1.0f};

    std::atomic<float> failAuxDisplay{0.0f};
    double displaySampleRate = 44100.0;

    juce::AudioBuffer<float> dryBuffer;

    // Each active GEN cascade stage's WowFlutter centers its pitch-modulation delay line at
    // WowFlutter::nominalDelayMs even at wow=flutter=0 (it's not an optional effect - the delay
    // line IS the modulation mechanism), so the wet path always lags the live input by that much
    // per stage, up to ~200ms at GEN=8. dryBuffer was previously mixed in unshifted, so MIX blended
    // a live dry sample against a wet sample that was actually up to ~200ms old - this delay line
    // re-times the dry copy to match, tracking genSmoothed's current (possibly mid-transition)
    // value each block. It only needs to track the *nominal* per-stage delay, not the wow/flutter
    // modulation itself - the modulation's own smearing between dry and wet is inherent to the
    // effect and can't be undone without defeating it.
    juce::dsp::DelayLine<float> dryCompensationDelay{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeRotAudioProcessor)
};
