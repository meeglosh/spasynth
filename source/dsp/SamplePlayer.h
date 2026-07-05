#pragma once

#include "SampleData.h"

namespace arsenal::dsp
{

// Classic sample playback for one slot within a voice: start offset, loop
// points, keytrack handled by the caller via rateRatio. Linear interpolation.
class SamplePlayer
{
public:
    struct Params
    {
        const SampleData* sample = nullptr;
        double rateRatio = 1.0;    // source-rate/engine-rate * pitch ratio
        bool loop = true;
        double loopStartNorm = 0.0;
        double loopEndNorm = 1.0;
    };

    void noteOn (const SampleData* sample, double startNorm) noexcept
    {
        position = sample != nullptr ? startNorm * sample->lengthSamples() : 0.0;
        done = sample == nullptr;
    }

    bool isDone() const noexcept { return done; }

    // Playback position in source-file seconds (drives the SFX followers).
    double positionSeconds (const SampleData* sample) const noexcept
    {
        return sample != nullptr ? position / sample->sourceSampleRate : 0.0;
    }

    struct StereoSample { float left = 0.0f, right = 0.0f; };

    StereoSample getNextSample (const Params& p) noexcept
    {
        if (done || p.sample == nullptr || p.sample->lengthSamples() < 2)
            return {};

        const auto len = (double) p.sample->lengthSamples();
        auto loopStart = p.loopStartNorm * len;
        auto loopEnd = p.loopEndNorm * len;
        if (loopEnd < loopStart + 64.0)  // degenerate loop -> ignore
            loopEnd = juce::jmin (loopStart + 64.0, len);

        if (p.loop && position >= loopEnd)
            position = loopStart + std::fmod (position - loopEnd, juce::jmax (1.0, loopEnd - loopStart));

        if (position >= len - 1.0)
        {
            done = true;
            return {};
        }

        const auto i0 = (int) position;
        const auto frac = (float) (position - (double) i0);
        const auto& audio = p.sample->audio;

        const auto* ch0 = audio.getReadPointer (0);
        const auto left = ch0[i0] + frac * (ch0[i0 + 1] - ch0[i0]);

        auto right = left;
        if (audio.getNumChannels() > 1)
        {
            const auto* ch1 = audio.getReadPointer (1);
            right = ch1[i0] + frac * (ch1[i0 + 1] - ch1[i0]);
        }

        position += p.rateRatio;
        return { left, right };
    }

private:
    double position = 0.0;   // in source samples
    bool done = true;
};

} // namespace arsenal::dsp
