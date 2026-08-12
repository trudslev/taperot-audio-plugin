#include "PluginProcessor.h"
#include "PluginEditor.h"
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

    // The file extension, named once. It was a literal in two places, which is two chances to
    // rename one and not the other - and a Program written under one spelling is invisible to a
    // scan using the other.
    constexpr const char* programFileExtension = ".taperotprogram";
}

TapeRotAudioProcessor::TapeRotAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createTapeRotParameterLayout())
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
    refreshUserProgramList();
    applyFactoryProgram(kFactoryPrograms[warmCassetteProgramIndex]);
    setCurrentId(factoryIdAt((int) warmCassetteProgramIndex));
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
        for (const auto& f : userProgramFiles)
            if (f.getFileNameWithoutExtension() == id)
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
    out.reserve(1 + kFactoryPrograms.size() + (size_t) userProgramFiles.size());

    out.push_back(initId());

    for (size_t i = 0; i < kFactoryPrograms.size(); ++i)
        out.push_back(factoryIdAt((int) i));

    for (const auto& f : userProgramFiles)
    {
        const auto stem = f.getFileNameWithoutExtension();
        out.push_back({ ProgramBank::user, stem, stem });
    }

    return out;
}

juce::String TapeRotAudioProcessor::displayLabelFor(const ProgramId& id) const
{
    // The number is computed here, at paint time, from the Program's position in the bank. It is a
    // label: nothing stores it and nothing resolves by it. Only Factory Programs get one - User
    // Programs sort alphabetically, so a number would change whenever one was saved.
    if (id.bank == ProgramBank::factory)
        if (const int pos = factoryPositionOf(id.id); pos >= 0)
            return juce::String(pos + 1).paddedLeft('0', 2) + " " + id.displayName;

    return id.displayName;
}

void TapeRotAudioProcessor::requestProgramChange(const ProgramId& id)
{
    {
        const juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingProgram = id;
        hasPendingProgram = true;
    }

    triggerAsyncUpdate();
}

void TapeRotAudioProcessor::handleAsyncUpdate()
{
    ProgramId id;

    {
        const juce::SpinLock::ScopedLockType lock(pendingLock);

        if (! hasPendingProgram)
            return;

        id = pendingProgram;
        hasPendingProgram = false;
    }

    applyProgram(id);
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
    if (justRestoredState.exchange(false, std::memory_order_relaxed) && index == getCurrentProgram())
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

juce::File TapeRotAudioProcessor::getUserProgramDirectory()
{
    // **Application data on every platform - no macOS special case.** This used to branch, putting
    // macOS Programs under ~/Library/Audio/Presets. That is Apple's location for the AU PRESET
    // FORMAT: .aupreset files the AU system itself scans, reads and writes. Our user Programs are
    // not those - they are application-owned data in our own XML format - so they belong where an
    // application keeps its data, and the AU folder should hold only what AU understands.
    //
    // The old comment argued that renaming the leaf "would move saved Programs somewhere no other
    // tool looks". That was true and beside the point: no other tool was ever going to read a
    // .taperotprogram out of Apple's preset folder either. Nothing was being made discoverable.
    //
    // **macOS needs the "Application Support" segment added by hand, and only macOS.** JUCE's
    // userApplicationDataDirectory is `~/Library` there - NOT `~/Library/Application Support` -
    // while it is `%APPDATA%` on Windows and `~/.config` on Linux, both of which are already the
    // right root. JUCE's own PropertiesFile appends the segment the same way, for the same reason.
    //
    // This was got wrong once in exactly the plausible direction: the note here used to claim JUCE
    // resolved the segment for us, and that hard-coding it would be wrong on two platforms out of
    // three. The first half was false, and the second half only argues for the `#if` - it is one
    // platform's extra segment, not a shared literal path. Programs landed directly in
    // `~/Library/<Company>/` for a while, which is not where application data goes on macOS and is
    // not a folder anything else writes into.
    //
    // No migration from the old location - nothing has shipped, so nothing is there to migrate.
    // See Elmer's ProgramManager for why that is a decision rather than an oversight.
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    dir = dir.getChildFile("Application Support");
   #endif

    return dir
        .getChildFile(pluginCompanyName)
        .getChildFile(pluginProductName)
        .getChildFile("Programs");
}

void TapeRotAudioProcessor::refreshUserProgramList()
{
    userProgramFiles.clear();
    const auto dir = getUserProgramDirectory();
    if (!dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, juce::String("*") + programFileExtension))
        userProgramFiles.add(entry.getFile());

    // **The displayed name, case-insensitively.** Two deliberate details:
    //
    // The STEM, not getFileName() - comparing with the extension attached sorts "AB C" before "AB",
    // because a space (0x20) precedes the dot (0x2E). Every casting had that bug.
    //
    // compareIgnoreCase, not operator< - which is a codepoint compare, so every lowercase name
    // sorted after every uppercase one and "Zebra" preceded "apple". The GUI forces uppercase as
    // you type, which masked it for anything created here; a file hand-renamed on disk, copied
    // between machines or restored from a backup keeps its own case and unmasks it.
    std::sort(userProgramFiles.begin(), userProgramFiles.end(),
               [](const juce::File& a, const juce::File& b)
               {
                   return a.getFileNameWithoutExtension()
                           .compareIgnoreCase(b.getFileNameWithoutExtension()) < 0;
               });
}

