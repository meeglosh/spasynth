#include "ArsenalProcessor.h"
#include "dsp/WavetableLoader.h"
#include "params/Randomizer.h"
#include "ui/ArsenalEditor.h"

namespace arsenal
{

namespace
{
    constexpr const char* wavetableStateType = "WAVETABLES";
    constexpr const char* sampleStateType = "SAMPLES";

    juce::Identifier slotPathProperty (int slot)
    {
        return { "slot" + juce::String (slot) };
    }

    const juce::Identifier wildnessProperty { "randomWildness" };
    const juce::Identifier lockMaskProperty { "randomLockMask" };
}

ArsenalProcessor::ArsenalProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout())
{
    raw.masterGain = apvts.getRawParameterValue (params::id::masterGain);
    raw.filterType = apvts.getRawParameterValue (params::id::filter1Type);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto& rs = raw.slots[(size_t) s];
        const auto pid = [s] (const char* key) { return params::id::oscSlot (s, key); };
        rs.enable      = apvts.getRawParameterValue (pid (params::id::osc::enable));
        rs.mode        = apvts.getRawParameterValue (pid (params::id::osc::mode));
        rs.phase       = apvts.getRawParameterValue (pid (params::id::osc::phase));
        rs.phaseMode   = apvts.getRawParameterValue (pid (params::id::osc::phaseMode));
        rs.unisonCount = apvts.getRawParameterValue (pid (params::id::osc::unisonCount));
        rs.sampleStart = apvts.getRawParameterValue (pid (params::id::osc::sampleStart));
        rs.loop        = apvts.getRawParameterValue (pid (params::id::osc::loop));
        rs.loopStart   = apvts.getRawParameterValue (pid (params::id::osc::loopStart));
        rs.loopEnd     = apvts.getRawParameterValue (pid (params::id::osc::loopEnd));
        rs.keytrack    = apvts.getRawParameterValue (pid (params::id::osc::keytrack));
        rs.rootNote    = apvts.getRawParameterValue (pid (params::id::osc::rootNote));
        rs.grainPitch  = apvts.getRawParameterValue (pid (params::id::osc::grainPitch));
    }

    for (int i = 0; i < params::numLFOs; ++i)
    {
        auto& rl = raw.lfos[(size_t) i];
        const auto pid = [i] (const char* key) { return params::id::lfoParam (i, key); };
        rl.shape    = apvts.getRawParameterValue (pid (params::id::lfo::shape));
        rl.rate     = apvts.getRawParameterValue (pid (params::id::lfo::rate));
        rl.sync     = apvts.getRawParameterValue (pid (params::id::lfo::sync));
        rl.division = apvts.getRawParameterValue (pid (params::id::lfo::division));
        rl.phase    = apvts.getRawParameterValue (pid (params::id::lfo::phase));
        rl.retrig   = apvts.getRawParameterValue (pid (params::id::lfo::retrig));
        rl.unipolar = apvts.getRawParameterValue (pid (params::id::lfo::unipolar));
    }

    for (int m = 0; m < params::numMacros; ++m)
        raw.macros[(size_t) m] = apvts.getRawParameterValue (params::id::macro (m));

    for (int r = 0; r < params::numModRoutes; ++r)
    {
        auto& rr = raw.routes[(size_t) r];
        rr.source = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::source));
        rr.dest   = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::dest));
        rr.depth  = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::depth));
    }

    {
        namespace ch = params::id::chaos;
        raw.chaos.enable         = apvts.getRawParameterValue (ch::enable);
        raw.chaos.pitchOn        = apvts.getRawParameterValue (ch::pitchOn);
        raw.chaos.pitchAmount    = apvts.getRawParameterValue (ch::pitchAmount);
        raw.chaos.phaseOn        = apvts.getRawParameterValue (ch::phaseOn);
        raw.chaos.phaseAmount    = apvts.getRawParameterValue (ch::phaseAmount);
        raw.chaos.positionOn     = apvts.getRawParameterValue (ch::positionOn);
        raw.chaos.positionAmount = apvts.getRawParameterValue (ch::positionAmount);
        raw.chaos.ampOn          = apvts.getRawParameterValue (ch::ampOn);
        raw.chaos.ampAmount      = apvts.getRawParameterValue (ch::ampAmount);
        raw.chaos.satOn          = apvts.getRawParameterValue (ch::satOn);
        raw.chaos.saturation     = apvts.getRawParameterValue (ch::saturation);
        raw.chaos.distOn         = apvts.getRawParameterValue (ch::distOn);
        raw.chaos.distortion     = apvts.getRawParameterValue (ch::distortion);
    }

    raw.dests.reserve ((size_t) params::numModDests());
    for (const auto& dest : params::modDestinations())
        raw.dests.push_back (apvts.getRawParameterValue (dest.def->id));

    {
        namespace fx = params::id::fx;
        auto& rf = raw.fx;
        rf.distEnable     = apvts.getRawParameterValue (fx::distEnable);
        rf.distType       = apvts.getRawParameterValue (fx::distType);
        rf.distDrive      = apvts.getRawParameterValue (fx::distDrive);
        rf.distTone       = apvts.getRawParameterValue (fx::distTone);
        rf.distMix        = apvts.getRawParameterValue (fx::distMix);
        rf.chorusEnable   = apvts.getRawParameterValue (fx::chorusEnable);
        rf.chorusRate     = apvts.getRawParameterValue (fx::chorusRate);
        rf.chorusDepth    = apvts.getRawParameterValue (fx::chorusDepth);
        rf.chorusFeedback = apvts.getRawParameterValue (fx::chorusFeedback);
        rf.chorusMix      = apvts.getRawParameterValue (fx::chorusMix);
        rf.delayEnable    = apvts.getRawParameterValue (fx::delayEnable);
        rf.delaySync      = apvts.getRawParameterValue (fx::delaySync);
        rf.delayTime      = apvts.getRawParameterValue (fx::delayTime);
        rf.delayDivision  = apvts.getRawParameterValue (fx::delayDivision);
        rf.delayFeedback  = apvts.getRawParameterValue (fx::delayFeedback);
        rf.delayPingPong  = apvts.getRawParameterValue (fx::delayPingPong);
        rf.delayMix       = apvts.getRawParameterValue (fx::delayMix);
        rf.reverbEnable   = apvts.getRawParameterValue (fx::reverbEnable);
        rf.reverbSize     = apvts.getRawParameterValue (fx::reverbSize);
        rf.reverbDamping  = apvts.getRawParameterValue (fx::reverbDamping);
        rf.reverbWidth    = apvts.getRawParameterValue (fx::reverbWidth);
        rf.reverbMix      = apvts.getRawParameterValue (fx::reverbMix);
        rf.eqEnable       = apvts.getRawParameterValue (fx::eqEnable);
        rf.eqLowGain      = apvts.getRawParameterValue (fx::eqLowGain);
        rf.eqMidFreq      = apvts.getRawParameterValue (fx::eqMidFreq);
        rf.eqMidGain      = apvts.getRawParameterValue (fx::eqMidGain);
        rf.eqHighGain     = apvts.getRawParameterValue (fx::eqHighGain);
    }

    factoryTable = std::make_shared<const dsp::Wavetable> (dsp::Wavetable::createBasicShapes());
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        slotTables[(size_t) s].current = factoryTable;
        slotTables[(size_t) s].live.store (factoryTable.get());
    }

    shared.telemetry = &telemetry;
    midiLearn = std::make_unique<MidiLearnManager> (apvts);

    synth.addSound (new dsp::ArsenalSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new dsp::ArsenalVoice (shared));

    presetManager = std::make_unique<library::PresetManager> (
        apvts,
        [this] { return buildStateTree (false); },   // presets carry no MIDI map
        [this] (const juce::ValueTree& state) { restoreStateTree (state); },
        library::defaultPresetsRoot());

    // Auto-discover the library and make sure factory presets exist — no
    // user setup required when content sits in a standard install location.
    juce::MessageManager::callAsync ([weak = juce::WeakReference<ArsenalProcessor> (this)]
    {
        if (weak != nullptr)
            weak->refreshLibrary();
    });

    startTimer (1000);  // purges retired wavetables
}

