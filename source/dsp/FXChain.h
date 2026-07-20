#pragma once

#include <juce_dsp/juce_dsp.h>
#include "ModEffect.h"
#include "../params/ParameterRegistry.h"

namespace spa::dsp
{

// Global stereo FX chain, processed after the synth mix and before master
// gain. Modules run in the order listed in `processOrder` — fixed for v1 but
// architected as an ordered list so reordering is a data change, not a
// rewrite.
class FXChain
{
public:
    FXChain() = default;

    // Append-only: module ids are serialized in the per-preset chain order.
    enum class Module { distortion, chorus, delay, reverb, eq, mod };
    static constexpr int numModules = 6;

    // Pack/unpack the chain order into a uint64 (4 bits/module): a single atomic
    // for the lock-free UI->audio hand-off and compact preset storage. Unpack
    // validates the value is a permutation and falls back to the natural order.
    static juce::uint64 packOrder (const Module* order)
    {
        juce::uint64 v = 0;
        for (int i = 0; i < numModules; ++i)
            v |= (juce::uint64) ((int) order[i] & 0xF) << (i * 4);
        return v;
    }
    static juce::uint64 defaultOrderPacked()
    {
        Module def[numModules] { Module::distortion, Module::chorus, Module::mod,
                                 Module::delay, Module::reverb, Module::eq };
        return packOrder (def);
    }
    static void unpackOrder (juce::uint64 packed, Module* order)
    {
        bool seen[16] = {}; bool ok = true; int tmp[numModules];
        for (int i = 0; i < numModules; ++i)
        {
            const int id = (int) ((packed >> (i * 4)) & 0xF);
            tmp[i] = id;
            if (id < 0 || id >= numModules || seen[id]) { ok = false; break; }
            seen[id] = true;
        }
        for (int i = 0; i < numModules; ++i)
            order[i] = ok ? (Module) tmp[i] : (Module) i;
    }

    struct Params
    {
        bool distEnable = false;
        int distType = 0;          // Soft/Hard/Fold
        float distDrive = 0.3f;
        float distToneHz = 8000.0f;
        float distMix = 1.0f;

        bool chorusEnable = false;
        float chorusRate = 0.8f;
        float chorusDepth = 0.3f;
        float chorusFeedback = 0.0f;
        float chorusMix = 0.5f;

        bool delayEnable = false;
        bool delaySync = true;
        float delayTimeMs = 350.0f;
        int delayDivision = 6;
        float delayFeedback = 0.35f;
        bool delayPingPong = false;
        float delayMix = 0.35f;

        bool reverbEnable = false;
        float reverbSize = 0.5f;
        float reverbDamping = 0.5f;
        float reverbWidth = 1.0f;
        float reverbMix = 0.3f;

        bool eqEnable = false;
        float eqLowGainDb = 0.0f;
        float eqMidFreq = 1000.0f;
        float eqMidGainDb = 0.0f;
        float eqHighGainDb = 0.0f;

        double bpm = 120.0;

        bool modEnable = false;
        int modType = 0;           // 0 = Phaser, 1 = Flanger
        float modRate = 0.5f;
        bool modSync = false;
        int modDivision = 6;
        float modDepth = 0.5f;
        float modFeedback = 0.3f;
        int modStages = 6;
        float modCentreHz = 800.0f;
        float modManualMs = 3.0f;
        float modWidth = 0.5f;
        float modMix = 0.5f;

        // Runtime FX processing order (drag-reorderable, saved per preset).
        Module order[numModules] {
            Module::distortion, Module::chorus, Module::mod,
            Module::delay, Module::reverb, Module::eq
        };
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void process (juce::AudioBuffer<float>& buffer, const Params& params);

    // Worst-case ring-out for AudioProcessor::getTailLengthSeconds().
    double tailSeconds (const Params& params) const;

private:
    void processDistortion (juce::AudioBuffer<float>&, const Params&);
    void processChorus (juce::AudioBuffer<float>&, const Params&);
    void processDelay (juce::AudioBuffer<float>&, const Params&);
    void processReverb (juce::AudioBuffer<float>&, const Params&);
    void processEQ (juce::AudioBuffer<float>&, const Params&);
    void processMod (juce::AudioBuffer<float>&, const Params&);

    double sampleRate = 48000.0;
    ModEffect modEffect;

    // Distortion tone filter (post-shaper lowpass), one per channel.
    std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> toneFilters;

    juce::dsp::Chorus<float> chorus;

    // Delay: fixed max 4 s ring buffer per channel.
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    juce::SmoothedValue<float> delaySamplesSmoothed;

    juce::Reverb reverb;

    // EQ: low shelf / mid peak / high shelf per channel.
    using IIR = juce::dsp::IIR::Filter<float>;
    std::array<IIR, 2> eqLow, eqMid, eqHigh;
    float lastLowGain = 1.0e9f, lastMidFreq = 0.0f, lastMidGain = 1.0e9f,
          lastHighGain = 1.0e9f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXChain)
};

} // namespace spa::dsp
