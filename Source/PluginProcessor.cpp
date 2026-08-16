#include "PluginProcessor.h"

#include <nf/UserProgramDirectory.h>
#include "PluginEditor.h"

#include <nf/BlockChunking.h>
#include <algorithm>

namespace
{
    // **Company and product come from CMake, not from JucePlugin_*.** The five siblings all read
    // NF_COMPANY_NAME / NF_PRODUCT_NAME, and this casting was the one still reading JUCE's own
    // macros - which meant it also had no guard. A missing definition is a hard error here rather
    // than a silent fallback, because a silent fallback is precisely the failure: the path this
    // builds is where the user's saved Programs live, and pointing it somewhere else loses them
    // without an error anywhere. Gatecrasher's hand-synced copy drifted to a dead company name
    // exactly that way.
#if !defined(NF_COMPANY_NAME) || !defined(NF_PRODUCT_NAME)
 #error "NF_COMPANY_NAME and NF_PRODUCT_NAME must be defined by CMake - see CMakeLists.txt."
#endif
    constexpr const char* pluginCompanyName = NF_COMPANY_NAME;
    constexpr const char* pluginProductName = NF_PRODUCT_NAME;

}

TapeRotAudioProcessor::TapeRotAudioProcessor(juce::File userDirectoryOverride)
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createTapeRotParameterLayout()),
      store(nf::userProgramDirectory(pluginCompanyName, pluginProductName, userDirectoryOverride),
            getProgramFileExtension(),
            maxProgramNameLength)
{
    driveParam = apvts.getRawParameterValue(ParamIDs::drive);
    wowParam = apvts.getRawParameterValue(ParamIDs::wow);
    flutterParam = apvts.getRawParameterValue(ParamIDs::flutter);
    modelParam = apvts.getRawParameterValue(ParamIDs::model);
    noiseParam = apvts.getRawParameterValue(ParamIDs::noise);
    noiseCharacterParam = apvts.getRawParameterValue(ParamIDs::noiseCharacter);
    humParam = apvts.getRawParameterValue(ParamIDs::hum);
    failureParam = apvts.getRawParameterValue(ParamIDs::failure);
    mixParam = apvts.getRawParameterValue(ParamIDs::mix);
    outputParam = apvts.getRawParameterValue(ParamIDs::output);
    spreadParam = apvts.getRawParameterValue(ParamIDs::spread);
    failureDropoutsParam = apvts.getRawParameterValue(ParamIDs::failureDropouts);
    failureSnagsParam = apvts.getRawParameterValue(ParamIDs::failureSnags);
    failureCrinklesParam = apvts.getRawParameterValue(ParamIDs::failureCrinkles);
    failureImbalanceParam = apvts.getRawParameterValue(ParamIDs::failureImbalance);
    genParam = apvts.getRawParameterValue(ParamIDs::gen);
    lpParam = apvts.getRawParameterValue(ParamIDs::lp);
    hpParam = apvts.getRawParameterValue(ParamIDs::hp);
    stopParam = apvts.getRawParameterValue(ParamIDs::stop);
    filterAuxParam = apvts.getRawParameterValue(ParamIDs::filterAux);
    failAuxParam = apvts.getRawParameterValue(ParamIDs::failAux);
    rampParam = apvts.getRawParameterValue(ParamIDs::ramp);
    switchModeParam = apvts.getRawParameterValue(ParamIDs::switchMode);

    for (int i = 0; i < maxGenerations; ++i)
        generationStages[(size_t) i] = std::make_unique<DegradationCore>(i);

    // Boots into Warm Cassette, which is now Program 01 rather than the entry behind Init.
    // **INIT is never the instantiation default** - it is a starting point the user chooses, not
    // the sound the plugin should make when a host loads it. Construction is single-threaded with
    // no host or automation attached, so applying this synchronously (rather than through the
    // pendingProgramIndex/AsyncUpdater path used by setCurrentProgram) is safe.
    store.refresh();
    applyFactoryProgram(kFactoryPrograms[defaultFactoryProgramIndex]);
    setCurrentId(factoryIdAt((int) defaultFactoryProgramIndex));
    captureProgramSnapshot();
}

//==============================================================================
// Identity. Nothing below this line addresses a Program by position except the four host overrides,
// which are grouped together and commented in the header.

ProgramId TapeRotAudioProcessor::factoryIdAt(int factoryPosition)
{
    const auto& p = kFactoryPrograms[(size_t) factoryPosition];
    return { ProgramBank::factory, p.slug, p.name };
}

ProgramId TapeRotAudioProcessor::initId()
{
    return { ProgramBank::init, kInitProgram.slug, kInitProgram.name };
}

ProgramId TapeRotAudioProcessor::getCurrentProgramId() const
{
    const juce::SpinLock::ScopedLockType lock(currentIdLock);
    return currentId;
}

void TapeRotAudioProcessor::setCurrentId(const ProgramId& id)
{
    const juce::SpinLock::ScopedLockType lock(currentIdLock);
    currentId = id;
}