ArsenalProcessor::~ArsenalProcessor()
{
    stopTimer();
}

void ArsenalProcessor::timerCallback()
{
    // Anything retired more than one timer period ago can no longer be in
    // use by the audio thread (it re-reads `live` every block).
    retiredTables.clear();
    retiredSamples.clear();
}

void ArsenalProcessor::installSample (int slot, std::shared_ptr<const dsp::SampleData> sample,
                                      juce::String path, juce::String error)
{
    auto& ss = slotSamples[(size_t) slot];
    ss.error = std::move (error);

    if (sample != nullptr)
    {
        if (ss.current != nullptr)
            retiredSamples.push_back (std::move (ss.current));
        ss.current = std::move (sample);
        ss.live.store (ss.current.get());
        ss.path = std::move (path);
    }

    sendChangeMessage();
}

void ArsenalProcessor::loadSampleFromFile (int slot, const juce::File& file)
{
    juce::Thread::launch ([this, slot, file]
    {
        auto result = dsp::loadSampleFromFile (file);

        juce::MessageManager::callAsync ([this, slot, loaded = std::move (result),
                                          path = file.getFullPathName()]() mutable
        {
            installSample (slot, std::move (loaded.sample), path, loaded.error);
        });
    });
}

juce::String ArsenalProcessor::getSampleName (int slot) const
{
    const auto& current = slotSamples[(size_t) slot].current;
    return current != nullptr ? current->name : juce::String();
}

