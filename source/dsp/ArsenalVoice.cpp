#include "ArsenalVoice.h"

namespace arsenal::dsp
{

ArsenalVoice::ArsenalVoice (const Wavetable& table, const VoiceParams& sharedParams)
    : params (sharedParams)
{
    osc.setTable (&table);
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
    pitchWheelMoved (currentPitchWheelPosition);

    osc.resetPhase();
    filter.reset();
    updateFrequency();

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
    updateFrequency();
}

void ArsenalVoice::updateFrequency()
{
    if (currentNote < 0)
        return;

    const auto note = (float) currentNote
                    + params.oscCoarse
                    + params.oscFine * 0.01f
                    + pitchBendSemitones;

    osc.setFrequency ((float) juce::MidiMessage::getMidiNoteInHertz (69)
                      * std::pow (2.0f, (note - 69.0f) / 12.0f));
}

void ArsenalVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                    int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    updateFrequency();
    ampEnv.setParameters (params.ampEnv);
    filter.setParams (params.filterType, params.filterCutoff,
                      params.filterResonance, params.filterDrive);

    // Constant-power pan.
    const auto panAngle = (params.oscPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const auto gainL = std::cos (panAngle) * params.oscGain * velocityGain;
    const auto gainR = std::sin (panAngle) * params.oscGain * velocityGain;

    auto* left = outputBuffer.getWritePointer (0, startSample);
    auto* right = outputBuffer.getNumChannels() > 1
                ? outputBuffer.getWritePointer (1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envValue = ampEnv.getNextSample();
        const auto raw = osc.getNextSample (params.oscPosition) * envValue;

        left[i] += filter.processSample (0, raw * gainL);
        if (right != nullptr)
            right[i] += filter.processSample (1, raw * gainR);

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

} // namespace arsenal::dsp
