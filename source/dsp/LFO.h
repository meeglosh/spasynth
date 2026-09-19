#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/ParameterRegistry.h"

#include <cmath>

namespace spa::dsp
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
        float smooth = 0.0f;   // 0..1, chunk-rate one-pole slew amount
        float jitter = 0.0f;   // 0..1, random blend renewed once per cycle
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
        smoothed = 0.0f;
        smoothPrimed = false;
        jitterValue = 0.0f;
        jitterCycle = 0;
        jitterPrimed = false;
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
        lastBasePhase = (float) std::fmod (basePhase, 1.0);

        if (p.retrig)
        {
            phase += inc * numSamples;
            if (phase >= 1.0e9)  // keep the accumulator bounded on long notes
                phase = std::fmod (phase, 1.0);
        }

        // Cycle index (unwrapped, so it advances every time the LFO wraps
        // once around), shared by sample & hold and jitter so both renew on
        // the same clock.
        const auto cycle = (juce::int64) (basePhase + (double) p.phaseOffset);

        // Sample & hold: new random value each time the cycle wraps.
        if (p.shape == params::LFOShape::sampleHold)
        {
            if (! shPrimed || cycle != shCycle)
            {
                shValue = random.nextFloat() * 2.0f - 1.0f;
                shCycle = cycle;
                shPrimed = true;
            }
        }

        // 1. shape
        auto value = shape ((float) ph, p.shape);

        // 2. jitter -- blends a random value into the shape, renewed once
        // per LFO cycle (own state, so it never disturbs the S&H state
        // machine above). j=0 leaves value untouched (bit-identical); j=1
        // is fully random. Stays bounded since it's a linear blend of two
        // values already in [-1, 1].
        if (p.jitter > 0.0f)
        {
            if (! jitterPrimed || cycle != jitterCycle)
            {
                jitterValue = random.nextFloat() * 2.0f - 1.0f;
                jitterCycle = cycle;
                jitterPrimed = true;
            }
            value = (1.0f - p.jitter) * value + p.jitter * jitterValue;
        }

        // 3. smooth -- chunk-rate one-pole slew on the bipolar value, BEFORE
        // unipolar folding, so stepped shapes (Square, S&H) stop popping at
        // the 64-sample modulation-chunk boundary. 0 = no smoothing,
        // special-cased so output stays bit-identical to pre-smoothing LFO
        // (never relies on a time constant merely approaching zero).
        if (p.smooth > 0.0f)
        {
            const auto tau = 0.001f * std::pow (300.0f, p.smooth);   // 1ms..300ms
            const auto dt = (float) numSamples / (float) sampleRate;
            const auto alpha = 1.0f - std::exp (-dt / tau);
            if (! smoothPrimed)
            {
                // Seed directly from the current value on the first smoothed
                // chunk (e.g. right after noteOn) -- otherwise every note
                // would start with an audible ramp up from zero.
                smoothed = value;
                smoothPrimed = true;
            }
            else
            {
                smoothed += alpha * (value - smoothed);
            }
            value = smoothed;
        }
        else
        {
            // Stay unseeded while smoothing is off, so turning it back on
            // mid-note reseeds from the then-current value instead of
            // ramping from a stale one.
            smoothPrimed = false;
        }

        // 4. unipolar conversion, last.
        if (p.unipolar)
            value = value * 0.5f + 0.5f;
        return value;
    }

    // Where in the cycle the last chunk started (for UI playheads).
    float getLastBasePhase() const noexcept { return lastBasePhase; }

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
    float lastBasePhase = 0.0f;
    float shValue = 0.0f;
    juce::int64 shCycle = 0;
    bool shPrimed = false;

    float smoothed = 0.0f;
    bool smoothPrimed = false;

    float jitterValue = 0.0f;
    juce::int64 jitterCycle = 0;
    bool jitterPrimed = false;
};

} // namespace spa::dsp
