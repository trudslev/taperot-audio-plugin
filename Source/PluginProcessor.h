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
#include <array>
#include <memory>

class TapeRotAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AsyncUpdater
{
public:
    TapeRotAudioProcessor();
    ~TapeRotAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

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

    /** Applies a deferred change right now instead of waiting for the message loop. Only the tests
        need this: the console app they run in has no message loop to deliver the async callback, so
        without it every requestProgramChange would silently never arrive. Matches the siblings'
        ProgramManager::flushPendingChange. */
    void flushPendingProgramChange() { handleUpdateNowIfNeeded(); }

    /** Clears the stale-replay guard. Called from the editor when a change is USER-originated -
        see the comment on justRestoredState. */
    void noteUserEdit() noexcept { justRestoredState.store(false, std::memory_order_relaxed); }

    /** True once any stored parameter differs from the Program that is currently showing, so the
        GUI can keep SAVE disabled until there is actually something worth saving. The snapshot is
        retaken whenever a Program is applied or a session is restored, and deliberately ignores
        the momentary STOP/FILTER/FAIL triggers - those are never part of a Program, so holding one
        must not light SAVE up. */
    bool isProgramModified() const;

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
    juce::File userProgramFile(const juce::String& stem) const;
    void applyFactoryProgram(const FactoryProgram& program);
    void refreshUserProgramList();
    static juce::File getUserProgramDirectory();

    // Taken from the live APVTS right after a Program is applied or a session restored, rather
    // than reconstructed from the Program's definition - that way there is exactly one description
    // of what a Program sets, in applyFactoryProgram, and no second copy to drift out of step.
    void captureProgramSnapshot();
    static const juce::StringArray& snapshotParamIds();
    std::vector<float> programSnapshot;
    // setStateInformation can arrive on any thread; the GUI polls isProgramModified on the message
    // thread. Contention is near-zero (writes happen on program change only), so a spin lock costs
    // nothing and never allocates.
    mutable juce::SpinLock snapshotLock;

    // Guarded rather than atomic: a ProgramId holds two juce::Strings. Contention is near-zero -
    // writes happen on a Program change only - so the spin lock costs nothing and never allocates.
    mutable juce::SpinLock currentIdLock;
    ProgramId currentId;

    /** **Guards against a host replaying a stale program index over a just-restored session.**

        Hosts have been observed calling setCurrentProgram AFTER setStateInformation, echoing back
        the presetNumber they remembered - which would apply a Factory Program over the state that
        was just restored, silently, with the panel then naming a sound the session never had.

        Armed by setStateInformation and disarmed by the FIRST of either: a setCurrentProgram call
        (which is itself ignored only if it matches what getCurrentProgram already reports - the
        exact shape of a replay), or a user-originated parameter change via noteUserEdit.

        **Automation must not disarm it.** A host may start writing automation on session load
        before it replays the index; if that disarmed the guard the replay would land unguarded,
        which is the whole failure being prevented. That is why this is driven from the editor's
        isMouseButtonDown-guarded callback rather than from a ValueTree listener, which cannot tell
        a person from an automation lane.

        Bounded on purpose: left armed indefinitely it would swallow a genuine matching call much
        later - the user editing a Program and re-selecting it from the host to revert. */
    std::atomic<bool> justRestoredState{false};
    // Sorted alphabetically by filename (stable across relaunches, unlike mtime-sort). Index i in
    // this array is program index kNumFactoryPrograms + i.
    juce::Array<juce::File> userProgramFiles;

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