int TapeRotAudioProcessor::factoryPositionOf(const juce::String& slug)
{
    for (size_t i = 0; i < kFactoryPrograms.size(); ++i)
        if (slug == kFactoryPrograms[i].slug)
            return (int) i;

    return -1;
}

ProgramId TapeRotAudioProcessor::resolve(ProgramBank bank, const juce::String& id,
                                          const juce::String& displayName) const
{
    if (bank == ProgramBank::init && id == kInitProgram.slug)
        return initId();

    if (bank == ProgramBank::factory)
        if (const int pos = factoryPositionOf(id); pos >= 0)
            return factoryIdAt(pos);

    if (bank == ProgramBank::user)
        if (store.fileFor(id) != juce::File())
            return { ProgramBank::user, id, id };

    // **Degrade honestly.** The parameter values restored from the session are correct and stay
    // put; only the NAME is unknown, so the panel says so rather than silently landing on whichever
    // Program now occupies some position. displayName is what gets painted - a factory slug is not
    // presentable, and "warm-cassette?" would read as a rendering fault.
    return { ProgramBank::unresolved, id, displayName.isNotEmpty() ? displayName : id };
}

std::vector<ProgramId> TapeRotAudioProcessor::listPrograms() const
{
    std::vector<ProgramId> out;
    out.reserve(1 + kFactoryPrograms.size() + (size_t) store.getFiles().size());

    out.push_back(initId());

    for (size_t i = 0; i < kFactoryPrograms.size(); ++i)
        out.push_back(factoryIdAt((int) i));

    for (const auto& f : store.getFiles())
    {
        const auto stem = f.getFileNameWithoutExtension();
        out.push_back({ ProgramBank::user, stem, stem });
    }

    return out;
}

juce::String TapeRotAudioProcessor::displayLabelFor(const ProgramId& id) const
{
    // The number is computed at paint time from the Program's position in the bank. It is a label:
    // nothing stores it and nothing resolves by it. Only Factory Programs get one - User Programs
    // sort alphabetically, so a number would change whenever one was saved.
    //
    // The position is resolved HERE because the Factory bank is this casting's own; core never
    // holds one.
    return nf::programDisplayLabel(id, id.bank == ProgramBank::factory ? factoryPositionOf(id.id)
                                                                       : -1);
}

/*  **The critical section is a SWAP now, and it used to be two assignments.**

    A `juce::String` copy is a refcount increment and reads as safe. The ASSIGNMENT is the other
    half: it releases whatever the target held first, and a refcount reaching zero calls `free()`.
    So `pendingProgram = id` and `id = pendingProgram` each did heap work, and both were inside the
    lock — on a path VST3 can deliver **on the audio thread**, since a program change is an
    automatable parameter there.

    **Measured at 0.12 us worst case against a 10,667 us block budget**, so this was never a dropout
    risk and is not sold as one. It is negligible because a refcount release happens to be cheap,
    not because anything guarantees the path stays heap-free — and the next person to add a field to
    `ProgramId` has no reason to think about it.

    The copy and the destruction both move OUT of the lock: `exchangePendingProgram` takes its
    argument by value, so the caller's copy is made in the caller's frame, and returns the previous
    program by value, so its release happens in the caller's frame too. What is left between the
    lock and the unlock is a pointer exchange.

    **Named functions rather than inline blocks because that is what makes it testable.** An
    allocation sentinel is not lock-aware, so a probe around `requestProgramChange` sees the same
    total either way — the change is WHERE the work happens, not whether it happens. Arming the
    sentinel around a function that IS the critical section is the only honest way to assert it. */
ProgramId TapeRotAudioProcessor::exchangePendingProgram (ProgramId incoming)
{
    const juce::SpinLock::ScopedLockType lock (pendingLock);

    std::swap (pendingProgram, incoming);
    hasPendingProgram = true;

    return incoming;   // the PREVIOUS pending program; it is released in the caller's frame
}

bool TapeRotAudioProcessor::takePendingProgram (ProgramId& out)
{
    const juce::SpinLock::ScopedLockType lock (pendingLock);

    if (! hasPendingProgram)
        return false;

    // `out` is empty on entry, so this is a pointer exchange and nothing is released here.
    std::swap (out, pendingProgram);
    hasPendingProgram = false;

    return true;
}

void TapeRotAudioProcessor::requestProgramChange (const ProgramId& id)
{
    // The copy is made HERE, in this frame: copying a ProgramId is two refcount increments, and an
    // increment never frees. The previous pending program comes back and is released here too.
    const ProgramId previous = exchangePendingProgram (id);
    juce::ignoreUnused (previous);

    triggerAsyncUpdate();
}

void TapeRotAudioProcessor::handleAsyncUpdate()
{
    ProgramId id;

    if (! takePendingProgram (id))
        return;

    applyProgram (id);
}

//==============================================================================
// The host adapter. See the header for why the list is Factory-only and what that costs.

