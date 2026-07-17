#pragma once

#include "SampleData.h"

#include <array>
#include <cmath>

namespace spa::dsp
{

// Granular playback for one slot within a voice. A fixed pool of grains reads
// from the shared sample buffer through Hann windows; the scheduler spawns
// grains at the density rate around the (modulatable) position, with spray
// randomizing each grain's start. No allocation at play time.
class GranularPlayer
{
public:
    static constexpr int maxGrains = 32;

    struct Params
    {
        const SampleData* sample = nullptr;
        float grainSizeSeconds = 0.08f;
        float density = 20.0f;         // grains per second
        float position = 0.0f;         // 0..1 into the sample
        float spray = 0.1f;            // 0..1 of sample length
        double pitchRatio = 1.0;       // includes keytrack + grain pitch
        double engineSampleRate = 48000.0;
    };

    void noteOn() noexcept
    {
        for (auto& g : grains)
            g.active = false;
        samplesToNextGrain = 0.0;
    }

    // Follower playhead: the scheduler's centre position, in source seconds.
    double positionSeconds (const SampleData* sample, float position) const noexcept
    {
        return sample != nullptr ? (double) position * sample->lengthSeconds() : 0.0;
    }

    // Snapshots the live grains for the UI's animated cloud: up to maxViz
    // active grains' normalized read positions (0..1) and window amplitudes
    // (0..1). Returns the count written. Read-only over the grain pool.
    int snapshotGrains (const SampleData* sample, float* posOut, float* ampOut,
                        int maxViz) const noexcept
    {
        if (sample == nullptr || sample->lengthSamples() < 2)
            return 0;

        const auto len = (double) sample->lengthSamples();
        int n = 0;
        for (const auto& g : grains)
        {
            if (n >= maxViz)
                break;
            if (! g.active)
                continue;
            posOut[n] = (float) juce::jlimit (0.0, 1.0, g.pos / len);
            ampOut[n] = 0.5f - 0.5f * std::cos ((float) g.phase
                                                * juce::MathConstants<float>::twoPi);
            ++n;
        }
        return n;
    }

    struct StereoSample { float left = 0.0f, right = 0.0f; };

    StereoSample getNextSample (const Params& p, juce::Random& random) noexcept
    {
        if (p.sample == nullptr || p.sample->lengthSamples() < 64)
            return {};

        // --- Scheduler ------------------------------------------------------
        samplesToNextGrain -= 1.0;
        if (samplesToNextGrain <= 0.0)
        {
            spawnGrain (p, random);
            samplesToNextGrain += p.engineSampleRate / juce::jmax (0.1f, p.density);
        }

        // --- Sum active grains ----------------------------------------------
        const auto len = p.sample->lengthSamples();
        const auto& audio = p.sample->audio;
        const auto* ch0 = audio.getReadPointer (0);
        const auto* ch1 = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : ch0;

        StereoSample out;

        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            if (g.phase >= 1.0 || g.pos >= (double) len - 1.0)
            {
                g.active = false;
                continue;
            }

            // Hann window from phase.
            const auto w = 0.5f - 0.5f * std::cos ((float) g.phase
                                                   * juce::MathConstants<float>::twoPi);

            const auto i0 = (int) g.pos;
            const auto frac = (float) (g.pos - (double) i0);
            const auto sL = ch0[i0] + frac * (ch0[i0 + 1] - ch0[i0]);
            const auto sR = ch1[i0] + frac * (ch1[i0 + 1] - ch1[i0]);

            out.left += sL * w * g.gainL;
            out.right += sR * w * g.gainR;

            g.pos += g.rate;
            g.phase += g.phaseInc;
        }

        return out;
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0;        // source samples
        double rate = 1.0;       // source samples per engine sample
        double phase = 0.0;      // 0..1 through the window
        double phaseInc = 0.0;
        float gainL = 1.0f, gainR = 1.0f;
    };

    void spawnGrain (const Params& p, juce::Random& random) noexcept
    {
        // Find a free grain; steal the most-finished one if the pool is full.
        Grain* slot = nullptr;
        double bestPhase = -1.0;
        for (auto& g : grains)
        {
            if (! g.active)
            {
                slot = &g;
                break;
            }
            if (g.phase > bestPhase)
            {
                bestPhase = g.phase;
                slot = &g;
            }
        }

        const auto len = (double) p.sample->lengthSamples();
        const auto sprayOffset = ((double) random.nextFloat() * 2.0 - 1.0)
                               * (double) p.spray * 0.25 * len;
        const auto start = juce::jlimit (0.0, len - 64.0,
                                         (double) p.position * len + sprayOffset);

        const auto durEngineSamples = juce::jmax (32.0, (double) p.grainSizeSeconds
                                                        * p.engineSampleRate);

        auto& g = *slot;
        g.active = true;
        g.pos = start;
        g.rate = p.pitchRatio * p.sample->sourceSampleRate / p.engineSampleRate;
        g.phase = 0.0;
        g.phaseInc = 1.0 / durEngineSamples;

        // Small random pan per grain widens the cloud slightly.
        const auto pan = 0.5f + 0.2f * (random.nextFloat() - 0.5f);
        g.gainL = std::cos (pan * juce::MathConstants<float>::halfPi);
        g.gainR = std::sin (pan * juce::MathConstants<float>::halfPi);
    }

    std::array<Grain, maxGrains> grains {};
    double samplesToNextGrain = 0.0;
};

} // namespace spa::dsp