const juce::StringArray& TapeRotAudioProcessor::snapshotParamIds()
{
    // Everything a Program stores. STOP/FILTER/FAIL are deliberately absent - they are momentary
    // triggers, never saved, so holding one must not make the Program look modified.
    static const juce::StringArray ids {
        ParamIDs::drive, ParamIDs::wow, ParamIDs::flutter, ParamIDs::model, ParamIDs::noise,
        ParamIDs::noiseCharacter, ParamIDs::hum, ParamIDs::failure, ParamIDs::failureDropouts,
        ParamIDs::failureSnags, ParamIDs::failureCrinkles, ParamIDs::failureImbalance,
        ParamIDs::mix, ParamIDs::output, ParamIDs::spread, ParamIDs::gen, ParamIDs::lp,
        ParamIDs::hp, ParamIDs::ramp, ParamIDs::switchMode };
    return ids;
}

void TapeRotAudioProcessor::captureProgramSnapshot()
{
    const auto& ids = snapshotParamIds();
    std::vector<float> fresh;
    fresh.reserve((size_t) ids.size());

    for (const auto& id : ids)
        if (auto* v = apvts.getRawParameterValue(id))
            fresh.push_back(v->load(std::memory_order_relaxed));
        else
            fresh.push_back(0.0f);

    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    programSnapshot = std::move(fresh);
}

