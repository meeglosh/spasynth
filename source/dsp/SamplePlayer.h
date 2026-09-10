#pragma once

#include "SampleData.h"

#include <array>
#include <cmath>

namespace spa::dsp
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

        // SYNC (time-stretch to host BPM, pitch preserved). When syncOn,
        // getNextSample ignores rateRatio and instead advances TIME by
        // stretchRatio (source-samples-per-output-sample from host/native
        // BPM, independent of pitch) while grains are read internally at
        // pitchRatio (keytrack/coarse/fine, sourceRate/engineRate folded
        // in) -- so timing follows the host and pitch follows the key.
        bool syncOn = false;
        double stretchRatio = 1.0;    // time-scale factor, source-samples/output-sample
        double pitchRatio = 1.0;      // grain-internal read rate, source-samples/output-sample
        double engineSampleRate = 48000.0;

        bool loop = true;
        double loopStartNorm = 0.0;
        double loopEndNorm = 1.0;
    };

    void noteOn (const SampleData* sample, double startNorm) noexcept
    {
        position = sample != nullptr ? startNorm * sample->lengthSamples() : 0.0;
        done = sample == nullptr;
        stretch = StretchState {};
        stretch.playheadSource = position;
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

        if (p.syncOn)
            return getNextStretchedSample (p);

        const auto len = (double) p.sample->lengthSamples();
        // The interpolator reads [i0, i0+1], so the last sample we can ever
        // legally start an interpolation from is len - 1. A loopEndNorm of
        // 1.0 (the default -- "loop the whole file") maps to exactly `len`,
        // which is one sample past that limit: the old code let the
        // unconditional end-of-buffer cutoff below fire at len - 1 BEFORE
        // position ever reached loopEnd (== len), so the wrap-to-loopStart
        // branch never ran and whole-file loops played once and stopped.
        // Clamping loopEnd (and loopStart, symmetrically) to the last legal
        // sample makes the wrap always win the race, for loop and reverse
        // playback alike -- the boundary is direction-agnostic since it's
        // expressed purely in source-sample position.
        const auto lastSample = juce::jmax (0.0, len - 1.0);
        auto loopStart = juce::jmin (p.loopStartNorm * len, lastSample);
        auto loopEnd = juce::jmin (p.loopEndNorm * len, lastSample);
        if (loopEnd < loopStart + 64.0)  // degenerate/zero-length loop -> clamp to a safe minimum span
            loopEnd = juce::jmin (loopStart + 64.0, lastSample);
        if (loopEnd <= loopStart)        // still degenerate (loopStart itself at/near EOF) -> single-sample loop, never hangs
            loopStart = juce::jmax (0.0, loopEnd - 1.0);

        if (p.loop && position >= loopEnd)
            position = loopStart + std::fmod (position - loopEnd, juce::jmax (1.0, loopEnd - loopStart));

        if (position >= lastSample)
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
    // --- SYNC time-stretch: 2-stream overlap-add granular stretcher -------
    // Fixed-capacity, no allocation. ~40 ms Hann grains at 50% overlap;
    // spawn cadence (hop) runs on the OUTPUT timeline so overlap stays 50%
    // regardless of stretch ratio, while each grain reads the source at
    // pitchRatio (so pitch tracks the key) starting from a position that
    // advances only at stretchRatio (so timing tracks the host tempo).
    // Grains crossing a detected onset are halved in length -- keeps
    // transients tighter instead of smearing them across the overlap.
    struct StretchState
    {
        static constexpr int numStreams = 2;
        struct Grain
        {
            double sourceStart = 0.0;   // source-sample pos the grain reads from
            double outPhase = 0.0;      // output samples elapsed since spawn
            double lenOut = 0.0;        // window length, output samples
            bool active = false;
        };
        std::array<Grain, numStreams> grains {};
        int nextStream = 0;
        double outputCounter = 0.0;
        double playheadSource = 0.0;
    };

    static float hann (double phase01) noexcept
    {
        return 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi * phase01);
    }

    static float readClamped (const juce::AudioBuffer<float>& audio, int channel, double pos) noexcept
    {
        const auto n = audio.getNumSamples();
        if (n < 2) return 0.0f;
        const auto p = juce::jlimit (0.0, (double) (n - 2), pos);
        const auto i0 = (int) p;
        const auto frac = (float) (p - (double) i0);
        const auto* ch = audio.getReadPointer (juce::jmin (channel, audio.getNumChannels() - 1));
        return ch[i0] + frac * (ch[i0 + 1] - ch[i0]);
    }

    StereoSample getNextStretchedSample (const Params& p) noexcept
    {
        const auto& audio = p.sample->audio;
        const auto len = (double) p.sample->lengthSamples();
        const auto lastSample = juce::jmax (0.0, len - 1.0);
        auto loopStart = juce::jmin (p.loopStartNorm * len, lastSample);
        auto loopEnd = juce::jmin (p.loopEndNorm * len, lastSample);
        if (loopEnd < loopStart + 64.0) loopEnd = juce::jmin (loopStart + 64.0, lastSample);
        if (loopEnd <= loopStart) loopStart = juce::jmax (0.0, loopEnd - 1.0);
        const auto loopSpan = juce::jmax (1.0, loopEnd - loopStart);

        // ~40 ms grains, halved to ~20 ms right at a detected onset.
        const auto baseLenOut = 0.04 * juce::jmax (1.0, p.engineSampleRate);
        const auto onsetSeconds = stretch.playheadSource / p.sample->sourceSampleRate;
        const auto lenOut = p.sample->onsetNear (onsetSeconds, 0.02) ? baseLenOut * 0.5 : baseLenOut;
        const auto hopOut = baseLenOut * 0.5;   // fixed cadence keeps 50% overlap

        if (stretch.outputCounter <= 0.0)
        {
            auto& g = stretch.grains[(size_t) stretch.nextStream];
            g.sourceStart = stretch.playheadSource;
            g.outPhase = 0.0;
            g.lenOut = lenOut;
            g.active = true;
            stretch.nextStream = (stretch.nextStream + 1) % StretchState::numStreams;
            stretch.outputCounter += hopOut;
        }
        stretch.outputCounter -= 1.0;

        float left = 0.0f, right = 0.0f;
        for (auto& g : stretch.grains)
        {
            if (! g.active) continue;

            const auto readOffset = g.outPhase * p.pitchRatio;
            auto readPos = g.sourceStart + readOffset;
            if (p.loop)
            {
                // Wrap the read position into the loop span (file-position
                // space), matching classic playback's loop semantics.
                if (readPos >= loopEnd)
                    readPos = loopStart + std::fmod (readPos - loopStart, loopSpan);
                else if (readPos < loopStart)
                    readPos = loopStart;
            }
            else
            {
                readPos = juce::jlimit (0.0, lastSample, readPos);
            }

            const auto w = hann (juce::jlimit (0.0, 1.0, g.outPhase / juce::jmax (1.0, g.lenOut)));
            left  += w * readClamped (audio, 0, readPos);
            right += w * readClamped (audio, audio.getNumChannels() > 1 ? 1 : 0, readPos);

            g.outPhase += 1.0;
            if (g.outPhase >= g.lenOut)
                g.active = false;
        }

        stretch.playheadSource += p.stretchRatio;
        position = stretch.playheadSource;   // keep positionSeconds()/followers in sync

        if (p.loop)
        {
            if (stretch.playheadSource >= loopEnd)
                stretch.playheadSource = loopStart + std::fmod (stretch.playheadSource - loopStart, loopSpan);
        }
        else if (stretch.playheadSource >= lastSample)
        {
            // Let already-active grains finish ringing out before stopping.
            bool anyActive = false;
            for (auto& g : stretch.grains) anyActive |= g.active;
            if (! anyActive)
                done = true;
        }

        return { left, right };
    }

    StretchState stretch;

    double position = 0.0;   // in source samples
    bool done = true;
};

} // namespace spa::dsp
