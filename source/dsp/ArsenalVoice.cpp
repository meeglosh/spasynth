#include "ArsenalVoice.h"

namespace arsenal::dsp
{

ArsenalVoice::ArsenalVoice (const VoiceParams& sharedParams)
    : params (sharedParams)
{
}

bool ArsenalVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<ArsenalSound*> (sound) != nullptr;
}

void ArsenalVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate > 0.0)
    {
        for (auto& osc : oscs)
            osc.prepare (newRate);
        filter.prepare (newRate);
        ampEnv.setSampleRate (newRate);
    }
}

void ArsenalVoice::startNote (int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    currentNote = midiNoteNumber;
    velocityGain = 0.2f + 0.8f * velocity;
    pitchBendSemitones = 2.0f * ((float) currentPitchWheelPosition - 8192.0f) / 8192.0f;

    for (int s = 0; s < params::numOscSlots; ++s)
        oscs[(size_t) s].noteOn (params.slots[(size_t) s].phaseMode,
                                 params.slots[(size_t) s].phase, &random);

    filter.reset();

    ampEnv.setParameters (params.ampEnv);
    ampEnv.noteOn();
}

void ArsenalVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        clearCurrentNote();
    }
}

void ArsenalVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchBendSemitones = 2.0f * ((float) newPitchWheelValue - 8192.0f) / 8192.0f;
}

void ArsenalVoice::updateSlotBlocks()
{
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& slot = params.slots[(size_t) s];
        if (! slot.enabled)
            continue;

        const auto note = (float) currentNote + slot.coarse + slot.fine * 0.01f
                        + pitchBendSemitones;
        const auto freq = 440.0f * std::exp2 ((note - 69.0f) / 12.0f);

        oscs[(size_t) s].updateBlock ({ slot.table, freq, slot.position,
                                        slot.unisonCount, slot.unisonDetune,
                                        slot.unisonBlend, slot.unisonWidth, slot.pan });
    }
}

void ArsenalVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                    int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    updateSlotBlocks();
    ampEnv.setParameters (params.ampEnv);
    filter.setParams (params.filterType, params.filterCutoff,
                      params.filterResonance, params.filterDrive);

    auto* left = outputBuffer.getWritePointer (0, startSample);
    auto* right = outputBuffer.getNumChannels() > 1
                ? outputBuffer.getWritePointer (1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int s = 0; s < params::numOscSlots; ++s)
        {
            const auto& slot = params.slots[(size_t) s];
            if (! slot.enabled)
                continue;

            const auto smp = oscs[(size_t) s].getNextSample();
            sumL += smp.left * slot.gain;
            sumR += smp.right * slot.gain;
        }

        const auto envValue = ampEnv.getNextSample() * velocityGain;

        left[i] += filter.processSample (0, sumL) * envValue;
        if (right != nullptr)
            right[i] += filter.processSample (1, sumR) * envValue;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

} // namespace arsenal::dsp