int TapeRotAudioProcessor::getCurrentProgram()
{
    const auto id = getCurrentProgramId();

    if (id.bank == ProgramBank::factory)
        if (const int pos = factoryPositionOf(id.id); pos >= 0)
            return pos;

    // INIT, a User Program or an unresolved id - none of which the host's list contains. 0 is the
    // accepted lossy answer; see the header.
    return 0;
}

void TapeRotAudioProcessor::setCurrentProgram(int index)
{
    if (! juce::isPositiveAndBelow(index, (int) kNumFactoryPrograms))
        return;

    // The stale-replay guard, disarmed by this call whether or not it is honoured. A replay carries
    // the position we last reported, so a matching index immediately after a restore is ignored;
    // anything else, and every later call, applies normally.
    if (userEdits.consumeRestore() && index == getCurrentProgram())
        return;

    requestProgramChange(factoryIdAt(index));
}

const juce::String TapeRotAudioProcessor::getProgramName(int index)
{
    // Raw, with no number: a host renders its own numbering and would otherwise print "01" twice.
    return juce::isPositiveAndBelow(index, (int) kNumFactoryPrograms)
               ? juce::String(kFactoryPrograms[(size_t) index].name)
               : juce::String();
}

juce::File TapeRotAudioProcessor::getUserProgramDirectory() const
{
    return store.getDirectory();
}

juce::File TapeRotAudioProcessor::getDefaultUserProgramDirectory()
{
    // The per-OS resolution, the "Application Support" segment macOS alone needs, and the reason
    // ~/Library/Audio/Presets is the wrong answer are all in nf/UserProgramDirectory.h now. That
    // reasoning was carried in six near-identical comment blocks, and the one time it was wrong it
    // was wrong in all six at once.
    return nf::userProgramDirectory(pluginCompanyName, pluginProductName);
}

bool TapeRotAudioProcessor::isMomentaryTrigger(const juce::String& parameterID)
{
    // STOP, FILTER and FAIL are momentary triggers: never stored, forced false on every apply, and
    // deliberately not part of the dirty check either - holding one down must not read as an edit.
    //
    // **Stated as an exclusion now, not as the inclusion list it replaced.** That list named all 20
    // stored parameters explicitly, so 20 + 3 = 23 exactly covered the APVTS - and a parameter added
    // later without a matching line would silently have gone unchecked, with SAVE staying dark while
    // it moved. The two forms are equivalent today; only this one stays equivalent.
    return parameterID == ParamIDs::stop
        || parameterID == ParamIDs::filterAux
        || parameterID == ParamIDs::failAux;
}

void TapeRotAudioProcessor::captureProgramSnapshot()
{
    programSnapshot.capture(*this);
}

bool TapeRotAudioProcessor::isProgramModified() const
{
    return programSnapshot.differsFrom(*this, isMomentaryTrigger);
}

void TapeRotAudioProcessor::applyProgram(const ProgramId& id)
{
    if (id.bank == ProgramBank::init)
    {
        applyFactoryProgram(kInitProgram);
    }
    else if (id.bank == ProgramBank::factory)
    {
        const int pos = factoryPositionOf(id.id);

        if (pos < 0)
            return;

        applyFactoryProgram(kFactoryPrograms[(size_t) pos]);
    }
    else if (id.bank == ProgramBank::user)
    {
        const auto file = store.fileFor(id.id);

        if (file == juce::File())
            return;

        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
            return;

        LegacyMigration::remapLegacyModelIndexIfNeeded(*xml);
        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        // Momentary triggers are stripped on save (see saveUserProgram) but a hand-edited or
        // pre-existing file could still have one set - force them off regardless, so a program can
        // never load in a "stuck engaged" state.
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::stop)) = false;
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::filterAux)) = false;
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failAux)) = false;
    }
    else
    {
        // Unresolved. The values are whatever the session restored and stay exactly as they are -
        // landing on some other Program would be the silent wrong answer this whole model exists to
        // prevent. Only the identity is recorded, so the panel can say it does not know the name.
        setCurrentId(id);
        captureProgramSnapshot();
        return;
    }

    setCurrentId(id);
    captureProgramSnapshot();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
}

void TapeRotAudioProcessor::applyFactoryProgram(const FactoryProgram& program)
{
    *dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::model)) = program.modelIndex;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::drive)) = program.drivePercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::wow)) = program.wowPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::flutter)) = program.flutterPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::noise)) = program.noisePercent;
    *dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::noiseCharacter)) = program.noiseCharacter;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::hum)) = program.hum;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::failure)) = program.failurePercent;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureDropouts)) = program.failureDropouts;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureSnags)) = program.failureSnags;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureCrinkles)) = program.failureCrinkles;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureImbalance)) = program.failureImbalance;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::mix)) = program.mixPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::output)) = program.outputDb;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::spread)) = program.spread;
    *dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(ParamIDs::gen)) = program.gen;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::lp)) = program.lpHz;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::hp)) = program.hpHz;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::ramp)) = program.rampSeconds;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::switchMode)) = program.switchMode;

    // stop/filterAux/failAux are momentary triggers, not part of a program's own state at all -
    // always forced false, regardless of what the struct/file might (never should, but might)
    // contain, so a program can never load in a "stuck engaged" state.
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::stop)) = false;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::filterAux)) = false;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failAux)) = false;
}