juce::String ArsenalProcessor::getSampleError (int slot) const
{
    return slotSamples[(size_t) slot].error;
}

float ArsenalProcessor::getRandomWildness() const
{
    return (float) (double) apvts.state.getProperty (wildnessProperty, 0.5);
}

void ArsenalProcessor::setRandomWildness (float wildness)
{
    apvts.state.setProperty (wildnessProperty, (double) wildness, nullptr);
}

bool ArsenalProcessor::isLockGroupLocked (int group) const
{
    const auto mask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);
    return (mask & (1u << group)) != 0;
}

void ArsenalProcessor::setLockGroupLocked (int group, bool locked)
{
    auto mask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);
    mask = locked ? (mask | (1u << group)) : (mask & ~(1u << group));
    apvts.state.setProperty (lockMaskProperty, (int) mask, nullptr);
}

void ArsenalProcessor::randomizeAll()
{
    auto& rng = juce::Random::getSystemRandom();
    const auto wildness = getRandomWildness();
    const auto lockedMask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);

    params::randomizeAll (apvts, wildness, lockedMask, rng);

    // --- Musicality post-pass ------------------------------------------------
    const auto setNorm = [this] (const juce::String& id, float norm)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };
    const auto realValue = [this] (const juce::String& id)
    {
        auto* param = apvts.getParameter (id);
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    };

    const bool oscsUnlocked = (lockedMask & (1u << (int) params::LockGroup::oscillators)) == 0;
    const bool filterUnlocked = (lockedMask & (1u << (int) params::LockGroup::filter)) == 0;

    if (oscsUnlocked)
    {
        namespace osc = params::id::osc;
        for (int s = 0; s < params::numOscSlots; ++s)
        {
            // A slot with no sample loaded can't be in a sample mode.
            if (slotSamples[(size_t) s].current == nullptr)
                setNorm (params::id::oscSlot (s, osc::mode), 0.0f);

            // Keep loop points ordered with a usable window.
            const auto loopStart = realValue (params::id::oscSlot (s, osc::loopStart));
            const auto loopEnd = realValue (params::id::oscSlot (s, osc::loopEnd));
            if (loopEnd < loopStart + 0.05f)
                setNorm (params::id::oscSlot (s, osc::loopEnd),
                         juce::jlimit (0.0f, 1.0f, loopStart + 0.2f));
        }
    }

    if (filterUnlocked)
    {
        // High-passing everything above the note range means silence: pull
        // HP/notch cutoffs back into a musical zone.
        const auto type = (params::FilterType) (int) realValue (params::id::filter1Type);
        const auto cutoff = realValue (params::id::filter1Cutoff);
        const bool subtractive = type == params::FilterType::hp12
                              || type == params::FilterType::hp24
                              || type == params::FilterType::notch12
                              || type == params::FilterType::notch24;

        if (subtractive && cutoff > 2000.0f)
            if (auto* param = apvts.getParameter (params::id::filter1Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 850.0f));
    }

    sendChangeMessage();
}

