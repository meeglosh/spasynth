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

        // Loop crossfade, 0..1 of the available crossfade room (see
        // computeXfadeSamples). 0 = hard loop, the pre-1.0.19 behaviour.
        double loopXfade = 0.0;

        // Beat-grid snap (LOOP + SYNC): loop points are quantised to the
        // sample's detected beat grid -- a whole number of beats (minimum 1),
        // anchored at the first detected onset -- before use. Applied here at
        // read time, not by rewriting loopStartNorm/loopEndNorm, so turning
        // SYNC off instantly restores the raw knob values.
        bool snapToGrid = false;
        double gridBeatSeconds = 0.5;     // source-time seconds per beat, native tempo
        double gridOffsetSeconds = 0.0;   // source-time anchor (first onset)
    };

    void noteOn (const SampleData* sample, double startNorm) noexcept
    {
        position = sample != nullptr ? startNorm * sample->lengthSamples() : 0.0;
        done = sample == nullptr;
        wrapped = false;
        stretch = StretchState {};
        stretch.playheadSource = position;
    }

    bool isDone() const noexcept { return done; }

    // Playback position in source-file seconds (drives the SFX followers).
    double positionSeconds (const SampleData* sample) const noexcept
    {
        return sample != nullptr ? position / sample->sourceSampleRate : 0.0;
    }

    // Hard-aligns the playhead (both the classic-playback position and the
    // stretcher's read pointer) to an absolute source-sample position, for
    // LOOP+SYNC transport phase lock. Any grain already ringing finishes
    // under its own window and the next spawned grain reads from the new
    // position -- a natural one-grain crossfade rather than a click.
    void alignPlayhead (double sourceSamplePos) noexcept
    {
        position = sourceSamplePos;
        stretch.playheadSource = sourceSamplePos;
    }

    // Current stretcher read pointer, source samples (for transport drift
    // correction -- compare against the transport-derived target).
    double stretchPlayheadSamples() const noexcept { return stretch.playheadSource; }

    // Effective (post-snap) loop bounds in source samples, for callers that
    // need to reason about the loop (transport phase-lock math). Mirrors the
    // snap logic getNextSample/getNextStretchedSample apply internally.
    static void effectiveLoopBoundsSamples (const Params& p, double lengthSamples,
                                             double& loopStartOut, double& loopEndOut) noexcept
    {
        computeLoopBounds (p, lengthSamples, loopStartOut, loopEndOut);
    }

    // Effective loop crossfade length (source samples) and direction, for
    // callers that need to reason about the crossfade window without owning
    // a SamplePlayer -- the UI's loop-crossfade visualization. Mirrors
    // computeXfadeSamples exactly (same pattern as
    // effectiveLoopBoundsSamples above, added for exactly this reason): the
    // display must never re-derive this math, so there is exactly one
    // implementation and the picture can never disagree with what's heard.
    static double effectiveXfadeSamples (const Params& p, double lastSample,
                                         double loopStart, double loopEnd,
                                         bool& usePreRollOut) noexcept
    {
        return computeXfadeSamples (p, lastSample, loopStart, loopEnd, usePreRollOut);
    }

    struct StereoSample { float left = 0.0f, right = 0.0f; };

    StereoSample getNextSample (const Params& p) noexcept
    {
        if (done || p.sample == nullptr || p.sample->lengthSamples() < 2)
            return {};

        if (p.syncOn)
            return getNextStretchedSample (p);

        const auto len = (double) p.sample->lengthSamples();
        const auto lastSample = juce::jmax (0.0, len - 1.0);
        double loopStart, loopEnd;
        computeLoopBounds (p, len, loopStart, loopEnd);

        if (p.loop && position >= loopEnd)
        {
            position = loopStart + std::fmod (position - loopEnd, juce::jmax (1.0, loopEnd - loopStart));
            wrapped = true;
        }

        if (position >= lastSample)
        {
            done = true;
            return {};
        }

        const auto& audio = p.sample->audio;
        const auto rightChan = audio.getNumChannels() > 1 ? 1 : 0;

        bool usePreRoll = true;
        const auto X = p.loop ? computeXfadeSamples (p, lastSample, loopStart, loopEnd, usePreRoll) : 0.0;

        float left, right;

        if (X > 0.0)
        {
            left  = readLoopCrossfaded (audio, 0, position, wrapped, loopStart, loopEnd, X, usePreRoll);
            right = readLoopCrossfaded (audio, rightChan, position, wrapped, loopStart, loopEnd, X, usePreRoll);
        }
        else
        {
            const auto i0 = (int) position;
            const auto frac = (float) (position - (double) i0);

            const auto* ch0 = audio.getReadPointer (0);
            left = ch0[i0] + frac * (ch0[i0 + 1] - ch0[i0]);

            right = left;
            if (audio.getNumChannels() > 1)
            {
                const auto* ch1 = audio.getReadPointer (1);
                right = ch1[i0] + frac * (ch1[i0 + 1] - ch1[i0]);
            }
        }

        position += p.rateRatio;
        return { left, right };
    }