bool TapeRotAudioProcessor::isProgramModified() const
{
    const auto& ids = snapshotParamIds();

    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    if (programSnapshot.size() != (size_t) ids.size())
        return false;                       // nothing captured yet - nothing to compare against

    for (int i = 0; i < ids.size(); ++i)
        if (auto* v = apvts.getRawParameterValue(ids[i]))
        {
            // Physical values, so the tolerance has to suit the widest range here (LP, 1k-20k).
            // Anything a user can hear moving is far larger than this.
            const float current = v->load(std::memory_order_relaxed);
            if (std::abs(current - programSnapshot[(size_t) i]) > 1.0e-3f)
                return true;
        }

    return false;
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
        const auto file = userProgramFile(id.id);

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

juce::File TapeRotAudioProcessor::userProgramFile(const juce::String& stem) const
{
    for (const auto& f : userProgramFiles)
        if (f.getFileNameWithoutExtension() == stem)
            return f;

    return {};
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
    // The GUI lets the name be typed, so it can arrive empty or as whitespace. Falling back here
    // rather than in the caller means every future caller is safe too - an empty name would
    // otherwise reduce to an empty filename and write a dotfile.
    const auto name = rawName.trim().isEmpty() ? juce::String("USER PROGRAM") : rawName.trim();

    const auto dir = getUserProgramDirectory();
    if (!dir.isDirectory())
        dir.createDirectory();

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute(LegacyMigration::stateSchemaVersionAttribute, LegacyMigration::currentStateSchemaVersion);

    // Momentary triggers are never part of a saved program (see applyProgramByIndex/
    // applyFactoryProgram) - stripped here too so the file on disk doesn't imply otherwise.
    for (auto* id : {ParamIDs::stop, ParamIDs::filterAux, ParamIDs::failAux})
        for (int i = xml->getNumChildElements(); --i >= 0;)
        {
            auto* child = xml->getChildElement(i);
            if (child->getStringAttribute("id") == juce::String(id))
                xml->removeChildElement(child, true);
        }

    juce::File file = dir.getChildFile(juce::File::createLegalFileName(name) + programFileExtension);

    // **SAVE never overwrites**, which BRAND.md states as a guarantee and this used to break: a
    // second save under the same name replaced the first Program's contents silently. It matters
    // more now that the filename IS the identity - two Programs cannot share one.
    if (file.existsAsFile())
        file = file.getNonexistentSibling();

    xml->writeTo(file);

    refreshUserProgramList();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));

    const auto stem = file.getFileNameWithoutExtension();
    requestProgramChange({ ProgramBank::user, stem, stem });
}

void TapeRotAudioProcessor::deleteUserProgram(const ProgramId& id)
{
    if (id.bank != ProgramBank::user)
        return;

    const auto file = userProgramFile(id.id);

    if (file == juce::File())
        return;

    const bool wasCurrent = getCurrentProgramId() == id;
    file.deleteFile();
    refreshUserProgramList();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));

    // Deliberately NOT the unresolved state: deleting from the panel is an unambiguous intent, so
    // it falls back to the default Program. Unresolved is for a session naming something gone.
    if (wasCurrent)
        requestProgramChange(factoryIdAt((int) warmCassetteProgramIndex));
}

void TapeRotAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) samplesPerBlock,
                                 (juce::uint32) getTotalNumOutputChannels()};

    saturator.prepare(spec);
    for (auto& stage : generationStages)
        stage->prepare(spec);
    hum.prepare(spec);
    failureEngine.prepare(spec);
    stereoSpread.prepare(spec);
    toneFilters.prepare(spec);
    tapeStop.prepare(spec);
    filterSweep.prepare(spec);
    failEnvelope.setSampleRate(sampleRate);
    outputStage.prepare(spec);

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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    saturator.process(buffer, drive01);

    // NONE bypasses the tape-model/GEN system entirely: there's no model character to compound
    // across generations, so force a single pass regardless of the GEN control's position (still
    // via genSmoothed, so entering/leaving NONE ramps rather than jumps).
    const int requestedGen = (size_t) model == noneModelIndex ? 1 : (int) std::round(genParam->load());
    genSmoothed.setTargetValue((float) juce::jlimit(1, maxGenerations, requestedGen));
    genSmoothed.skip(numSamples);
    const float genValue = genSmoothed.getCurrentValue();

    const int floorGen = juce::jlimit(1, maxGenerations, (int) std::floor(genValue));
    const int ceilGen = juce::jlimit(1, maxGenerations, (int) std::ceil(genValue));
    const float genFraction = genValue - (float) floorGen;
    const bool genTransitioning = floorGen != ceilGen;

    // Every active stage adds its realised deviation into one buffer, so the trace shows the whole
    // cascade rather than one stage - which is what the GEN readout sitting beside it implies.
    float* const deviationAccum = pitchMeter.getScratch(numSamples);

    for (int stage = 0; stage < ceilGen; ++stage)
    {
        generationStages[(size_t) stage]->process(buffer, wow01, flutter01, model, clunkMode, noise01,
                                                  noiseCharacter, deviationAccum);

        if (genTransitioning && stage == floorGen - 1)
            genFloorSnapshot.makeCopyOf(buffer, true);
    }

    if (genTransitioning)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* floorTap = genFloorSnapshot.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = floorTap[i] * (1.0f - genFraction) + wet[i] * genFraction;
        }

    hum.process(buffer, humEnabled);

    failEnvelope.setEngaged(failAuxEnabled);
    failEnvelope.setRampSeconds(rampSeconds);
    const float failAuxValue = failEnvelope.advanceBlock(numSamples);
    const float effectiveFailure01 = juce::jmax(failure01, failAuxValue);
    failureEngine.process(buffer, effectiveFailure01, dropouts, snags, crinkles, imbalance);

    stereoSpread.process(buffer, spread);
    toneFilters.process(buffer, lpHz, hpHz);
    tapeStop.process(buffer, stopEnabled, rampSeconds);
    filterSweep.process(buffer, filterAuxEnabled, rampSeconds);

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

    outputStage.process(buffer, dryBuffer, mix01, outputDb);

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
    // the finished buffer, which is a DSP change rather than a wrapper check. Flagged, not smuggled
    // in here.
    bool hostIsPlaying = true;
    if (wrapperType != wrapperType_Standalone)
        if (auto* playHead = getPlayHead())
            if (auto position = playHead->getPosition())
                hostIsPlaying = position->getIsPlaying();

    transportGateSmoothed.setTargetValue(hostIsPlaying ? 1.0f : 0.0f);
    transportGateSmoothed.skip(numSamples);
    buffer.applyGain(transportGateSmoothed.getCurrentValue());

    constexpr float displayTimeConstantSeconds = 0.15f;
    const float blockSeconds = (float) buffer.getNumSamples() / (float) displaySampleRate;
    const float smoothingCoeff = 1.0f - std::exp(-blockSeconds / displayTimeConstantSeconds);

    auto smoothDisplay = [smoothingCoeff](std::atomic<float>& display, float target)
    {
        const float current = display.load(std::memory_order_relaxed);
        display.store(current + smoothingCoeff * (target - current), std::memory_order_relaxed);
    };

    smoothDisplay(failAuxDisplay, failAuxValue);

    // OUT, measured on the finished buffer.
    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));
        outLevelSmoothed += (peak - outLevelSmoothed) * levelSmoothingCoeff;
    }

    const auto toDb = [](float linear)
    {
        return linear > 1.0e-5f ? 20.0f * std::log10(linear) : -99.9f;
    };

    inputLevelDb.store(toDb(inLevelSmoothed), std::memory_order_relaxed);
    outputLevelDb.store(toDb(outLevelSmoothed), std::memory_order_relaxed);

    // Hand the block's realised pitch deviation to the scope. Decimates and drops if the GUI is not
    // draining; never blocks.
    pitchMeter.pushBlock(deviationAccum, numSamples);
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
                                                       (int) warmCassetteProgramIndex);

                if (savedSchema < 4)
                    savedIndex = LegacyMigration::remapProgramIndexV3ToV4(savedIndex);

                if (savedIndex == -1)
                    restored = initId();
                else if (juce::isPositiveAndBelow(savedIndex, (int) kNumFactoryPrograms))
                    restored = factoryIdAt(savedIndex);
                else if (const int u = savedIndex - (int) kNumFactoryPrograms;
                         u >= 0 && u < userProgramFiles.size())
                {
                    const auto stem = userProgramFiles.getReference(u).getFileNameWithoutExtension();
                    restored = { ProgramBank::user, stem, stem };
                }
                else
                    restored = factoryIdAt((int) warmCassetteProgramIndex);
            }

            setCurrentId(restored);

            // Snapshot the restored state, not the named Program's definition: reopening a session
            // should not present as "modified" before the user has touched anything.
            captureProgramSnapshot();

            // **Armed AFTER replaceState**, or the restore's own parameter writes would be mistaken
            // for activity and disarm it immediately. See the member's comment for what it guards.
            justRestoredState.store(true, std::memory_order_relaxed);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeRotAudioProcessor();
}
