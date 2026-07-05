#pragma once

#include "Wavetable.h"

namespace arsenal::dsp
{

// Single wavetable playback voice: phase accumulator with linear
// interpolation within the frame and crossfade between adjacent frames for
// the morph position. Real-time safe; the table itself is owned elsewhere.
class WavetableOscillator
{
public:
    void setTable (const Wavetable* t) noexcept { table = t; }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        phase = 0.0;
    }

    void setFrequency (float frequencyHz) noexcept
    {
        phaseIncrement = (double) frequencyHz / sampleRate;

        if (table != nullptr)
            mipLevel = table->mipLevelForFrequency (frequencyHz, sampleRate);
    }

    void resetPhase (double newPhase = 0.0) noexcept { phase = newPhase; }

    // position: 0..1 morph across frames.
    float getNextSample (float position) noexcept
    {
        if (table == nullptr || table->getNumFrames() == 0)
            return 0.0f;

        const auto numFrames = table->getNumFrames();
        const auto framePos = juce::jlimit (0.0f, 1.0f, position) * (float) (numFrames - 1);
        const auto frameA = juce::jmin ((int) framePos, numFrames - 1);
        const auto frameB = juce::jmin (frameA + 1, numFrames - 1);
        const auto frameFrac = framePos - (float) frameA;

        const auto idx = phase * (double) Wavetable::tableSize;
        const auto i0 = (int) idx;
        const auto frac = (float) (idx - (double) i0);

        const auto* a = table->getFrame (mipLevel, frameA);
        const auto* b = table->getFrame (mipLevel, frameB);

        const auto sampleA = a[i0] + frac * (a[i0 + 1] - a[i0]);
        const auto sampleB = b[i0] + frac * (b[i0 + 1] - b[i0]);

        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= 1.0;

        return sampleA + frameFrac * (sampleB - sampleA);
    }

private:
    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    int mipLevel = 0;
};

} // namespace arsenal::dsp
