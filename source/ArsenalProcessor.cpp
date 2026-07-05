#include "ArsenalProcessor.h"
#include "dsp/WavetableLoader.h"
#include "ui/ArsenalEditor.h"

namespace arsenal
{

namespace
{
    constexpr const char* wavetableStateType = "WAVETABLES";

    juce::Identifier slotPathProperty (int slot)
    {
        return { "slot" + juce::String (slot) };
    }
}

ArsenalProcessor::ArsenalProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout())
{
    raw.masterGain      = apvts.getRawParameterValue (params::id::masterGain);
    raw.filterType      = apvts.getRawParameterValue (params::id::filter1Type);
    raw.filterCutoff    = apvts.getRawParameterValue (params::id::filter1Cutoff);
    raw.filterResonance = apvts.getRawParameterValue (params::id::filter1Resonance);
    raw.filterDrive     = apvts.getRawParameterValue (params::id::filter1Drive);
    raw.ampAttack       = apvts.getRawParameterValue (params::id::ampAttack);
    raw.ampDecay        = apvts.getRawParameterValue (params::id::ampDecay);
    raw.ampSustain      = apvts.getRawParameterValue (params::id::ampSustain);
    raw.ampRelease      = apvts.getRawParameterValue (params::id::ampRelease);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto& rs = raw.slots[(size_t) s];
        const auto pid = [s] (const char* key) { return params::id::oscSlot (s, key); };
        rs.enable       = apvts.getRawParameterValue (pid (params::id::osc::enable));
        rs.position     = apvts.getRawParameterValue (pid (params::id::osc::position));
        rs.coarse       = apvts.getRawParameterValue (pid (params::id::osc::coarse));
        rs.fine         = apvts.getRawParameterValue (pid (params::id::osc::fine));
        rs.level        = apvts.getRawParameterValue (pid (params::id::osc::level));
        rs.pan          = apvts.getRawParameterValue (pid (params::id::osc::pan));
        rs.phase        = apvts.getRawParameterValue (pid (params::id::osc::phase));
        rs.phaseMode    = apvts.getRawParameterValue (pid (params::id::osc::phaseMode));
        rs.unisonCount  = apvts.getRawParameterValue (pid (params::id::osc::unisonCount));
        rs.unisonDetune = apvts.getRawParameterValue (pid (params::id::osc::unisonDetune));
        rs.unisonBlend  = apvts.getRawParameterValue (pid (params::id::osc::unisonBlend));
        rs.unisonWidth  = apvts.getRawParameterValue (pid (params::id::osc::unisonWidth));
    }

    factoryTable = std::make_shared<const dsp::Wavetable> (dsp::Wavetable::createBasicShapes());
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        slotTables[(size_t) s].current = factoryTable;
        slotTables[(size_t) s].live.store (factoryTable.get());
    }

    synth.addSound (new dsp::ArsenalSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new dsp::ArsenalVoice (voiceParams));

    startTimer (1000);  // purges retired wavetables
}

ArsenalProcessor::~ArsenalProcessor()
{
    stopTimer();
}

void ArsenalProcessor::timerCallback()
{
    // Any table retired more than one timer period ago can no longer be in
    // use by the audio thread (it re-reads `live` every block).
    retiredTables.clear();
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

void ArsenalProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
}

bool ArsenalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void ArsenalProcessor::updateVoiceParams()
{
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& rs = raw.slots[(size_t) s];
        auto& slot = voiceParams.slots[(size_t) s];

        slot.enabled      = rs.enable->load() >= 0.5f;
        slot.table        = slotTables[(size_t) s].live.load();
        slot.position     = rs.position->load();
        slot.coarse       = rs.coarse->load();
        slot.fine         = rs.fine->load();
        slot.gain         = juce::Decibels::decibelsToGain (rs.level->load(), -60.0f);
        slot.pan          = rs.pan->load();
        slot.phase        = rs.phase->load();
        slot.phaseMode    = (params::PhaseMode) (int) rs.phaseMode->load();
        slot.unisonCount  = (int) rs.unisonCount->load();
        slot.unisonDetune = rs.unisonDetune->load();
        slot.unisonBlend  = rs.unisonBlend->load();
        slot.unisonWidth  = rs.unisonWidth->load();
    }

    voiceParams.filterType      = (params::FilterType) (int) raw.filterType->load();
    voiceParams.filterCutoff    = raw.filterCutoff->load();
    voiceParams.filterResonance = raw.filterResonance->load();
    voiceParams.filterDrive     = raw.filterDrive->load();
    voiceParams.ampEnv          = { raw.ampAttack->load(), raw.ampDecay->load(),
                                    raw.ampSustain->load(), raw.ampRelease->load() };
}

void ArsenalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateVoiceParams();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
    masterGain.applyGain (buffer, buffer.getNumSamples());
}

void ArsenalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    auto wavetables = state.getOrCreateChildWithName (wavetableStateType, nullptr);
    for (int s = 0; s < params::numOscSlots; ++s)
        wavetables.setProperty (slotPathProperty (s), slotTables[(size_t) s].path, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ArsenalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    // Wavetable paths ride along in the state tree but are not parameters.
    auto wavetables = state.getChildWithName (wavetableStateType);
    if (wavetables.isValid())
        state.removeChild (wavetables, nullptr);

    apvts.replaceState (state);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto path = wavetables.isValid()
                        ? wavetables.getProperty (slotPathProperty (s)).toString()
                        : juce::String();

        juce::MessageManager::callAsync ([this, s, path]
        {
            if (path.isEmpty())
                setFactoryWavetable (s);
            else
                loadWavetableFromFile (s, juce::File (path));
        });
    }
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