void TapeRotAudioProcessor::saveUserProgram(const juce::String& rawName)
{
    // **What a Program CONTAINS stays here.** The whole APVTS state, less the three momentary
    // triggers - core owns naming, the collision check and the write, and takes finished XML.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute(LegacyMigration::stateSchemaVersionAttribute, LegacyMigration::currentStateSchemaVersion);

    // Momentary triggers are never part of a saved program (see applyProgramByIndex/
    // applyFactoryProgram) - stripped here too so the file on disk doesn't imply otherwise.
    for (int i = xml->getNumChildElements(); --i >= 0;)
    {
        auto* child = xml->getChildElement(i);

        if (isMomentaryTrigger(child->getStringAttribute("id")))
            xml->removeChildElement(child, true);
    }

    // **The empty-name fallback is `TAKE n` now, not `USER PROGRAM`.** The suite had six different
    // ones - USER PROGRAM, NEW PROGRAM three times, UNTITLED, TAKE n - and this is the one that is
    // better rather than merely different: consecutive empty saves give TAKE 3, TAKE 4 instead of
    // leaning on getNonexistentSibling for "USER PROGRAM (2)". A player meeting UNTITLED on one
    // casting and TAKE 3 on another is meeting drift, not character.
    //
    // Trimming, upper-casing and the 25-character cap also apply on every path now. They used to
    // live in ProgramHeader's keystroke filter alone, so any programmatic save bypassed all three.
    const auto file = store.save(rawName, *xml);

    if (file == juce::File())
        return;   // the write failed; the panel keeps naming the Program it was already on

    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));

    // **The stem comes off the file core returned, not off the requested name.** A collision takes
    // the next free sibling, so taking it from the request would point the panel at the first file
    // while the values came from the second.
    const auto stem = file.getFileNameWithoutExtension();
    requestProgramChange({ ProgramBank::user, stem, stem });
}

void TapeRotAudioProcessor::deleteUserProgram(const ProgramId& id)
{
    if (id.bank != ProgramBank::user)
        return;

    const bool wasCurrent = getCurrentProgramId() == id;

    if (! store.remove(id.id))
        return;

    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));

    // Deliberately NOT the unresolved state: deleting from the panel is an unambiguous intent, so
    // it falls back to the default Program. Unresolved is for a session naming something gone.
    if (wasCurrent)
        requestProgramChange(factoryIdAt((int) defaultFactoryProgramIndex));
}

void TapeRotAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) samplesPerBlock,
                                 (juce::uint32) getTotalNumOutputChannels()};

    saturator.prepare(spec, driveParam->load() * 0.01f);

    /*  **Every stage is prepared knowing which model is selected**, rather than being prepared to
        NONE and discovering the real one on its first block.

        `TapeModelEQ` stored `activeModelIndex = 0` on every prepare while the default Program
        selects 5, so the first block after any prepare crossfaded from a state nobody chose — 26.75 %
        of peak in FADE, 97.55 % in CLUNK. A host re-fires `prepareToPlay` on every sample-rate and
        buffer-size change, so this was not a once-per-instance cost.

        Read off the live parameter for the same reason Elmer's output stage does: a session restore
        writes the APVTS before the host prepares, so this is the value the first block should
        already be at. */
    const int initialModel = (int) modelParam->load();

    for (auto& stage : generationStages)
        stage->prepare(spec, initialModel, noiseParam->load() * 0.01f);
    hum.prepare(spec, humParam->load() > 0.5f);
    failureEngine.prepare(spec);
    stereoSpread.prepare(spec, spreadParam->load() > 0.5f);
    toneFilters.prepare(spec);
    tapeStop.prepare(spec);
    filterSweep.prepare(spec);
    failEnvelope.setSampleRate(sampleRate);
    outputStage.prepare(spec, mixParam->load() * 0.01f, outputParam->load());

    genSmoothed.reset(sampleRate, 0.04);
    genSmoothed.setCurrentAndTargetValue(genParam->load());
    genFloorSnapshot.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);

    const int maxDryDelaySamples =
        (int) std::ceil((double) maxGenerations * WowFlutter::nominalDelayMs * 0.001 * sampleRate);
    dryCompensationDelay.setMaximumDelayInSamples(juce::jmax(1, maxDryDelaySamples));
    dryCompensationDelay.prepare(spec);
    dryCompensationDelay.reset();

    transportGateSmoothed.reset(sampleRate, 0.05);
    transportGateSmoothed.setCurrentAndTargetValue(1.0f);

    displaySampleRate = sampleRate;

    pitchMeter.prepare(samplesPerBlock);
    // Stage 0's rates stand for the cascade in the readout - the stages are deliberately detuned
    // from one another, so there is no single "the" rate, and stage 0 is the one always running.
    wowRateDisplay.store(generationStages[0]->getWowRateHz(), std::memory_order_relaxed);
    flutterRateDisplay.store(generationStages[0]->getFlutterRateHz(), std::memory_order_relaxed);
    levelSmoothingCoeff = 1.0f - std::exp(-1.0f / (0.15f * (float) sampleRate));
    inLevelSmoothed = 0.0f;
    outLevelSmoothed = 0.0f;

    setLatencySamples(saturator.getLatencySamples());
}

