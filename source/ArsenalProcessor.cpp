#include "ArsenalProcessor.h"
#include "ui/ArsenalEditor.h"

namespace arsenal
{

ArsenalProcessor::ArsenalProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout()),
      wavetable (dsp::Wavetable::createBasicShapes())
{
    raw.masterGain      = apvts.getRawParameterValue (params::id::masterGain);
    raw.oscPosition     = apvts.getRawParameterValue (params::id::oscAPosition);
    raw.oscCoarse       = apvts.getRawParameterValue (params::id::oscACoarse);
    raw.oscFine         = apvts.getRawParameterValue (params::id::oscAFine);
    raw.oscLevel        = apvts.getRawParameterValue (params::id::oscALevel);
    raw.oscPan          = apvts.getRawParameterValue (params::id::oscAPan);
    raw.filterType      = apvts.getRawParameterValue (params::id::filter1Type);
    raw.filterCutoff    = apvts.getRawParameterValue (params::id::filter1Cutoff);
    raw.filterResonance = apvts.getRawParameterValue (params::id::filter1Resonance);
    raw.filterDrive     = apvts.getRawParameterValue (params::id::filter1Drive);
    raw.ampAttack       = apvts.getRawParameterValue (params::id::ampAttack);
    raw.ampDecay        = apvts.getRawParameterValue (params::id::ampDecay);
    raw.ampSustain      = apvts.getRawParameterValue (params::id::ampSustain);
    raw.ampRelease      = apvts.getRawParameterValue (params::id::ampRelease);

    synth.addSound (new dsp::ArsenalSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new dsp::ArsenalVoice (wavetable, voiceParams));
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
    voiceParams.oscPosition     = raw.oscPosition->load();
    voiceParams.oscCoarse       = raw.oscCoarse->load();
    voiceParams.oscFine         = raw.oscFine->load();
    voiceParams.oscGain         = juce::Decibels::decibelsToGain (raw.oscLevel->load(), -60.0f);
    voiceParams.oscPan          = raw.oscPan->load();
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
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ArsenalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
