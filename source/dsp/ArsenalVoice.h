#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Wavetable.h"
#include "WavetableOscillator.h"
#include "MultiModeFilter.h"

namespace arsenal::dsp
{

struct ArsenalSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Snapshot of the per-voice parameter values, refreshed once per block by the
// processor from the APVTS. Voices never touch parameter objects directly.
struct VoiceParams
{
    float oscPosition = 0.0f;
    float oscCoarse = 0.0f;
    float oscFine = 0.0f;
    float oscGain = 0.5f;        // linear
    float oscPan = 0.0f;
    params::FilterType filterType = params::FilterType::lp12;
    float filterCutoff = 20000.0f;
    float filterResonance = 0.0f;
    float filterDrive = 0.0f;
    juce::ADSR::Parameters ampEnv { 0.005f, 0.2f, 0.8f, 0.15f };
};

class ArsenalVoice : public juce::SynthesiserVoice
{
public:
    ArsenalVoice (const Wavetable& table, const VoiceParams& sharedParams);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

private:
    void updateFrequency();

    const VoiceParams& params;
    WavetableOscillator osc;
    MultiModeFilter filter;
    juce::ADSR ampEnv;

    int currentNote = -1;
    float velocityGain = 1.0f;
    float pitchBendSemitones = 0.0f;
};

} // namespace arsenal::dsp