void ArsenalProcessor::installTable (int slot, std::shared_ptr<const dsp::Wavetable> table,
                                     juce::String path, juce::String error)
{
    auto& st = slotTables[(size_t) slot];
    st.error = std::move (error);

    if (table != nullptr)
    {
        retiredTables.push_back (std::move (st.current));
        st.current = std::move (table);
        st.live.store (st.current.get());
        st.path = std::move (path);
    }

    sendChangeMessage();
}

void ArsenalProcessor::loadWavetableFromFile (int slot, const juce::File& file)
{
    juce::Thread::launch ([this, slot, file]
    {
        auto result = dsp::loadWavetableFromFile (file);

        juce::MessageManager::callAsync ([this, slot, loaded = std::move (result),
                                          path = file.getFullPathName()]() mutable
        {
            installTable (slot, std::move (loaded.table), path, loaded.error);
        });
    });
}

void ArsenalProcessor::setFactoryWavetable (int slot)
{
    installTable (slot, factoryTable, {}, {});
}

juce::String ArsenalProcessor::getWavetableName (int slot) const
{
    const auto& current = slotTables[(size_t) slot].current;
    return current != nullptr ? current->getName() : juce::String();
}

juce::String ArsenalProcessor::getWavetableError (int slot) const
{
    return slotTables[(size_t) slot].error;
}

void ArsenalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    fxChain.prepare (sampleRate, samplesPerBlock);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
}

void ArsenalProcessor::updateFXParams()
{
    const auto& rf = raw.fx;
    auto& p = fxParams;

    p.distEnable     = rf.distEnable->load() >= 0.5f;
    p.distType       = (int) rf.distType->load();
    p.distDrive      = rf.distDrive->load();
    p.distToneHz     = rf.distTone->load();
    p.distMix        = rf.distMix->load();
    p.chorusEnable   = rf.chorusEnable->load() >= 0.5f;
    p.chorusRate     = rf.chorusRate->load();
    p.chorusDepth    = rf.chorusDepth->load();
    p.chorusFeedback = rf.chorusFeedback->load();
    p.chorusMix      = rf.chorusMix->load();
    p.delayEnable    = rf.delayEnable->load() >= 0.5f;
    p.delaySync      = rf.delaySync->load() >= 0.5f;
    p.delayTimeMs    = rf.delayTime->load();
    p.delayDivision  = (int) rf.delayDivision->load();
    p.delayFeedback  = rf.delayFeedback->load();
    p.delayPingPong  = rf.delayPingPong->load() >= 0.5f;
    p.delayMix       = rf.delayMix->load();
    p.reverbEnable   = rf.reverbEnable->load() >= 0.5f;
    p.reverbSize     = rf.reverbSize->load();
    p.reverbDamping  = rf.reverbDamping->load();
    p.reverbWidth    = rf.reverbWidth->load();
    p.reverbMix      = rf.reverbMix->load();
    p.eqEnable       = rf.eqEnable->load() >= 0.5f;
    p.eqLowGainDb    = rf.eqLowGain->load();
    p.eqMidFreq      = rf.eqMidFreq->load();
    p.eqMidGainDb    = rf.eqMidGain->load();
    p.eqHighGainDb   = rf.eqHighGain->load();
    p.bpm            = shared.bpm;
}

bool ArsenalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void ArsenalProcessor::scanMidiControllers (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isController() && m.getControllerNumber() == 1)
            lastModWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure())
            lastAftertouch = (float) m.getChannelPressureValue() / 127.0f;
    }

    shared.modWheel = lastModWheel;
    shared.aftertouch = lastAftertouch;
}

