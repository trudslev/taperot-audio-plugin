#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

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

    // Boots into Warm Cassette, not Init - construction is single-threaded and has no host/
    // automation attached yet, so applying this synchronously (rather than through the
    // pendingProgramIndex/AsyncUpdater path used by setCurrentProgram) is safe.
    refreshUserPresetList();
    applyFactoryPreset(kFactoryPresets[warmCassetteProgramIndex]);
    currentProgramIndex.store((int) warmCassetteProgramIndex, std::memory_order_relaxed);
}

void TapeRotAudioProcessor::handleAsyncUpdate()
{
    const int index = pendingProgramIndex.exchange(-1, std::memory_order_relaxed);
    if (index >= 0)
        applyProgramByIndex(index);
}

int TapeRotAudioProcessor::getNumPrograms()
{
    return (int) kNumFactoryPresets + userPresetFiles.size();
}

void TapeRotAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    pendingProgramIndex.store(index, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

const juce::String TapeRotAudioProcessor::getProgramName(int index)
{
    if (isFactoryPreset(index))
        return kFactoryPresets[(size_t) index].name;

    const int userIndex = index - (int) kNumFactoryPresets;
    if (userIndex >= 0 && userIndex < userPresetFiles.size())
        return userPresetFiles.getReference(userIndex).getFileNameWithoutExtension();
    return {};
}

juce::File TapeRotAudioProcessor::getUserPresetDirectory()
{
   #if JUCE_WINDOWS
    // %APPDATA%\<Manufacturer>\<Plugin>\Presets - there's no Windows equivalent of macOS's
    // ~/Library/Audio/Presets convention, so this follows the common per-vendor AppData layout
    // instead.
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(JucePlugin_Manufacturer)
        .getChildFile(JucePlugin_Name)
        .getChildFile("Presets");
   #else
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library")
        .getChildFile("Audio")
        .getChildFile("Presets")
        .getChildFile(JucePlugin_Manufacturer)
        .getChildFile(JucePlugin_Name);
   #endif
}

void TapeRotAudioProcessor::refreshUserPresetList()
{
    userPresetFiles.clear();
    const auto dir = getUserPresetDirectory();
    if (!dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.taperotpreset"))
        userPresetFiles.add(entry.getFile());

    std::sort(userPresetFiles.begin(), userPresetFiles.end(),
               [](const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
}

void TapeRotAudioProcessor::applyProgramByIndex(int index)
{
    if (isFactoryPreset(index))
    {
        applyFactoryPreset(kFactoryPresets[(size_t) index]);
    }
    else
    {
        const int userIndex = index - (int) kNumFactoryPresets;
        if (userIndex < 0 || userIndex >= userPresetFiles.size())
            return;

        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(userPresetFiles.getReference(userIndex)));
        if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
            return;

        LegacyMigration::remapLegacyModelIndexIfNeeded(*xml);
        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        // Momentary triggers are stripped on save (see saveUserPreset) but a hand-edited or
        // pre-existing file could still have one set - force them off regardless, so a preset can
        // never load in a "stuck engaged" state.
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::stop)) = false;
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::filterAux)) = false;
        *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failAux)) = false;
    }

    currentProgramIndex.store(index, std::memory_order_relaxed);
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
}

void TapeRotAudioProcessor::applyFactoryPreset(const FactoryPreset& preset)
{
    *dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::model)) = preset.modelIndex;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::drive)) = preset.drivePercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::wow)) = preset.wowPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::flutter)) = preset.flutterPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::noise)) = preset.noisePercent;
    *dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::noiseCharacter)) = preset.noiseCharacter;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::hum)) = preset.hum;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::failure)) = preset.failurePercent;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureDropouts)) = preset.failureDropouts;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureSnags)) = preset.failureSnags;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureCrinkles)) = preset.failureCrinkles;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failureImbalance)) = preset.failureImbalance;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::mix)) = preset.mixPercent;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::output)) = preset.outputDb;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::spread)) = preset.spread;
    *dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(ParamIDs::gen)) = preset.gen;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::lp)) = preset.lpHz;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::hp)) = preset.hpHz;
    *dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::ramp)) = preset.rampSeconds;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::switchMode)) = preset.switchMode;

    // stop/filterAux/failAux are momentary triggers, not part of a preset's own state at all -
    // always forced false, regardless of what the struct/file might (never should, but might)
    // contain, so a preset can never load in a "stuck engaged" state.
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::stop)) = false;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::filterAux)) = false;
    *dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(ParamIDs::failAux)) = false;
}

