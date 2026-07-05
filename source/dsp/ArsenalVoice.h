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
struct SlotParams
{
    bool enabled = false;
    const Wavetable* table = nullptr;   // owned by the processor's table store
    float position = 0.0f;
    float coarse = 0.0f;
    float fine = 0.0f;
    float gain = 0.5f;                  // linear
    float pan = 0.0f;
    float phase = 0.0f;
    params::PhaseMode phaseMode = params::PhaseMode::reset;
    int unisonCount = 1;
    float unisonDetune = 0.0f;
    float unisonBlend = 0.7f;
    float unisonWidth = 0.8f;
};

struct VoiceParams
{
    std::array<SlotParams, params::maxOscSlots> slots {};
    params::FilterType filterType = params::FilterType::lp12;
    float filterCutoff = 20000.0f;
    float filterResonance = 0.0f;
    float filterDrive = 0.0f;
    juce::ADSR::Parameters ampEnv { 0.005f, 0.2f, 0.8f, 0.15f };
};

class ArsenalVoice : public juce::SynthesiserVoice
{
public:
    explicit ArsenalVoice (const VoiceParams& sharedParams);

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
    void updateSlotBlocks();

    const VoiceParams& params;
    std::array<UnisonOscillator, params::maxOscSlots> oscs;
    MultiModeFilter filter;
    juce::ADSR ampEnv;
    juce::Random random;

    int currentNote = -1;
    float velocityGain = 1.0f;
    float pitchBendSemitones = 0.0f;
};

} // namespace arsenal::dsp