void ArsenalProcessor::updateSharedState (int blockLength)
{
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& rs = raw.slots[(size_t) s];
        auto& slot = shared.slots[(size_t) s];

        slot.enabled     = rs.enable->load() >= 0.5f;
        slot.mode        = (params::OscMode) (int) rs.mode->load();
        slot.table       = slotTables[(size_t) s].live.load();
        slot.sample      = slotSamples[(size_t) s].live.load();
        slot.phase       = rs.phase->load();
        slot.phaseMode   = (params::PhaseMode) (int) rs.phaseMode->load();
        slot.unisonCount = (int) rs.unisonCount->load();
        slot.sampleStart = rs.sampleStart->load();
        slot.loop        = rs.loop->load() >= 0.5f;
        slot.loopStart   = rs.loopStart->load();
        slot.loopEnd     = rs.loopEnd->load();
        slot.keytrack    = rs.keytrack->load() >= 0.5f;
        slot.rootNote    = (int) rs.rootNote->load();
        slot.grainPitch  = rs.grainPitch->load();
    }

    shared.filterType = (params::FilterType) (int) raw.filterType->load();

    // Normalized base values for every mod destination.
    const auto& dests = params::modDestinations();
    for (size_t d = 0; d < dests.size(); ++d)
        shared.baseNorm[d] = dests[d].def->range.convertTo0to1 (raw.dests[d]->load());

    // Compact list of routes that actually do something.
    shared.numActiveRoutes = 0;
    for (int r = 0; r < params::numModRoutes; ++r)
    {
        const auto& rr = raw.routes[(size_t) r];
        const auto source = (int) rr.source->load();
        const auto destChoice = (int) rr.dest->load();
        const auto depth = rr.depth->load();

        if (source == 0 || destChoice == 0 || depth == 0.0f)
            continue;

        auto& route = shared.routes[(size_t) shared.numActiveRoutes++];
        route.source = source;
        route.destIndex = destChoice - 1;  // choice 0 is "None"
        route.depth = depth;
    }

    // Transport.
    shared.bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm())
                shared.bpm = *bpm;

    // LFO params + global free-running phases (value at block start; advanced
    // past the block for next time).
    for (int i = 0; i < params::numLFOs; ++i)
    {
        const auto& rl = raw.lfos[(size_t) i];
        auto& lp = shared.lfo[(size_t) i];
        lp.shape       = (params::LFOShape) (int) rl.shape->load();
        lp.rateHz      = rl.rate->load();
        lp.sync        = rl.sync->load() >= 0.5f;
        lp.division    = (int) rl.division->load();
        lp.phaseOffset = rl.phase->load();
        lp.retrig      = rl.retrig->load() >= 0.5f;
        lp.unipolar    = rl.unipolar->load() >= 0.5f;

        const auto inc = (double) dsp::LFO::effectiveRateHz (lp, shared.bpm) / currentSampleRate;
        lfoPhaseAccum[(size_t) i] = std::fmod (lfoPhaseAccum[(size_t) i], 1.0);
        shared.lfoGlobalPhase[(size_t) i] = lfoPhaseAccum[(size_t) i];
        lfoPhaseAccum[(size_t) i] += inc * blockLength;
    }

    for (int m = 0; m < params::numMacros; ++m)
        shared.macros[(size_t) m] = raw.macros[(size_t) m]->load();

    auto& ch = shared.chaos;
    ch.enabled          = raw.chaos.enable->load() >= 0.5f;
    ch.pitchOn          = raw.chaos.pitchOn->load() >= 0.5f;
    ch.pitchAmountCents = raw.chaos.pitchAmount->load();
    ch.phaseOn          = raw.chaos.phaseOn->load() >= 0.5f;
    ch.phaseAmount      = raw.chaos.phaseAmount->load();
    ch.positionOn       = raw.chaos.positionOn->load() >= 0.5f;
    ch.positionAmount   = raw.chaos.positionAmount->load();
    ch.ampOn            = raw.chaos.ampOn->load() >= 0.5f;
    ch.ampAmount        = raw.chaos.ampAmount->load();
    ch.satOn            = raw.chaos.satOn->load() >= 0.5f;
    ch.saturation       = raw.chaos.saturation->load();
    ch.distOn           = raw.chaos.distOn->load() >= 0.5f;
    ch.distortion       = raw.chaos.distortion->load();
}

void ArsenalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    scanMidiControllers (midi);
    midiLearn->processMidi (midi);
    updateSharedState (buffer.getNumSamples());
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    updateFXParams();
    fxChain.process (buffer, fxParams);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
    masterGain.applyGain (buffer, buffer.getNumSamples());

    // Block-level telemetry.
    int active = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++active;
    telemetry.activeVoices.store (active, std::memory_order_relaxed);
    telemetry.peakL.store (buffer.getMagnitude (0, 0, buffer.getNumSamples()),
                           std::memory_order_relaxed);
    telemetry.peakR.store (buffer.getNumChannels() > 1
                               ? buffer.getMagnitude (1, 0, buffer.getNumSamples())
                               : telemetry.peakL.load (std::memory_order_relaxed),
                           std::memory_order_relaxed);
}

juce::ValueTree ArsenalProcessor::buildStateTree (bool includeMidiMap)
{
    const auto libraryRoot = library::findLibraryRoot();
    auto state = apvts.copyState();

    if (includeMidiMap)
        state.appendChild (midiLearn->toValueTree(), nullptr);

    auto wavetables = state.getOrCreateChildWithName (wavetableStateType, nullptr);
    auto samples = state.getOrCreateChildWithName (sampleStateType, nullptr);
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& wtPath = slotTables[(size_t) s].path;
        const auto& smpPath = slotSamples[(size_t) s].path;
        wavetables.setProperty (slotPathProperty (s),
                                wtPath.isEmpty() ? juce::String()
                                    : library::toPortable (juce::File (wtPath), libraryRoot),
                                nullptr);
        samples.setProperty (slotPathProperty (s),
                             smpPath.isEmpty() ? juce::String()
                                 : library::toPortable (juce::File (smpPath), libraryRoot),
                             nullptr);
    }

    return state;
}

void ArsenalProcessor::restoreStateTree (const juce::ValueTree& incoming)
{
    if (! incoming.hasType (apvts.state.getType()))
        return;

    auto state = incoming.createCopy();
    const auto libraryRoot = library::findLibraryRoot();

    // Wavetable/sample paths ride along in the state tree but are not
    // parameters.
    auto wavetables = state.getChildWithName (wavetableStateType);
    if (wavetables.isValid())
        state.removeChild (wavetables, nullptr);
    auto samples = state.getChildWithName (sampleStateType);
    if (samples.isValid())
        state.removeChild (samples, nullptr);

    // MIDI map: restore when present (host sessions); presets omit it and
    // leave the current hardware mapping untouched.
    auto midiMap = state.getChildWithName (MidiLearnManager::mapTreeType);
    if (midiMap.isValid())
    {
        state.removeChild (midiMap, nullptr);
        midiLearn->restoreFromValueTree (midiMap);
    }

    apvts.replaceState (state);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto wtPath = wavetables.isValid()
                          ? wavetables.getProperty (slotPathProperty (s)).toString()
                          : juce::String();
        const auto smpPath = samples.isValid()
                           ? samples.getProperty (slotPathProperty (s)).toString()
                           : juce::String();

        juce::MessageManager::callAsync ([this, s, wtPath, smpPath, libraryRoot]
        {
            if (wtPath.isEmpty())
                setFactoryWavetable (s);
            else
                loadWavetableFromFile (s, library::fromPortable (wtPath, libraryRoot));

            if (smpPath.isNotEmpty())
                loadSampleFromFile (s, library::fromPortable (smpPath, libraryRoot));
        });
    }
}

void ArsenalProcessor::refreshLibrary()
{
    const auto root = library::findLibraryRoot();
    if (! root.isDirectory())
        return;

    const auto packs = library::scanLibrary (root);
    presetManager->generateFactoryPresets (packs, root);
    presetManager->rescan();
}

void ArsenalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = buildStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void ArsenalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ArsenalProcessor::createEditor()
{
    return new ArsenalEditor (*this);
}

} // namespace arsenal

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new arsenal::ArsenalProcessor();
}