//==============================================================================
/** A host's reset - a transport locate, a buffer clear - propagated to the DSP.

    **JUCE's base implementation is a no-op, and none of the six castings overrode it**, so until
    stage 1c a host asking every plugin in the session to clear itself was answered by nothing
    anywhere. Measured tails surviving a reset: Gatecrasher 0.679, Chorus-60 0.429, Reflect-84 0.111.

    Routed to the same per-stage `reset()` calls `prepareToPlay` already makes, and deliberately NOT
    to `prepareToPlay` itself: re-preparing would also re-run whatever a prepare re-arms, and this
    suite has a measured example of that being audible.
*/
void TapeRotAudioProcessor::reset()
{
    saturator.reset();
    for (auto& stage : generationStages)
        if (stage != nullptr)
            stage->reset();
    hum.reset();
    failureEngine.reset();
    stereoSpread.reset();
    toneFilters.reset();
    tapeStop.reset();
    filterSweep.reset();
    outputStage.reset();
    dryCompensationDelay.reset();
}

void TapeRotAudioProcessor::releaseResources()
{
}

bool TapeRotAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void TapeRotAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // IN is measured before the chain, OUT after it. Both are display-only.
    {
        const int n = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(c)[i]));
            inLevelSmoothed += (peak - inLevelSmoothed) * levelSmoothingCoeff;
        }
    }

    const float drive01 = driveParam->load() / 100.0f;
    const float wow01 = wowParam->load() / 100.0f;
    const float flutter01 = flutterParam->load() / 100.0f;
    const int model = (int) modelParam->load();
    const float noise01 = noiseParam->load() / 100.0f;
    const int noiseCharacter = (int) noiseCharacterParam->load();
    const bool humEnabled = humParam->load() > 0.5f;
    const float failure01 = failureParam->load() / 100.0f;
    const float mix01 = mixParam->load() / 100.0f;
    const float outputDb = outputParam->load();
    const bool spread = spreadParam->load() > 0.5f;
    const bool dropouts = failureDropoutsParam->load() > 0.5f;
    const bool snags = failureSnagsParam->load() > 0.5f;
    const bool crinkles = failureCrinklesParam->load() > 0.5f;
    const bool imbalance = failureImbalanceParam->load() > 0.5f;
    const float lpHz = lpParam->load();
    const float hpHz = hpParam->load();
    const bool stopEnabled = stopParam->load() > 0.5f;
    const bool filterAuxEnabled = filterAuxParam->load() > 0.5f;
    const bool failAuxEnabled = failAuxParam->load() > 0.5f;
    const float rampSeconds = rampParam->load();
    const bool clunkMode = switchModeParam->load() > 0.5f;

    // **The over-delivery policy, and here it is LOAD-BEARING rather than tidy.** Saturator.cpp:29
    // sizes the oversampler with initProcessing(maximumBlockSize) and Saturator.cpp:120 feeds
    // processSamplesUp whatever arrives, unclamped - so any block larger than the prepared maximum
    // writes OUT OF BOUNDS. That was once bisected as "survives 257, 300, 400, crashes by 450", and
    // every figure in it was measured and meaningless: the writes corrupted adjacent heap and
    // returned finite output, so the probe reported "survived, finite" truthfully and about
    // nothing. 450 was not a threshold, only the first size that reached an unmapped page.
    //
    // Chunking removes it BY CONSTRUCTION: no span is longer than the prepared size, so
    // processSamplesUp is never handed more than initProcessing allocated for.
    //
    // **THE BUS QUESTION, ASKED HERE.** Gatecrasher had to move its getBusBuffer calls inside the
    // loop, because asking once outside hands every span the whole block's length and undoes the
    // chunking while every assertion still passes. That does not arise here, and the reason is the
    // one that matters rather than the verdict: **there is no getBusBuffer call anywhere in this
    // file.** This casting reads the buffer directly and takes its channel count from
    // buffer.getNumChannels(), which a span reports correctly for itself. The call site was looked
    // for rather than inferred from the bus layout, because a casting can call getBusBuffer for its
    // main bus without having a second one.
    //
    // ScopedNoDenormals, the IN meter and the parameter reads stay OUTSIDE. The guard is scoped;
    // the IN meter is measured on the untouched input before the chain and would otherwise run per
    // span on a buffer the chain has already modified; the parameter reads are per block by design.
    nf::processInChunks(buffer, getBlockSize(), [&](juce::AudioBuffer<float>& span)
    {
    const int numSamples = span.getNumSamples();
    const int numChannels = span.getNumChannels();
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, span, ch, 0, numSamples);

    saturator.process(span, drive01);

    // NONE bypasses the tape-model/GEN system entirely: there's no model character to compound
    // across generations, so force a single pass regardless of the GEN control's position (still
    // via genSmoothed, so entering/leaving NONE ramps rather than jumps).
    const int requestedGen = (size_t) model == noneModelIndex ? 1 : (int) std::round(genParam->load());
    genSmoothed.setTargetValue((float) juce::jlimit(1, maxGenerations, requestedGen));

    float* const deviationAccum = pitchMeter.getScratch(numSamples);

    // **THE GEN CROSSING SUBDIVISION, and it lives INSIDE nf::processInChunks rather than instead of
    // it.** The two look interchangeable and are not: the chunker splits on a fixed length and
    // bounds span LENGTH, which is what keeps the oversampler in bounds and is therefore a SAFETY
    // property; this splits where genValue crosses an integer and bounds span CONTENT — one stage
    // count per sub-span — which is correctness. A buffer in which GEN happens not to cross is ONE
    // sub-span, so this loop cannot substitute for the chunker: it would hand the oversampler the
    // whole over-delivered buffer and reinstate the out-of-bounds write, with a green suite,
    // because nothing automates GEN in the test that would catch it.
    //
    // ## Why the previous fix was half a fix
    //
    // 65fb765 ramped the crossfade WEIGHT per sample and argued the block-end stage count was "the
    // edge rather than the case". That was written from prediction; `InvarianceTests`' genSmoothed
    // arm refuted it at 2.820183516. GEN swept 1 to 8 across 49152 samples moves 0.0091 per
    // 64-sample block and 0.29 per 2048, so floorGen/ceilGen taken from the block's END value make
    // the STAGE COUNT step with the buffer, and a per-sample weight cannot repair a per-block count.
    //
    // ## Why this is buffer-independent rather than merely finer
    //
    // The boundaries are read off the smoother's own trajectory, and `genSmoothed` advances by
    // SAMPLE COUNT — so its value at a given absolute sample is the same however the stream is cut.
    // The crossings therefore land at fixed absolute samples and the sub-spans are identical at
    // every block size. There is no interval to choose and no free parameter to get wrong.
    //
    // The step is taken from a COPY of the smoother rather than derived: `SmoothedValue` owns the
    // arithmetic, and reproducing it here would be a second copy to drift. Re-probed each sub-span
    // because the step becomes zero once the ramp settles.
    for (int done = 0; done < numSamples; )
    {
        const float genAtStart = genSmoothed.getCurrentValue();

        auto probe = genSmoothed;
        probe.skip(1);
        const float perSample = probe.getCurrentValue() - genAtStart;

        int subSamples = numSamples - done;

        if (perSample != 0.0f)
        {
            // The next integer the value will reach, in whichever direction it is travelling.
            const float boundary = perSample > 0.0f ? std::floor(genAtStart) + 1.0f
                                                    : std::ceil(genAtStart) - 1.0f;
            const int toBoundary = (int) std::ceil((boundary - genAtStart) / perSample);

            if (toBoundary > 0)
                subSamples = juce::jmin(subSamples, toBoundary);
        }

        genSmoothed.skip(subSamples);

        const float genAtEnd = genSmoothed.getCurrentValue();

        // Floor and ceil come from the sub-span, which is the whole point: within it the value
        // stays inside one integer interval, so one stage count is correct for all of it.
        const float genSpanValue = juce::jmax(genAtStart, genAtEnd);
        const int floorGen = juce::jlimit(1, maxGenerations, (int) std::floor(juce::jmin(genAtStart, genAtEnd)));
        const int ceilGen = juce::jlimit(1, maxGenerations, (int) std::ceil(genSpanValue));
        const bool genTransitioning = floorGen != ceilGen;

        juce::AudioBuffer<float> sub(span.getArrayOfWritePointers(), numChannels, done, subSamples);

        for (int stage = 0; stage < ceilGen; ++stage)
        {
            generationStages[(size_t) stage]->process(sub, wow01, flutter01, model, clunkMode, noise01,
                                                      noiseCharacter, deviationAccum + done);

            if (genTransitioning && stage == floorGen - 1)
                genFloorSnapshot.makeCopyOf(sub, true);
        }

        if (genTransitioning)
        {
            // The weight still ramps within the sub-span - the stage COUNT is now constant across
            // it, but the blend between floorGen and ceilGen stages still travels.
            const float startFraction = juce::jlimit(0.0f, 1.0f, genAtStart - (float) floorGen);
            const float endFraction = juce::jlimit(0.0f, 1.0f, genAtEnd - (float) floorGen);
            const float fractionStep = subSamples > 1 ? (endFraction - startFraction) / (float) (subSamples - 1)
                                                      : 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* wet = sub.getWritePointer(ch);
                const auto* floorTap = genFloorSnapshot.getReadPointer(ch);

                for (int i = 0; i < subSamples; ++i)
                {
                    const float f = startFraction + fractionStep * (float) i;
                    wet[i] = floorTap[i] * (1.0f - f) + wet[i] * f;
                }
            }
        }

        done += subSamples;
    }

    // The block's settled generation count, for the dry-path compensation below. Read after the
    // loop rather than carried through it: the loop advanced the smoother sub-span by sub-span, so
    // this is the same value one skip(numSamples) would have left, and the compensation delay is a
    // per-block quantity either way.
    const float genValue = genSmoothed.getCurrentValue();

    hum.process(span, humEnabled);

    failEnvelope.setEngaged(failAuxEnabled);
    failEnvelope.setRampSeconds(rampSeconds);
    const float failAuxValue = failEnvelope.advanceBlock(numSamples);
    const float effectiveFailure01 = juce::jmax(failure01, failAuxValue);
    failureEngine.process(span, effectiveFailure01, dropouts, snags, crinkles, imbalance);

    stereoSpread.process(span, spread);
    toneFilters.process(span, lpHz, hpHz);
    tapeStop.process(span, stopEnabled, rampSeconds);
    filterSweep.process(span, filterAuxEnabled, rampSeconds);

    // Re-time the dry copy to match the wet path's per-stage WowFlutter group delay (see the
    // dryCompensationDelay member comment in PluginProcessor.h) before MIX blends them.
    const float perStageDrySamples = (float) (WowFlutter::nominalDelayMs * 0.001 * displaySampleRate);
    dryCompensationDelay.setDelay(juce::jlimit(0.0f, (float) dryCompensationDelay.getMaximumDelayInSamples(),
                                                genValue * perStageDrySamples));
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* dry = dryBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            dryCompensationDelay.pushSample(ch, dry[i]);
            dry[i] = dryCompensationDelay.popSample(ch);
        }
    }

    outputStage.process(span, dryBuffer, mix01, outputDb);

    // The gate exists because this plugin generates sound of its own - hiss, hum, dropouts - which
    // would otherwise run forever in a session that is simply parked. Muting the output when the
    // transport stops is the right instinct for that.
    //
    // It must NOT apply in the Standalone. JUCE's standalone wrapper supplies a play head whose
    // getIsPlaying() is permanently false, so the `= true` fallback below never fires there and the
    // whole output was gated to silence for good: input metering read live while OUT sat at the
    // -99.9 floor, which looks exactly like a plugin that does not work. There is no transport to
    // park in a standalone, so there is nothing for the gate to mean.
    //
    // Live monitoring through a STOPPED DAW is still silenced, which is the same behaviour as
    // before and arguably still wrong - but changing it means gating the generated hiss rather than
    // the finished span, which is a DSP change rather than a wrapper check. Flagged, not smuggled
    // in here.
    bool hostIsPlaying = true;
    if (wrapperType != wrapperType_Standalone)
        if (auto* playHead = getPlayHead())
            if (auto position = playHead->getPosition())
                hostIsPlaying = position->getIsPlaying();

    transportGateSmoothed.setTargetValue(hostIsPlaying ? 1.0f : 0.0f);

    // **A gain ramp advanced per block and applied flat is a staircase on every transport start and
    // stop, and it coarsens as the span grows.** skip() then applyGain() moved the whole block to
    // the ramp's end value, so the fade in and out of a parked session was quantised to the host's
    // span size - inaudible at 64, a step at 2048.
    //
    // Per sample only while it is actually moving: settled, current == target and one applyGain is
    // both exact and cheaper.
    if (transportGateSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float gateGain = transportGateSmoothed.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
                span.getWritePointer(ch)[i] *= gateGain;
        }
    }
    else
    {
        span.applyGain(transportGateSmoothed.getCurrentValue());
    }

    constexpr float displayTimeConstantSeconds = 0.15f;
    const float blockSeconds = (float) span.getNumSamples() / (float) displaySampleRate;
    const float smoothingCoeff = 1.0f - std::exp(-blockSeconds / displayTimeConstantSeconds);

    auto smoothDisplay = [smoothingCoeff](std::atomic<float>& display, float target)
    {
        const float current = display.load(std::memory_order_relaxed);
        display.store(current + smoothingCoeff * (target - current), std::memory_order_relaxed);
    };

    smoothDisplay(failAuxDisplay, failAuxValue);

    // OUT, measured on the finished span.
    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax(peak, std::abs(span.getReadPointer(ch)[i]));
        outLevelSmoothed += (peak - outLevelSmoothed) * levelSmoothingCoeff;
    }

    // Hand the SPAN's realised pitch deviation to the scope. Inside the loop, because
    // deviationAccum is the scratch this span filled and numSamples is this span's length - the
    // FIFO decimates and drops, so more, shorter pushes are a finer trace rather than a different
    // one. Left outside it would have read one span's scratch as the whole block's.
    pitchMeter.pushBlock(deviationAccum, numSamples);
    });

    /*  **The meter readout is CLAMPED at both ends, and both ends were live defects.**

        Suite ruling 2026-08-14: floor sentinel, +99.9 ceiling, one decimal always, no plus at
        exactly 0.0 dB. The widest string any casting's meter well can be asked to draw is then
        **5** characters, as a guarantee rather than as a range, and 64 px of well holds it.

        **The floor was off by one character on every fade to silence.** 20*log10(1e-5) is exactly
        -100.0, so a linear value just above the threshold gave a dB just above -100.0, which
        `String (db, 1)` rounds to `"-100.0"` — six characters, from the one casting whose GUI had no
        clamp at all. The band is 1e-5 to 1.0058e-5, **0.58 % wide**, and a smoothed level crosses it
        whenever audio stops. Not an edge case: it is what happens at the end of every note, with a
        visible "-100.0" -> "-99.9" jump at the boundary.

        **The ceiling never existed anywhere in the suite.** No readout path in any casting had one,
        so the widest string was bounded only by how loud the signal got. Gatecrasher is the evidence
        that nobody considered the numerals needed one: it has a `Layout::meterCeilingDb`, and that
        constant feeds its meter BAR while its readout ignores it.

        Clamped here rather than in `ProgramHeader` because these two getters are the only readout
        path, so this is the one place both ends can be guaranteed rather than two places that have
        to agree. `Tests/MeteringTests.cpp` sweeps the linear range and asserts the character count.
    */
    const auto toDb = [](float linear)
    {
        const float db = linear > 1.0e-5f ? 20.0f * std::log10(linear) : meterFloorDb;
        return juce::jlimit(meterFloorDb, meterCeilingDb, db);
    };

    inputLevelDb.store(toDb(inLevelSmoothed), std::memory_order_relaxed);
    outputLevelDb.store(toDb(outLevelSmoothed), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TapeRotAudioProcessor::createEditor()
{
    return new TapeRotAudioProcessorEditor(*this);
}

void TapeRotAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute(LegacyMigration::stateSchemaVersionAttribute, LegacyMigration::currentStateSchemaVersion);

    // **The bank, the identifier, and the full parameter state.** The values above are what makes
    // the session sound right; the identity below only decides what the panel CALLS them. That
    // split is the point: whatever happens to the bank between versions, a session restores the
    // sound it was saved with, and at worst loses the name.
    const auto id = getCurrentProgramId();
    xml->setAttribute(LegacyMigration::programBankAttribute, LegacyMigration::bankAttributeValue(id.bank));
    xml->setAttribute(LegacyMigration::programIdAttribute, id.id);
    xml->setAttribute(LegacyMigration::programNameAttribute, id.displayName);

    copyXmlToBinary(*xml, destData);
}

void TapeRotAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes)); xml != nullptr)
        if (xml->hasTagName(apvts.state.getType()))
        {
            // Cancels any not-yet-applied program change: one requested just before a restore
            // could still be pending, and if the AsyncUpdater dispatched it AFTER this returned it
            // would overwrite everything just restored. A full state restore is authoritative.
            {
                const juce::SpinLock::ScopedLockType lock(pendingLock);
                hasPendingProgram = false;
            }

            LegacyMigration::remapLegacyModelIndexIfNeeded(*xml);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

            const int savedSchema = xml->getIntAttribute(LegacyMigration::stateSchemaVersionAttribute, 1);
            ProgramId restored;

            if (savedSchema >= 5)
            {
                restored = resolve(LegacyMigration::bankFromAttribute(
                                       xml->getStringAttribute(LegacyMigration::programBankAttribute)),
                                   xml->getStringAttribute(LegacyMigration::programIdAttribute),
                                   xml->getStringAttribute(LegacyMigration::programNameAttribute));
            }
            else
            {
                // v4 and older stored a position. Map it through the CURRENT bank - correct here
                // because nothing has shipped and the bank has not moved since v4 - and through the
                // v3->v4 hop first if this predates Init leaving the numbered bank.
                int savedIndex = xml->getIntAttribute("taperotCurrentProgramIndex",
                                                       (int) defaultFactoryProgramIndex);

                if (savedSchema < 4)
                    savedIndex = LegacyMigration::remapProgramIndexV3ToV4(savedIndex);

                if (savedIndex == -1)
                    restored = initId();
                else if (juce::isPositiveAndBelow(savedIndex, (int) kNumFactoryPrograms))
                    restored = factoryIdAt(savedIndex);
                else if (const int u = savedIndex - (int) kNumFactoryPrograms;
                         u >= 0 && u < store.getFiles().size())
                {
                    const auto stem = store.getFiles().getReference(u).getFileNameWithoutExtension();
                    restored = { ProgramBank::user, stem, stem };
                }
                else
                    restored = factoryIdAt((int) defaultFactoryProgramIndex);
            }

            setCurrentId(restored);

            // Snapshot the restored state, not the named Program's definition: reopening a session
            // should not present as "modified" before the user has touched anything.
            captureProgramSnapshot();

            // **Armed AFTER replaceState**, or the restore's own parameter writes would be mistaken
            // for activity and disarm it immediately. See the member's comment for what it guards.
            userEdits.armRestore();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeRotAudioProcessor();
}
