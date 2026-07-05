#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/ParameterRegistry.h"

#include <cmath>

namespace arsenal::dsp
{

// Per-voice LFO. Retriggered LFOs own their phase; free-running LFOs read a
// processor-global phase so every voice agrees. Tempo-sync derives the rate
// from host BPM. Output is bipolar -1..1 (or 0..1 when unipolar).
class LFO
{
public:
    struct Params
    {
        params::LFOShape shape = params::LFOShape::sine;
        float rateHz = 1.0f;
        bool sync = false;
        int division = 6;        // choice index into lfoDivisionBeats()
        float phaseOffset = 0.0f;
        bool retrig = true;
        bool unipolar = false;
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        phase = 0.0;
    }

    void noteOn (const Params& p) noexcept
    {
        if (p.retrig)
            phase = 0.0;
        shValue = 0.0f;
        shPrimed = false;
    }

    // Effective cycles-per-second given sync state and host tempo.
    static float effectiveRateHz (const Params& p, double bpm) noexcept
    {
        if (! p.sync)
            return p.rateHz;

        const auto beatsPerCycle = params::lfoDivisionBeats (p.division);
        return (float) (bpm / 60.0) / beatsPerCycle;
    }

    // Advances by numSamples and returns the value at the *start* of the
    // chunk. globalPhase is used when the LFO is not retriggered.
    float processChunk (const Params& p, int numSamples, double bpm,
                        double globalPhase, juce::Random& random) noexcept
    {
        const auto rate = effectiveRateHz (p, bpm);
        const auto inc = (double) rate / sampleRate;

        const auto basePhase = p.retrig ? phase : globalPhase;
        const auto ph = std::fmod (basePhase + (double) p.phaseOffset, 1.0);

        if (p.retrig)
        {
            phase += inc * numSamples;
            if (phase >= 1.0e9)  // keep the accumulator bounded on long notes
                phase = std::fmod (phase, 1.0);
        }

        // Sample & hold: new random value each time the cycle wraps.
        if (p.shape == params::LFOShape::sampleHold)
        {
            const auto cycle = (juce::int64) (basePhase + (double) p.phaseOffset);
            if (! shPrimed || cycle != shCycle)
            {
                shValue = random.nextFloat() * 2.0f - 1.0f;
                shCycle = cycle;
                shPrimed = true;
            }
        }

        auto value = shape ((float) ph, p.shape);
        if (p.unipolar)
            value = value * 0.5f + 0.5f;
        return value;
    }

private:
    float shape (float ph, params::LFOShape s) const noexcept
    {
        switch (s)
        {
            case params::LFOShape::sine:
                return std::sin (juce::MathConstants<float>::twoPi * ph);
            case params::LFOShape::triangle:
                return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case params::LFOShape::sawUp:
                return 2.0f * ph - 1.0f;
            case params::LFOShape::sawDown:
                return 1.0f - 2.0f * ph;
            case params::LFOShape::square:
                return ph < 0.5f ? 1.0f : -1.0f;
            case params::LFOShape::sampleHold:
                return shValue;
        }
        return 0.0f;
    }

    double sampleRate = 44100.0;
    double phase = 0.0;          // retriggered phase (cycles, unwrapped)
    float shValue = 0.0f;
    juce::int64 shCycle = 0;
    bool shPrimed = false;
};

} // namespace arsenal::dsp
