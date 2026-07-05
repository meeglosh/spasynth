#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Wavetable.h"
#include "WavetableOscillator.h"
#include "MultiModeFilter.h"
#include "LFO.h"

namespace arsenal::dsp
{

struct ArsenalSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Everything voices need for a block, written once per block by the processor
// (audio thread) before Synthesiser::renderNextBlock. Voices only read.
struct SharedState
{
    struct SlotStatic
    {
        bool enabled = false;
        const Wavetable* table = nullptr;
        params::PhaseMode phaseMode = params::PhaseMode::reset;
        float phase = 0.0f;
        int unisonCount = 1;
    };

    struct Route
    {
        int source = 0;      // ModSource index
        int destIndex = 0;   // dense mod-dest index
        float depth = 0.0f;
    };

    std::array<SlotStatic, params::maxOscSlots> slots {};
    params::FilterType filterType = params::FilterType::lp12;

    // Normalized (0..1) base values for every mod destination, indexed by the
    // registry's dense mod-dest index.
    std::array<float, params::maxModDests> baseNorm {};

    int numActiveRoutes = 0;
    std::array<Route, params::numModRoutes> routes {};

    std::array<LFO::Params, params::numLFOs> lfo {};
    std::array<double, params::numLFOs> lfoGlobalPhase {};  // at block start

    std::array<float, params::numMacros> macros {};
    float modWheel = 0.0f;
    float aftertouch = 0.0f;
    double bpm = 120.0;
};

class ArsenalVoice : public juce::SynthesiserVoice
{
public:
    // Modulation is evaluated at this granularity within a block.
    static constexpr int chunkSize = 64;

    explicit ArsenalVoice (const SharedState& shared);

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
    // Computes modulation sources and effective parameter values for one
    // chunk, then configures oscillators/filter/envelopes from them.
    // blockOffset = samples since block start (for global LFO phase).
    void computeChunk (int blockOffset, int chunkLen);

    float baseValue (int destIndex) const;  // denormalized, no modulation

    const SharedState& shared;
    std::array<UnisonOscillator, params::maxOscSlots> oscs;
    MultiModeFilter filter;
    juce::ADSR ampEnv, env2, env3;
    std::array<LFO, params::numLFOs> lfos;
    juce::Random random;

    int currentNote = -1;
    float velocity = 1.0f;
    float pitchBendSemitones = 0.0f;
    float ampEnvLast = 0.0f;   // amp env value fed back as mod source
    std::array<float, params::maxOscSlots> slotGains {};  // per-chunk linear slot gains

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalVoice)
};

} // namespace arsenal::dsp
