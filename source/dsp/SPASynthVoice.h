#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Wavetable.h"
#include "WavetableOscillator.h"
#include "MultiModeFilter.h"
#include "LFO.h"
#include "ChaosGenerator.h"
#include "SamplePlayer.h"
#include "GranularPlayer.h"
#include "ExtraOscillators.h"
#include "Telemetry.h"

namespace spa::dsp
{

struct SPASynthSound : public juce::SynthesiserSound
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
        params::OscMode mode = params::OscMode::wavetable;
        const Wavetable* table = nullptr;
        const SampleData* sample = nullptr;
        params::PhaseMode phaseMode = params::PhaseMode::reset;
        float phase = 0.0f;
        int unisonCount = 1;

        float sampleStart = 0.0f;
        bool loop = true;
        float loopStart = 0.0f;
        float loopEnd = 1.0f;
        bool keytrack = true;
        int rootNote = 60;
        float grainPitch = 0.0f;

        int analogShape = 0;
        float fmRatio = 2.0f;
        int noiseColor = 0;
    };

    struct Route
    {
        int source = 0;      // ModSource index
        int destIndex = 0;   // dense mod-dest index
        float depth = 0.0f;
    };

    std::array<SlotStatic, params::maxOscSlots> slots {};
    params::FilterType filterType = params::FilterType::lp12;
    float filterKeytrack = 0.0f;
    bool filter2Enabled = false;
    params::FilterType filter2Type = params::FilterType::lp12;
    float filter2Keytrack = 0.0f;
    bool filterParallel = false;

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

    Telemetry* telemetry = nullptr;   // audio -> UI channel, set once at startup

    // Organic Chaos statics (depth/rate/mix are mod destinations and live in
    // baseNorm; these are the plain toggles/amounts).
    struct Chaos
    {
        bool enabled = true;
        bool pitchOn = true;
        float pitchAmountCents = 8.0f;
        bool phaseOn = true;
        float phaseAmount = 0.15f;
        bool positionOn = true;
        float positionAmount = 0.2f;
        bool ampOn = true;
        float ampAmount = 0.2f;
        bool satOn = false;
        float saturation = 0.3f;
        bool distOn = false;
        float distortion = 0.2f;
    } chaos;
};

class SPASynthVoice : public juce::SynthesiserVoice
{
public:
    // Modulation is evaluated at this granularity within a block.
    static constexpr int chunkSize = 64;

    explicit SPASynthVoice (const SharedState& shared);

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
    std::array<SamplePlayer, params::maxOscSlots> samplePlayers;
    std::array<GranularPlayer, params::maxOscSlots> granularPlayers;
    std::array<AnalogOscillator, params::maxOscSlots> analogOscs;
    std::array<FMOscillator, params::maxOscSlots> fmOscs;
    std::array<NoiseGenerator, params::maxOscSlots> noiseGens;
    std::array<PluckString, params::maxOscSlots> plucks;
    std::array<float, params::maxOscSlots> slotPulseWidth {}, slotFMIndex {}, slotPluckDamp {};
    std::array<SamplePlayer::Params, params::maxOscSlots> sampleParams {};
    std::array<GranularPlayer::Params, params::maxOscSlots> granularParams {};
    MultiModeFilter filter, filter2;
    juce::ADSR ampEnv, env2, env3;
    std::array<LFO, params::numLFOs> lfos;
    ChaosGenerator chaosGen;
    juce::Random random;

    int currentNote = -1;
    int noteSerial = -1;       // telemetry writer arbitration
    float velocity = 1.0f;
    float pitchBendSemitones = 0.0f;
    float ampEnvLast = 0.0f;   // amp env value fed back as mod source
    std::array<float, params::maxOscSlots> slotGains {};  // per-chunk linear slot gains

    // Chaos state carried between chunks: effective depth/rate/mix from the
    // previous chunk (chaos feeds the matrix that modulates them — one-chunk
    // latency breaks the cycle), plus the current shaper drives.
    float chaosDepth = 0.0f, chaosRate = 2.0f, chaosMix = 1.0f;
    float satDrive = 0.0f, distDrive = 0.0f, chaosAmpGain = 1.0f;
    float filterMixValue = 1.0f;    // per-chunk dry/wet, filter 1
    float filter2MixValue = 1.0f;   // per-chunk dry/wet, filter 2

    // Per-slot pan gains for sample/granular modes (wavetable pan is handled
    // inside the unison oscillator), and last chunk's effective grain position
    // (feeds the follower playhead, same one-chunk-latency trick as chaos).
    std::array<float, params::maxOscSlots> slotPanL {}, slotPanR {};
    std::array<float, params::maxOscSlots> lastGrainPos {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SPASynthVoice)
};

} // namespace spa::dsp