void TapeRotAudioProcessor::saveUserPreset(const juce::String& name)
{
    const auto dir = getUserPresetDirectory();
    if (!dir.isDirectory())
        dir.createDirectory();

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute(LegacyMigration::stateSchemaVersionAttribute, LegacyMigration::currentStateSchemaVersion);

    // Momentary triggers are never part of a saved preset (see applyProgramByIndex/
    // applyFactoryPreset) - stripped here too so the file on disk doesn't imply otherwise.
    for (auto* id : {ParamIDs::stop, ParamIDs::filterAux, ParamIDs::failAux})
        for (int i = xml->getNumChildElements(); --i >= 0;)
        {
            auto* child = xml->getChildElement(i);
            if (child->getStringAttribute("id") == juce::String(id))
                xml->removeChildElement(child, true);
        }

    const juce::File file = dir.getChildFile(juce::File::createLegalFileName(name) + ".taperotpreset");
    xml->writeTo(file);

    refreshUserPresetList();
    const int newIndex = (int) kNumFactoryPresets + userPresetFiles.indexOf(file);
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
    setCurrentProgram(newIndex);
}

void TapeRotAudioProcessor::deleteUserPreset(int index)
{
    if (isFactoryPreset(index))
        return;

    const int userIndex = index - (int) kNumFactoryPresets;
    if (userIndex < 0 || userIndex >= userPresetFiles.size())
        return;

    const bool wasCurrent = currentProgramIndex.load(std::memory_order_relaxed) == index;
    userPresetFiles.getReference(userIndex).deleteFile();
    refreshUserPresetList();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));

    if (wasCurrent)
        setCurrentProgram((int) warmCassetteProgramIndex);
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

    transportGateSmoothed.reset(sampleRate, 0.05);
    transportGateSmoothed.setCurrentAndTargetValue(1.0f);

    displaySampleRate = sampleRate;

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

    for (int stage = 0; stage < ceilGen; ++stage)
    {
        generationStages[(size_t) stage]->process(buffer, wow01, flutter01, model, clunkMode, noise01, noiseCharacter);

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
    outputStage.process(buffer, dryBuffer, mix01, outputDb);

    bool hostIsPlaying = true;
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

    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax(peak, std::abs(data[i]));
    }
    const int writeIdx = scopeWriteIndex.load(std::memory_order_relaxed);
    scopeLevels[(size_t) writeIdx].store(peak, std::memory_order_relaxed);
    scopeWriteIndex.store((writeIdx + 1) % scopeHistorySize, std::memory_order_relaxed);
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
    // Sticky display metadata only - restored clamped/defaulted below, never re-validated against
    // the session's actual knob values (a session saved after manually tweaking a loaded preset
    // still remembers which preset name it was tweaked from).
    xml->setAttribute("taperotCurrentProgramIndex", currentProgramIndex.load(std::memory_order_relaxed));
    copyXmlToBinary(*xml, destData);
}

void TapeRotAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes)); xml != nullptr)
        if (xml->hasTagName(apvts.state.getType()))
        {
            // Cancels any not-yet-applied setCurrentProgram call: without this, a program change
            // requested just before a session/state restore could still be sitting in
            // pendingProgramIndex, waiting for the message thread's next AsyncUpdater dispatch -
            // and if that dispatch lands *after* this restore returns, it would silently
            // overwrite the just-restored parameter values with the stale pending preset. A full
            // state restore is always authoritative, so any such pending program change is
            // dropped rather than left to fire later.
            pendingProgramIndex.store(-1, std::memory_order_relaxed);

            LegacyMigration::remapLegacyModelIndexIfNeeded(*xml);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

            const int savedProgramIndex = xml->getIntAttribute("taperotCurrentProgramIndex", (int) warmCassetteProgramIndex);
            currentProgramIndex.store(juce::isPositiveAndBelow(savedProgramIndex, getNumPrograms())
                                           ? savedProgramIndex : (int) warmCassetteProgramIndex,
                                       std::memory_order_relaxed);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeRotAudioProcessor();
}