private:
    // Crossfade room: we only ever borrow audio that exists OUTSIDE the loop
    // -- the run-up before loopStart, or the tail after loopEnd -- so the
    // loop period is never shortened (product decision: XFADE must not
    // change timing, which would drift against SYNC's beat grid). Capped at
    // half the loop so a short loop can't crossfade into itself end-to-end.
    // Shared by BOTH the classic path and the SYNC stretcher's per-grain
    // reads (see readLoopCrossfaded) -- the stretcher's overlap-add smooths
    // GRAIN boundaries, not the loop seam itself: each grain's read position
    // still folds from loopEnd back to loopStart mid-grain, so a real
    // discontinuity survives there too, worse on a beat-locked loop that
    // hits the same seam on every repeat.
    //
    // Direction (pre-roll vs. post-roll) is chosen from which side has MORE
    // available room, INDEPENDENT of the knob (loopXfade)/X. Comparing
    // against X instead would make the direction depend on knob position --
    // on a sample with a small run-up but a large tail, low knob values
    // would borrow from the run-up and then, as the knob passed the point
    // where X exceeds preRoom, silently flip to borrowing from the tail,
    // audibly changing character mid-sweep. Choosing by room is stable
    // across the whole knob range; since room == max(preRoom, postRoom),
    // maxX (and therefore X) is unchanged by this -- only the direction is.
    static double computeXfadeSamples (const Params& p, double lastSample,
                                       double loopStart, double loopEnd,
                                       bool& usePreRollOut) noexcept
    {
        usePreRollOut = true;
        if (! p.loop || p.loopXfade <= 0.0)
            return 0.0;

        const auto preRoom  = loopStart;
        const auto postRoom = juce::jmax (0.0, lastSample - loopEnd);
        const auto usePreRoll = (preRoom >= postRoom);
        const auto room = usePreRoll ? preRoom : postRoom;
        const auto maxX = juce::jmin ((loopEnd - loopStart) * 0.5, room);
        const auto X = juce::jlimit (0.0, maxX, p.loopXfade * maxX);

        usePreRollOut = usePreRoll;
        return X;
    }

    // Reads the source at `pos` (already folded into the loop) applying the
    // loop crossfade. Shared by the classic path (getNextSample) and the
    // SYNC stretcher's per-grain reads (getNextStretchedSample) so the two
    // can never disagree. `hasWrapped` says whether this read head has
    // already passed loopEnd at least once: the post-roll form must not
    // blend post-loop tail material into the very FIRST pass through the
    // loop start, because at that point nothing has been heard from after
    // loopEnd yet. Returns the plain interpolated sample unchanged when
    // xfadeSamples <= 0 or pos is outside the crossfade window.
    // Position-domain (source samples), so it's rate-agnostic: correct under
    // both classic playback and time-stretch, since X naturally stretches
    // with the material.
    static float readLoopCrossfaded (const juce::AudioBuffer<float>& audio, int channel,
                                     double pos, bool hasWrapped,
                                     double loopStart, double loopEnd,
                                     double xfadeSamples, bool usePreRoll) noexcept
    {
        const auto X = xfadeSamples;

        if (X > 0.0 && usePreRoll && pos < loopEnd && pos >= loopEnd - X)
        {
            // Pre-roll: crossfade the last X samples before the wrap into a
            // preview of what plays immediately after it (the same
            // material, read one loop-length ahead of loopStart) -- so by
            // the time the wrap actually fires, we're already fully on the
            // incoming side and the wrap itself is inaudible. Correct on
            // the very first pass through the tail, no state needed.
            const auto d = pos - (loopEnd - X);
            const auto w = juce::jlimit (0.0, 1.0, d / X);
            const auto ang = w * juce::MathConstants<double>::halfPi;
            const auto gOut = (float) std::cos (ang);
            const auto gIn  = (float) std::sin (ang);
            const auto incomingPos = pos - (loopEnd - loopStart);

            return gOut * readClamped (audio, channel, pos) + gIn * readClamped (audio, channel, incomingPos);
        }

        if (X > 0.0 && hasWrapped && ! usePreRoll && pos >= loopStart && (pos - loopStart) < X)
        {
            // Post-roll: no run-up room before loopStart, so instead the
            // material just after loopStart (incoming) is crossfaded with
            // the tail that would have played had the previous lap
            // continued past loopEnd (outgoing) -- borrowed from just after
            // loopEnd. Only valid once we've actually wrapped once.
            const auto d = pos - loopStart;
            const auto w = juce::jlimit (0.0, 1.0, d / X);
            const auto ang = w * juce::MathConstants<double>::halfPi;
            const auto gOut = (float) std::cos (ang);
            const auto gIn  = (float) std::sin (ang);
            const auto outgoingPos = loopEnd + d;

            return gIn * readClamped (audio, channel, pos) + gOut * readClamped (audio, channel, outgoingPos);
        }

        return readClamped (audio, channel, pos);
    }

    // Effective loop bounds in source samples: raw loopStartNorm/loopEndNorm
    // * length, optionally quantised to the beat grid (see snapToGrid), then
    // clamped/degenerate-guarded exactly as before snapping was added. The
    // interpolator reads [i0, i0+1], so the last sample we can ever legally
    // start an interpolation from is len - 1; a loopEndNorm of 1.0 (loop the
    // whole file) maps to exactly `len`, one sample past that limit, so both
    // bounds are clamped to lastSample -- otherwise the unconditional
    // end-of-buffer cutoff fires before position ever reaches loopEnd and
    // whole-file loops play once and stop. Direction-agnostic (loop or
    // reverse playback both read this the same way).
    static void computeLoopBounds (const Params& p, double len, double& loopStartOut, double& loopEndOut) noexcept
    {
        const auto lastSample = juce::jmax (0.0, len - 1.0);
        auto rawStart = p.loopStartNorm * len;
        auto rawEnd = p.loopEndNorm * len;

        if (p.snapToGrid && p.gridBeatSeconds > 1.0e-6 && p.sample != nullptr)
        {
            const auto beatSamples = p.gridBeatSeconds * p.sample->sourceSampleRate;
            const auto offsetSamples = p.gridOffsetSeconds * p.sample->sourceSampleRate;
            auto snap = [&] (double pos)
            {
                const auto k = std::round ((pos - offsetSamples) / beatSamples);
                return offsetSamples + k * beatSamples;
            };
            rawStart = snap (rawStart);
            rawEnd = snap (rawEnd);
            if (rawEnd < rawStart + beatSamples)
                rawEnd = rawStart + beatSamples;   // whole-beat length, minimum 1 beat
            // A near-file-start loopStart can snap to a NEGATIVE grid line
            // (the nearest one may be before t=0, e.g. if the first onset
            // sits more than half a beat in). Shift BOTH bounds by the same
            // whole-beat amount to land non-negative -- shifting rawStart
            // alone would silently collide it back into rawEnd above,
            // corrupting the just-computed whole-beat length.
            if (rawStart < 0.0)
            {
                const auto shift = std::ceil (-rawStart / beatSamples) * beatSamples;
                rawStart += shift;
                rawEnd += shift;
            }
        }

        loopStartOut = juce::jmin (rawStart, lastSample);
        loopEndOut = juce::jmin (rawEnd, lastSample);
        if (loopEndOut < loopStartOut + 64.0)
            loopEndOut = juce::jmin (loopStartOut + 64.0, lastSample);
        if (loopEndOut <= loopStartOut)
            loopStartOut = juce::jmax (0.0, loopEndOut - 1.0);
    }

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
        double loopStart, loopEnd;
        computeLoopBounds (p, len, loopStart, loopEnd);
        const auto loopSpan = juce::jmax (1.0, loopEnd - loopStart);
        const auto rightChan = audio.getNumChannels() > 1 ? 1 : 0;

        bool usePreRoll = true;
        const auto X = p.loop ? computeXfadeSamples (p, lastSample, loopStart, loopEnd, usePreRoll) : 0.0;

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
            // `wrapped` (has the PLAYHEAD looped at least once, not this
            // grain individually) is shared by every grain so overlapping
            // grains reading the same region always agree on which
            // crossfade form applies -- otherwise two grains straddling a
            // wrap could disagree about the source value at the same
            // position and the overlap-add would reconstruct a hybrid
            // rather than the intended equal-power law. It is also more
            // correct: a grain spawned just before the wrap and still
            // ringing across it is genuinely reading its own continuation,
            // not borrowed material from an unrelated earlier pass.
            left  += w * readLoopCrossfaded (audio, 0, readPos, wrapped, loopStart, loopEnd, X, usePreRoll);
            right += w * readLoopCrossfaded (audio, rightChan, readPos, wrapped, loopStart, loopEnd, X, usePreRoll);

            g.outPhase += 1.0;
            if (g.outPhase >= g.lenOut)
                g.active = false;
        }

        stretch.playheadSource += p.stretchRatio;
        position = stretch.playheadSource;   // keep positionSeconds()/followers in sync

        if (p.loop)
        {
            if (stretch.playheadSource >= loopEnd)
            {
                stretch.playheadSource = loopStart + std::fmod (stretch.playheadSource - loopStart, loopSpan);
                wrapped = true;
            }
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
    bool wrapped = false;    // has looped at least once since noteOn (post-roll crossfade gate)
};

} // namespace spa::dsp
